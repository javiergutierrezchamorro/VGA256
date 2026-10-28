#ifndef _VGA256_H
#define _VGA256_H
#include <i86.h>
#include <string.h>


#pragma pack (push, 1)

#define VESA_OK           0
#define VESA_FAIL         1
#define VESA_NOTSUPPORTED 2
#define VESA_INVALIDMODE  3

/* Bit-Definition of the ControllerInfo Capability fields */
#define CAP_8bit_DAC              1
#define CAP_VGACompatible         2
#define CAP_Use_VBE_DAC_Functions 4

/* Bit-Definition of the ModeInfo Attribute fields */
#define ATTR_HardwareMode         1
#define ATTR_TTY_Support          2
#define ATTR_ColorMode            4
#define ATTR_GraphicsMode         8
#define ATTR_No_VGA_Mode         16
#define ATTR_No_SegA000          32
#define ATTR_LFB_Support         64

/* Bit-Definitions of the Window Attributes */
#define WATTR_Relocatable         1
#define WATTR_Readable            2
#define WATTR_Writeable           4

/* Definitions of the MemoryModel Field */
#define MM_TextMode               0
#define MM_CGA_Graphics           1
#define MM_Hercules_Graphics      2
#define MM_Planar                 3
#define MM_PackedPixel            4
#define MM_UnChained              5
#define MM_DirectColor            6
#define MM_YUV                    7


#ifdef __cplusplus
extern "C" {
#endif

typedef void ( * tagSetDisplayStartType )(short x, short y);
typedef void ( * tagSetBankType )(short bnk);

struct bcd16 {
	unsigned char lo;
	unsigned char hi;
};

struct DPMI_PTR
{
	unsigned short int segment;
	unsigned short int selector;
};

struct VBE_VbeInfoBlock
{
	char           vbeSignature[4];
	struct bcd16   vbeVersion;
	char *         OemStringPtr;
	unsigned long  Capabilities;
	unsigned short * VideoModePtr;
	unsigned short TotalMemory;
	unsigned short OemSoftwareRev;
	char *         OemVendorNamePtr;
	char *         OemProductNamePtr;
	char *         OemProductRevPtr;
	char           Reserved[222];
	char           OemData[256];
};

struct  VBE_ModeInfoBlock
{
	unsigned short ModeAttributes;
	char WinAAttributes;
	char WinBAttributes;
	unsigned short Granularity;
	unsigned short WinSize;
	unsigned short WinASegment;
	unsigned short WinBSegment;
	void *         WinFuncPtr;
	unsigned short BytesPerScanline;
	unsigned short XResolution;
	unsigned short YResolution;
	char XCharSize;
	char YCharSize;
	char NumberOfPlanes;
	char BitsPerPixel;
	char NumberOfBanks;
	char MemoryModel;
	char BankSize;
	char NumberOfImagePages;
	char Reserved;
	char RedMaskSize;
	char RedFieldPosition;
	char GreenMaskSize;
	char GreenFieldPosition;
	char BlueMaskSize;
	char BlueFieldPosition;
	char RsvdMaskSize;
	char RsvdFieldPosition;
	char DirectColorModeInfo;
	void * PhysBasePtr;
	void * OffScreenMemOffset;
	unsigned short OffScreenMemSize;
	char  reserved2[206];
};

#pragma pack (pop)

void VBE_Init (void);
void VBE_Done (void);
void DPMI_AllocDOSMem (short int paras, struct DPMI_PTR *p);
void DPMI_FreeDOSMem (struct DPMI_PTR *p);
void * DPMI_MAP_PHYSICAL (void *p, unsigned long size);
void DPMI_UNMAP_PHYSICAL (void *p);
int VBE_Test (void);
void VBE_Controller_Information  (struct VBE_VbeInfoBlock * a);
int VBE_IsModeLinear (short Mode);
extern tagSetBankType VBE_SetBank;
unsigned int VBE_VideoMemory (void);
void VBE_Mode_Information (short Mode, struct VBE_ModeInfoBlock * a);
int VBE_FindMode (int xres, int yres, char bpp);
void VBE_SetMode (short Mode, int linear, int clear);
char * VBE_GetVideoPtr (short mode);
extern tagSetDisplayStartType VBE_SetDisplayStart;
void VBE_SetPixelsPerScanline (short Pixels);
short VBE_MaxBytesPerScanline (void);
void VBE_SetDACWidth (char bits);
int VBE_8BitDAC (void);


/*------------------------------------------------------------------------------------------------------- */
#if !defined(VGA256_MODE_320X200X8) && !defined(VGA256_MODE_640X480X8) && !defined(VGA256_MODE_1024X768X8)
#define VGA256_MODE_640X480X8
#endif
#if defined(VGA256_MODE_320X200X8)
	#define VGA256_WIDTH (320)
	#define VGA256_HEIGHT (200)
#elif defined(VGA256_MODE_640X480X8)
	#define VGA256_WIDTH (640)
	#define VGA256_HEIGHT (480)
#elif defined(VGA256_MODE_1024X768X8)
	#define VGA256_WIDTH (1024)
	#define VGA256_HEIGHT (768)
#endif


/*------------------------------------------------------------------------------------------------------- */
#define VGA256PutScreen(pVideo, b) (memcpy(pVideo, b, VGA256_WIDTH * VGA256_HEIGHT))
#define _VGA256ABS(a) ((a < 0) ? -a : a)
#define _VGA256Sgn(a) (a > 0 ? 1 : (a < 0 ? -1 : 0))


/*------------------------------------------------------------------------------------------------------- */
extern const int VGA256SinDeg[];
extern const int VGA256CosDeg[];
extern const unsigned char VGA256Font[];
void VGA256PutPixel(void *pVideo, unsigned int x, unsigned int y, unsigned int color);
unsigned int VGA256GetPixel(void *pVideo, unsigned int x, unsigned int y);
void VGA256LineH(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int color);
void VGA256LineV(void *pVideo, unsigned int x, unsigned int y, unsigned int height, unsigned int color);
void VGA256DrawBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void VGA256FillBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void VGA256ClearScreen(void *pVideo, unsigned int color);
void VGA256PutSprite(void *pVideo, void *pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);
void VGA256PutImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);
void VGA256GetImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);
void VGA256WaitVRetrace(void);
void VGA256SetPalette(const void* pal);
void VGA256GetPalette(void* pal);
void VGA256FadeOut(void);
void VGA256FadeIn(unsigned char* paleta);
void VGA256FadeTo(unsigned char* paleta);
void VGA256Circle(void* pVideo, unsigned int x, unsigned int y, unsigned int radio, unsigned int color);
void VGA256Line(void* pVideo, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned char color);
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);
void VGA256ScaleImage025x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights);
void VGA256ScaleImage05x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights);
void VGA256ScaleImage2x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights);
void VGA256ScaleImage4x(unsigned char* pDest, unsigned char* pSource, unsigned int widths, unsigned int heights);
void VGA256RotateImage(unsigned char* pDest, unsigned char* pSource, unsigned int width, unsigned int height, int angle);
void VGA256FloodFill(void* pVideo, unsigned int x, unsigned int y, unsigned int new_col, unsigned int old_col);
void VGA256OutText(void* pVideo, char* string, unsigned int x, unsigned int y, unsigned int color, unsigned char* font);
void VGA256OutText2x(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char* font);
void VGA256OutText4x(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char* font);
void VGA256OutText2xOK(void* pVideo, char* string, unsigned int x_cursor, unsigned int y_cursor, unsigned int color, unsigned char* font);
int VGA256LoadPCX(char* filename, unsigned char* dest, unsigned char* pal);
int VGA256DecodePCX(unsigned char* dest, unsigned char* buffer);
int VGA256LoadBMP(char* filename, unsigned char* dest, unsigned char* pal);
int VGA256KbHit(void);
int VGA256GetCh(void);



/*------------------------------------------------------------------------------------------------------- */
//void VGA256SetPalette(const void *pal);
#pragma aux VGA256SetPalette = \
"cld" \
"mov ecx, 256 * 3" \
"xor eax, eax" \
"mov dx, 3c8h" \
"out dx, al" \
"inc dx" \
"rep outsb" \
parm [esi] \
modify exact [esi edx ecx eax];


/*------------------------------------------------------------------------------------------------------- */
//void VGA256GetPalette(void* pal);
#pragma aux VGA256GetPalette = \
"cld" \
"mov ecx, 256 * 3" \
"xor eax, eax" \
"mov dx, 3c7h" \
"out dx, al" \
"add dx, 2" \
"rep insb" \
parm [edi] \
modify exact [edi edx ecx eax];


/*------------------------------------------------------------------------------------------------------- */
void _VGA256MemCpy0(void *pDest, void *pSource, size_t iLen);
#pragma aux _VGA256MemCpy0 = \
"mov edx, ecx" \
"shr ecx, 2" \
"jz v0_tail" \
"v0_lp:" \
"mov eax, [esi]" \
"add esi, 4" \
"test eax, eax" \
"jz v0_adv" \
"test al, al" \
"jz v0_1" \
"mov [edi], al" \
"v0_1:" \
"test ah, ah" \
"jz v0_2" \
"mov [edi+1], ah" \
"v0_2:" \
"shr eax, 16" \
"test al, al" \
"jz v0_3" \
"mov [edi+2], al" \
"v0_3:" \
"test ah, ah" \
"jz v0_adv" \
"mov [edi+3], ah" \
"v0_adv:" \
"add edi, 4" \
"dec ecx" \
"jnz v0_lp" \
"v0_tail:" \
"and edx, 3" \
"jz v0_done" \
"v0_b:" \
"mov al, [esi]" \
"inc esi" \
"test al, al" \
"jz v0_nb" \
"mov [edi], al" \
"v0_nb:" \
"inc edi" \
"dec edx" \
"jnz v0_b" \
"v0_done:" \
parm [edi] [esi] [ecx] \
modify exact [eax ecx edx esi edi];

/*------------------------------------------------------------------------------------------------------- */
void VGA256MemCpyMMX(char* Dest, char* Src, unsigned int Len);
#pragma aux VGA256MemCpyMMX = \
"cld" \
"mov eax, ecx" \
"shr ecx, 4" \
"jz mm_tail" \
"mm_lp:" \
"movq mm0, [esi]" \
"movq mm1, [esi+8]" \
"movq [edi], mm0" \
"movq [edi+8], mm1" \
"add esi, 16" \
"add edi, 16" \
"dec ecx" \
"jnz mm_lp" \
"mm_tail:" \
"emms" \
"mov ecx, eax" \
"and ecx, 15" \
"rep movsb" \
parm [edi] [esi] [ecx] \
modify exact [eax ecx esi edi];

/*------------------------------------------------------------------------------------------------------- */
#pragma aux VGA256ScaleImage = \
"cmp eax, ecx" \
"jne sc_gen" \
"cmp ebx, edx" \
"jne sc_gen" \
"mul ebx" \
"mov ecx, eax" \
"cld" \
"shr ecx, 2" \
"rep movsd" \
"mov ecx, eax" \
"and ecx, 3" \
"rep movsb" \
"jmp sc_ret" \
"sc_gen: push ebp" \
"mov ebp, esp" \
"sub esp, 20" \
"mov [ebp-4], eax" \
"mov [ebp-8], ecx" \
"mov [ebp-12], esi" \
"mov [ebp-20], edx" \
"mov eax, ecx" \
"mov edx, eax" \
"shl eax, 16" \
"shr edx, 16" \
"div dword ptr [ebp-4]" \
"mov [ebp-16], eax" \
"mov eax, [ebp-20]" \
"mov edx, eax" \
"shl eax, 16" \
"shr edx, 16" \
"div ebx" \
"mov [ebp-20], eax" \
"xor eax, eax" \
"sc_y: push eax" \
"push ebx" \
"shr eax, 16" \
"mul dword ptr [ebp-8]" \
"mov esi, [ebp-12]" \
"add esi, eax" \
"xor ebx, ebx" \
"mov ecx, [ebp-4]" \
"mov edx, [ebp-16]" \
"sc_x: mov eax, ebx" \
"shr eax, 16" \
"mov al, [esi+eax]" \
"mov [edi], al" \
"inc edi" \
"add ebx, edx" \
"dec ecx" \
"jnz sc_x" \
"pop ebx" \
"pop eax" \
"add eax, [ebp-20]" \
"dec ebx" \
"jnz sc_y" \
"mov esp, ebp" \
"pop ebp" \
"sc_ret: nop" \
parm [edi] [esi] [eax] [ebx] [ecx] [edx] \
modify exact [eax ebx ecx edx esi edi ebp];
#ifdef __cplusplus
}
#endif
#endif
