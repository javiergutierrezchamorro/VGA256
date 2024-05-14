#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "VGA256.h"


unsigned char *VGA256_Video;

void DemoPorsche(void);
void DemoDraw(void);



/*------------------------------------------------------------------------------------------------------- */
void main( int argc, char *argv[] )
{
	short iMode;
	unsigned int i, j, k;
	unsigned char* logo;
	unsigned char* rotated;


	VBE_Init();
	iMode = VBE_FindMode(VGA256_WIDTH, VGA256_HEIGHT, 8);
	if (iMode == -1)
	{
		printf ("Mode not found\n");
		return;
	}

	if (!VBE_IsModeLinear(iMode))
	{
		printf ("Mode has no Linear Frame Buffer\n");
		return;
	}

	VGA256FadeOut();

	VBE_SetMode(iMode, 1, 1);
	VGA256_Video = VBE_GetVideoPtr(iMode);
	
	DemoPorsche();
	VGA256GetCh();
	
	//DemoDraw();
	//VGA256GetCh();

	/*
	logo = malloc(250 * 151);
	rotated = malloc(640 * 480);
	VGA256LoadPCX("logo.pcx", logo, NULL);
	for (i = 0; i < 360; i++)
	{
		VGA256RotateImage(rotated, logo, 250, 151, i);
		VGA256PutImage(VGA256_Video, rotated, 300, 200, 250, 151);
	}

	j = 25;
	k = 15;
	for (i = 0; i < 150; i++)
	{
		VGA256ScaleImage(rotated, logo, j, k, 250, 151);
		VGA256PutImage(VGA256_Video, rotated, 0, 0, j, k);
		j += 5;
		k += 3;
	}
	*/

	free(logo);
	VGA256GetCh();


	for (i = 0; i < VGA256_HEIGHT; i += 16)
	{
		VGA256OutText2x(VGA256_Video, "VGA256 Watcom/OpenWatcom Library", 60, i, 40, (unsigned char*)VGA256Font);
	}

	for (i = 0; i < VGA256_HEIGHT; i += 8)
	{
		VGA256OutText(VGA256_Video, "VGA256 Watcom/OpenWatcom Library", 160, i, 50, (unsigned char*)VGA256Font);
	}

	

	VGA256GetCh();

	//Test rotate

	VGA256FadeOut();
	
	VGA256ClearScreen(VGA256_Video, 50);
	VGA256GetCh();
	
	

	VBE_SetMode (3, 0, 1);
	VBE_Done();
}


/*------------------------------------------------------------------------------------------------------- */
void DemoDraw(void)
{
	VGA256OutText(VGA256_Video, "Test de prueba en VESA", 20, 20, 10, (unsigned char *) VGA256Font);

	VGA256PutPixel(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 50);

	VGA256LineH(VGA256_Video, 0, VGA256_HEIGHT / 2, VGA256_WIDTH, 50);

	VGA256LineV(VGA256_Video, 320, 0, VGA256_WIDTH, 100);

	VGA256DrawBox(VGA256_Video, 10, 10, VGA256_WIDTH - 20, VGA256_HEIGHT - 20, 200);

	VGA256FillBox(VGA256_Video, 20, 20, VGA256_WIDTH - 40, VGA256_HEIGHT - 40, 240);

	VGA256Circle(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 200, 40);

	VGA256FloodFill(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 10, 40);

	VGA256Line(VGA256_Video, 0, 0, VGA256_WIDTH, VGA256_HEIGHT, 40);
}


/*------------------------------------------------------------------------------------------------------- */
void DemoPorsche(void)
{
	unsigned int i, j, k;
	unsigned char *porsche, *logo, *pal;
	unsigned char* porsche320x240, *porsche640x480;

	porsche = malloc(640 * 1071);
	logo = malloc(250 * 151);
	pal = malloc(768);
	VGA256LoadPCX("911.pcx", porsche, pal);
	VGA256LoadPCX("logo.pcx", logo, NULL);
	VGA256FadeIn(pal);
	free(pal);


	porsche640x480 = malloc(640 * 480);
	porsche320x240 = malloc(320 * 240);
	//VGA256ScaleImage(porsche320x240, &porsche[640*400], 160, 120, 640, 480);
	VGA256ScaleImage025x(porsche320x240, &porsche[640 * 400], 640, 480);

	for (i = 0; i < 360; i++)
	{
		VGA256RotateImage(porsche640x480, porsche320x240, 160, 120, i);
		VGA256ScaleImage4x(VGA256_Video, porsche640x480, 160, 120);
	}
	free(porsche320x240);
	free(porsche640x480);
	
	for (i = 0; i < 1071 - 480; i++)
	{
		VGA256PutScreen(VGA256_Video, porsche + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, logo, 0, 0, 250, 151);
		VGA256WaitVRetrace();
	}

	for (i = 1071 - 480; i > 0; i--)
	{
		VGA256PutScreen(VGA256_Video, porsche + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, logo, 0, 0, 250, 151);
		VGA256WaitVRetrace();
	}
	free(logo);
	
}


/*------------------------------------------------------------------------------------------------------- */
void Demo(void)
{
	unsigned int i, j, k;
	unsigned char* b, * c, * p;
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
