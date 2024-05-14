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
void VGA256RotateImage(unsigned char* pDest, unsigned char* pSource, unsigned int width, unsigned int height, int angle)
{
	unsigned int hwidth = width >> 1;
	unsigned int hheight = height >> 1;
	int sinma = VGA256SinDeg[angle];
	int cosma = VGA256CosDeg[angle];
	int ys, ys_cosma, ys_sinma;
	int xt, xs;
	unsigned int x, y;

	for (y = 0; y < height; y++)
	{
		ys = y - hheight;
		ys_cosma = ys * cosma;
		ys_sinma = ys * sinma;
		for (x = 0; x < width; x++)
		{
			xt = x - hwidth;
			xs = (cosma * xt - ys_sinma) >> 14;
			xs += hwidth;
			ys = (ys_cosma + sinma * xt) >> 14;
			ys += hheight;
			if ((unsigned int)xs < width && (unsigned int)ys < height)
			{
				pDest[y * width + x] = pSource[ys * width + xs];
			}
			else
			{
				pDest[y * width + x] = 0;
			}
		}
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256RotateImageOK(unsigned char* pDest, unsigned char* pSource, unsigned int width, unsigned int height, int angle)
{
	unsigned int hwidth = width >> 1;
    unsigned int hheight = height >> 1;
    int sinma = VGA256SinDeg[angle];
    int cosma = VGA256CosDeg[angle];
    int xt, yt, xs, ys;
    unsigned int x, y;

    for (x = 0; x < width; x++)
    {
        xt = x - hwidth;
        for (y = 0; y < height; y++)
        {
            yt = y - hheight;
            xs = (cosma * xt - sinma * yt) >> 14;
            xs += hwidth;
            ys = (sinma * xt + cosma * yt) >> 14;
            ys += hheight;
            if (xs >= 0 && xs < (unsigned int) width && ys >= 0 && ys < (unsigned int) height)
            {
                pDest[y*width+x]=pSource[ys*width+xs];
            }
            else
            {
                pDest[y*width+x] = 0;
            }
        }
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
void VGA256OutText(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char *font)
{
	unsigned int x, y;
	unsigned int scount = 0;
	unsigned char *cptr;
	unsigned char font_bits;
	unsigned char bitset = 0;
	unsigned char* offset;

	if (font == NULL)
	{
		//font = (unsigned char*) MK_FP(0xF000, 0xFA6E);
		font = (unsigned char*)MK_FP(0xF000, 0);
	}

	while (string[scount] != 0)
	{
		cptr = &font[string[scount] << 3];
		for (y = 0; y < 8; y++)
		{
			font_bits = cptr[y];
			offset = &VGA256OFFSET(pVideo, x_cursor, y_cursor + y);
			for (x = 0; x < 8; x++)
			{
				bitset = (font_bits >> (7 - (x & 7))) & 0x1;
				if (bitset)
				{
					//VGA256PutPixel(pVideo, x + x_cursor, y + y_cursor, color);
					*(offset) = color;
				}
				offset++;
			}
		}
		x_cursor += 8;
		scount++;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText2x(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char* font)
{
	unsigned int x, y;
	unsigned int scount = 0;
	unsigned char* cptr;
	unsigned char font_bits;
	unsigned char bitset = 0;
	unsigned char* offset;

	if (font == NULL)
	{
		font = (unsigned char*)MK_FP(0xF000, 0);
	}

	while (string[scount] != 0)
	{
		cptr = &font[string[scount] << 3];
		for (y = 0; y < 8; y++)
		{
			font_bits = cptr[y];
			offset = &VGA256OFFSET(pVideo, x_cursor, y_cursor + (y << 1));
			for (x = 0; x < 8; x++)
			{
				bitset = (font_bits >> (7 - (x & 7))) & 0x1;
				if (bitset)
				{
					*(offset) = color;
					*(offset + 1) = color;
					*(offset + VGA256_WIDTH) = color;
					*(offset + VGA256_WIDTH + 1) = color;
				}
				offset += 2;
			}
		}
		x_cursor += 16;
		scount++;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText2xOK(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char* font)
{
	unsigned int width, height;
	unsigned int slen, x, y;
	unsigned int scount;
	unsigned char* cptr;
	unsigned char font_bits;
	unsigned char bitset = 0;
	unsigned char* offset;

	if (font == NULL)
	{
		font = (unsigned char*)MK_FP(0xF000, 0xFA6E);
	}

	slen = strlen(string);

	for (scount = 0; scount < slen; scount++)
	{
		cptr = &font[string[scount] << 3];
		for (y = 0; y < 8; y++)
		{
			font_bits = cptr[y];
			for (x = 0; x < 8; x++)
			{
				bitset = (font_bits >> (7 - (x & 7))) & 0x1;
				if (bitset)
				{
					offset = &VGA256OFFSET(pVideo, (x << 1) + x_cursor, (y << 1) + y_cursor);
					*offset = color;
					*(offset + 1) = color;
					*(offset + VGA256_WIDTH) = color;
					*(offset + VGA256_WIDTH + 1) = color;
				}
			}
		}
		x_cursor += 16;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutTextKO(void *pVideo, char *text, unsigned int x, unsigned int y, unsigned int color)
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
	unsigned int i;
    unsigned int bufptr = 0;
    unsigned int mode = 0;    /* BYTEMODE */
    int readlen = 0;
    int infile = -1;
    unsigned int outbyte = 0, bytecount = 0;
    unsigned char* buffer = NULL;
    unsigned int imagesize = 0;
    struct pcx_header pcx_header;
    int res = -9;


    if ((infile = _open(filename, O_BINARY | O_RDONLY)) == -1)
    {
        res = -1;   /* Cannot open */
    }
    else
    {
        readlen = _read(infile, &pcx_header, sizeof(pcx_header));
        if ((pcx_header.manufacturer == 10) && (pcx_header.bits_per_pixel == 8))
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
                    }
                    free(buffer);
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
			res = imagesize;
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


/*------------------------------------------------------------------------------------------------------- */
int VGA256DecodePCX(unsigned char *dest, unsigned char *buffer)
{
	unsigned int i;
	unsigned int bufptr = 0;
	unsigned int mode = 0;    /* BYTEMODE */
	unsigned int outbyte = 0, bytecount = 0;
	unsigned int imagesize = 0;
	struct pcx_header pcx_header;
	int res = -9;

	memcpy(&pcx_header, buffer, sizeof(pcx_header));
	if ((pcx_header.manufacturer == 10) && (pcx_header.bits_per_pixel == 8))
	{
		imagesize = (unsigned int)((pcx_header.xmax - pcx_header.xmin + 1) * (pcx_header.ymax - pcx_header.ymin + 1) * pcx_header.bits_per_pixel >> 3);
		bufptr += sizeof(pcx_header);
		for (i = 0; i < imagesize; i++)
		{
			if (mode == 0)  /* BYTEMODE */
			{
				outbyte = (unsigned char)buffer[bufptr++];
				if (outbyte > 0xbf)
				{
					bytecount = (unsigned int)((unsigned int)outbyte & 0x3f);
					outbyte = (unsigned char)buffer[bufptr++];
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
			*dest++ = (unsigned char)outbyte;
		}
		for (i = 0; i < 768; i++)
		{
			(*dest++) = (unsigned char)(buffer[bufptr++] >> 2);
		}
		res = imagesize;
	}
	else
	{
		res = -2;   /* Invalid PCX */
	}
	return(res);
}




/*------------------------------------------------------------------------------------------------------- */
const int VGA256SinDeg[] =
{
	0, // 0 0.000000
	-285, // 1 -0.017452
	-571, // 2 -0.034899
	-857, // 3 -0.052336
	-1142, // 4 -0.069756
	-1427, // 5 -0.087156
	-1712, // 6 -0.104528
	-1996, // 7 -0.121869
	-2280, // 8 -0.139173
	-2563, // 9 -0.156434
	-2845, // 10 -0.173648
	-3126, // 11 -0.190809
	-3406, // 12 -0.207912
	-3685, // 13 -0.224951
	-3963, // 14 -0.241922
	-4240, // 15 -0.258819
	-4516, // 16 -0.275637
	-4790, // 17 -0.292372
	-5062, // 18 -0.309017
	-5334, // 19 -0.325568
	-5603, // 20 -0.342020
	-5871, // 21 -0.358368
	-6137, // 22 -0.374607
	-6401, // 23 -0.390731
	-6663, // 24 -0.406737
	-6924, // 25 -0.422618
	-7182, // 26 -0.438371
	-7438, // 27 -0.453990
	-7691, // 28 -0.469472
	-7943, // 29 -0.484810
	-8191, // 30 -0.500000
	-8438, // 31 -0.515038
	-8682, // 32 -0.529919
	-8923, // 33 -0.544639
	-9161, // 34 -0.559193
	-9397, // 35 -0.573576
	-9630, // 36 -0.587785
	-9860, // 37 -0.601815
	-10086, // 38 -0.615661
	-10310, // 39 -0.629320
	-10531, // 40 -0.642788
	-10748, // 41 -0.656059
	-10963, // 42 -0.669131
	-11173, // 43 -0.681998
	-11381, // 44 -0.694658
	-11585, // 45 -0.707107
	-11785, // 46 -0.719340
	-11982, // 47 -0.731354
	-12175, // 48 -0.743145
	-12365, // 49 -0.754710
	-12550, // 50 -0.766044
	-12732, // 51 -0.777146
	-12910, // 52 -0.788011
	-13084, // 53 -0.798636
	-13254, // 54 -0.809017
	-13420, // 55 -0.819152
	-13582, // 56 -0.829038
	-13740, // 57 -0.838671
	-13894, // 58 -0.848048
	-14043, // 59 -0.857167
	-14188, // 60 -0.866025
	-14329, // 61 -0.874620
	-14466, // 62 -0.882948
	-14598, // 63 -0.891007
	-14725, // 64 -0.898794
	-14848, // 65 -0.906308
	-14967, // 66 -0.913545
	-15081, // 67 -0.920505
	-15190, // 68 -0.927184
	-15295, // 69 -0.933580
	-15395, // 70 -0.939693
	-15491, // 71 -0.945519
	-15582, // 72 -0.951057
	-15668, // 73 -0.956305
	-15749, // 74 -0.961262
	-15825, // 75 -0.965926
	-15897, // 76 -0.970296
	-15964, // 77 -0.974370
	-16025, // 78 -0.978148
	-16082, // 79 -0.981627
	-16135, // 80 -0.984808
	-16182, // 81 -0.987688
	-16224, // 82 -0.990268
	-16261, // 83 -0.992546
	-16294, // 84 -0.994522
	-16321, // 85 -0.996195
	-16344, // 86 -0.997564
	-16361, // 87 -0.998630
	-16374, // 88 -0.999391
	-16381, // 89 -0.999848
	-16384, // 90 -1.000000
	-16381, // 91 -0.999848
	-16374, // 92 -0.999391
	-16361, // 93 -0.998630
	-16344, // 94 -0.997564
	-16321, // 95 -0.996195
	-16294, // 96 -0.994522
	-16261, // 97 -0.992546
	-16224, // 98 -0.990268
	-16182, // 99 -0.987688
	-16135, // 100 -0.984808
	-16082, // 101 -0.981627
	-16025, // 102 -0.978148
	-15964, // 103 -0.974370
	-15897, // 104 -0.970296
	-15825, // 105 -0.965926
	-15749, // 106 -0.961262
	-15668, // 107 -0.956305
	-15582, // 108 -0.951057
	-15491, // 109 -0.945519
	-15395, // 110 -0.939693
	-15295, // 111 -0.933580
	-15190, // 112 -0.927184
	-15081, // 113 -0.920505
	-14967, // 114 -0.913545
	-14848, // 115 -0.906308
	-14725, // 116 -0.898794
	-14598, // 117 -0.891007
	-14466, // 118 -0.882948
	-14329, // 119 -0.874620
	-14188, // 120 -0.866025
	-14043, // 121 -0.857167
	-13894, // 122 -0.848048
	-13740, // 123 -0.838671
	-13582, // 124 -0.829038
	-13420, // 125 -0.819152
	-13254, // 126 -0.809017
	-13084, // 127 -0.798636
	-12910, // 128 -0.788011
	-12732, // 129 -0.777146
	-12550, // 130 -0.766044
	-12365, // 131 -0.754710
	-12175, // 132 -0.743145
	-11982, // 133 -0.731354
	-11785, // 134 -0.719340
	-11585, // 135 -0.707107
	-11381, // 136 -0.694658
	-11173, // 137 -0.681998
	-10963, // 138 -0.669131
	-10748, // 139 -0.656059
	-10531, // 140 -0.642788
	-10310, // 141 -0.629320
	-10086, // 142 -0.615661
	-9860, // 143 -0.601815
	-9630, // 144 -0.587785
	-9397, // 145 -0.573576
	-9161, // 146 -0.559193
	-8923, // 147 -0.544639
	-8682, // 148 -0.529919
	-8438, // 149 -0.515038
	-8191, // 150 -0.500000
	-7943, // 151 -0.484810
	-7691, // 152 -0.469472
	-7438, // 153 -0.453990
	-7182, // 154 -0.438371
	-6924, // 155 -0.422618
	-6663, // 156 -0.406737
	-6401, // 157 -0.390731
	-6137, // 158 -0.374607
	-5871, // 159 -0.358368
	-5603, // 160 -0.342020
	-5334, // 161 -0.325568
	-5062, // 162 -0.309017
	-4790, // 163 -0.292372
	-4516, // 164 -0.275637
	-4240, // 165 -0.258819
	-3963, // 166 -0.241922
	-3685, // 167 -0.224951
	-3406, // 168 -0.207912
	-3126, // 169 -0.190809
	-2845, // 170 -0.173648
	-2563, // 171 -0.156434
	-2280, // 172 -0.139173
	-1996, // 173 -0.121869
	-1712, // 174 -0.104528
	-1427, // 175 -0.087156
	-1142, // 176 -0.069756
	-857, // 177 -0.052336
	-571, // 178 -0.034899
	-285, // 179 -0.017452
	0, // 180 -0.000000
	285, // 181 0.017452
	571, // 182 0.034899
	857, // 183 0.052336
	1142, // 184 0.069756
	1427, // 185 0.087156
	1712, // 186 0.104528
	1996, // 187 0.121869
	2280, // 188 0.139173
	2563, // 189 0.156434
	2845, // 190 0.173648
	3126, // 191 0.190809
	3406, // 192 0.207912
	3685, // 193 0.224951
	3963, // 194 0.241922
	4240, // 195 0.258819
	4516, // 196 0.275637
	4790, // 197 0.292372
	5062, // 198 0.309017
	5334, // 199 0.325568
	5603, // 200 0.342020
	5871, // 201 0.358368
	6137, // 202 0.374607
	6401, // 203 0.390731
	6663, // 204 0.406737
	6924, // 205 0.422618
	7182, // 206 0.438371
	7438, // 207 0.453990
	7691, // 208 0.469472
	7943, // 209 0.484810
	8192, // 210 0.500000
	8438, // 211 0.515038
	8682, // 212 0.529919
	8923, // 213 0.544639
	9161, // 214 0.559193
	9397, // 215 0.573576
	9630, // 216 0.587785
	9860, // 217 0.601815
	10086, // 218 0.615661
	10310, // 219 0.629320
	10531, // 220 0.642788
	10748, // 221 0.656059
	10963, // 222 0.669131
	11173, // 223 0.681998
	11381, // 224 0.694658
	11585, // 225 0.707107
	11785, // 226 0.719340
	11982, // 227 0.731354
	12175, // 228 0.743145
	12365, // 229 0.754710
	12550, // 230 0.766044
	12732, // 231 0.777146
	12910, // 232 0.788011
	13084, // 233 0.798636
	13254, // 234 0.809017
	13420, // 235 0.819152
	13582, // 236 0.829038
	13740, // 237 0.838671
	13894, // 238 0.848048
	14043, // 239 0.857167
	14188, // 240 0.866025
	14329, // 241 0.874620
	14466, // 242 0.882948
	14598, // 243 0.891007
	14725, // 244 0.898794
	14848, // 245 0.906308
	14967, // 246 0.913545
	15081, // 247 0.920505
	15190, // 248 0.927184
	15295, // 249 0.933580
	15395, // 250 0.939693
	15491, // 251 0.945519
	15582, // 252 0.951057
	15668, // 253 0.956305
	15749, // 254 0.961262
	15825, // 255 0.965926
	15897, // 256 0.970296
	15964, // 257 0.974370
	16025, // 258 0.978148
	16082, // 259 0.981627
	16135, // 260 0.984808
	16182, // 261 0.987688
	16224, // 262 0.990268
	16261, // 263 0.992546
	16294, // 264 0.994522
	16321, // 265 0.996195
	16344, // 266 0.997564
	16361, // 267 0.998630
	16374, // 268 0.999391
	16381, // 269 0.999848
	16384, // 270 1.000000
	16381, // 271 0.999848
	16374, // 272 0.999391
	16361, // 273 0.998630
	16344, // 274 0.997564
	16321, // 275 0.996195
	16294, // 276 0.994522
	16261, // 277 0.992546
	16224, // 278 0.990268
	16182, // 279 0.987688
	16135, // 280 0.984808
	16082, // 281 0.981627
	16025, // 282 0.978148
	15964, // 283 0.974370
	15897, // 284 0.970296
	15825, // 285 0.965926
	15749, // 286 0.961262
	15668, // 287 0.956305
	15582, // 288 0.951057
	15491, // 289 0.945519
	15395, // 290 0.939693
	15295, // 291 0.933580
	15190, // 292 0.927184
	15081, // 293 0.920505
	14967, // 294 0.913545
	14848, // 295 0.906308
	14725, // 296 0.898794
	14598, // 297 0.891007
	14466, // 298 0.882948
	14329, // 299 0.874620
	14188, // 300 0.866025
	14043, // 301 0.857167
	13894, // 302 0.848048
	13740, // 303 0.838671
	13582, // 304 0.829038
	13420, // 305 0.819152
	13254, // 306 0.809017
	13084, // 307 0.798636
	12910, // 308 0.788011
	12732, // 309 0.777146
	12550, // 310 0.766044
	12365, // 311 0.754710
	12175, // 312 0.743145
	11982, // 313 0.731354
	11785, // 314 0.719340
	11585, // 315 0.707107
	11381, // 316 0.694658
	11173, // 317 0.681998
	10963, // 318 0.669131
	10748, // 319 0.656059
	10531, // 320 0.642788
	10310, // 321 0.629320
	10086, // 322 0.615661
	9860, // 323 0.601815
	9630, // 324 0.587785
	9397, // 325 0.573576
	9161, // 326 0.559193
	8923, // 327 0.544639
	8682, // 328 0.529919
	8438, // 329 0.515038
	8192, // 330 0.500000
	7943, // 331 0.484810
	7691, // 332 0.469472
	7438, // 333 0.453990
	7182, // 334 0.438371
	6924, // 335 0.422618
	6663, // 336 0.406737
	6401, // 337 0.390731
	6137, // 338 0.374607
	5871, // 339 0.358368
	5603, // 340 0.342020
	5334, // 341 0.325568
	5062, // 342 0.309017
	4790, // 343 0.292372
	4516, // 344 0.275637
	4240, // 345 0.258819
	3963, // 346 0.241922
	3685, // 347 0.224951
	3406, // 348 0.207912
	3126, // 349 0.190809
	2845, // 350 0.173648
	2563, // 351 0.156434
	2280, // 352 0.139173
	1996, // 353 0.121869
	1712, // 354 0.104528
	1427, // 355 0.087156
	1142, // 356 0.069756
	857, // 357 0.052336
	571, // 358 0.034899
	285, // 359 0.017452
	0 // 360 0.000000
};


/*------------------------------------------------------------------------------------------------------- */
const int VGA256CosDeg[] =
{
	16384, // 0 1.000000
	16381, // 1 0.999848
	16374, // 2 0.999391
	16361, // 3 0.998630
	16344, // 4 0.997564
	16321, // 5 0.996195
	16294, // 6 0.994522
	16261, // 7 0.992546
	16224, // 8 0.990268
	16182, // 9 0.987688
	16135, // 10 0.984808
	16082, // 11 0.981627
	16025, // 12 0.978148
	15964, // 13 0.974370
	15897, // 14 0.970296
	15825, // 15 0.965926
	15749, // 16 0.961262
	15668, // 17 0.956305
	15582, // 18 0.951057
	15491, // 19 0.945519
	15395, // 20 0.939693
	15295, // 21 0.933580
	15190, // 22 0.927184
	15081, // 23 0.920505
	14967, // 24 0.913545
	14848, // 25 0.906308
	14725, // 26 0.898794
	14598, // 27 0.891007
	14466, // 28 0.882948
	14329, // 29 0.874620
	14188, // 30 0.866025
	14043, // 31 0.857167
	13894, // 32 0.848048
	13740, // 33 0.838671
	13582, // 34 0.829038
	13420, // 35 0.819152
	13254, // 36 0.809017
	13084, // 37 0.798636
	12910, // 38 0.788011
	12732, // 39 0.777146
	12550, // 40 0.766044
	12365, // 41 0.754710
	12175, // 42 0.743145
	11982, // 43 0.731354
	11785, // 44 0.719340
	11585, // 45 0.707107
	11381, // 46 0.694658
	11173, // 47 0.681998
	10963, // 48 0.669131
	10748, // 49 0.656059
	10531, // 50 0.642788
	10310, // 51 0.629320
	10086, // 52 0.615661
	9860, // 53 0.601815
	9630, // 54 0.587785
	9397, // 55 0.573576
	9161, // 56 0.559193
	8923, // 57 0.544639
	8682, // 58 0.529919
	8438, // 59 0.515038
	8192, // 60 0.500000
	7943, // 61 0.484810
	7691, // 62 0.469472
	7438, // 63 0.453990
	7182, // 64 0.438371
	6924, // 65 0.422618
	6663, // 66 0.406737
	6401, // 67 0.390731
	6137, // 68 0.374607
	5871, // 69 0.358368
	5603, // 70 0.342020
	5334, // 71 0.325568
	5062, // 72 0.309017
	4790, // 73 0.292372
	4516, // 74 0.275637
	4240, // 75 0.258819
	3963, // 76 0.241922
	3685, // 77 0.224951
	3406, // 78 0.207912
	3126, // 79 0.190809
	2845, // 80 0.173648
	2563, // 81 0.156434
	2280, // 82 0.139173
	1996, // 83 0.121869
	1712, // 84 0.104528
	1427, // 85 0.087156
	1142, // 86 0.069756
	857, // 87 0.052336
	571, // 88 0.034899
	285, // 89 0.017452
	0, // 90 0.000000
	-285, // 91 -0.017452
	-571, // 92 -0.034899
	-857, // 93 -0.052336
	-1142, // 94 -0.069756
	-1427, // 95 -0.087156
	-1712, // 96 -0.104528
	-1996, // 97 -0.121869
	-2280, // 98 -0.139173
	-2563, // 99 -0.156434
	-2845, // 100 -0.173648
	-3126, // 101 -0.190809
	-3406, // 102 -0.207912
	-3685, // 103 -0.224951
	-3963, // 104 -0.241922
	-4240, // 105 -0.258819
	-4516, // 106 -0.275637
	-4790, // 107 -0.292372
	-5062, // 108 -0.309017
	-5334, // 109 -0.325568
	-5603, // 110 -0.342020
	-5871, // 111 -0.358368
	-6137, // 112 -0.374607
	-6401, // 113 -0.390731
	-6663, // 114 -0.406737
	-6924, // 115 -0.422618
	-7182, // 116 -0.438371
	-7438, // 117 -0.453990
	-7691, // 118 -0.469472
	-7943, // 119 -0.484810
	-8191, // 120 -0.500000
	-8438, // 121 -0.515038
	-8682, // 122 -0.529919
	-8923, // 123 -0.544639
	-9161, // 124 -0.559193
	-9397, // 125 -0.573576
	-9630, // 126 -0.587785
	-9860, // 127 -0.601815
	-10086, // 128 -0.615661
	-10310, // 129 -0.629320
	-10531, // 130 -0.642788
	-10748, // 131 -0.656059
	-10963, // 132 -0.669131
	-11173, // 133 -0.681998
	-11381, // 134 -0.694658
	-11585, // 135 -0.707107
	-11785, // 136 -0.719340
	-11982, // 137 -0.731354
	-12175, // 138 -0.743145
	-12365, // 139 -0.754710
	-12550, // 140 -0.766044
	-12732, // 141 -0.777146
	-12910, // 142 -0.788011
	-13084, // 143 -0.798636
	-13254, // 144 -0.809017
	-13420, // 145 -0.819152
	-13582, // 146 -0.829038
	-13740, // 147 -0.838671
	-13894, // 148 -0.848048
	-14043, // 149 -0.857167
	-14188, // 150 -0.866025
	-14329, // 151 -0.874620
	-14466, // 152 -0.882948
	-14598, // 153 -0.891007
	-14725, // 154 -0.898794
	-14848, // 155 -0.906308
	-14967, // 156 -0.913545
	-15081, // 157 -0.920505
	-15190, // 158 -0.927184
	-15295, // 159 -0.933580
	-15395, // 160 -0.939693
	-15491, // 161 -0.945519
	-15582, // 162 -0.951057
	-15668, // 163 -0.956305
	-15749, // 164 -0.961262
	-15825, // 165 -0.965926
	-15897, // 166 -0.970296
	-15964, // 167 -0.974370
	-16025, // 168 -0.978148
	-16082, // 169 -0.981627
	-16135, // 170 -0.984808
	-16182, // 171 -0.987688
	-16224, // 172 -0.990268
	-16261, // 173 -0.992546
	-16294, // 174 -0.994522
	-16321, // 175 -0.996195
	-16344, // 176 -0.997564
	-16361, // 177 -0.998630
	-16374, // 178 -0.999391
	-16381, // 179 -0.999848
	-16384, // 180 -1.000000
	-16381, // 181 -0.999848
	-16374, // 182 -0.999391
	-16361, // 183 -0.998630
	-16344, // 184 -0.997564
	-16321, // 185 -0.996195
	-16294, // 186 -0.994522
	-16261, // 187 -0.992546
	-16224, // 188 -0.990268
	-16182, // 189 -0.987688
	-16135, // 190 -0.984808
	-16082, // 191 -0.981627
	-16025, // 192 -0.978148
	-15964, // 193 -0.974370
	-15897, // 194 -0.970296
	-15825, // 195 -0.965926
	-15749, // 196 -0.961262
	-15668, // 197 -0.956305
	-15582, // 198 -0.951057
	-15491, // 199 -0.945519
	-15395, // 200 -0.939693
	-15295, // 201 -0.933580
	-15190, // 202 -0.927184
	-15081, // 203 -0.920505
	-14967, // 204 -0.913545
	-14848, // 205 -0.906308
	-14725, // 206 -0.898794
	-14598, // 207 -0.891007
	-14466, // 208 -0.882948
	-14329, // 209 -0.874620
	-14188, // 210 -0.866025
	-14043, // 211 -0.857167
	-13894, // 212 -0.848048
	-13740, // 213 -0.838671
	-13582, // 214 -0.829038
	-13420, // 215 -0.819152
	-13254, // 216 -0.809017
	-13084, // 217 -0.798636
	-12910, // 218 -0.788011
	-12732, // 219 -0.777146
	-12550, // 220 -0.766044
	-12365, // 221 -0.754710
	-12175, // 222 -0.743145
	-11982, // 223 -0.731354
	-11785, // 224 -0.719340
	-11585, // 225 -0.707107
	-11381, // 226 -0.694658
	-11173, // 227 -0.681998
	-10963, // 228 -0.669131
	-10748, // 229 -0.656059
	-10531, // 230 -0.642788
	-10310, // 231 -0.629320
	-10086, // 232 -0.615661
	-9860, // 233 -0.601815
	-9630, // 234 -0.587785
	-9397, // 235 -0.573576
	-9161, // 236 -0.559193
	-8923, // 237 -0.544639
	-8682, // 238 -0.529919
	-8438, // 239 -0.515038
	-8192, // 240 -0.500000
	-7943, // 241 -0.484810
	-7691, // 242 -0.469472
	-7438, // 243 -0.453990
	-7182, // 244 -0.438371
	-6924, // 245 -0.422618
	-6663, // 246 -0.406737
	-6401, // 247 -0.390731
	-6137, // 248 -0.374607
	-5871, // 249 -0.358368
	-5603, // 250 -0.342020
	-5334, // 251 -0.325568
	-5062, // 252 -0.309017
	-4790, // 253 -0.292372
	-4516, // 254 -0.275637
	-4240, // 255 -0.258819
	-3963, // 256 -0.241922
	-3685, // 257 -0.224951
	-3406, // 258 -0.207912
	-3126, // 259 -0.190809
	-2845, // 260 -0.173648
	-2563, // 261 -0.156434
	-2280, // 262 -0.139173
	-1996, // 263 -0.121869
	-1712, // 264 -0.104528
	-1427, // 265 -0.087156
	-1142, // 266 -0.069756
	-857, // 267 -0.052336
	-571, // 268 -0.034899
	-285, // 269 -0.017452
	0, // 270 -0.000000
	285, // 271 0.017452
	571, // 272 0.034899
	857, // 273 0.052336
	1142, // 274 0.069756
	1427, // 275 0.087156
	1712, // 276 0.104528
	1996, // 277 0.121869
	2280, // 278 0.139173
	2563, // 279 0.156434
	2845, // 280 0.173648
	3126, // 281 0.190809
	3406, // 282 0.207912
	3685, // 283 0.224951
	3963, // 284 0.241922
	4240, // 285 0.258819
	4516, // 286 0.275637
	4790, // 287 0.292372
	5062, // 288 0.309017
	5334, // 289 0.325568
	5603, // 290 0.342020
	5871, // 291 0.358368
	6137, // 292 0.374607
	6401, // 293 0.390731
	6663, // 294 0.406737
	6924, // 295 0.422618
	7182, // 296 0.438371
	7438, // 297 0.453990
	7691, // 298 0.469472
	7943, // 299 0.484810
	8192, // 300 0.500000
	8438, // 301 0.515038
	8682, // 302 0.529919
	8923, // 303 0.544639
	9161, // 304 0.559193
	9397, // 305 0.573576
	9630, // 306 0.587785
	9860, // 307 0.601815
	10086, // 308 0.615661
	10310, // 309 0.629320
	10531, // 310 0.642788
	10748, // 311 0.656059
	10963, // 312 0.669131
	11173, // 313 0.681998
	11381, // 314 0.694658
	11585, // 315 0.707107
	11785, // 316 0.719340
	11982, // 317 0.731354
	12175, // 318 0.743145
	12365, // 319 0.754710
	12550, // 320 0.766044
	12732, // 321 0.777146
	12910, // 322 0.788011
	13084, // 323 0.798636
	13254, // 324 0.809017
	13420, // 325 0.819152
	13582, // 326 0.829038
	13740, // 327 0.838671
	13894, // 328 0.848048
	14043, // 329 0.857167
	14188, // 330 0.866025
	14329, // 331 0.874620
	14466, // 332 0.882948
	14598, // 333 0.891007
	14725, // 334 0.898794
	14848, // 335 0.906308
	14967, // 336 0.913545
	15081, // 337 0.920505
	15190, // 338 0.927184
	15295, // 339 0.933580
	15395, // 340 0.939693
	15491, // 341 0.945519
	15582, // 342 0.951057
	15668, // 343 0.956305
	15749, // 344 0.961262
	15825, // 345 0.965926
	15897, // 346 0.970296
	15964, // 347 0.974370
	16025, // 348 0.978148
	16082, // 349 0.981627
	16135, // 350 0.984808
	16182, // 351 0.987688
	16224, // 352 0.990268
	16261, // 353 0.992546
	16294, // 354 0.994522
	16321, // 355 0.996195
	16344, // 356 0.997564
	16361, // 357 0.998630
	16374, // 358 0.999391
	16381, // 359 0.999848
	16384 // 360 1.000000
};


const unsigned char VGA256Font[] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 000 (.)
	0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E,	// Char 001 (.)
	0x7E, 0xFF, 0xDB, 0xFF, 0xC3, 0xE7, 0xFF, 0x7E,	// Char 002 (.)
	0x6C, 0xFE, 0xFE, 0xFE, 0x7C, 0x38, 0x10, 0x00,	// Char 003 (.)
	0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x10, 0x00,	// Char 004 (.)
	0x38, 0x7C, 0x38, 0xFE, 0xFE, 0x7C, 0x38, 0x7C,	// Char 005 (.)
	0x10, 0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x7C,	// Char 006 (.)
	0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00,	// Char 007 (.)
	0xFF, 0xFF, 0xE7, 0xC3, 0xC3, 0xE7, 0xFF, 0xFF,	// Char 008 (.)
	0x00, 0x3C, 0x66, 0x42, 0x42, 0x66, 0x3C, 0x00,	// Char 009 (.)
	0xFF, 0xC3, 0x99, 0xBD, 0xBD, 0x99, 0xC3, 0xFF,	// Char 010 (.)
	0x0F, 0x07, 0x0F, 0x7D, 0xCC, 0xCC, 0xCC, 0x78,	// Char 011 (.)
	0x3C, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x7E, 0x18,	// Char 012 (.)
	0x3F, 0x33, 0x3F, 0x30, 0x30, 0x70, 0xF0, 0xE0,	// Char 013 (.)
	0x7F, 0x63, 0x7F, 0x63, 0x63, 0x67, 0xE6, 0xC0,	// Char 014 (.)
	0x99, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x99,	// Char 015 (.)
	0x80, 0xE0, 0xF8, 0xFE, 0xF8, 0xE0, 0x80, 0x00,	// Char 016 (.)
	0x02, 0x0E, 0x3E, 0xFE, 0x3E, 0x0E, 0x02, 0x00,	// Char 017 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x18, 0x7E, 0x3C, 0x18,	// Char 018 (.)
	0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00,	// Char 019 (.)
	0x7F, 0xDB, 0xDB, 0x7B, 0x1B, 0x1B, 0x1B, 0x00,	// Char 020 (.)
	0x3C, 0x66, 0x38, 0x6C, 0x6C, 0x38, 0xCC, 0x78,	// Char 021 (.)
	0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x00,	// Char 022 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x7E, 0x3C, 0x18, 0xFF,	// Char 023 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 024 (.)
	0x18, 0x18, 0x18, 0x18, 0x7E, 0x3C, 0x18, 0x00,	// Char 025 (.)
	0x00, 0x18, 0x0C, 0xFE, 0x0C, 0x18, 0x00, 0x00,	// Char 026 (.)
	0x00, 0x30, 0x60, 0xFE, 0x60, 0x30, 0x00, 0x00,	// Char 027 (.)
	0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xFE, 0x00, 0x00,	// Char 028 (.)
	0x00, 0x24, 0x66, 0xFF, 0x66, 0x24, 0x00, 0x00,	// Char 029 (.)
	0x00, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x00, 0x00,	// Char 030 (.)
	0x00, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00, 0x00,	// Char 031 (.)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 032 ( )
	0x30, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x00,	// Char 033 (!)
	0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 034 (")
	0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00,	// Char 035 (#)
	0x30, 0x7C, 0xC0, 0x78, 0x0C, 0xF8, 0x30, 0x00,	// Char 036 ($)
	0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00,	// Char 037 (%)
	0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00,	// Char 038 (&)
	0x60, 0x60, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 039 (')
	0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00,	// Char 040 (()
	0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00,	// Char 041 ())
	0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00,	// Char 042 (*)
	0x00, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x00, 0x00,	// Char 043 (+)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60,	// Char 044 (,)
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00,	// Char 045 (-)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,	// Char 046 (.)
	0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00,	// Char 047 (/)
	0x7C, 0xC6, 0xCE, 0xDE, 0xF6, 0xE6, 0x7C, 0x00,	// Char 048 (0)
	0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00,	// Char 049 (1)
	0x78, 0xCC, 0x0C, 0x38, 0x60, 0xC0, 0xFC, 0x00,	// Char 050 (2)
	0x78, 0xCC, 0x0C, 0x38, 0x0C, 0xCC, 0x78, 0x00,	// Char 051 (3)
	0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x00,	// Char 052 (4)
	0xFC, 0xC0, 0xF8, 0x0C, 0x0C, 0xCC, 0x78, 0x00,	// Char 053 (5)
	0x38, 0x60, 0xC0, 0xF8, 0xCC, 0xCC, 0x78, 0x00,	// Char 054 (6)
	0xFC, 0x0C, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00,	// Char 055 (7)
	0x78, 0xCC, 0xCC, 0x78, 0xCC, 0xCC, 0x78, 0x00,	// Char 056 (8)
	0x78, 0xCC, 0xCC, 0x7C, 0x0C, 0x18, 0x70, 0x00,	// Char 057 (9)
	0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x00,	// Char 058 (:)
	0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x60,	// Char 059 (;)
	0x18, 0x30, 0x60, 0xC0, 0x60, 0x30, 0x18, 0x00,	// Char 060 (<)
	0x00, 0x00, 0xFC, 0x00, 0x00, 0xFC, 0x00, 0x00,	// Char 061 (=)
	0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00,	// Char 062 (>)
	0x78, 0xCC, 0x0C, 0x18, 0x30, 0x00, 0x30, 0x00,	// Char 063 (?)
	0x7C, 0xC6, 0xDE, 0xDE, 0xDE, 0xC0, 0x78, 0x00,	// Char 064 (@)
	0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00,	// Char 065 (A)
	0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00,	// Char 066 (B)
	0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00,	// Char 067 (C)
	0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00,	// Char 068 (D)
	0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00,	// Char 069 (E)
	0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00,	// Char 070 (F)
	0x3C, 0x66, 0xC0, 0xC0, 0xCE, 0x66, 0x3E, 0x00,	// Char 071 (G)
	0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00,	// Char 072 (H)
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 073 (I)
	0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00,	// Char 074 (J)
	0x66, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x66, 0x00,	// Char 075 (K)
	0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00,	// Char 076 (L)
	0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0x00,	// Char 077 (M)
	0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00,	// Char 078 (N)
	0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00,	// Char 079 (O)
	0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00,	// Char 080 (P)
	0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00,	// Char 081 (Q)
	0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00,	// Char 082 (R)
	0x3C, 0x66, 0x70, 0x38, 0x0E, 0x66, 0x3C, 0x00,	// Char 083 (S)
	0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 084 (T)
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00,	// Char 085 (U)
	0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00,	// Char 086 (V)
	0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00,	// Char 087 (W)
	0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00,	// Char 088 (X)
	0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00,	// Char 089 (Y)
	0xFE, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFE, 0x00,	// Char 090 (Z)
	0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00,	// Char 091 ([)
	0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00,	// Char 092 (\)
	0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00,	// Char 093 (])
	0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00,	// Char 094 (^)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,	// Char 095 (_)
	0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 096 (`)
	0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3A, 0x00,	// Char 097 (a)
	0x60, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x5C, 0x00,	// Char 098 (b)
	0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00,	// Char 099 (c)
	0x06, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3A, 0x00,	// Char 100 (d)
	0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00,	// Char 101 (e)
	0x1C, 0x36, 0x30, 0x78, 0x30, 0x30, 0x30, 0x00,	// Char 102 (f)
	0x00, 0x00, 0x3A, 0x66, 0x66, 0x3E, 0x06, 0x3C,	// Char 103 (g)
	0x60, 0x60, 0x6C, 0x76, 0x66, 0x66, 0x66, 0x00,	// Char 104 (h)
	0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 105 (i)
	0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78,	// Char 106 (j)
	0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00,	// Char 107 (k)
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 108 (l)
	0x00, 0x00, 0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0x00,	// Char 109 (m)
	0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00,	// Char 110 (n)
	0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00,	// Char 111 (o)
	0x00, 0x00, 0x5C, 0x66, 0x66, 0x7C, 0x60, 0x60,	// Char 112 (p)
	0x00, 0x00, 0x3A, 0x66, 0x66, 0x3E, 0x06, 0x06,	// Char 113 (q)
	0x00, 0x00, 0x5C, 0x76, 0x60, 0x60, 0x60, 0x00,	// Char 114 (r)
	0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00,	// Char 115 (s)
	0x30, 0x30, 0x7C, 0x30, 0x30, 0x34, 0x18, 0x00,	// Char 116 (t)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3A, 0x00,	// Char 117 (u)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00,	// Char 118 (v)
	0x00, 0x00, 0xC6, 0xD6, 0xFE, 0xFE, 0x6C, 0x00,	// Char 119 (w)
	0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00,	// Char 120 (x)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C,	// Char 121 (y)
	0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00,	// Char 122 (z)
	0x1C, 0x30, 0x30, 0xE0, 0x30, 0x30, 0x1C, 0x00,	// Char 123 ({)
	0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00,	// Char 124 (|)
	0xE0, 0x30, 0x30, 0x1C, 0x30, 0x30, 0xE0, 0x00,	// Char 125 (})
	0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 126 (~)
	0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0x00,	// Char 127 (.)
	0x0E, 0x1E, 0x36, 0x66, 0x7E, 0x66, 0x66, 0x00,	// Char 128 (.)
	0x7C, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C, 0x00,	// Char 129 (.)
	0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00,	// Char 130 (.)
	0x7E, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00,	// Char 131 (.)
	0x1C, 0x3C, 0x6C, 0x6C, 0x6C, 0x6C, 0xFE, 0xC6,	// Char 132 (.)
	0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00,	// Char 133 (.)
	0xDB, 0xDB, 0x7E, 0x3C, 0x7E, 0xDB, 0xDB, 0x00,	// Char 134 (.)
	0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00,	// Char 135 (.)
	0x66, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x66, 0x00,	// Char 136 (.)
	0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x66, 0x00,	// Char 137 (.)
	0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00,	// Char 138 (.)
	0x0E, 0x1E, 0x36, 0x66, 0x66, 0x66, 0x66, 0x00,	// Char 139 (.)
	0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xD6, 0xC6, 0x00,	// Char 140 (.)
	0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00,	// Char 141 (.)
	0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00,	// Char 142 (.)
	0x7E, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,	// Char 143 (.)
	0x7C, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00,	// Char 144 (.)
	0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00,	// Char 145 (.)
	0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 146 (.)
	0x66, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00,	// Char 147 (.)
	0x7E, 0xDB, 0xDB, 0xDB, 0x7E, 0x18, 0x18, 0x00,	// Char 148 (.)
	0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00,	// Char 149 (.)
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7F, 0x03,	// Char 150 (.)
	0x66, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x06, 0x00,	// Char 151 (.)
	0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xFF, 0x00,	// Char 152 (.)
	0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xFF, 0x03,	// Char 153 (.)
	0xE0, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C, 0x00,	// Char 154 (.)
	0xC6, 0xC6, 0xC6, 0xF6, 0xDE, 0xDE, 0xF6, 0x00,	// Char 155 (.)
	0x60, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C, 0x00,	// Char 156 (.)
	0x78, 0x8C, 0x06, 0x3E, 0x06, 0x8C, 0x78, 0x00,	// Char 157 (.)
	0xCE, 0xDB, 0xDB, 0xFB, 0xDB, 0xDB, 0xCE, 0x00,	// Char 158 (.)
	0x3E, 0x66, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x00,	// Char 159 (.)
	0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3A, 0x00,	// Char 160 (.)
	0x00, 0x3C, 0x60, 0x3C, 0x66, 0x66, 0x3C, 0x00,	// Char 161 (.)
	0x00, 0x00, 0x7C, 0x66, 0x7C, 0x66, 0x7C, 0x00,	// Char 162 (.)
	0x00, 0x00, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x00,	// Char 163 (.)
	0x00, 0x00, 0x1C, 0x3C, 0x6C, 0x6C, 0xFE, 0x82,	// Char 164 (.)
	0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00,	// Char 165 (.)
	0x00, 0x00, 0xDB, 0x7E, 0x3C, 0x7E, 0xDB, 0x00,	// Char 166 (.)
	0x00, 0x00, 0x3C, 0x66, 0x0C, 0x66, 0x3C, 0x00,	// Char 167 (.)
	0x00, 0x00, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x00,	// Char 168 (.)
	0x00, 0x18, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x00,	// Char 169 (.)
	0x00, 0x00, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00,	// Char 170 (.)
	0x00, 0x00, 0x0E, 0x1E, 0x36, 0x66, 0x66, 0x00,	// Char 171 (.)
	0x00, 0x00, 0xC6, 0xFE, 0xFE, 0xD6, 0xD6, 0x00,	// Char 172 (.)
	0x00, 0x00, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00,	// Char 173 (.)
	0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00,	// Char 174 (.)
	0x00, 0x00, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00,	// Char 175 (.)
	0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44,	// Char 176 (.)
	0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,	// Char 177 (.)
	0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77,	// Char 178 (.)
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,	// Char 179 (.)
	0x18, 0x18, 0x18, 0xF8, 0x18, 0x18, 0x18, 0x18,	// Char 180 (.)
	0x18, 0xF8, 0x18, 0xF8, 0x18, 0x18, 0x18, 0x18,	// Char 181 (.)
	0x36, 0x36, 0x36, 0xF6, 0x36, 0x36, 0x36, 0x36,	// Char 182 (.)
	0x00, 0x00, 0x00, 0xFE, 0x36, 0x36, 0x36, 0x36,	// Char 183 (.)
	0x00, 0xF8, 0x18, 0xF8, 0x18, 0x18, 0x18, 0x18,	// Char 184 (.)
	0x36, 0xF6, 0x06, 0xF6, 0x36, 0x36, 0x36, 0x36,	// Char 185 (.)
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,	// Char 186 (.)
	0x00, 0xFE, 0x06, 0xF6, 0x36, 0x36, 0x36, 0x36,	// Char 187 (.)
	0x36, 0xF6, 0x06, 0xFE, 0x00, 0x00, 0x00, 0x00,	// Char 188 (.)
	0x36, 0x36, 0x36, 0xFE, 0x00, 0x00, 0x00, 0x00,	// Char 189 (.)
	0x18, 0xF8, 0x18, 0xF8, 0x00, 0x00, 0x00, 0x00,	// Char 190 (.)
	0x00, 0x00, 0x00, 0xF8, 0x18, 0x18, 0x18, 0x18,	// Char 191 (.)
	0x18, 0x18, 0x18, 0x1F, 0x00, 0x00, 0x00, 0x00,	// Char 192 (.)
	0x18, 0x18, 0x18, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 193 (.)
	0x00, 0x00, 0x00, 0xFF, 0x18, 0x18, 0x18, 0x18,	// Char 194 (.)
	0x18, 0x18, 0x18, 0x1F, 0x18, 0x18, 0x18, 0x18,	// Char 195 (.)
	0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 196 (.)
	0x18, 0x18, 0x18, 0xFF, 0x18, 0x18, 0x18, 0x18,	// Char 197 (.)
	0x18, 0x1F, 0x18, 0x1F, 0x18, 0x18, 0x18, 0x18,	// Char 198 (.)
	0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36, 0x36,	// Char 199 (.)
	0x36, 0x37, 0x30, 0x3F, 0x00, 0x00, 0x00, 0x00,	// Char 200 (.)
	0x00, 0x3F, 0x30, 0x37, 0x36, 0x36, 0x36, 0x36,	// Char 201 (.)
	0x36, 0xF7, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 202 (.)
	0x00, 0xFF, 0x00, 0xF7, 0x36, 0x36, 0x36, 0x36,	// Char 203 (.)
	0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36, 0x36,	// Char 204 (.)
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 205 (.)
	0x36, 0xF7, 0x00, 0xF7, 0x36, 0x36, 0x36, 0x36,	// Char 206 (.)
	0x18, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 207 (.)
	0x36, 0x36, 0x36, 0xFF, 0x00, 0x00, 0x00, 0x00,	// Char 208 (.)
	0x00, 0xFF, 0x00, 0xFF, 0x18, 0x18, 0x18, 0x18,	// Char 209 (.)
	0x00, 0x00, 0x00, 0xFF, 0x36, 0x36, 0x36, 0x36,	// Char 210 (.)
	0x36, 0x36, 0x36, 0x3F, 0x00, 0x00, 0x00, 0x00,	// Char 211 (.)
	0x18, 0x1F, 0x18, 0x1F, 0x00, 0x00, 0x00, 0x00,	// Char 212 (.)
	0x00, 0x1F, 0x18, 0x1F, 0x18, 0x18, 0x18, 0x18,	// Char 213 (.)
	0x00, 0x00, 0x00, 0x3F, 0x36, 0x36, 0x36, 0x36,	// Char 214 (.)
	0x36, 0x36, 0x36, 0xFF, 0x36, 0x36, 0x36, 0x36,	// Char 215 (.)
	0x18, 0xFF, 0x18, 0xFF, 0x18, 0x18, 0x18, 0x18,	// Char 216 (.)
	0x18, 0x18, 0x18, 0xF8, 0x00, 0x00, 0x00, 0x00,	// Char 217 (.)
	0x00, 0x00, 0x00, 0x1F, 0x18, 0x18, 0x18, 0x18,	// Char 218 (.)
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// Char 219 (.)
	0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	// Char 220 (.)
	0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,	// Char 221 (.)
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,	// Char 222 (.)
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 223 (.)
	0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x00,	// Char 224 (.)
	0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00,	// Char 225 (.)
	0x00, 0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 226 (.)
	0x00, 0x00, 0x66, 0x66, 0x3E, 0x06, 0x7C, 0x00,	// Char 227 (.)
	0x00, 0x00, 0x7E, 0xDB, 0xDB, 0x7E, 0x18, 0x00,	// Char 228 (.)
	0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00,	// Char 229 (.)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7F, 0x03,	// Char 230 (.)
	0x00, 0x00, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00,	// Char 231 (.)
	0x00, 0x00, 0xDB, 0xDB, 0xDB, 0xDB, 0xFF, 0x00,	// Char 232 (.)
	0x00, 0x00, 0xDB, 0xDB, 0xDB, 0xDB, 0xFF, 0x03,	// Char 233 (.)
	0x00, 0x00, 0xE0, 0x60, 0x7C, 0x66, 0x7C, 0x00,	// Char 234 (.)
	0x00, 0x00, 0xC6, 0xC6, 0xF6, 0xDE, 0xF6, 0x00,	// Char 235 (.)
	0x00, 0x00, 0x60, 0x60, 0x7C, 0x66, 0x7C, 0x00,	// Char 236 (.)
	0x00, 0x00, 0x7C, 0x06, 0x3E, 0x06, 0x7C, 0x00,	// Char 237 (.)
	0x00, 0x00, 0xCE, 0xDB, 0xFB, 0xDB, 0xCE, 0x00,	// Char 238 (.)
	0x00, 0x00, 0x3E, 0x66, 0x3E, 0x36, 0x66, 0x00,	// Char 239 (.)
	0x00, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0xFE, 0x00,	// Char 240 (.)
	0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x7C, 0x00,	// Char 241 (.)
	0x00, 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30,	// Char 242 (.)
	0x00, 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C,	// Char 243 (.)
	0x0E, 0x1B, 0x1B, 0x18, 0x18, 0x18, 0x18, 0x18,	// Char 244 (.)
	0x18, 0x18, 0x18, 0x18, 0x18, 0xD8, 0xD8, 0x70,	// Char 245 (.)
	0x00, 0x18, 0x18, 0x00, 0x7E, 0x00, 0x18, 0x18,	// Char 246 (.)
	0x00, 0x76, 0xDC, 0x00, 0x76, 0xDC, 0x00, 0x00,	// Char 247 (.)
	0x00, 0x38, 0x6C, 0x6C, 0x38, 0x00, 0x00, 0x00,	// Char 248 (.)
	0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,	// Char 249 (.)
	0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,	// Char 250 (.)
	0x03, 0x02, 0x06, 0x04, 0xCC, 0x68, 0x38, 0x10,	// Char 251 (.)
	0x3C, 0x42, 0x99, 0xA1, 0xA1, 0x99, 0x42, 0x3C,	// Char 252 (.)
	0x30, 0x48, 0x10, 0x20, 0x78, 0x00, 0x00, 0x00,	// Char 253 (.)
	0x00, 0x00, 0x7C, 0x7C, 0x7C, 0x7C, 0x00, 0x00,	// Char 254 (.)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x7E, 0x00	// Char 255 (.)
};
