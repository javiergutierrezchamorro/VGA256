#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vga256.h"


unsigned char *VGA256_Video;

void DemoPorsche(void);
void DemoDraw(void);
void DemoEnd(void);


/*------------------------------------------------------------------------------------------------------- */
int main( int argc, char *argv[] )
{
	short iMode;


	if (!VBE_Init())
	{
		printf ("VBE not available\n");
		return 1;
	}

	iMode = VBE_FindMode(VGA256_WIDTH, VGA256_HEIGHT, 8);
	if (iMode == -1)
	{
		printf ("Mode not found\n");
		VBE_Done();
		return 1;
	}

	if (!VBE_IsModeLinear(iMode))
	{
		printf ("Mode has no Linear Frame Buffer\n");
		VBE_Done();
		return 1;
	}

	VGA256FadeOut();

	VBE_SetMode(iMode, 1, 1);
	VGA256_Video = (unsigned char *) VBE_GetVideoPtr(iMode);
	if (VGA256_Video == NULL)
	{
		VBE_SetMode (3, 0, 1);
		printf ("Cannot map the video memory\n");
		VBE_Done();
		return 1;
	}


	//Todo: Crop
	//Rotate 180 / FlipV
	//FlipH

	DemoDraw();
	VGA256GetCh();

	DemoPorsche();
	VGA256GetCh();

	DemoEnd();

	VGA256FadeOut();

	VGA256GetCh();


	VBE_SetMode (3, 0, 1);
	VBE_Done();
	return 0;
}


/*------------------------------------------------------------------------------------------------------- */
void DemoDraw(void)
{
	unsigned int i;

	VGA256DrawBox(VGA256_Video, 10, 10, VGA256_WIDTH - 20, VGA256_HEIGHT - 20, 200);

	VGA256FillBox(VGA256_Video, 20, 20, VGA256_WIDTH - 40, VGA256_HEIGHT - 40, 240);

	VGA256Circle(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 200, 40);

	VGA256FloodFill(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 10, 240);

	for (i = 0; i < VGA256_HEIGHT; i += 32)
	{
		VGA256OutText4x(VGA256_Video, "VGA256", 150, i, 40, VGA256Font);
	}
	VGA256GetCh();

	for (i = 0; i < VGA256_HEIGHT; i += 16)
	{
		VGA256OutText2x(VGA256_Video, "VGA256 Watcom/OpenWatcom Library", 60, i, 40, VGA256Font);
	}

	for (i = 0; i < VGA256_HEIGHT; i += 8)
	{
		VGA256OutText(VGA256_Video, "VGA256 Watcom/OpenWatcom Library", 160, i, 50, VGA256Font);
	}

	VGA256LineH(VGA256_Video, 0, VGA256_HEIGHT / 2, VGA256_WIDTH, 50);

	VGA256LineV(VGA256_Video, 320, 0, VGA256_HEIGHT, 100);

	VGA256Line(VGA256_Video, 0, 0, VGA256_WIDTH - 1, VGA256_HEIGHT - 1, 40);

	VGA256PutPixel(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 50);

}


/*------------------------------------------------------------------------------------------------------- */
void DemoPorsche(void)
{
	unsigned int i, iRows, iLogoRows;
	int iSize, iSizeLogo;
	unsigned char *porsche, *logo, *pal;
	unsigned char* porsche160x120, *rotated;

	iSize = VGA256LoadPCX("911.pcx", NULL, NULL);
	iSizeLogo = VGA256LoadPCX("logo.pcx", NULL, NULL);
	if (iSize < 640 * 880 || iSizeLogo < 250)
	{
		return;
	}
	iRows = iSize / 640;
	iLogoRows = iSizeLogo / 250;

	porsche = malloc(iSize);
	logo = malloc(iSizeLogo);
	pal = malloc(768);
	if (porsche == NULL || logo == NULL || pal == NULL || VGA256LoadPCX("911.pcx", porsche, pal) < 0 || VGA256LoadPCX("logo.pcx", logo, NULL) < 0)
	{
		free(pal);
		free(logo);
		free(porsche);
		return;
	}
	VGA256FadeOut();
	VGA256ClearScreen(VGA256_Video, 0);
	VGA256FadeIn(pal);
	free(pal);


	rotated = malloc(160 * 120);
	porsche160x120 = malloc(160 * 120);
	if (rotated != NULL && porsche160x120 != NULL)
	{
		//VGA256ScaleImage(porsche160x120, &porsche[640*400], 160, 120, 640, 480);
		VGA256ScaleImage025x(porsche160x120, &porsche[640 * 400], 640, 480);

		for (i = 0; i < 360; i++)
		{
			VGA256RotateImage(rotated, porsche160x120, 160, 120, i);
			VGA256ScaleImage4x(VGA256_Video, rotated, 160, 120);
		}
	}
	free(rotated);
	free(porsche160x120);

	for (i = 0; i < iRows - 480; i++)
	{
		VGA256PutScreen(VGA256_Video, porsche + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, logo, 0, 0, 250, iLogoRows);
		VGA256WaitVRetrace();
	}

	for (i = iRows - 480; i > 0; i--)
	{
		VGA256PutScreen(VGA256_Video, porsche + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, logo, 0, 0, 250, iLogoRows);
		VGA256WaitVRetrace();
	}
	free(logo);
	free(porsche);
}



/*------------------------------------------------------------------------------------------------------- */
void DemoEnd(void)
{
	unsigned int i, j, k, l;
	unsigned char *background, *background160x120, *rotated, *scaled, *pal;

	background = malloc(640 * 480);
	background160x120 = malloc(160 * 120);
	rotated = malloc(160 * 120);
	scaled = malloc(640 * 480);
	pal = malloc(768);

	if (background != NULL && background160x120 != NULL && rotated != NULL && scaled != NULL && pal != NULL)
	{
		VGA256GetImage(VGA256_Video, background, 0, 0, 640, 480);
		VGA256ScaleImage025x(background160x120, background, 640, 480);
		VGA256GetPalette(pal);

		j = 640;
		k = 480;
		for (i = 0; i < 360; i++)
		{
			VGA256RotateImage(rotated, background160x120, 160, 120, i);
			VGA256ScaleImage(scaled, rotated, j, k, 160, 120);
			VGA256WaitVRetrace();
			if ((i & 31) == 1)
			{
				VGA256ClearScreen(VGA256_Video, 0);
			}
			VGA256PutImage(VGA256_Video, scaled, (640 - j) >> 1, (480 - k) >> 1, j, k);
			if ((i & 31) == 0)
			{
				j -= 4;
				k -= 3;
			}
			else if ((i & 3) == 0)
			{
				for (l = 0; l < 768; l++)
				{
					if (pal[l] > 0)
					{
						pal[l]--;
					}
				}
				VGA256SetPalette(pal);
			}
		}
	}
	VGA256ClearScreen(VGA256_Video, 0);
	free(pal);
	free(scaled);
	free(rotated);
	free(background160x120);
	free(background);
}




/*------------------------------------------------------------------------------------------------------- */
void Demo(void)
{
	unsigned int i;
	unsigned char* b, * c;
	clock_t start_time, end_time;

	c = malloc(320 * 200);
	VGA256LoadPCX("perin.pcx", c, NULL);
	VGA256PutImage(VGA256_Video, c, 10, 10, 320, 200);
	VGA256PutSprite(VGA256_Video, c, 300, 200, 320, 200);
	VGA256GetCh();

	VGA256ClearScreen(VGA256_Video, 0);
	b = malloc(VGA256_WIDTH * VGA256_HEIGHT);


	start_time = clock();
	VGA256ScaleImage(b, c, 640, 480, 320, 200);
	VGA256PutImage(VGA256_Video, b, 0, 0, 640, 480);
	end_time = clock();
	VGA256GetCh();

	VGA256ScaleImage2x(b, c, 320, 200);
	VGA256PutImage(VGA256_Video, b, 0, 0, 640, 400);
	VGA256ScaleImage05x(b, c, 320, 200);
	VGA256PutImage(VGA256_Video, b, 0, 0, 160, 100);
	free(b);
	VGA256GetCh();

	b = malloc(VGA256_WIDTH * VGA256_HEIGHT);
	for (i = 0; i < 100; i++)
	{
		VGA256GetImage(VGA256_Video, b, i, 0, 320, 200);
		VGA256PutSprite(VGA256_Video, c, i, 0, 320, 200);
		VGA256WaitVRetrace();
		VGA256PutImage(VGA256_Video, b, i, 0, 320, 200);
	}
	free(b);
	free(c);
	VGA256GetCh();

	//printf("Execution time was %ld clocks\n", (end_time - start_time));
}
