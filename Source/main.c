#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "VGA256.h"


unsigned char *VGA256_Video;


/*------------------------------------------------------------------------------------------------------- */
void main( int argc, char *argv[] )
{
	short iMode;
	unsigned int i;
	unsigned char *b;

	
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
	
	VGA256PutPixel(VGA256_Video, VGA256_WIDTH/2, VGA256_HEIGHT/2, 50);
	getch();
	
	VGA256LineH(VGA256_Video, 0, VGA256_HEIGHT/2, VGA256_WIDTH, 50);
	VGA256LineV(VGA256_Video, 320, 0, VGA256_WIDTH, 100);
	VGA256DrawBox(VGA256_Video, 10, 10, VGA256_WIDTH - 20, VGA256_HEIGHT - 20, 200);
	VGA256FillBox(VGA256_Video, 20, 20, VGA256_WIDTH - 40, VGA256_HEIGHT - 40, 240);
	VGA256Circle(VGA256_Video, VGA256_WIDTH / 2, VGA256_HEIGHT / 2, 200, 40);
	VGA256Line(VGA256_Video, 0, 0, VGA256_WIDTH, VGA256_HEIGHT, 40);
	getch();
	
	VGA256PutImage(VGA256_Video, gacPerin, 10, 10, 320, 200);
	VGA256PutSprite(VGA256_Video, gacPerin, 300, 200, 320, 200);
	getch();

	VGA256ClearScreen(VGA256_Video, 0);
	b = malloc(VGA256_WIDTH * VGA256_HEIGHT * 4);
	for (i = 0; i < 240; i++)
	{
		VGA256ScaleImage(b, gacPerin, 320+i, 200+i, 320, 200);
		VGA256PutImage(VGA256_Video, b, 0, 0, 320+i, 200+i);
	}
	for (i = 0; i < 400; i++)
	{
		VGA256ScaleImage(b, gacPerin, 640 - i, 480 - i, 320, 200);
		VGA256PutImage(VGA256_Video, b, 0, 0, 640 - i, 480 - i);
	}
	free(b);
	getch();

	b = malloc(VGA256_WIDTH * VGA256_HEIGHT);
	for (i = 0; i < 100; i++)
	{
		VGA256GetImage(VGA256_Video, b, i, 0, 320, 200);
		VGA256PutSprite(VGA256_Video, gacPerin, i, 0, 320, 200);
		VGA256WaitVRetrace();
		VGA256PutImage(VGA256_Video, b, i, 0, 320, 200);
	}
	free(b);
	getch();

	VGA256FadeOut();
	
	VGA256ClearScreen(VGA256_Video, 50);
	getch();
	
	

	VBE_SetMode (3, 0, 1);
	VBE_Done();
}
