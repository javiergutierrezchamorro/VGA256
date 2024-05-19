#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <io.h>



/*------------------------------------------------------------------------------------------------------- */
#pragma pack (push, 1)
struct bmp_header
{
	unsigned short type;             // Magic identifier: 0x4d42
	unsigned long size;             // File size in bytes
	unsigned short reserved1;        // Not used
	unsigned short reserved2;        // Not used
	unsigned long offset;           // Offset to image data in bytes from beginning of file (54 bytes)
	unsigned long dib_header_size;  // DIB Header size in bytes (40 bytes)
	long width_px;         // Width of the image
	long height_px;        // Height of image
	unsigned short  num_planes;       // Number of color planes
	unsigned short  bits_per_pixel;   // Bits per pixel
	unsigned long  compression;      // Compression type
	unsigned long  image_size_bytes; // Image size in bytes
	long x_resolution_ppm; // Pixels per meter
	long y_resolution_ppm; // Pixels per meter
	unsigned long  num_colors;       // Number of colors  
	unsigned long  important_colors; // Important colors 

};
#pragma pack (pop)


/*------------------------------------------------------------------------------------------------------- */
int VGA256LoadBMP(char* filename, unsigned char* dest, unsigned char* pal)
{
	int readlen = 0;
	int infile = -1;
	struct bmp_header bmp_header;
	int res = -9;


	if ((infile = _open(filename, O_BINARY | O_RDONLY)) == -1)
	{
		res = -1;   /* Cannot open */
	}
	else
	{
		readlen = _read(infile, &bmp_header, sizeof(bmp_header));
		if ((bmp_header.type == 0x4d42) && (bmp_header.bits_per_pixel == 8) && (bmp_header.num_planes == 1) && (bmp_header.num_colors <= 256) && (bmp_header.compression == 0))
		{
			if (pal)
			{
				readlen = _read(infile, pal, 3 * 256);
				/*
				for (i = 0; i < readlen; i++)
				{
					pal[i] = pal[i] >> 2;
				}
				*/
			}
			if (dest)
			{
				_lseek(infile, bmp_header.offset, SEEK_SET);
				readlen = _read(infile, dest, bmp_header.image_size_bytes);
			}
			res = bmp_header.image_size_bytes;
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

	unsigned char data[64000];
	unsigned char pal[768];
	VGA256LoadBMP("perin.bmp", data, pal);

}
