#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);


/*------------------------------------------------------------------------------------------------------- */
void main(int argc, char* argv[])
{
    unsigned char* b;
    unsigned char s[] = { 1, 1, 1, 2, 2, 2, 3, 3, 3 };
    int aiSin[900];

    #define M_PI          3.141592653589793238462643383279502884L
    for (unsigned int i = 0; i < 900; i++)
    {
        aiSin[i] = (int) sin(((double) i / 10) * (M_PI / 180));
    }


    b = malloc(10000);
    //VGA256RotateImage(b, s, 320, 200, 10);
    free(b);
}




/*------------------------------------------------------------------------------------------------------- */
void VGA256RotateImage(unsigned char* pDest, unsigned char* pSource, unsigned int width, unsigned int height, int angle)
{
    unsigned int x, y;
    unsigned int nx, ny;

    for (y = 0; y < height; y++)
    {
        for (x = 0; y < width; x++)
        {
            nx = x * cos(angle) - y * sin(angle);
            ny = x * sin(angle) + y * cos(angle);
        }
    }
}
