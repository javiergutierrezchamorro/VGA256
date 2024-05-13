#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <io.h>


/*------------------------------------------------------------------------------------------------------- */
void main(int argc, char* argv[])
{
    unsigned char image[256000];
    unsigned char pal[768];
    int r;

    r = VGA256LoadPCX("car.pcx", image, pal);
}


/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)
struct pcx_header
{
    char manufacturer;
    char version;
    char encoding;
    char bits_per_pixel;
    short int  xmin, ymin;
    short int  xmax, ymax;
    short int  hres;
    short int  vres;
    char palette16[48];
    char reserved;
    char color_planes;
    short int  bytes_per_line;
    short int  palette_type;
    char filler[58];
};
#pragma pack (pop)

/*------------------------------------------------------------------------------------------------------- */
int VGA256LoadPCX(char* filename, unsigned char* dest, unsigned char* pal)
{
#define VGA256LoadPCXBufLen (16384)
    unsigned int i = 0;
    unsigned int bufptr = 0;
    unsigned int mode = 0;    /* BYTEMODE */
    unsigned int readlen = 0;
    int infile = -1;
    unsigned char outbyte = 0, bytecount = 0;
    unsigned char* buffer = NULL;
    struct pcx_header pcx_header;
    int res = -9;


    if ((infile = _open(filename, O_BINARY)) == -1)
    {
        res = -1;   /* Cannot open */
    }
    else
    {
        readlen = _read(infile, &pcx_header, sizeof(pcx_header));
        if ((pcx_header.manufacturer == 10) && (pcx_header.encoding == 1) && (pcx_header.bits_per_pixel == 8))
        {
            if (dest)
            {
                buffer = malloc(VGA256LoadPCXBufLen);
                if (buffer)
                {
                    readlen = 0;
                    for (i = 0; i < VGA256LoadPCXBufLen; i++)
                    {
                        if (mode == 0)  /* BYTEMODE */
                        {
                            if (bufptr >= readlen)
                            {
                                bufptr = 0;
                                if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) == 0)
                                {
                                    break;
                                }
                            }
                            outbyte = buffer[bufptr++];
                            if (outbyte > 0xbf)
                            {
                                bytecount = (int)((int)outbyte & 0x3f);
                                printf("bytecount: %d\n", bytecount);
                                if (bufptr >= readlen)
                                {
                                    bufptr = 0;
                                    if ((readlen = _read(infile, buffer, VGA256LoadPCXBufLen)) == 0)
                                    {
                                        break;
                                    }
                                }
                                outbyte = buffer[bufptr++];
                                if (--bytecount > 0)
                                {
                                    mode = 1;   /* RUNMODE */
                                }
                            }
                        }
                        else if (--bytecount == 0)
                        {
                            mode = 0;   /* BYTEMODE */
                        }
                        i++;
                        printf("%u, %p\n", i, dest);
                        *dest++ = outbyte;
                    }
                    free(buffer);
                    res = (unsigned int)((pcx_header.xmax - pcx_header.xmin + 1) * (pcx_header.ymax - pcx_header.ymin + 1) * pcx_header.bits_per_pixel >> 3);
                    printf("res: %d\n", res);
                }
                else
                {
                    res = -3; /* Cannot allocate */
                }
            }
            if (pal)
            {
                _lseek(infile, -768L, SEEK_END);
                readlen = _read(infile, pal, 3 * 256);
                for (i = 0; i < readlen; i++)
                {
                    pal[i] = pal[i] >> 2;
                }
            }
        }
        else
        {
            res = -2;   /* Invalid PCX */
        }
    }
    if (infile != -1)
    {
        _close(infile);
    }
    return(res);
}


