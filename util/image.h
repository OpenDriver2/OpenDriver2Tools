#ifndef IMAGE_H
#define IMAGE_H

#include "math/dktypes.h"

// Define targa header.
#pragma pack( push, 1 )
typedef struct
{
	int8	identsize;              // Size of ID field that follows header (0)
	int8	colorMapType;           // 0 = None, 1 = paletted
	int8	imageType;              // 0 = none, 1 = indexed, 2 = rgb, 3 = grey, +8=rle
	unsigned short	colorMapStart;          // First colour map entry
	unsigned short	colorMapLength;         // Number of colors
	unsigned char 	colorMapBits;   // bits per palette entry
	unsigned short	xstart;                 // image x origin
	unsigned short	ystart;                 // image y origin
	unsigned short	width;                  // width in pixels
	unsigned short	height;                 // height in pixels
	int8	bits;                   // bits per pixel (8 16, 24, 32)
	int8	descriptor;             // image descriptor
} TGAHEADER;
#pragma pack( pop )

struct TIMHDR
{
	int magic;
	int flags;
};

struct TIMIMAGEHDR
{
	int len;
	short origin_x;
	short origin_y;
	short width;
	short height;
};

//-------------------------------------------------------------------

void SaveTGA(const char* filename, ubyte* data, int w, int h, int c);

void SaveTIM_4bit(char* filename,
	ubyte* image_data, int image_size,
	int x, int y, int w, int h,
	ubyte* clut_data, int clut_h);

#endif // IMAGE_H