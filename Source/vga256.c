/*------------------------------------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <conio.h>
#include <malloc.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <mem.h>
#include "vga256.h"



/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)

#define VBE2SIGNATURE "VBE2"
#define VBE3SIGNATURE "VBE3"

static struct rminfo
{
  unsigned long EDI;
  unsigned long ESI;
  unsigned long EBP;
  unsigned long reserved_by_system;
  unsigned long EBX;
  unsigned long EDX;
  unsigned long ECX;
  unsigned long EAX;
  unsigned short flags;
  unsigned short ES,DS,FS,GS,IP,CS,SP,SS;
} RMI;

 /*
  * Structures that hold the preallocated DOS-Memory Aeras
  * and their translated near-pointers!
  */

static struct DPMI_PTR VbeInfoPool = {0,0};
static struct DPMI_PTR VbeModePool = {0,0};
static struct VBE_VbeInfoBlock  *VBE_Controller_Info_Pointer;
static struct VBE_ModeInfoBlock *VBE_ModeInfo_Pointer;
static void   *LastPhysicalMapping = NULL;
/*
 * Structures, that hold the Informations needed to invoke
 * PM-Interrupts
 */

static union  REGS  regs;
static struct SREGS sregs;

 /*
  * Some informations 'bout the last mode which was set
  * These informations are required to compensate some differencies
  * between the normal and direct PM calling methods
  */

static signed short  vbelastbank = -1;
static unsigned long BytesPerScanline;
static unsigned char BitsPerPixel;

 /*
  * Protected Mode Direct Call Informations
  */

void VBE_InitPM (void);
static void * pm_setwindowcall;
static void * pm_setdisplaystartcall;
static void * pmcode = NULL;


/*
 *  This function pointers will be initialized after you called
 *  VBE_Init. It'll be set to the realmode or protected mode call
 *  code
 */

tagSetDisplayStartType VBE_SetDisplayStart;
tagSetBankType         VBE_SetBank;

void Mystique_ModeInformation(short Mode, struct VBE_ModeInfoBlock * a);
void Mystique_SetMode(short Mode, int linear, int clear);
int  Mystique_Detect(void);
int  Mystique_FindMode(int xres, int yres, char bpp);
static int    mga_maxmode;
static int    mga_detect = 0;
static char   *mgabase1;       /* 16k register address space */

#pragma pack (pop)

static void PrepareRegisters (void)
{
  memset(&RMI,0,sizeof(RMI));
  memset(&sregs,0,sizeof(sregs));
  memset(&regs,0,sizeof(regs));
}

static void RMIRQ (char irq)
{
  memset (&regs, 0, sizeof (regs));
  regs.w.ax =  0x0300;               // Simulate Real-Mode interrupt
  regs.h.bl =  irq;
  sregs.es =   FP_SEG(&RMI);
  regs.x.edi = FP_OFF(&RMI);
  int386x(0x31, &regs, &regs, &sregs);
}

void DPMI_AllocDOSMem (short int paras, struct DPMI_PTR *p)
{
  /* DPMI call 100h allocates DOS memory */
  PrepareRegisters();
  regs.w.ax=0x0100;
  regs.w.bx=paras;
  int386x( 0x31, &regs, &regs, &sregs);
  p->segment=regs.w.ax;
  p->selector=regs.w.dx;
}

void DPMI_FreeDOSMem (struct DPMI_PTR *p)
{
  /* DPMI call 101h free DOS memory */
  memset(&sregs,0,sizeof(sregs));
  regs.w.ax=0x0101;
  regs.w.dx=p->selector;
  int386x( 0x31, &regs, &regs, &sregs);
}

void DPMI_UNMAP_PHYSICAL (void *p)
{
  /* DPMI call 800h map physical memory*/
  PrepareRegisters();
  regs.w.ax=0x0801;
  regs.w.bx=(unsigned short) (((unsigned long)p)>>16);
  regs.w.cx=(unsigned short) (((unsigned long)p)&0xffff);
  int386x( 0x31, &regs, &regs, &sregs);
}

void * DPMI_MAP_PHYSICAL (void *p, unsigned long size)
{
  /* DPMI call 800h map physical memory*/
  PrepareRegisters();
  regs.w.ax=0x0800;
  regs.w.bx=(unsigned short) (((unsigned long)p)>>16);
  regs.w.cx=(unsigned short) (((unsigned long)p)&0xffff);
  regs.w.si=(unsigned short) (((unsigned long)size)>>16);
  regs.w.di=(unsigned short) (((unsigned long)size)&0xffff);
  int386x( 0x31, &regs, &regs, &sregs);
  return (void *) ((regs.w.bx << 16) + regs.w.cx);
}

void VBE_Controller_Information  (struct VBE_VbeInfoBlock * a)
{
  memcpy (a, VBE_Controller_Info_Pointer, sizeof (struct VBE_VbeInfoBlock));
}

void VBE_Mode_Information (short Mode, struct VBE_ModeInfoBlock * a)
{
  if ( (mga_detect) && (Mode>mga_maxmode)) {
    Mystique_ModeInformation (Mode, a);
    return;
  }
  PrepareRegisters();
  RMI.EAX=0x00004f01;               // Get SVGA-Mode Information
  RMI.ECX=Mode;
  RMI.ES=VbeModePool.segment;       // Segment of realmode data
  RMI.EDI=0;                        // offset of realmode data
  RMIRQ (0x10);
  memcpy (a, VBE_ModeInfo_Pointer, sizeof (struct VBE_ModeInfoBlock));
}

int VBE_IsModeLinear (short Mode)
{
  struct VBE_ModeInfoBlock a;
  if ( Mode==0x13 ) {
    return 1;
  }
  VBE_Mode_Information (Mode, &a);
  return ((a.ModeAttributes & 128)==128);
}

void asmSDS (short lowaddr, short hiaddr);
#pragma aux asmSDS = "mov ax, 0x4f07" \
                     "xor ebx, ebx" \
                     "call [pm_setdisplaystartcall]" \
                     parm[cx][dx] modify [eax ebx ecx edx esi edi];

static void PM_SetDisplayStart (short x, short y)
{
  unsigned long  addr   = (y*BytesPerScanline+x);
  unsigned short loaddr = addr & 0xffff;
  unsigned short hiaddr = (addr>>16);
  asmSDS(loaddr, hiaddr);
}

void asmSB (short bnk);
#pragma aux asmSB = "mov ax, 0x4f05" \
                    "mov bx, 0" \
                    "call [pm_setwindowcall]" \
                    parm [dx] modify [eax ebx ecx edx esi edi];

static void PM_SetBank (short bnk)
{
  if ( bnk==vbelastbank ) return;
  vbelastbank=bnk;
  asmSB(bnk);
}

static void RM_SetDisplayStart (short x, short y)
{
  PrepareRegisters();
  RMI.EAX=0x00004f07;
  RMI.EBX=0;
  RMI.ECX=x;
  RMI.EDX=y;
  RMIRQ (0x10);
}

void setbiosmode (unsigned short c);
#pragma aux setbiosmode = "int 0x10" parm[ax] modify[eax ebx ecx edx esi edi];

void VBE_SetMode (short Mode, int linear, int clear)
{
  struct VBE_ModeInfoBlock a;
  int rawmode;
  /* Is it a normal textmode? if so set it directly! */
  if ( Mode == 3 ) {
    setbiosmode (Mode);
    return;
  }
  if ( Mode == 0x13 ) {
    setbiosmode (Mode);
    return;
  }
  if ( (mga_detect) && (Mode>mga_maxmode)) {
    Mystique_SetMode (Mode, linear, clear);
    return;
  }
  rawmode = Mode & 0x01ff;
  if ( linear) rawmode |= 1<<14;
  if (!clear)  rawmode |= 1<<15;
  if ( linear && clear ) {
    /*
     * in case of a S3 card the following bugfix prevents a system-crash
     * when a mode with lfb+clearscreen will be set. We simulate the same
     * behaviour doing the clearscreen without LFB and setting the LFB
     * without clearscreen afterwards (a little bit messy...)
     */

    PrepareRegisters();
    RMI.EAX=0x00004f02;
    RMI.EBX=Mode & 0x01ff;                    // mode without LFB but clear
    RMIRQ (0x10);

    PrepareRegisters();
    RMI.EAX=0x00004f02;
    RMI.EBX=(Mode & 0x01ff)|(1<<14)|(1<<15);  // mode with LFB but without clear
    RMIRQ (0x10);
  } else {
    PrepareRegisters();
    RMI.EAX=0x00004f02;
    RMI.EBX=rawmode;
    RMIRQ (0x10);
  }
}

int VBE_Test (void)
{
  return (VBE_Controller_Info_Pointer->vbeVersion.hi>=0x2);
}

unsigned int VBE_VideoMemory (void)
{
  return (VBE_Controller_Info_Pointer->TotalMemory*1024*64);
}

int VBE_FindMode (int xres, int yres, char bpp)
{
  int i;
  struct VBE_ModeInfoBlock Info;
  // First try to find the mode in the ControllerInfoBlock:
  for (i=0; VBE_Controller_Info_Pointer->VideoModePtr[i]!=0xffff; i++ ) {
    VBE_Mode_Information (VBE_Controller_Info_Pointer->VideoModePtr[i], &Info);
    if ((xres == Info.XResolution) &&
        (yres == Info.YResolution) &&
        (bpp == Info.BitsPerPixel)) {
          return VBE_Controller_Info_Pointer->VideoModePtr[i];
        }
  }
  if ( mga_detect ) {
    return Mystique_FindMode (xres, yres, bpp);
  }
  return -1;
}

char * VBE_GetVideoPtr (short mode)
{
  void *phys;
  struct VBE_ModeInfoBlock ModeInfo;
  if ( mode==13 ) return (char *) 0xa0000;
  VBE_Mode_Information (mode, &ModeInfo);
  /* Unmap the last physical mapping (if there is one...) */
  if ( LastPhysicalMapping ) {
    DPMI_UNMAP_PHYSICAL (LastPhysicalMapping);
    LastPhysicalMapping = NULL;
  }
  LastPhysicalMapping = DPMI_MAP_PHYSICAL ((void *) ModeInfo.PhysBasePtr,
         (long)(VBE_Controller_Info_Pointer->TotalMemory)*64*1024);
  return (char *) LastPhysicalMapping;
}

void RM_SetBank (short bnk)
{
  if ( bnk==vbelastbank ) return;
  PrepareRegisters();
  RMI.EAX=0x00004f05;
  RMI.EBX=0;
  RMI.EDX=bnk;
  RMIRQ (0x10);
  vbelastbank=bnk;
}

short VBE_MaxBytesPerScanline (void)
{
  PrepareRegisters();
  RMI.EAX=0x00004f06;
  RMI.EBX=3;
  RMIRQ (0x10);
  return regs.w.bx;
}

void VBE_SetPixelsPerScanline (short Pixels)
{
  PrepareRegisters();
  RMI.EAX=0x00004f06;
  RMI.EBX=0;
  RMI.ECX=Pixels;
  RMIRQ (0x10);
  BytesPerScanline = (Pixels*BitsPerPixel/8);
}

void VBE_SetDACWidth (char bits)
{
  PrepareRegisters();
  RMI.EAX=0x00004f08;
  RMI.EBX=bits<<8;
  RMIRQ (0x10);
}

int VBE_8BitDAC (void)
{
  return (VBE_Controller_Info_Pointer->Capabilities & 1 );
}

void VBE_InitPM (void)
{
  unsigned short *pm_pointer;
  VBE_SetDisplayStart = RM_SetDisplayStart;
  VBE_SetBank         = RM_SetBank;
  PrepareRegisters();
  RMI.EAX=0x00004f0a;
  RMIRQ (0x10);
  if ( (RMI.EAX) == 0x004f ) {
    if (pmcode) free (pmcode);
    // get some memory to copy the stuff.
    pmcode = malloc (RMI.ECX  & 0x0000ffff);
    pm_pointer =   (unsigned short *) (((unsigned long )RMI.ES<<4)|(RMI.EDI));
    memcpy (pmcode, pm_pointer, (RMI.ECX  & 0x0000ffff));
    pm_pointer = (unsigned short *) pmcode;
    pm_setwindowcall =       (void *) (((unsigned long )RMI.ES<<4)|(RMI.EDI+pm_pointer[0]));
    pm_setdisplaystartcall = (void *) (((unsigned long )RMI.ES<<4)|(RMI.EDI+pm_pointer[1]));
    VBE_SetDisplayStart = PM_SetDisplayStart;
    VBE_SetBank         = PM_SetBank;
  }
}

void VBE_Init (void)
{
  /* Allocate the Dos Memory for Mode and Controller Information Blocks */
  /* and translate their pointers into flat memory space                */
  DPMI_AllocDOSMem (512/16, &VbeInfoPool);
  DPMI_AllocDOSMem (256/16, &VbeModePool);
  VBE_ModeInfo_Pointer =  (struct VBE_ModeInfoBlock *) (VbeModePool.segment*16);
  VBE_Controller_Info_Pointer = (struct VBE_VbeInfoBlock *) (VbeInfoPool.segment*16);

  /* Get Controller Information Block only once and copy this block on  */
  /* all requests                                                       */
  memset (VBE_Controller_Info_Pointer, 0, sizeof (struct VBE_VbeInfoBlock));
  memcpy (VBE_Controller_Info_Pointer->vbeSignature, VBE2SIGNATURE, 4);
  PrepareRegisters();
  RMI.EAX=0x00004f00;               // Get SVGA-Information
  RMI.ES=VbeInfoPool.segment;       // Segment of realmode data
  RMI.EDI=0;                        // offset of realmode data
  RMIRQ (0x10);
  // Translate the Realmode Pointers into flat-memory address space
  VBE_Controller_Info_Pointer->OemStringPtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemStringPtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemStringPtr);
  VBE_Controller_Info_Pointer->VideoModePtr=(unsigned short*)((((unsigned long)VBE_Controller_Info_Pointer->VideoModePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->VideoModePtr);
  VBE_Controller_Info_Pointer->OemVendorNamePtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemVendorNamePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemVendorNamePtr);
  VBE_Controller_Info_Pointer->OemProductNamePtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemProductNamePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemProductNamePtr);
  VBE_Controller_Info_Pointer->OemProductRevPtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemProductRevPtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemProductRevPtr);

  VBE_InitPM();

  Mystique_Detect ();
}

void VBE_Done (void)
{
  if ( LastPhysicalMapping ) {
    DPMI_UNMAP_PHYSICAL (LastPhysicalMapping);
  }
  DPMI_FreeDOSMem (&VbeModePool);
  DPMI_FreeDOSMem (&VbeInfoPool);
  if (pmcode) free (pmcode);
}

unsigned long PCI_ReadDword (unsigned char id, short offset)
{
  PrepareRegisters();
  RMI.EAX=0x0000b10a;
  RMI.EBX=id<<3;
  RMI.EDI=offset;
  RMIRQ (0x1a);
  return RMI.ECX;
}

int Mystique_Detect (void)
{
  int i;
  unsigned long value;
  /*-----------------05-25-97 04:05pm-----------------
   * Check for the PCI BIOS extionsions
   * --------------------------------------------------*/
  mga_detect = 0;
  PrepareRegisters();
  RMI.EAX=0x0000b101;
  RMIRQ (0x1a);
  if (!((RMI.EDX == 0x20494350) && ((RMI.flags &1 )==0))) return 0;

  for ( i=0; i<32; i++ )  {
    value = PCI_ReadDword (i, 0);
    if (( (value & 0xffff) == 0x102b ) && ((value >> 16) == 0x051a))
    {
      /* Matrox Mystique Detect successfull */
      mga_detect = 1;
      /* Get Mystique Memory Mapped IO-Area */
      mgabase1 = (char *) DPMI_MAP_PHYSICAL((void *)(PCI_ReadDword (i, 0x10) & 0xfffffff0), 16*1024);
      /* Scan the BIOS Mode List. Find the highest mode-number */
      mga_maxmode = 0;
      for (i=0; VBE_Controller_Info_Pointer->VideoModePtr[i]!=0xffff; i++ )
        if ( VBE_Controller_Info_Pointer->VideoModePtr[i]>mga_maxmode)
          mga_maxmode = VBE_Controller_Info_Pointer->VideoModePtr[i];
      mga_maxmode+=10;
      return 1;
    }
  }
  return 0;
}

void Mystique_Zoom (int activate)
/*-----------------05-25-97 04:06pm-----------------
 * Enable or Disable the Matrox Mystique Hardware Zoom
 * --------------------------------------------------*/
{
  char val;
  if ( activate ) {
    // Zoom X Enable (DAC Control)
    mgabase1[0x3c00]=0x38;
    mgabase1[0x3c0a]=1;
    // Zoom Y Enable (CRTCX Control)
    mgabase1[0x1fd4]=0x09;
    val= mgabase1[0x1fd5];
    mgabase1[0x1fd4]=0x09;
    mgabase1[0x1fd5]=val|128;
  } else {
    // Zoom X Disable (DAC Control)
    mgabase1[0x3c00]=0x38;
    mgabase1[0x3c0a]=0;
    // Zoom Y Disable (CRTCX Control)
    mgabase1[0x1fd4]=0x09;
    val= mgabase1[0x1fd5];
    mgabase1[0x1fd4]=0x09;
    mgabase1[0x1fd5]=val &~128;
  }
}

int Mystique_FindMode (int xres, int yres, char bpp)
{
  int usemode=-1;
  if ( !mga_detect ) return -1;
  switch ( bpp ) {
        case 8:
          if ((xres==320) && (yres==240)) usemode =  mga_maxmode+1;
          if ((xres==400) && (yres==300)) usemode =  mga_maxmode+2;
          if ((xres==512) && (yres==384)) usemode =  mga_maxmode+3;
          break;
        case 15:
          if ((xres==320) && (yres==200)) usemode =  mga_maxmode+4;
          if ((xres==320) && (yres==240)) usemode =  mga_maxmode+5;
          if ((xres==400) && (yres==300)) usemode =  mga_maxmode+6;
          if ((xres==512) && (yres==384)) usemode =  mga_maxmode+7;
          break;
        case 16:
          if ((xres==320) && (yres==200)) usemode =  mga_maxmode+8;
          if ((xres==320) && (yres==240)) usemode =  mga_maxmode+9;
          if ((xres==400) && (yres==300)) usemode =  mga_maxmode+10;
          if ((xres==512) && (yres==384)) usemode =  mga_maxmode+11;
          break;
        case 32:
          if ((xres==320) && (yres==200)) usemode =  mga_maxmode+12;
          if ((xres==320) && (yres==240)) usemode =  mga_maxmode+13;
          if ((xres==400) && (yres==300)) usemode =  mga_maxmode+14;
          if ((xres==512) && (yres==384)) usemode =  mga_maxmode+15;
          break;
  }
  return usemode;
}

int Mystique_TranslateMode (short mode)
{
 int usemode = -1;
 if (mode == mga_maxmode+1)  usemode =   VBE_FindMode (640,480,8);
 if (mode == mga_maxmode+2)  usemode =   VBE_FindMode (800,600,8);
 if (mode == mga_maxmode+3)  usemode =   VBE_FindMode (1024,768,8);
 if (mode == mga_maxmode+4)  usemode =   VBE_FindMode (640,400,15);
 if (mode == mga_maxmode+5)  usemode =   VBE_FindMode (640,480,15);
 if (mode == mga_maxmode+6)  usemode =   VBE_FindMode (800,600,15);
 if (mode == mga_maxmode+7)  usemode =   VBE_FindMode (1024,768,15);
 if (mode == mga_maxmode+8)  usemode =   VBE_FindMode (640,400,16);
 if (mode == mga_maxmode+9)  usemode =   VBE_FindMode (640,480,16);
 if (mode == mga_maxmode+10) usemode =   VBE_FindMode (800,600,16);
 if (mode == mga_maxmode+11) usemode =   VBE_FindMode (1024,768,16);
 if (mode == mga_maxmode+12) usemode =   VBE_FindMode (640,400,32);
 if (mode == mga_maxmode+13) usemode =   VBE_FindMode (640,480,32);
 if (mode == mga_maxmode+14) usemode =   VBE_FindMode (800,600,32);
 if (mode == mga_maxmode+15) usemode =   VBE_FindMode (1024,768,32);
 return usemode;
}

void Mystique_ModeInformation (short Mode, struct VBE_ModeInfoBlock * a)
{
  int vbemode;
  vbemode = Mystique_TranslateMode (Mode);
  if ( vbemode!=-1 ) {
    /* Create a faked Vesa Mode Information Block */
    PrepareRegisters();
    RMI.EAX=0x00004f01;
    RMI.ECX=vbemode;
    RMI.ES=VbeModePool.segment;
    RMI.EDI=0;
    RMIRQ (0x10);
    memcpy (a, VBE_ModeInfo_Pointer, sizeof (struct VBE_ModeInfoBlock));
    a->XResolution >>= 1;
    a->YResolution >>= 1;
    a->BytesPerScanline >>= 1;
  }
}

void Mystique_SetBytesPerScanline (int nbytes)
{
  char lo, hi, val;
  nbytes>>=4;
  lo=nbytes&0xff;
  hi=(nbytes>>8)&3;
  mgabase1[0x1fd4]=0x13;
  mgabase1[0x1fd5]=lo;
  mgabase1[0x1fde]=0x00;
  val = mgabase1[0x1fdf];
  val&=0xcf; val|= (hi<<4);
  mgabase1[0x1fde]=0x00;
  mgabase1[0x1fdf]=val;
}

void Mystique_SetMode (short Mode, int linear, int clear)
{
  struct VBE_ModeInfoBlock a;
  int rawmode;
  if ( Mode> mga_maxmode ) {
    rawmode = Mystique_TranslateMode (Mode & 0x01ff);
  } else {
    rawmode = (Mode & 0x01ff);
  }
  if ( linear) rawmode |= 1<<14;
  if (!clear)  rawmode |= 1<<15;
  PrepareRegisters();
  RMI.EAX=0x00004f02;               // Get SVGA-Mode Information
  RMI.EBX=rawmode;
  RMIRQ (0x10);
  VBE_Mode_Information (Mode, &a);
  BytesPerScanline = a.BytesPerScanline;
  BitsPerPixel = a.BitsPerPixel;
  if (Mode>mga_maxmode) Mystique_Zoom (1); else Mystique_Zoom (0);
  Mystique_SetBytesPerScanline (a.BytesPerScanline);
}



#if defined(VGA256_MODE_320X200X8)
	#define VGA256OFFSET(pVideo, x, y) (((unsigned char *) pVideo)[(y << 8) + (y << 6) + x])
#elif defined(VGA256_MODE_640X480X8)
    #define VGA256OFFSET(pVideo, x, y) (((unsigned char *) pVideo)[(y << 9) + (y << 7) + x])
#elif defined(VGA256_MODE_1024X768X8)
    #define VGA256OFFSET(pVideo, x, y) (((unsigned char *) pVideo)[(y << 10) + x])
#endif


/*------------------------------------------------------------------------------------------------------- */
#define peekb(s,o)			(*((unsigned char *) MK_FP((s),(o))))
#define peekw(s,o)			(*((unsigned short *) MK_FP((s),(o))))
#define peekl(s,o)			(*((unsigned long *) MK_FP((s),(o))))
#define pokeb(s,o,x)		(*((unsigned char *) MK_FP((s),(o))) = (unsigned char)(x))
#define pokew(s,o,x)		(*((unsigned short *) MK_FP((s),(o))) = (unsigned short)(x))
#define pokel(s,o,x)		(*((unsigned long *) MK_FP((s),(o))) = (unsigned long)(x))


/*------------------------------------------------------------------------------------------------------- */
void VGA256PutPixel(void *pVideo, unsigned int x, unsigned int y, unsigned int color)
{
	 VGA256OFFSET(pVideo, x, y) = color;  
}


/*------------------------------------------------------------------------------------------------------- */
unsigned int VGA256GetPixel(void *pVideo, unsigned int x, unsigned int y)
{
	return(VGA256OFFSET(pVideo, x, y));
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256LineH(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int color)
{
	memset(&VGA256OFFSET(pVideo, x, y), color, width);
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256LineV(void *pVideo, unsigned int x, unsigned int y, unsigned int height, unsigned int color)
{
	unsigned int h;
	unsigned char *offset = (unsigned char *) &VGA256OFFSET(pVideo, x, y);
	
	
	for (h = 0; h < height; h++)
	{
		*offset = color;
		offset += VGA256_WIDTH;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256DrawBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
	VGA256LineV(pVideo, x, y, width, color);
	VGA256LineV(pVideo, x + width, y, height, color);
	VGA256LineH(pVideo, x, y, width + 1, color);
	VGA256LineH(pVideo, x, y + height, width + 1, color);
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256FillBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
	unsigned int h;
	
	for(h = 0; h < height; h++)
	{
		VGA256LineH(pVideo, x, y+h, width, color);
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256ClearScreen(void *pVideo, unsigned int color)
{
	memset(&VGA256OFFSET(pVideo, 0, 0), color, VGA256_WIDTH * VGA256_HEIGHT);
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256PutSprite(void *pVideo, void *pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight)
{
	unsigned int h;
	unsigned char *offset = (unsigned char *) &VGA256OFFSET(pVideo, piX, piY);
	
	
	for(h = 0; h < piHeight; h++)
	{
		_VGA256MemCpy0((void *) offset, (void *) pcSource, (size_t) piWidth);
		offset += VGA256_WIDTH;
		(unsigned char *) pcSource += piWidth;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256PutImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight)
{
    unsigned int h;
    unsigned char* offset = (unsigned char*)&VGA256OFFSET(pVideo, piX, piY);


    for (h = 0; h < piHeight; h++)
    {
        memcpy((void*) offset, (void*) pcSource, (size_t) piWidth);
        offset += VGA256_WIDTH;
        (unsigned char*)pcSource += piWidth;
    }
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256GetImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight)
{
    unsigned int h;
    unsigned char* offset = (unsigned char*)&VGA256OFFSET(pVideo, piX, piY);


    for (h = 0; h < piHeight; h++)
    {
        memcpy((void*) pcSource, (void*) offset, (size_t) piWidth);
        offset += VGA256_WIDTH;
        (unsigned char*) pcSource += piWidth;
    }
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int x, y;
    unsigned int ywidthd, yheightsheightdy;

    for (y = 0; y < heightd; y++)
    {
        ywidthd = y * widthd;
        yheightsheightdy = y * heights / heightd;
        for (x = 0; x < widthd; x++)
        {
            //pDest[y * widthd + x] = pSource[(y * heights / heightd) * widths + (x * widths / widthd)];
            pDest[ywidthd + x] = pSource[(yheightsheightdy) * widths + (x * widths / widthd)];
        }
    }
}


void VGA256ScaleImageKO2(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned char pixel;
    unsigned int runw, runh;
    unsigned int carryw, carryh;
    div_t widthr, heightr;

    heightr = div(heightd, heights);
    widthr = div(widthd, widths);

    runh = 0;
    carryh = 0;
    for (h = 0; h < heights; h++)
    {
        runh += heightr.quot;
        carryh += heightr.rem;
        if (carryh >= heights)
        {
            carryh = 0;
            runh++;
        }
        if (runh > 0)
        {
            runw = 0;
            carryw = 0;
            for (w = 0; w < widths; w++)
            {
                runw += widthr.quot;
                carryw += widthr.rem;
                if (carryw >= widths)
                {
                    carryw = 0;
                    runw++;
                }
                pixel = *pSource;
                pSource++;
                while (runw > 0)
                {
                    *pDest = pixel;
                    pDest++;
                    runw--;
                }
            }
            runh--;
            while (runh > 0)
            {
                memcpy(pDest, pDest - widthd, widthd);
                pDest += widthd;
                runh--;
            }
        }
        else
        {
            pSource += widths;
        }
    }
}

/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImageKO(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int widthrun, widthtarget;
    unsigned int heightrun, heighttarget;
    unsigned int h, w;
    unsigned char pixel;


    heightrun = 0;
    heighttarget = 0;
    for (h = 0; h < heights; h++)
    {
        widthrun = 0;
        widthtarget = 0;
        for (w = 0; w < widths; w++)
        {
            widthtarget += widthd;
            pixel = *pSource;
            pSource++;
            while (widthrun < widthtarget)
            {
                *pDest = pixel;
                pDest++;
                widthrun += widths;
            }
        }

        heighttarget += heightd;
        while (heightrun < heighttarget)
        {
            memcpy(pDest, pDest - widthd, widthd);
            pDest += widthd;
            heightrun += heights;
        }
    }
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage05x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned char c;

    for (h = 0; h < heights; h += 2)
    {
        for (w = 0; w < widths; w += 2)
        {
            *pDest = *pSource;
            pSource += 2;
            pDest++;
        }
        pSource += widths;
    }
}

/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage025x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned char c;

    for (h = 0; h < heights; h += 4)
    {
        for (w = 0; w < widths; w += 4)
        {
            *pDest = *pSource;
            pSource += 4;
            pDest++;
        }
        pSource += widths * 3;
    }
}




/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage2x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned int widthd = widths << 1;
    unsigned short* d;
    unsigned char c;

    d = (unsigned short*)pDest;
    for (h = 0; h < heights; h++)
    {
        for (w = 0; w < widths; w++)
        {
            c = *pSource;
            pSource++;
            *d = (c << 8) | c;
            d++;
        }
        memcpy(d, d - widths, widthd);
        d += widths;
    }
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage4x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned int widthd = widths << 2;
    unsigned int* d;
    unsigned char c;

    d = (unsigned int*)pDest;
    for (h = 0; h < heights; h++)
    {
        for (w = 0; w < widths; w++)
        {
            c = *pSource;
            pSource++;
            *d = (c << 24) | (c << 16) | (c << 8) | c;
            d++;
        }
        memcpy(d, d - widths, widthd);
        d += widths;
        memcpy(d, d - widths, widthd);
        d += widths;
        memcpy(d, d - widths, widthd);
        d += widths;
    }
}





/*------------------------------------------------------------------------------------------------------- */
void VGA256WaitVRetrace(void)
{
    while ((inp(0x03DA) & 8) != 0);
    while ((inp(0x03DA) & 8) == 0);
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256FadeOut(void)
{
    unsigned int x, y;
    char paleta[768];

    VGA256GetPalette(paleta);

    for (y = 0; y < 63; y++)
    {
        for (x = 0; x < sizeof(paleta); x++)
        {
            if (paleta[x] > 0)
            {
                paleta[x]--;
            }
        }
        VGA256WaitVRetrace();
        VGA256SetPalette(paleta);
    }
}

/*------------------------------------------------------------------------------------------------------- */
void VGA256FadeIn(char* paleta)
{
    unsigned int x, y;
    char pal[768];

    memset(pal, 0, sizeof(pal));
    for (y = 0; y < 63; y++)
    {
        for (x = 0; x < sizeof(pal); x++)
        {
            if (pal[x] < paleta[x])
            {
                pal[x]++;
            }
        }
        VGA256WaitVRetrace();
        VGA256SetPalette(pal);
    }
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256FadeTo(char* paleta)
{
    unsigned int x, y;
    char pal[768];
    
    VGA256GetPalette(pal);

    for (y = 0; y < 63; y++)
    {
        for (x = 0; x < sizeof(pal); x++)
        {
            if (pal[x] < paleta[x])
            {
                pal[x]++;
            }
            if (pal[x] > paleta[x])
            {
                pal[x]--;
            }
        }
        VGA256WaitVRetrace();
        VGA256SetPalette(pal);
    }
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256Circle(void* pVideo, unsigned x, unsigned int y, unsigned int radio, unsigned int color)
{
    unsigned int xl, yl;

    if (radio == 0)
    {
        VGA256PutPixel(pVideo, x, y, color);
        return;
    }

    xl = 0;
    yl = radio;
    radio = radio * radio + 1;

    while (yl != 0)
    {
        VGA256PutPixel(pVideo, x + xl, y + yl, color);
        VGA256PutPixel(pVideo, x - xl, y + yl, color);
        VGA256PutPixel(pVideo, x + xl, y - yl, color);
        VGA256PutPixel(pVideo, x - xl, y - yl, color);
        if (((xl * xl) + (yl * yl)) >= radio)
        {
            yl--;
        }
        else
        {
            xl++;
        }
    }                                     // repeat ... Until Yl = 0;
    VGA256PutPixel(pVideo, x + xl, y + yl, color);
    VGA256PutPixel(pVideo, x - xl, y + yl, color);
    VGA256PutPixel(pVideo, x + xl, y - yl, color);
    VGA256PutPixel(pVideo, x - xl, y - yl, color);
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256Line(void* pVideo, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned char color)
{
    unsigned int i, n, m;
    int d1x, d1y, d2x, d2y, s, u, v;

    u = c - a;
    v = d - b;
    d1x = _VGA256Sgn(u);
    d1y = _VGA256Sgn(v);
    d2x = _VGA256Sgn(u);
    d2y = 0;
    m = _VGA256ABS(u);
    n = _VGA256ABS(v);
    if (m > n == 0)
    {
        d2x = 0;
        d2y = _VGA256Sgn(v);
        m = _VGA256ABS(v);
        n = _VGA256ABS(u);
    }
    s = m >> 1;
    for (i = 0; i <= m; i++)
    {
        VGA256PutPixel(pVideo, a, b, color);
        s += n;
        if (s < m == 0)
        {
            s -= m;
            a += d1x;
            b += d1y;
        }
        else
        {
            a += d2x;
            b += d2y;
        }
    }
}


/* ----------------------------------------------------------------------------------------------------------------- */
int VGA256KbHit(void)
{
    return(peekw(0x40, 0x1A) - peekw(0x40, 0x1C));
}


/* ----------------------------------------------------------------------------------------------------------------- */
int VGA256GetCh(void)
{
    union REGS r;

    while (!VGA256KbHit());
    r.h.ah = 0;
    int386(0x16, &r, &r);
    return((int) r.h.al);
}


/* ----------------------------------------------------------------------------------------------------------------- */
void VGA256FloodFill(void *pVideo, unsigned int x, unsigned int y, unsigned int new_col, unsigned int old_col)
{
    unsigned char* offset = (unsigned char*)&VGA256OFFSET(pVideo, x, y);

    if (*offset == (unsigned char) old_col)
    {
        *offset = (unsigned char) new_col;
        VGA256FloodFill(pVideo, x + 1, y, new_col, old_col);
        VGA256FloodFill(pVideo, x - 1, y, new_col, old_col);
        VGA256FloodFill(pVideo, x, y + 1, new_col, old_col);
        VGA256FloodFill(pVideo, x, y - 1, new_col, old_col);
    }
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText(void *pVideo, char *text, unsigned int x, unsigned int y, unsigned int color)
{
    unsigned int j, k;
    unsigned char *offset = (unsigned char*)&VGA256OFFSET(pVideo, x, y);
    unsigned char *romptr;
    unsigned char* charset;
    unsigned char mask = 0x80;
    union REGPACK r;

    charset = (unsigned char *) MK_FP(0xF000, 0xFA6E);
    /*r.w.ax = 0x1130;
    r.h.bh = 6;
    intr(0x10, &r);
    charset = (unsigned char *) MK_FP(r.w.es, r.w.bp);*/

    romptr = charset + (*text) * 8;
    while (*text != NULL)
    {
        for (j = 0; j < 8; j++)
        {
            mask = 0x80;
            for (k = 0; k < 8; k++)
            {
                if ((*romptr & mask))
                {
                    *(offset + k) = color;
                }
                mask = (mask >> 1);
            }
            offset += VGA256_WIDTH;
            romptr++;
        }
        x += 8;
        text++;
        offset = (unsigned char*)&VGA256OFFSET(pVideo, x, y);
        romptr = charset + (*text) * 8;
    }
}


/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)
struct pcx_header
{
    char manufacturer;
    char version;
    char encoding;
    char bits_per_pixel;
    short int xmin, ymin;
    short int xmax, ymax;
    short int hres;
    short int vres;
    char palette16[48];
    char reserved;
    char color_planes;
    short int bytes_per_line;
    short int palette_type;
    char filler[58];
};
#pragma pack (pop)

/*------------------------------------------------------------------------------------------------------- */
int VGA256LoadPCX(char* filename, unsigned char* dest, unsigned char* pal)
{
    #define VGA256LoadPCXBufLen (16384)
    unsigned int i = 0;
    unsigned int bufptr = 0;
    unsigned int mode = 0;    /* BYTEMODE */
    int readlen = 0;
    int infile = -1;
    unsigned int outbyte = 0, bytecount = 0;
    unsigned char* buffer = NULL;
    unsigned int imagesize = 0;
    struct pcx_header pcx_header;
    int res = -9;


    if ((infile = _open(filename, O_BINARY)) == -1)
    {
        res = -1;   /* Cannot open */
    }
    else
    {
        readlen = _read(infile, &pcx_header, sizeof(pcx_header));
        if ((pcx_header.manufacturer == 10) && (pcx_header.encoding == 1) && (pcx_header.bits_per_pixel == 8))
        {
            if (dest)
            {
                buffer = malloc(VGA256LoadPCXBufLen);
                if (buffer)
                {
                    readlen = 0;
                    imagesize = (unsigned int)((pcx_header.xmax - pcx_header.xmin + 1) * (pcx_header.ymax - pcx_header.ymin + 1) * pcx_header.bits_per_pixel >> 3);
                    for (i = 0; i < imagesize; i++)
                    {
                        if (mode == 0)  /* BYTEMODE */
                        {
                            if (bufptr >= readlen)
                            {
                                bufptr = 0;
                                if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) <= 0)
                                {
                                    break;
                                }
                            }
                            outbyte = (unsigned char) buffer[bufptr++];
                            if (outbyte > 0xbf)
                            {
                                bytecount = (unsigned int)((unsigned int)outbyte & 0x3f);
                                if (bufptr >= readlen)
                                {
                                    bufptr = 0;
                                    if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) <= 0)
                                    {
                                        break;
                                    }
                                }
                                outbyte = (unsigned char) buffer[bufptr++];
                                if (--bytecount > 0)
                                {
                                    mode = 1;   /* RUNMODE */
                                }
                            }
                        }
                        else if (--bytecount == 0)
                        {
                            mode = 0;   /* BYTEMODE */
                        }
                        *dest++ = (unsigned char) outbyte;
                        //i++;
                    }
                    free(buffer);
                    res = imagesize;
                }
                else
                {
                    res = -3; /* Cannot allocate */
                }
            }
            if (pal)
            {
                _lseek(infile, -768L, SEEK_END);
                readlen = _read(infile, pal, 3 * 256);
                for (i = 0; i < readlen; i++)
                {
                    pal[i] = pal[i] >> 2;
                }
            }
        }
        else
        {
            res = -2;   /* Invalid PCX */
        }
    }
    if (infile != -1)
    {
        _close(infile);
    }
    return(res);
}


