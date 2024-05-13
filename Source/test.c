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
    VGA256ScaleImage((unsigned char *) b, (unsigned char *) image, 5, 2, 10, 4);

    
}



/*------------------------------------------------------------------------------------------------------- */
void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned char pixel;
    unsigned int runw, runh;
    unsigned int carryw, carryh;
    div_t widthr, heightr;

    heightr = div(heightd, heights);
    widthr = div(widthd, widths); 

    runh = 0;
    carryh = 0;
    for (h = 0; h < heights; h++)
    {
        runh += heightr.quot;
        carryh += heightr.rem;
        if (carryh >= heights)
        {
            carryh = 0;
            runh++;
        }
        if (runh > 0)
        {
            runw = 0;
            carryw = 0;
            for (w = 0; w < widths; w++)
            {
                runw += widthr.quot;
                carryw += widthr.rem;
                if (carryw >= widths)
                {
                    carryw = 0;
                    runw++;
                }
                pixel = *pSource;
                pSource++;
                while (runw > 0)
                {
                    *pDest = pixel;
                    pDest++;
                    runw--;
                }
            }
            runh--;
            while (runh > 0)
            {
                memcpy(pDest, pDest - widthd, widthd);
                pDest += widthd;
                runh--;
            }
        }
        else
        {
            pSource += widths;
        }
    }
}
