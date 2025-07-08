#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <io.h>



void VGA256ScaleImageKO2(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
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

void VGA256ScaleImage(unsigned char* pDest, unsigned char* pSource, unsigned int widthd, unsigned int heightd, unsigned int widths, unsigned int heights)
{
    unsigned int h, w;
    unsigned int height_accum = 0;
    unsigned char *dest_line = pDest;
    unsigned char *src_line;
    unsigned int width_accum;
    unsigned char* dst;
    unsigned char pixel;

    for (h = 0; h < heights; h++)
    {
        // Puntero a la línea de origen
        src_line = pSource + h * widths;

        // Escalado horizontal de una línea
        width_accum = 0;
        dst = dest_line;

        for (w = 0; w < widths; w++)
        {
            pixel = src_line[w];
            width_accum += widthd;

            // Escribir pixel tantas veces como sea necesario
            while (width_accum >= widths)
            {
                *dst++ = pixel;
                width_accum -= widths;
            }
        }

        // Calcular cuántas veces duplicar esta línea en vertical
        height_accum += heightd;
        while (height_accum >= heights)
        {
            dest_line += widthd;
            memcpy(dest_line, dest_line - widthd, widthd);
            height_accum -= heights;
        }
    }
}
    
    
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
	unsigned char m[2][5];
    unsigned char M[8][20];


    VGA256ScaleImage(m, image, 5, 2, 10, 4);
    //VGA256ScaleImage(M, image, 20, 8, 10, 4);
    ;
}
