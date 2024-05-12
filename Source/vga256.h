#pragma once
#ifndef _VGA256_H
#define _VGA256_H

#pragma pack (1)

#include <i86.h>
#include <string.h>

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

#pragma pack (1)

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
#define VGA256_MODE_640X480X8
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

void VGA256PutPixel(void *pVideo, unsigned int x, unsigned int y, unsigned int color);
unsigned int VGA256GetPixel(void *pVideo, unsigned int x, unsigned int y);
void VGA256LineH(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int color);
void VGA256LineV(void *pVideo, unsigned int x, unsigned int y, unsigned int height, unsigned int color);
void VGA256DrawBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void VGA256FillBox(void *pVideo, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void VGA256ClearScreen(void *pVideo, unsigned int color);
void VGA256PutSprite(void *pcDest, void *pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);
void VGA256PutImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);
void VGA256GetImage(void* pVideo, void* pcSource, unsigned int piX, unsigned int piY, unsigned int piWidth, unsigned int piHeight);


void VGA256WaitVRetrace(void);
void VGA256SetPalette(const void* pal);
void VGA256GetPalette(void* pal);
void VGA256FadeOut(void);
void VGA256FadeIn(char* paleta);
void VGA256FadeTo(char* paleta);
void VGA256Circle(void* pVideo, unsigned x, unsigned int y, unsigned int radio, unsigned int color);
void VGA256Line(void* pVideo, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned char color);
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);


#define _VGA256Sgn(a) (a > 0 ? 1 : (a < 0 ? -1 : 0))
void _VGA256MemCpy0(void *pDest, void *pSource, size_t iLen);

#ifdef __cplusplus
}
#endif
#endif
