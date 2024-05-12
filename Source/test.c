#include <stdio.h>
#include <stdlib.h>

void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);


/*------------------------------------------------------------------------------------------------------- */
void main(int argc, char* argv[])
{
    unsigned char* b;
    unsigned char s[] = { 1, 1, 2, 2, 3, 3 };

    b = malloc(1000);
    VGA256ScaleImage(b, s, 12, 1, 6, 1);
    free(b);
}




/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned int widthrun, widthtarget;
    unsigned int heightrun, heighttarget;
    unsigned char pixel;


    heightrun = 0;
    heighttarget = 0;
    for (h = 0; h < heights; h++)
    {
        widthrun = 0;
        widthtarget = 0;
        for (w = 0; w < widths; w++)
        {
            pixel = *pSource;
            widthtarget += widthd;
            while (widthrun < widthtarget)
            {
                *pDest = pixel;
                pDest++;
                widthrun += widths;
            }
            pSource++;
        }
        widthtarget += widthd;

        heighttarget += heightd;
        while (heightrun < heighttarget)
        {
            memcpy(pDest, pDest - widthd, widthd);
            pDest += widthd;
            heightrun += heights;
        }
        pSource += widths;
    }
}
