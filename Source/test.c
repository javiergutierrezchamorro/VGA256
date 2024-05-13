#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <io.h>

void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);

/*------------------------------------------------------------------------------------------------------- */
void main(int argc, char* argv[])
{
    unsigned char image[4][10] =
    {
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' },
        { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J' },
        { 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T' },
        { 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd' }
    };
    unsigned char b[2][5];
    VGA256ScaleImage05x((unsigned char *) b, (unsigned char *) image, 10, 4);

    
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int widthrun, widthtarget;
    unsigned int heightrun, heighttarget;
    unsigned int h, w;
    unsigned char pixel;


    heightrun = heights;
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
        while (heightrun < heighttarget)
        {
            memcpy(pDest, pDest - widthd, widthd);
            pDest += widthd;
            heightrun += heights;
        }
        heighttarget += heightd;

    }
}

