#include <stdio.h>
#include <stdlib.h>

void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights);


/*------------------------------------------------------------------------------------------------------- */
void main(int argc, char* argv[])
{
    unsigned char* b;
    unsigned char s[] = { 1, 1, 1, 2, 2, 2, 3, 3, 3 };

    b = malloc(1000);
    VGA256ScaleImage(b, s, 6, 3, 3, 3);
    free(b);
}




/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned int widthrun, widthtarget;
    unsigned int heightrun, heighttarget;


    heightrun = 0;
    heighttarget = 0;
    for (h = 0; h < heights; h++)
    {
        widthrun = 0;
        widthtarget = 0;
        for (w = 0; w < widths; w++)
        {
            widthtarget += widthd;
            while (widthrun < widthtarget)
            {
                *pDest = *pSource;
                pDest++;
                widthrun += widths;
            }
            pSource++;
        }
        
        heighttarget += heightd;
        while (heightrun < heighttarget)
        {
            memcpy(pDest, pDest - widthd, widthd);
            pDest += widthd;
            heightrun += heights;
        }
    }
}
