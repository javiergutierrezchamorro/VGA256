#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "VGA256.h"


unsigned char *VGA256_Video;


/*------------------------------------------------------------------------------------------------------- */
void main( int argc, char *argv[] )
{
	short iMode;
	unsigned int i, j, k;
	unsigned char *b, *c, *p;
	clock_t start_time, end_time;


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
	VBE_SetMode (iMode, 1, 1);
	VGA256_Video = VBE_GetVideoPtr(iMode);


	b = malloc(640 * 1071);
	c = malloc(250 * 151);
	p = malloc(768);
	VGA256LoadPCX("911.pcx", b, p);
	VGA256LoadPCX("logo.pcx", c, NULL);
	VGA256SetPalette(p);
	/*
	for (i = 0; i < 1071 - 480; i++)
	{
		VGA256PutScreen(VGA256_Video, b + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, c, 0, 0, 250, 151);
		VGA256WaitVRetrace();
	}
	for (i = 1071 - 480; i > 0; i--)
	{
		VGA256PutScreen(VGA256_Video, b + (i * 640));
		//VGA256MemCpyMMX(VGA256_Video, b + (i * 640), VGA256_WIDTH * VGA256_HEIGHT);
		VGA256PutSprite(VGA256_Video, c, 0, 0, 250, 151);
		VGA256WaitVRetrace();
	}
	*/
	free(c);
	c = malloc(287 * 480);
	VGA256ScaleImage(c, b, 287, 480, 640, 1171);
	VGA256PutImage(VGA256_Video, c, 287, 480, 640, 1171);


	free(p);
	free(b);
	VGA256GetCh();

	VGA256OutText(VGA256_Video, "Test de prueba en VESA", 20, 20, 10);
	VGA256PutPixel(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 50);
	
	VGA256LineH(VGA256_Video, 0, VGA256_HEIGHT/2, VGA256_WIDTH, 50);
	VGA256LineV(VGA256_Video, 320, 0, VGA256_WIDTH, 100);
	VGA256DrawBox(VGA256_Video, 10, 10, VGA256_WIDTH - 20, VGA256_HEIGHT - 20, 200);
	VGA256FillBox(VGA256_Video, 20, 20, VGA256_WIDTH - 40, VGA256_HEIGHT - 40, 240);
	VGA256Circle(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 200, 40);
	VGA256FloodFill(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 10, 40);
	VGA256Line(VGA256_Video, 0, 0, VGA256_WIDTH, VGA256_HEIGHT, 40);
	VGA256GetCh();


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

	VGA256FadeOut();
	
	VGA256ClearScreen(VGA256_Video, 50);
	VGA256GetCh();
	
	

	VBE_SetMode (3, 0, 1);
	VBE_Done();

	printf("Execution time was %ld clocks\n", (end_time - start_time));

}
