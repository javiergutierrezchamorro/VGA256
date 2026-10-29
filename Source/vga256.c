/*------------------------------------------------------------------------------------------------------- */
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include "vga256.h"



/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)

#define VBE2SIGNATURE "VBE2"
#define VESASIGNATURE "VESA"

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

static void VBE_InitPM (void);
static void * pm_setwindowcall;
static void * pm_setdisplaystartcall;
static void * pmcode = NULL;


/*
*  This function pointers will be initialized after you called
*  VBE_Init. It'll be set to the realmode or protected mode call
*  code
*/

tagSetDisplayStartType VBE_SetDisplayStart;
tagSetBankType	VBE_SetBank;


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
	regs.w.ax =  0x0300;	      // Simulate Real-Mode interrupt
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
	if (regs.x.cflag) {
		p->segment = 0;
		p->selector = 0;
		return;
	}
	p->segment=regs.w.ax;
	p->selector=regs.w.dx;
}

void DPMI_FreeDOSMem (struct DPMI_PTR *p)
{
	/* DPMI call 101h free DOS memory */
	if (p->selector == 0) {
		return;
	}
	PrepareRegisters();
	regs.w.ax=0x0101;
	regs.w.dx=p->selector;
	int386x( 0x31, &regs, &regs, &sregs);
	p->segment = 0;
	p->selector = 0;
}

void DPMI_UNMAP_PHYSICAL (void *p)
{
	/* DPMI call 801h unmap physical memory*/
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
	if (regs.x.cflag) {
		return NULL;
	}
	return (void *) (((unsigned long) regs.w.bx << 16) + regs.w.cx);
}

void VBE_Controller_Information  (struct VBE_VbeInfoBlock * a)
{
	memcpy (a, VBE_Controller_Info_Pointer, sizeof (struct VBE_VbeInfoBlock));
}

void VBE_Mode_Information (short Mode, struct VBE_ModeInfoBlock * a)
{
	PrepareRegisters();
	RMI.EAX=0x00004f01;       // Get SVGA-Mode Information
	RMI.ECX=Mode;
	RMI.ES=VbeModePool.segment;       // Segment of realmode data
	RMI.EDI=0;                // offset of realmode data
	RMIRQ (0x10);
	if ( (RMI.EAX & 0xffff) != 0x004f ) {
		memset (a, 0, sizeof (struct VBE_ModeInfoBlock));
		return;
	}
	memcpy (a, VBE_ModeInfo_Pointer, sizeof (struct VBE_ModeInfoBlock));
}

int VBE_IsModeLinear (short Mode)
{
	struct VBE_ModeInfoBlock a;
	if ( Mode==0x13 ) {
		return 1;
	}
	VBE_Mode_Information (Mode, &a);
	return ((a.ModeAttributes & ATTR_LFB_Support)==ATTR_LFB_Support);
}

void asmSDS (short x, short y);
#pragma aux asmSDS = "mov ax, 0x4f07" \
"xor ebx, ebx" \
"call [pm_setdisplaystartcall]" \
parm[cx][dx] modify [eax ebx ecx edx esi edi];

static void PM_SetDisplayStart (short x, short y)
{
	asmSDS(x, y);
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
		BytesPerScanline = 320;
		BitsPerPixel = 8;
		vbelastbank = -1;
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
		RMI.EBX=Mode & 0x01ff;		  // mode without LFB but clear
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
	/* Cache the info needed by PM_SetDisplayStart and VBE_SetPixelsPerScanline */
	VBE_Mode_Information (Mode, &a);
	BytesPerScanline = a.BytesPerScanline;
	BitsPerPixel = a.BitsPerPixel;
	vbelastbank = -1;
}

int VBE_Test (void)
{
	return (VBE_Controller_Info_Pointer->vbeVersion.hi>=0x2);
}

unsigned int VBE_VideoMemory (void)
{
	return ((unsigned int) VBE_Controller_Info_Pointer->TotalMemory << 16);
}

int VBE_FindMode (int xres, int yres, char bpp)
{
	int i;
	struct VBE_ModeInfoBlock Info;
	// The standard VGA 320x200x256 mode is not part of the VBE mode list
	if ((xres == 320) && (yres == 200) && (bpp == 8)) {
		return 0x13;
	}
	// Find the mode in the ControllerInfoBlock:
	for (i=0; (i<1024) && (VBE_Controller_Info_Pointer->VideoModePtr[i]!=0xffff); i++ ) {
		VBE_Mode_Information (VBE_Controller_Info_Pointer->VideoModePtr[i], &Info);
		if ((Info.ModeAttributes & ATTR_HardwareMode) && (xres == Info.XResolution) && (yres == Info.YResolution) && (bpp == Info.BitsPerPixel)) {
			return VBE_Controller_Info_Pointer->VideoModePtr[i];
		}
	}
	return -1;
}

char * VBE_GetVideoPtr (short mode)
{
	struct VBE_ModeInfoBlock ModeInfo;
	if ( mode==0x13 ) return (char *) 0xa0000;
	VBE_Mode_Information (mode, &ModeInfo);
	if ( ModeInfo.PhysBasePtr == NULL ) return NULL;
	/* Unmap the last physical mapping (if there is one...) */
	if ( LastPhysicalMapping ) {
		DPMI_UNMAP_PHYSICAL (LastPhysicalMapping);
		LastPhysicalMapping = NULL;
	}
	LastPhysicalMapping = DPMI_MAP_PHYSICAL (ModeInfo.PhysBasePtr, (unsigned long)(VBE_Controller_Info_Pointer->TotalMemory)*64*1024);
	return (char *) LastPhysicalMapping;
}

static void RM_SetBank (short bnk)
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
	return (short) (RMI.EBX & 0xffff);
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
	return (VBE_Controller_Info_Pointer->Capabilities & CAP_8bit_DAC);
}

static void VBE_InitPM (void)
{
	unsigned short *pm_pointer;
	VBE_SetDisplayStart = RM_SetDisplayStart;
	VBE_SetBank	= RM_SetBank;
	PrepareRegisters();
	RMI.EAX=0x00004f0a;
	RMIRQ (0x10);
	if ( (RMI.EAX & 0xffff) == 0x004f ) {
		if (pmcode) free (pmcode);
		// copy the PM code to safe memory and call the copy, because
		// the BIOS buffer can be reused by other VBE calls
		pmcode = malloc (RMI.ECX  & 0x0000ffff);
		if (pmcode == NULL) return;
		pm_pointer =   (unsigned short *) (((unsigned long )RMI.ES<<4)+(RMI.EDI & 0x0000ffff));
		memcpy (pmcode, pm_pointer, (RMI.ECX  & 0x0000ffff));
		pm_pointer = (unsigned short *) pmcode;
		pm_setwindowcall =       (void *) ((char *) pmcode + pm_pointer[0]);
		pm_setdisplaystartcall = (void *) ((char *) pmcode + pm_pointer[1]);
		VBE_SetDisplayStart = PM_SetDisplayStart;
		VBE_SetBank	= PM_SetBank;
	}
}

int VBE_Init (void)
{
	/* Allocate the Dos Memory for Mode and Controller Information Blocks */
	/* and translate their pointers into flat memory space                */
	DPMI_AllocDOSMem (512/16, &VbeInfoPool);
	DPMI_AllocDOSMem (256/16, &VbeModePool);
	if ( (VbeInfoPool.segment == 0) || (VbeModePool.segment == 0) ) {
		DPMI_FreeDOSMem (&VbeModePool);
		DPMI_FreeDOSMem (&VbeInfoPool);
		return 0;
	}
	VBE_ModeInfo_Pointer =  (struct VBE_ModeInfoBlock *) (VbeModePool.segment*16);
	VBE_Controller_Info_Pointer = (struct VBE_VbeInfoBlock *) (VbeInfoPool.segment*16);

	/* Get Controller Information Block only once and copy this block on  */
	/* all requests                                                       */
	memset (VBE_Controller_Info_Pointer, 0, sizeof (struct VBE_VbeInfoBlock));
	memcpy (VBE_Controller_Info_Pointer->vbeSignature, VBE2SIGNATURE, 4);
	PrepareRegisters();
	RMI.EAX=0x00004f00;       // Get SVGA-Information
	RMI.ES=VbeInfoPool.segment;       // Segment of realmode data
	RMI.EDI=0;                // offset of realmode data
	RMIRQ (0x10);
	if ( ((RMI.EAX & 0xffff) != 0x004f) || (memcmp (VBE_Controller_Info_Pointer->vbeSignature, VESASIGNATURE, 4) != 0) ) {
		DPMI_FreeDOSMem (&VbeModePool);
		DPMI_FreeDOSMem (&VbeInfoPool);
		return 0;
	}
	// Translate the Realmode Pointers into flat-memory address space
	VBE_Controller_Info_Pointer->OemStringPtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemStringPtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemStringPtr);
	VBE_Controller_Info_Pointer->VideoModePtr=(unsigned short*)((((unsigned long)VBE_Controller_Info_Pointer->VideoModePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->VideoModePtr);
	VBE_Controller_Info_Pointer->OemVendorNamePtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemVendorNamePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemVendorNamePtr);
	VBE_Controller_Info_Pointer->OemProductNamePtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemProductNamePtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemProductNamePtr);
	VBE_Controller_Info_Pointer->OemProductRevPtr=(char*)((((unsigned long)VBE_Controller_Info_Pointer->OemProductRevPtr>>16)<<4)+(unsigned short)VBE_Controller_Info_Pointer->OemProductRevPtr);

	VBE_InitPM();
	return 1;
}

void VBE_Done (void)
{
	if ( LastPhysicalMapping ) {
		DPMI_UNMAP_PHYSICAL (LastPhysicalMapping);
		LastPhysicalMapping = NULL;
	}
	vbelastbank = -1;
	DPMI_FreeDOSMem (&VbeModePool);
	DPMI_FreeDOSMem (&VbeInfoPool);
	if (pmcode) {
		free (pmcode);
		pmcode = NULL;
	}
}


#if defined(VGA256_MODE_320X200X8)
	#define VGA256OFFSET(pVideo, x, y) (((unsigned char *) (pVideo))[((y) << 8) + ((y) << 6) + (x)])
#elif defined(VGA256_MODE_640X480X8)
	#define VGA256OFFSET(pVideo, x, y) (((unsigned char *) (pVideo))[((y) << 9) + ((y) << 7) + (x)])
#elif defined(VGA256_MODE_1024X768X8)
	#define VGA256OFFSET(pVideo, x, y) (((unsigned char *) (pVideo))[((y) << 10) + (x)])
#endif


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
	VGA256LineV(pVideo, x, y, height + 1, color);
	VGA256LineV(pVideo, x + width, y, height + 1, color);
	VGA256LineH(pVideo, x, y, width + 1, color);
	VGA256LineH(pVideo, x, y + height, width + 1, color);
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256FillBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
	unsigned int h;
	unsigned char *offset = (unsigned char *) &VGA256OFFSET(pVideo, x, y);

	for(h = 0; h < height; h++)
	{
		memset(offset, color, width);
		offset += VGA256_WIDTH;
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
		pcSource = (unsigned char *) pcSource + piWidth;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256PutImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight)
{
	unsigned int h;
	unsigned char* offset = (unsigned char*)&VGA256OFFSET(pVideo, piX, piY);


	if (piWidth == VGA256_WIDTH)
	{
		memcpy((void*) offset, (void*) pcSource, (size_t) (piWidth * piHeight));
		return;
	}

	for (h = 0; h < piHeight; h++)
	{
		memcpy((void*) offset, (void*) pcSource, (size_t) piWidth);
		offset += VGA256_WIDTH;
		pcSource = (unsigned char*)pcSource + piWidth;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256GetImage(void* pVideo, void* pcDest, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight)
{
	unsigned int h;
	unsigned char* offset = (unsigned char*)&VGA256OFFSET(pVideo, piX, piY);


	if (piWidth == VGA256_WIDTH)
	{
		memcpy((void*) pcDest, (void*) offset, (size_t) (piWidth * piHeight));
		return;
	}

	for (h = 0; h < piHeight; h++)
	{
		memcpy((void*) pcDest, (void*) offset, (size_t) piWidth);
		offset += VGA256_WIDTH;
		pcDest = (unsigned char*) pcDest + piWidth;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
	unsigned int x, y;
	unsigned int xpos, ypos, xstep, ystep;
	unsigned char* pRow;

	if ((widthd == widths) && (heightd == heights))
	{
		memcpy(pDest, pSource, widthd * heightd);
		return;
	}
	if ((widthd == 0) || (heightd == 0))
	{
		return;
	}

	xstep = (widths << 16) / widthd;
	ystep = (heights << 16) / heightd;
	ypos = 0;
	for (y = 0; y < heightd; y++)
	{
		pRow = pSource + (ypos >> 16) * widths;
		xpos = 0;
		for (x = 0; x < widthd; x++)
		{
			*pDest++ = pRow[xpos >> 16];
			xpos += xstep;
		}
		ypos += ystep;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage05x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
	unsigned int h, w;
	unsigned char* pRow;

	for (h = 0; h < heights; h += 2)
	{
		pRow = pSource + h * widths;
		for (w = 0; w < widths; w += 2)
		{
			*pDest++ = pRow[w];
		}
	}
}

/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage025x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
	unsigned int h, w;
	unsigned char* pRow;

	for (h = 0; h < heights; h += 4)
	{
		pRow = pSource + h * widths;
		for (w = 0; w < widths; w += 4)
		{
			*pDest++ = pRow[w];
		}
	}
}




/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage2x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
	unsigned int h, w, r;
	unsigned short* d = (unsigned short*)pDest;

	/* Every destination row is written once and sequentially, never read back (the destination is usually video memory) */
	for (h = 0; h < heights; h++)
	{
		for (r = 0; r < 2; r++)
		{
			for (w = 0; w < widths; w++)
			{
				*d++ = pSource[w] * 0x0101;
			}
		}
		pSource += widths;
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage4x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights)
{
	unsigned int h, w, r;
	unsigned int* d = (unsigned int*)pDest;

	/* Every destination row is written once and sequentially, never read back (the destination is usually video memory) */
	for (h = 0; h < heights; h++)
	{
		for (r = 0; r < 4; r++)
		{
			for (w = 0; w < widths; w++)
			{
				*d++ = pSource[w] * 0x01010101;
			}
		}
		pSource += widths;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256RotateImage(unsigned char* pDest, unsigned char* pSource, unsigned int width, unsigned int height, int angle)
{
	int hwidth = width >> 1;
	int hheight = height >> 1;
	int sinma, cosma;
	int x, y;
	int xs, yd;
	int fx, fy;

	angle %= 360;
	if (angle < 0) angle += 360;
	sinma = VGA256CosDeg[(angle + 90) % 360];
	cosma = VGA256CosDeg[angle];

	for (y = 0; y < (int) height; y++)
	{
		/* Source position in 18.14 fixed point, advanced by one step per pixel instead of two multiplications */
		fx = hwidth * (16384 - cosma) - (y - hheight) * sinma;
		fy = (y - hheight) * cosma + (hheight << 14) - hwidth * sinma;

		for (x = 0; x < (int) width; x++)
		{
			xs = fx >> 14;
			yd = fy >> 14;

			if (((unsigned) xs < width) && ((unsigned) yd < height))
			{
				*pDest++ = pSource[yd * width + xs];
			}
			else
			{
				*pDest++ = 0;
			}
			fx += cosma;
			fy += sinma;
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
	unsigned char paleta[768];

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
void VGA256FadeIn(unsigned char* paleta)
{
	unsigned int x, y;
	unsigned char pal[768];

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
void VGA256FadeTo(unsigned char* paleta)
{
	unsigned int x, y;
	unsigned char pal[768];

	VGA256GetPalette(pal);

	for (y = 0; y < 63; y++)
	{
		for (x = 0; x < sizeof(pal); x++)
		{
			if (pal[x] < paleta[x])
			{
				pal[x]++;
			}
			else if (pal[x] > paleta[x])
			{
				pal[x]--;
			}
		}
		VGA256WaitVRetrace();
		VGA256SetPalette(pal);
	}
}


/*------------------------------------------------------------------------------------------------------- */
void VGA256Circle(void* pVideo, unsigned int x, unsigned int y, unsigned int radio, unsigned int color)
{
	int xl, yl, ylw;
	unsigned int d, limit;
	unsigned char* pCenter;

	if (radio == 0)
	{
		VGA256PutPixel(pVideo, x, y, color);
		return;
	}

	pCenter = (unsigned char*) &VGA256OFFSET(pVideo, x, y);
	xl = 0;
	yl = radio;
	ylw = yl * VGA256_WIDTH;
	d = radio * radio;
	limit = d + 1;

	/* d is kept equal to xl * xl + yl * yl without multiplications */
	while (yl != 0)
	{
		pCenter[xl + ylw] = color;
		pCenter[-xl + ylw] = color;
		pCenter[xl - ylw] = color;
		pCenter[-xl - ylw] = color;
		if (d >= limit)
		{
			d -= (yl << 1) - 1;
			yl--;
			ylw -= VGA256_WIDTH;
		}
		else
		{
			xl++;
			d += (xl << 1) - 1;
		}
	}
	pCenter[xl] = color;
	pCenter[-xl] = color;
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256Line(void* pVideo, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned char color)
{
	unsigned int i, n, m;
	int d1x, d1y, d2x, d2y, s, u, v;
	int step1, step2;
	unsigned char* offset;

	u = c - a;
	v = d - b;
	d1x = _VGA256Sgn(u);
	d1y = _VGA256Sgn(v);
	d2x = _VGA256Sgn(u);
	d2y = 0;
	m = _VGA256ABS(u);
	n = _VGA256ABS(v);
	if (m <= n)
	{
		d2x = 0;
		d2y = _VGA256Sgn(v);
		m = _VGA256ABS(v);
		n = _VGA256ABS(u);
	}
	step1 = d1x + d1y * VGA256_WIDTH;
	step2 = d2x + d2y * VGA256_WIDTH;
	offset = (unsigned char*) &VGA256OFFSET(pVideo, a, b);
	s = m >> 1;
	for (i = 0; i <= m; i++)
	{
		*offset = color;
		s += n;
		if (s >= m)
		{
			s -= m;
			offset += step1;
		}
		else
		{
			offset += step2;
		}
	}
}


/* ----------------------------------------------------------------------------------------------------------------- */
int VGA256KbHit(void)
{
	return(kbhit());
}


/* ----------------------------------------------------------------------------------------------------------------- */
int VGA256GetCh(void)
{
	return(getch());
}


/* ----------------------------------------------------------------------------------------------------------------- */
static int VGA256FloodPush(unsigned long** ppila, unsigned int* psp, unsigned int* pcap, unsigned long seed)
{
	unsigned long* npila;

	if (*psp == *pcap)
	{
		npila = (unsigned long*) realloc(*ppila, ((*pcap) << 1) * sizeof(unsigned long));
		if (npila == NULL) return 0;
		*ppila = npila;
		*pcap <<= 1;
	}
	(*ppila)[*psp] = seed;
	(*psp)++;
	return 1;
}

void VGA256FloodFill(void *pVideo, unsigned int x, unsigned int y, unsigned int new_col, unsigned int old_col)
{
	unsigned char* video = (unsigned char*) pVideo;
	unsigned char* row;
	unsigned char* adj;
	unsigned long* pila = NULL;
	unsigned long seed;
	unsigned int sp = 0, cap = 256;
	unsigned int sx, sy, xl, xr, nx;
	unsigned char cnew = (unsigned char) new_col;
	unsigned char cold = (unsigned char) old_col;

	if (cnew == cold) return;
	if (x >= VGA256_WIDTH || y >= VGA256_HEIGHT) return;
	if (video[y * VGA256_WIDTH + x] != cold) return;

	pila = (unsigned long*) malloc(cap * sizeof(unsigned long));
	if (pila == NULL) return;
	pila[sp++] = ((unsigned long) y << 16) | x;

	while (sp > 0)
	{
		sp--;
		seed = pila[sp];
		sx = (unsigned int) (seed & 0xffff);
		sy = (unsigned int) (seed >> 16);
		row = video + sy * VGA256_WIDTH;
		xl = sx;
		while (xl > 0 && row[xl - 1] == cold) xl--;
		xr = xl;
		while (xr < VGA256_WIDTH && row[xr] == cold) xr++;
		memset(&row[xl], cnew, xr - xl);
		if (sy > 0)
		{
			adj = row - VGA256_WIDTH;
			nx = xl;
			while (nx < xr)
			{
				if (adj[nx] == cold)
				{
					if (!VGA256FloodPush(&pila, &sp, &cap, ((unsigned long) (sy - 1) << 16) | nx))
					{
						free(pila);
						return;
					}
					while (nx < xr && adj[nx] == cold) nx++;
				}
				else nx++;
			}
		}
		if (sy + 1 < VGA256_HEIGHT)
		{
			adj = row + VGA256_WIDTH;
			nx = xl;
			while (nx < xr)
			{
				if (adj[nx] == cold)
				{
					if (!VGA256FloodPush(&pila, &sp, &cap, ((unsigned long) (sy + 1) << 16) | nx))
					{
						free(pila);
						return;
					}
					while (nx < xr && adj[nx] == cold) nx++;
				}
				else nx++;
			}
		}
	}
	free(pila);
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText(void* pVideo, const char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, const unsigned char* font)
{
	unsigned int y;
	unsigned char mask;
	const unsigned char* cptr;
	unsigned char* offset;

	if (font == NULL)
	{
		font = (const unsigned char*) 0xFFA6E;
	}

	while (*string != 0)
	{
		cptr = &font[(unsigned char) *string << 3];
		for (y = 0; y < 8; y++)
		{
			offset = &VGA256OFFSET(pVideo, x_cursor, y_cursor + y);
			for (mask = 0x80; mask != 0; mask >>= 1)
			{
				if (cptr[y] & mask)
				{
					*(offset) = color;
				}
				offset++;
			}
		}
		x_cursor += 8;
		string++;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText2x(void* pVideo, const char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, const unsigned char* font)
{
	unsigned int y;
	unsigned char mask;
	const unsigned char* cptr;
	unsigned short* offset;
	unsigned short c;

	if (font == NULL)
	{
		font = (const unsigned char*) 0xFFA6E;
	}

	c = (color & 0xff) * 0x0101;
	while (*string != 0)
	{
		cptr = &font[(unsigned char) *string << 3];
		for (y = 0; y < 8; y++)
		{
			offset = (unsigned short *) &VGA256OFFSET(pVideo, x_cursor, y_cursor + (y << 1));
			for (mask = 0x80; mask != 0; mask >>= 1)
			{
				if (cptr[y] & mask)
				{
					*(offset) = c;
					*(offset + (VGA256_WIDTH >> 1)) = c;
				}
				offset++;
			}
		}
		x_cursor += 16;
		string++;
	}
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256OutText4x(void* pVideo, const char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, const unsigned char* font)
{
	unsigned int y;
	unsigned char mask;
	const unsigned char* cptr;
	unsigned int* offset;
	unsigned int c;

	if (font == NULL)
	{
		font = (const unsigned char*) 0xFFA6E;
	}

	c = (color & 0xff) * 0x01010101;
	while (*string != 0)
	{
		cptr = &font[(unsigned char) *string << 3];
		for (y = 0; y < 8; y++)
		{
			offset = (unsigned int *) &VGA256OFFSET(pVideo, x_cursor, y_cursor + (y << 2));
			for (mask = 0x80; mask != 0; mask >>= 1)
			{
				if (cptr[y] & mask)
				{
					*(offset) = c;
					*(offset + (VGA256_WIDTH >> 2)) = c;
					*(offset + (VGA256_WIDTH >> 1)) = c;
					*(offset + ((VGA256_WIDTH >> 2) * 3)) = c;
				}
				offset++;
			}
		}
		x_cursor += 32;
		string++;
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
int VGA256LoadPCX(const char* filename, unsigned char* dest, unsigned char* pal)
{
	#define VGA256LoadPCXBufLen (16384)
	unsigned int i;
	unsigned int bufptr = 0;
	int readlen = 0;
	int infile = -1;
	unsigned int outbyte = 0, bytecount = 0;
	unsigned int width, height, bpl, remaining, col;
	unsigned char* buffer = NULL;
	unsigned char marker;
	struct pcx_header pcx_header;
	int res = -9;


	if ((infile = _open(filename, O_BINARY | O_RDONLY)) == -1)
	{
		res = -1;   /* Cannot open */
	}
	else
	{
		readlen = _read(infile, &pcx_header, sizeof(pcx_header));
		if ((readlen == sizeof(pcx_header)) && (pcx_header.manufacturer == 10) && (pcx_header.bits_per_pixel == 8) && (pcx_header.color_planes == 1) && (pcx_header.xmax >= pcx_header.xmin) && (pcx_header.ymax >= pcx_header.ymin) && (pcx_header.bytes_per_line >= pcx_header.xmax - pcx_header.xmin + 1))
		{
			width = pcx_header.xmax - pcx_header.xmin + 1;
			height = pcx_header.ymax - pcx_header.ymin + 1;
			bpl = (unsigned short) pcx_header.bytes_per_line;
			res = (int) (width * height);
			if (dest)
			{
				buffer = malloc(VGA256LoadPCXBufLen);
				if (buffer)
				{
					readlen = 0;
					remaining = bpl * height;
					col = 0;
					/* Every scanline is stored with bytes_per_line bytes, the padding is skipped */
					while (remaining > 0)
					{
						if (bufptr >= (unsigned int) readlen)
						{
							bufptr = 0;
							if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) <= 0)
							{
								break;
							}
						}
						outbyte = buffer[bufptr++];
						bytecount = 1;
						if (outbyte > 0xbf)
						{
							bytecount = outbyte & 0x3f;
							if (bufptr >= (unsigned int) readlen)
							{
								bufptr = 0;
								if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) <= 0)
								{
									break;
								}
							}
							outbyte = buffer[bufptr++];
						}
						while ((bytecount > 0) && (remaining > 0))
						{
							if (col < width)
							{
								*dest++ = (unsigned char) outbyte;
							}
							if (++col == bpl)
							{
								col = 0;
							}
							bytecount--;
							remaining--;
						}
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
				_lseek(infile, -769L, SEEK_END);
				_read(infile, &marker, 1);
				readlen = _read(infile, pal, 3 * 256);
				if ((marker == 12) && (readlen == 3 * 256))
				{
					for (i = 0; i < 3 * 256; i++)
					{
						pal[i] = pal[i] >> 2;
					}
				}
				else
				{
					res = -2;   /* No 256 color palette */
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


/*------------------------------------------------------------------------------------------------------- */
int VGA256DecodePCX(unsigned char *dest, const unsigned char *buffer)
{
	unsigned int i;
	unsigned int bufptr = 0;
	unsigned int outbyte = 0, bytecount = 0;
	unsigned int width, height, bpl, remaining, col;
	struct pcx_header pcx_header;
	int res = -9;

	memcpy(&pcx_header, buffer, sizeof(pcx_header));
	if ((pcx_header.manufacturer == 10) && (pcx_header.bits_per_pixel == 8) && (pcx_header.color_planes == 1) && (pcx_header.xmax >= pcx_header.xmin) && (pcx_header.ymax >= pcx_header.ymin) && (pcx_header.bytes_per_line >= pcx_header.xmax - pcx_header.xmin + 1))
	{
		width = pcx_header.xmax - pcx_header.xmin + 1;
		height = pcx_header.ymax - pcx_header.ymin + 1;
		bpl = (unsigned short) pcx_header.bytes_per_line;
		bufptr += sizeof(pcx_header);
		remaining = bpl * height;
		col = 0;
		/* Every scanline is stored with bytes_per_line bytes, the padding is skipped */
		while (remaining > 0)
		{
			outbyte = buffer[bufptr++];
			bytecount = 1;
			if (outbyte > 0xbf)
			{
				bytecount = outbyte & 0x3f;
				outbyte = buffer[bufptr++];
			}
			while ((bytecount > 0) && (remaining > 0))
			{
				if (col < width)
				{
					*dest++ = (unsigned char) outbyte;
				}
				if (++col == bpl)
				{
					col = 0;
				}
				bytecount--;
				remaining--;
			}
		}
		/* The palette is stored right after the image, so dest must have room for width * height + 768 bytes */
		if (buffer[bufptr] == 0x0c) bufptr++;   /* skip PCX-256 palette marker if present */
		for (i = 0; i < 768; i++)
		{
			(*dest++) = (unsigned char)(buffer[bufptr++] >> 2);
		}
		res = (int) (width * height);
	}
	else
	{
		res = -2;   /* Invalid PCX */
	}
	return(res);
}


/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)
struct bmp_header
{
	unsigned short type;        // Magic identifier: 0x4d42
	unsigned long size;     // File size in bytes
	unsigned short reserved1;        // Not used
	unsigned short reserved2;        // Not used
	unsigned long offset;     // Offset to image data in bytes from beginning of file (54 bytes)
	unsigned long dib_header_size;  // DIB Header size in bytes (40 bytes)
	long width_px;    // Width of the image
	long height_px;        // Height of image
	unsigned short  num_planes;       // Number of color planes
	unsigned short  bits_per_pixel;   // Bits per pixel
	unsigned long  compression;      // Compression type
	unsigned long  image_size_bytes; // Image size in bytes
	long x_resolution_ppm; // Pixels per meter
	long y_resolution_ppm; // Pixels per meter
	unsigned long  num_colors;       // Number of colors
	unsigned long  important_colors; // Important colors

};
#pragma pack (pop)


/*------------------------------------------------------------------------------------------------------- */
int VGA256LoadBMP(const char* filename, unsigned char* dest, unsigned char* pal)
{
	int infile = -1;
	struct bmp_header bmp_header;
	unsigned char bgra[1024];
	unsigned int colors, i, padding;
	long y, step, width, height;
	int res = -9;


	if ((infile = _open(filename, O_BINARY | O_RDONLY)) == -1)
	{
		res = -1;   /* Cannot open */
	}
	else
	{
		if ((_read(infile, &bmp_header, sizeof(bmp_header)) == sizeof(bmp_header)) && (bmp_header.type == 0x4d42) && (bmp_header.bits_per_pixel == 8) && (bmp_header.num_planes == 1) && (bmp_header.num_colors <= 256) && (bmp_header.compression == 0) && (bmp_header.width_px > 0) && (bmp_header.height_px != 0))
		{
			colors = (bmp_header.num_colors == 0) ? 256 : bmp_header.num_colors;
			width = bmp_header.width_px;
			height = bmp_header.height_px;
			if (height < 0)
			{
				/* Top-down BMP */
				height = -height;
				y = 0;
				step = 1;
			}
			else
			{
				y = height - 1;
				step = -1;
			}
			res = (int) (width * height);
			if (pal)
			{
				_lseek(infile, 14 + bmp_header.dib_header_size, SEEK_SET);
				_read(infile, bgra, colors * 4);
				for (i = 0; i < colors; i++)
				{
					pal[i * 3] = bgra[i * 4 + 2] >> 2;
					pal[i * 3 + 1] = bgra[i * 4 + 1] >> 2;
					pal[i * 3 + 2] = bgra[i * 4] >> 2;
				}
			}
			if (dest)
			{
				padding = (unsigned int) (((width + 3) & ~3L) - width);
				_lseek(infile, bmp_header.offset, SEEK_SET);
				for (i = 0; i < (unsigned int) height; i++)
				{
					_read(infile, &dest[y * width], (unsigned int) width);
					if (padding)
					{
						_lseek(infile, (long) padding, SEEK_CUR);
					}
					y += step;
				}
			}
		}
		else
		{
			res = -2;   /* Invalid BMP */
		}
	}
	if (infile != -1)
	{
		_close(infile);
	}
	return(res);
}



/*------------------------------------------------------------------------------------------------------- */
const int VGA256CosDeg[] =
{
	16384, // 0 1.000000
	16382, // 1 0.999848
	16374, // 2 0.999391
	16362, // 3 0.998630
	16344, // 4 0.997564
	16322, // 5 0.996195
	16294, // 6 0.994522
	16262, // 7 0.992546
	16225, // 8 0.990268
	16182, // 9 0.987688
	16135, // 10 0.984808
	16083, // 11 0.981627
	16026, // 12 0.978148
	15964, // 13 0.974370
	15897, // 14 0.970296
	15826, // 15 0.965926
	15749, // 16 0.961262
	15668, // 17 0.956305
	15582, // 18 0.951057
	15491, // 19 0.945519
	15396, // 20 0.939693
	15296, // 21 0.933580
	15191, // 22 0.927184
	15082, // 23 0.920505
	14968, // 24 0.913545
	14849, // 25 0.906308
	14726, // 26 0.898794
	14598, // 27 0.891007
	14466, // 28 0.882948
	14330, // 29 0.874620
	14189, // 30 0.866025
	14044, // 31 0.857167
	13894, // 32 0.848048
	13741, // 33 0.838671
	13583, // 34 0.829038
	13421, // 35 0.819152
	13255, // 36 0.809017
	13085, // 37 0.798636
	12911, // 38 0.788011
	12733, // 39 0.777146
	12551, // 40 0.766044
	12365, // 41 0.754710
	12176, // 42 0.743145
	11982, // 43 0.731354
	11786, // 44 0.719340
	11585, // 45 0.707107
	11381, // 46 0.694658
	11174, // 47 0.681998
	10963, // 48 0.669131
	10749, // 49 0.656059
	10531, // 50 0.642788
	10311, // 51 0.629320
	10087, // 52 0.615661
	9860, // 53 0.601815
	9630, // 54 0.587785
	9397, // 55 0.573576
	9162, // 56 0.559193
	8923, // 57 0.544639
	8682, // 58 0.529919
	8438, // 59 0.515038
	8192, // 60 0.500000
	7943, // 61 0.484810
	7692, // 62 0.469472
	7438, // 63 0.453990
	7182, // 64 0.438371
	6924, // 65 0.422618
	6664, // 66 0.406737
	6402, // 67 0.390731
	6138, // 68 0.374607
	5872, // 69 0.358368
	5604, // 70 0.342020
	5334, // 71 0.325568
	5063, // 72 0.309017
	4790, // 73 0.292372
	4516, // 74 0.275637
	4240, // 75 0.258819
	3964, // 76 0.241922
	3686, // 77 0.224951
	3406, // 78 0.207912
	3126, // 79 0.190809
	2845, // 80 0.173648
	2563, // 81 0.156434
	2280, // 82 0.139173
	1997, // 83 0.121869
	1713, // 84 0.104528
	1428, // 85 0.087156
	1143, // 86 0.069756
	857, // 87 0.052336
	572, // 88 0.034899
	286, // 89 0.017452
	0, // 90 0.000000
	-286, // 91 -0.017452
	-572, // 92 -0.034899
	-857, // 93 -0.052336
	-1143, // 94 -0.069756
	-1428, // 95 -0.087156
	-1713, // 96 -0.104528
	-1997, // 97 -0.121869
	-2280, // 98 -0.139173
	-2563, // 99 -0.156434
	-2845, // 100 -0.173648
	-3126, // 101 -0.190809
	-3406, // 102 -0.207912
	-3686, // 103 -0.224951
	-3964, // 104 -0.241922
	-4240, // 105 -0.258819
	-4516, // 106 -0.275637
	-4790, // 107 -0.292372
	-5063, // 108 -0.309017
	-5334, // 109 -0.325568
	-5604, // 110 -0.342020
	-5872, // 111 -0.358368
	-6138, // 112 -0.374607
	-6402, // 113 -0.390731
	-6664, // 114 -0.406737
	-6924, // 115 -0.422618
	-7182, // 116 -0.438371
	-7438, // 117 -0.453990
	-7692, // 118 -0.469472
	-7943, // 119 -0.484810
	-8192, // 120 -0.500000
	-8438, // 121 -0.515038
	-8682, // 122 -0.529919
	-8923, // 123 -0.544639
	-9162, // 124 -0.559193
	-9397, // 125 -0.573576
	-9630, // 126 -0.587785
	-9860, // 127 -0.601815
	-10087, // 128 -0.615661
	-10311, // 129 -0.629320
	-10531, // 130 -0.642788
	-10749, // 131 -0.656059
	-10963, // 132 -0.669131
	-11174, // 133 -0.681998
	-11381, // 134 -0.694658
	-11585, // 135 -0.707107
	-11786, // 136 -0.719340
	-11982, // 137 -0.731354
	-12176, // 138 -0.743145
	-12365, // 139 -0.754710
	-12551, // 140 -0.766044
	-12733, // 141 -0.777146
	-12911, // 142 -0.788011
	-13085, // 143 -0.798636
	-13255, // 144 -0.809017
	-13421, // 145 -0.819152
	-13583, // 146 -0.829038
	-13741, // 147 -0.838671
	-13894, // 148 -0.848048
	-14044, // 149 -0.857167
	-14189, // 150 -0.866025
	-14330, // 151 -0.874620
	-14466, // 152 -0.882948
	-14598, // 153 -0.891007
	-14726, // 154 -0.898794
	-14849, // 155 -0.906308
	-14968, // 156 -0.913545
	-15082, // 157 -0.920505
	-15191, // 158 -0.927184
	-15296, // 159 -0.933580
	-15396, // 160 -0.939693
	-15491, // 161 -0.945519
	-15582, // 162 -0.951057
	-15668, // 163 -0.956305
	-15749, // 164 -0.961262
	-15826, // 165 -0.965926
	-15897, // 166 -0.970296
	-15964, // 167 -0.974370
	-16026, // 168 -0.978148
	-16083, // 169 -0.981627
	-16135, // 170 -0.984808
	-16182, // 171 -0.987688
	-16225, // 172 -0.990268
	-16262, // 173 -0.992546
	-16294, // 174 -0.994522
	-16322, // 175 -0.996195
	-16344, // 176 -0.997564
	-16362, // 177 -0.998630
	-16374, // 178 -0.999391
	-16382, // 179 -0.999848
	-16384, // 180 -1.000000
	-16382, // 181 -0.999848
	-16374, // 182 -0.999391
	-16362, // 183 -0.998630
	-16344, // 184 -0.997564
	-16322, // 185 -0.996195
	-16294, // 186 -0.994522
	-16262, // 187 -0.992546
	-16225, // 188 -0.990268
	-16182, // 189 -0.987688
	-16135, // 190 -0.984808
	-16083, // 191 -0.981627
	-16026, // 192 -0.978148
	-15964, // 193 -0.974370
	-15897, // 194 -0.970296
	-15826, // 195 -0.965926
	-15749, // 196 -0.961262
	-15668, // 197 -0.956305
	-15582, // 198 -0.951057
	-15491, // 199 -0.945519
	-15396, // 200 -0.939693
	-15296, // 201 -0.933580
	-15191, // 202 -0.927184
	-15082, // 203 -0.920505
	-14968, // 204 -0.913545
	-14849, // 205 -0.906308
	-14726, // 206 -0.898794
	-14598, // 207 -0.891007
	-14466, // 208 -0.882948
	-14330, // 209 -0.874620
	-14189, // 210 -0.866025
	-14044, // 211 -0.857167
	-13894, // 212 -0.848048
	-13741, // 213 -0.838671
	-13583, // 214 -0.829038
	-13421, // 215 -0.819152
	-13255, // 216 -0.809017
	-13085, // 217 -0.798636
	-12911, // 218 -0.788011
	-12733, // 219 -0.777146
	-12551, // 220 -0.766044
	-12365, // 221 -0.754710
	-12176, // 222 -0.743145
	-11982, // 223 -0.731354
	-11786, // 224 -0.719340
	-11585, // 225 -0.707107
	-11381, // 226 -0.694658
	-11174, // 227 -0.681998
	-10963, // 228 -0.669131
	-10749, // 229 -0.656059
	-10531, // 230 -0.642788
	-10311, // 231 -0.629320
	-10087, // 232 -0.615661
	-9860, // 233 -0.601815
	-9630, // 234 -0.587785
	-9397, // 235 -0.573576
	-9162, // 236 -0.559193
	-8923, // 237 -0.544639
	-8682, // 238 -0.529919
	-8438, // 239 -0.515038
	-8192, // 240 -0.500000
	-7943, // 241 -0.484810
	-7692, // 242 -0.469472
	-7438, // 243 -0.453990
	-7182, // 244 -0.438371
	-6924, // 245 -0.422618
	-6664, // 246 -0.406737
	-6402, // 247 -0.390731
	-6138, // 248 -0.374607
	-5872, // 249 -0.358368
	-5604, // 250 -0.342020
	-5334, // 251 -0.325568
	-5063, // 252 -0.309017
	-4790, // 253 -0.292372
	-4516, // 254 -0.275637
	-4240, // 255 -0.258819
	-3964, // 256 -0.241922
	-3686, // 257 -0.224951
	-3406, // 258 -0.207912
	-3126, // 259 -0.190809
	-2845, // 260 -0.173648
	-2563, // 261 -0.156434
	-2280, // 262 -0.139173
	-1997, // 263 -0.121869
	-1713, // 264 -0.104528
	-1428, // 265 -0.087156
	-1143, // 266 -0.069756
	-857, // 267 -0.052336
	-572, // 268 -0.034899
	-286, // 269 -0.017452
	0, // 270 0.000000
	286, // 271 0.017452
	572, // 272 0.034899
	857, // 273 0.052336
	1143, // 274 0.069756
	1428, // 275 0.087156
	1713, // 276 0.104528
	1997, // 277 0.121869
	2280, // 278 0.139173
	2563, // 279 0.156434
	2845, // 280 0.173648
	3126, // 281 0.190809
	3406, // 282 0.207912
	3686, // 283 0.224951
	3964, // 284 0.241922
	4240, // 285 0.258819
	4516, // 286 0.275637
	4790, // 287 0.292372
	5063, // 288 0.309017
	5334, // 289 0.325568
	5604, // 290 0.342020
	5872, // 291 0.358368
	6138, // 292 0.374607
	6402, // 293 0.390731
	6664, // 294 0.406737
	6924, // 295 0.422618
	7182, // 296 0.438371
	7438, // 297 0.453990
	7692, // 298 0.469472
	7943, // 299 0.484810
	8192, // 300 0.500000
	8438, // 301 0.515038
	8682, // 302 0.529919
	8923, // 303 0.544639
	9162, // 304 0.559193
	9397, // 305 0.573576
	9630, // 306 0.587785
	9860, // 307 0.601815
	10087, // 308 0.615661
	10311, // 309 0.629320
	10531, // 310 0.642788
	10749, // 311 0.656059
	10963, // 312 0.669131
	11174, // 313 0.681998
	11381, // 314 0.694658
	11585, // 315 0.707107
	11786, // 316 0.719340
	11982, // 317 0.731354
	12176, // 318 0.743145
	12365, // 319 0.754710
	12551, // 320 0.766044
	12733, // 321 0.777146
	12911, // 322 0.788011
	13085, // 323 0.798636
	13255, // 324 0.809017
	13421, // 325 0.819152
	13583, // 326 0.829038
	13741, // 327 0.838671
	13894, // 328 0.848048
	14044, // 329 0.857167
	14189, // 330 0.866025
	14330, // 331 0.874620
	14466, // 332 0.882948
	14598, // 333 0.891007
	14726, // 334 0.898794
	14849, // 335 0.906308
	14968, // 336 0.913545
	15082, // 337 0.920505
	15191, // 338 0.927184
	15296, // 339 0.933580
	15396, // 340 0.939693
	15491, // 341 0.945519
	15582, // 342 0.951057
	15668, // 343 0.956305
	15749, // 344 0.961262
	15826, // 345 0.965926
	15897, // 346 0.970296
	15964, // 347 0.974370
	16026, // 348 0.978148
	16083, // 349 0.981627
	16135, // 350 0.984808
	16182, // 351 0.987688
	16225, // 352 0.990268
	16262, // 353 0.992546
	16294, // 354 0.994522
	16322, // 355 0.996195
	16344, // 356 0.997564
	16362, // 357 0.998630
	16374, // 358 0.999391
	16382, // 359 0.999848
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
