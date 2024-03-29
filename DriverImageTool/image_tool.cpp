﻿#include <math.h>

#include <malloc.h>
#include "core/VirtualStream.h"
#include "core/cmdlib.h"
#include "core/dktypes.h"
#include "util/image.h"
#include "util/util.h"

#include <string.h>
#include <nstd/String.hpp>
#include <nstd/Array.hpp>

#include "math/Vector.h"

#pragma pack(push, 1)
struct BMP_INFO_HEADER
{
	int size;
	int width;
	int height;
	short planes;
	short bitcount;
	int compression;
	int sizeimage;
	int xpelsPerMeter;
	int ypelsPerMeter;
	int clrused;
	int clrimportant;
};

struct BMP_FILE_HEADER
{
	char type[2];
	int size;
	short _reserved[2];
	int offbits;
	BMP_INFO_HEADER info;
};
#pragma pack(pop)

const int TPAGE_SIZE_8_BIT = (256 * 256);
const int TPAGE_SIZE_16_BIT = (256 * 256 * sizeof(short));

// TSD flags
const int TEX_8BIT = 0x10;

const int SKY_CLUT_START_Y = 252;
const int SKY_SIZE_W = 256 / 4;
const int SKY_SIZE_H = 84;

const int BG_SPLICE_W = 64;
const int BG_SPLICE_H = 256;
const int BG_SPLICE_SIZE = BG_SPLICE_W * 2 * BG_SPLICE_H;

void CopyTpageImage(const ushort* tp_src, ushort* dst, int x, int y, int dst_w, int dst_h)
{
	const ushort* src = tp_src + x + 128 * y;

	for (int i = 0; i < dst_h; i++) 
	{
		memcpy(dst, src, dst_w * sizeof(short));
		dst += dst_w;
		src += 128;
	}
}

void ExportSkyImage(const char* skyFileName, const ubyte* data, int x_idx, int y_idx, int sky_id, int n)
{
	ubyte imageCopy[SKY_SIZE_W * SKY_SIZE_H];
	ushort imageClut[16];

	int clut_x = x_idx * 16; // FIXME: is that correct?
	int clut_y = SKY_CLUT_START_Y + y_idx;

	CopyTpageImage((ushort*)data, (ushort*)imageCopy, x_idx * SKY_SIZE_W / 2, y_idx * SKY_SIZE_H, SKY_SIZE_W / 2, SKY_SIZE_H);
	CopyTpageImage((ushort*)data, (ushort*)imageClut, clut_x, clut_y, 16, 1);

	SaveTIM_4bit(varargs("%s_%d_%d.TIM", skyFileName, sky_id, n),
		imageCopy, 64 * SKY_SIZE_H, 0, 0, SKY_SIZE_W*2, SKY_SIZE_H, (ubyte*)imageClut, 1);
}

void ConvertSkyIndexedImage8(uint* colorData, const ubyte* srcIndexed, int x_idx, int y_idx, bool outputBGR, bool originalTransparencyKey)
{
	ushort imageClut[16];

	int clut_x = x_idx * 16;
	int clut_y = SKY_CLUT_START_Y + y_idx;

	CopyTpageImage((ushort*)srcIndexed, (ushort*)imageClut, clut_x, clut_y, 16, 1);
	
	int ox = x_idx * SKY_SIZE_W * 2;
	int oy = y_idx * SKY_SIZE_H;
	int w = SKY_SIZE_W * 2;
	int h = SKY_SIZE_H;

	int tp_wx = ox + w;
	int tp_hy = oy + h;
	
	for (int y = oy; y < tp_hy; y++)
	{
		for (int x = ox; x < tp_wx; x++)
		{
			ubyte clindex = srcIndexed[y * 256 + x / 2];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			// flip texture by Y
			int ypos = (256 - y - 1) * 512;

			if (outputBGR)
			{
				TVec4D<ubyte> color = rgb5a1_ToBGRA8(imageClut[clindex], originalTransparencyKey);
				colorData[ypos + x] = *(uint*)(&color);
			}
			else
			{
				TVec4D<ubyte> color = rgb5a1_ToRGBA8(imageClut[clindex], originalTransparencyKey);
				colorData[ypos + x] = *(uint*)(&color);
			}
		}
	}
}

// this function just unpacks sky data
void ConvertSkyD1(const char* skyFileName)
{
	FILE* fp = fopen(skyFileName, "rb");
	if (!fp)
	{
		MsgError("Unable to open '%s'\n", skyFileName);
		return;
	}

	for (int skyNum = 0; skyNum < 13; skyNum++)
	{
		fseek(fp, skyNum * 48, SEEK_SET);

		int offsetInfo[16];
		fread(offsetInfo, 1, sizeof(offsetInfo), fp);

		// 12 BMPs for each sky
		for (int i = 0; i < 12; i++)
		{
			FILE* pNewFile = fopen(varargs("%s_%d_%d.BMP", skyFileName, skyNum, i), "wb");
			if (!pNewFile)
			{
				MsgError("Unable to save file!\n");
				return;
			}

			// get the BMP data
			fseek(fp, offsetInfo[i], SEEK_SET);

			BMP_FILE_HEADER hdr;
			fread(&hdr, 1, sizeof(BMP_FILE_HEADER), fp);

			int bmpSize = hdr.size;
			char* data = new char[bmpSize];
			fseek(fp, offsetInfo[i], SEEK_SET);
			fread(data, 1, bmpSize, fp);

			// write header
			fwrite(data, 1, bmpSize, pNewFile);

			// dun
			fclose(pNewFile);

			delete[] data;
		}
	}

	//LoadfileSeg("DATA\\SKY.BIN", mallocptr, gSkyNumber * 0x30, 0x34);
	//LoadfileSeg("DATA\\SKY.BIN", &DAT_000f8600, *texturePtr, texturePtr[0xc] - *texturePtr);

	fclose(fp);
}

void ConvertSky(const char* skyFileName, bool saveTGA)
{
	FILE* fp = fopen(skyFileName, "rb");
	if(!fp)
	{
		MsgError("Unable to open '%s'\n", skyFileName);
		return;
	}

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	ubyte* data = (ubyte*)malloc(size);

	fread(data, size, 1, fp);
	fclose(fp);

	const int OFFSET_STEP = 0x10000;

#define SKY_TEX_CHANNELS 4
#define SKY_TEXPAGE_SIZE		(512*256)
	
	uint* color_data;
	int imgSize = SKY_TEXPAGE_SIZE * SKY_TEX_CHANNELS;
	
	if(saveTGA)
	{
		color_data = (uint*)malloc(imgSize);

		MsgInfo("Converting '%s' to separata TGAs...\n", skyFileName);
	}
	else
	{
		MsgInfo("Converting '%s' to separate TIM files...\n", skyFileName);
	}

	// 5 skies in total
	for(int i = 0; i < 5; i++)
	{
		int n = 0;
		ubyte* skyImage = data + i * OFFSET_STEP;

		if (saveTGA)
		{
			memset(color_data, 0, imgSize);
		}

		// 3x4 images (128x84 makes 256x252 tpage)
		for (int y = 0; y < 3; y++)
		{
			for (int x = 0; x < 4; x++, n++)
			{
				if(saveTGA)
				{
					ConvertSkyIndexedImage8(color_data, skyImage, x, y, true, true);
				}
				else
				{
					ExportSkyImage(skyFileName, skyImage, x, y, i, n);
				}
			}
		}

		if(saveTGA)
		{
			SaveTGA(varargs("%s_%d.TGA", skyFileName, i), (ubyte*)color_data, 512, 256, SKY_TEX_CHANNELS);
		}
	}

	if (saveTGA)
	{
		free(color_data);
	}

	MsgInfo("Done!\n");
}

//-----------------------------------------------------

void ExportExtraImage(const char* filename, ubyte* data, ubyte* clut_data, int n)
{
	ubyte imageCopy[64 * 219];
	ushort imageClut[16];

	// size is 64x219. palette goes after it

	//CopyTpageImage((ushort*)data, (ushort*)imageCopy, 0, 0, 64, 219);
	//CopyTpageImage((ushort*)data, (ushort*)imageClut, clut_x, clut_y, 16, 1);

	SaveTIM_4bit(varargs("%s_%d.TIM", filename, n),
		data, 64 * 2 * 219, 0, 0, 64 * 4, 219, clut_data, 1);
}

// In frontend, extra poly has a preview image drawn
void ConvertBackgroundRaw(const char* filename, const char* extraFilename)
{	
	FILE* bgFp = fopen(filename, "rb");

	if (!bgFp)
	{
		MsgError("Unable to open '%s'\n", filename);
		return;
	}

	MsgInfo("Converting background '%s' to separate TIM files...\n", filename);

	// read background file
	fseek(bgFp, 0, SEEK_END);
	int bgSize = ftell(bgFp);
	fseek(bgFp, 0, SEEK_SET);

	ubyte* bgData = (ubyte*)malloc(bgSize);
	fread(bgData, bgSize, 1, bgFp);

	fclose(bgFp);

	ubyte* imageClut = bgData + 11 * BG_SPLICE_SIZE;
	
	// convert background image and store
	{
		String str = String::fromCString(filename).toLowerCase();
		
		// MCBACK.RAW case
		if (str.find("mcback"))
		{
			SaveTIM_16bit(varargs("%s.TIM", filename),
				bgData, 512 * 256 * 2, 0, 0, 512, 256);// , bgData, 1);
			return;
		}

		ubyte* timData = (ubyte*)malloc(64 * 6 * 512);
		int rect_w = 64;
		int rect_h = 256;

		for (int i = 0; i < 6; i++)
		{
			ubyte* bgImagePiece = bgData + i * BG_SPLICE_SIZE;

			int rect_y = i / 3;
			int rect_x = (i - (rect_y & 1) * 3) * 128;
			rect_y *= 256;
			
			for (int y = 0; y < rect_h; y++)
			{
				for (int x = 0; x < rect_w * 2; x++)
				{
					timData[(rect_y + y) * 64 * 6 + rect_x + x] = bgImagePiece[y * 128 + x];
				}
			}
		}

		SaveTIM_4bit(varargs("%s.TIM", filename),
			timData, 64 * 6 * 512, 0, 0, 384*2, 512, imageClut, 1);


		// check if it's GFX.RAW
		// if(str.find("gfx"))
		{
			imageClut = bgData + 0x30000 + 5 * BG_SPLICE_SIZE + 128;

			// extract font
			ubyte* bgImagePiece = bgData + 0x30000;

			SaveTIM_4bit(varargs("%s_FONT.TIM", filename),
				bgImagePiece, 64*2*256, 0, 0, 256, 256, imageClut, 1);
			
			imageClut += 128;
			bgImagePiece += BG_SPLICE_SIZE;

			// extract selection texture
			SaveTIM_4bit(varargs("%s_SEL.TIM", filename),
				bgImagePiece, 64 * 2 * 36, 0, 0, 256, 36, imageClut, 1);
		}

		//else // pointless to have font and selection texture as tpage, but i'll leave it here as reference code
		{
			// try extract the rest
			for (int i = 0; i < 6; i++)
			{
				ubyte* bgImagePiece = bgData + 0x30000 + i * BG_SPLICE_SIZE;

				int rect_y = i / 3;
				int rect_x = (i - (rect_y & 1) * 3) * 128;
				rect_y *= 256;

				for (int y = 0; y < rect_h; y++)
				{
					for (int x = 0; x < rect_w * 2; x++)
					{
						timData[(rect_y + y) * 64 * 6 + rect_x + x] = bgImagePiece[y * 128 + x];
					}
				}
			}

			SaveTIM_4bit(varargs("%s_REST.TIM", filename),
				timData, 64 * 6 * 512, 0, 0, 384 * 2, 512, (ubyte*)imageClut, 1);
		}

		free(timData);
	}

	// load extra file if specified
	if(extraFilename)
	{
		FILE* fp = fopen(extraFilename, "rb");
		if (!fp)
		{
			free(bgData);
			MsgError("Unable to open '%s'\n", extraFilename);
			return;
		}

		MsgInfo("Converting '%s' to separate TIM files...\n", extraFilename);

		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		ubyte* data = (ubyte*)malloc(size);

		fread(data, size, 1, fp);
		fclose(fp);

		for (int i = 0; i < 16; i++)
		{
			ubyte* imageAddr = data + i * BG_SPLICE_SIZE;

			if ((i * BG_SPLICE_SIZE) < size)
				ExportExtraImage(extraFilename, imageAddr, imageClut, i);
		}

		free(data);
	}

	free(bgData);

	MsgInfo("Done!\n");
}

//-----------------------------------------------------

// builds image out of separate TIM slices
bool BuildBackgroundRaw(char* out, const Array<char*>& timFileNames)
{
	Array<usize> size_cd;
	Array<ubyte*> clut_data;

	// TODO: check for maximum number of images

	clut_data.resize(timFileNames.size());
	size_cd.resize(timFileNames.size());

	FILE* fp = fopen(out, "wb");

	if (!fp)
	{
		MsgError("Unable to open '%s' file\n", out);
		return false;
	}
	
	for (usize i = 0; i < timFileNames.size(); i++)
	{
		FILE* inFp = fopen(timFileNames[i], "rb");
		if (!inFp)
		{
			MsgError("Unable to open '%s' file\n", timFileNames[i]);
			return false;
		}
		
		MsgInfo("Adding file: '%s'\n", timFileNames[i]);
		
		TIMHDR timHdr;
		TIMIMAGEHDR cluthdr;
		TIMIMAGEHDR header;
		ubyte* image_data;

		// Check header
		fread(&timHdr, 1, sizeof(timHdr), inFp);
		if (timHdr.magic != 0x10)
		{
			MsgError("Input file is not a TIM file!\n");
			fclose(inFp);
			return false;
		}

		if (timHdr.flags != 0x08)
		{
			MsgError("Input image must be 4-bit!\n");
			fclose(inFp);
			return false;
		}

		// FIXME: need to check background image size properties!!!

		// Parsing CLUT Header
		fread(&cluthdr, 1, sizeof(cluthdr), inFp);
		clut_data[i] = static_cast<ubyte*>(malloc(cluthdr.len - sizeof(cluthdr)));
		fread(clut_data[i], 1, cluthdr.len - sizeof(cluthdr), inFp);

		// Parsing image data header
		fread(&header, 1, sizeof(header), inFp);
		image_data = static_cast<ubyte*>(malloc(header.len - sizeof(header)));
		fread(image_data, 1, header.len - sizeof(header), inFp);

		size_cd[i] = cluthdr.len - sizeof(cluthdr);
		
		fclose(inFp);
		MsgInfo("Writing image data of file '%s' to '%s'\n", timFileNames[i], out);
	
		ubyte* bgImagePiece = static_cast<ubyte*>(malloc(BG_SPLICE_SIZE));

		for (int j = 0; j < 6; j++)
		{
			int rect_y = j / 3;
			int rect_x = (j - (rect_y & 1) * 3) * (BG_SPLICE_W * 2);
			rect_y *= BG_SPLICE_H;

			for (int y = 0; y < BG_SPLICE_H; y++)
			{
				for (int x = 0; x < BG_SPLICE_W * 2; x++)
				{
					bgImagePiece[y * BG_SPLICE_W * 2 + x] = image_data[(rect_y + y) * BG_SPLICE_W * 6 + rect_x + x];
				}
			}

			fwrite(bgImagePiece, 1, BG_SPLICE_SIZE, fp);			
		}

		free(bgImagePiece);
		free(image_data);
	}

	// Set offset to start of CLUT data
	fseek(fp, 0x58000, SEEK_SET);
	for (usize i = 0; i < timFileNames.size(); i++)
	{
		MsgInfo("Writting CLUT data to '%s'\n", timFileNames[i]);
		fwrite(clut_data[i], 1, size_cd[i], fp);

		free(clut_data[i]);
	}

	fclose(fp);
	MsgAccept("'%s' compiled with success !\n", out);

	return true;
}

//-----------------------------------------------------

struct PalEntry
{
	ubyte peRed;
	ubyte peGreen;
	ubyte peBlue;
	ubyte peFlags;
};

void ConvertIndexedTexImage8(uint* colorData, const ubyte* srcIndexed, const PalEntry* palette)
{
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = srcIndexed[y * 256 + x];

			// flip texture by Y
			const int ypos = (256 - y - 1) * 256;

			TVec4D<ubyte> color;
			color.x = palette[clindex].peRed;
			color.y = palette[clindex].peGreen;
			color.z = palette[clindex].peBlue;
			color.w = 0;

			colorData[ypos + x] = *(uint*)(&color);
		}
	}
}

void ConvertIndexedTexImage16(uint* colorData, const ushort* srcIndexed)
{
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ushort clindex = srcIndexed[y * 256 + x];

			// flip texture by Y
			const int ypos = (256 - y - 1) * 256;

			TVec4D<ubyte> color = rgb5a1_ToRGBA8(clindex, false);
			colorData[ypos + x] = *(uint*)(&color);
		}
	}
}

void ConvertTex(const char* fileName)
{
	FILE* fp = fopen(fileName, "rb");

	if (!fp)
	{
		MsgError("Unable to open '%s' file\n", fileName);
		return;
	}

	ubyte* parentTextureMem = (ubyte*)malloc(TPAGE_SIZE_16_BIT);
	ubyte* childTextureMem = (ubyte*)malloc(TPAGE_SIZE_16_BIT);

	const short MAX_PALETTES = 50;

	ubyte palette[MAX_PALETTES][1024];
	bool palettePresent[MAX_PALETTES]{ false };

	short numPalettes = 0;
	fread(&numPalettes, sizeof(short), 1, fp);

	Msg("Processing %d palettes\n", numPalettes);

	short paletteLoop = numPalettes;
	while (paletteLoop--)
	{
		short slotNo = 0;
		fread(&slotNo, sizeof(short), 1, fp);

		if (slotNo < 0 || slotNo >= MAX_PALETTES)
		{
			// skip
			fseek(fp, 1024, SEEK_CUR);
		}
		else
		{
			fread(palette[slotNo], 1024, 1, fp);
			palettePresent[slotNo] = true;
		}
	}

	int numTSets = 0;
	fread(&numTSets, sizeof(int), 1, fp);

	Msg("Processing %d texture sets\n", numTSets);

	uint* colorData = (uint*)malloc(TPAGE_SIZE_8_BIT * 4);

	for (int i = 0; i < numTSets; ++i)
	{
		short parentFlags;
		short parentData;
		fread(&parentFlags, sizeof(short), 1, fp);
		fread(&parentData, sizeof(short), 1, fp);

		int parent = i;
		int child = -1;

		if (parentFlags & TEX_8BIT)
		{
			MsgInfo("Got 8 bit image\n");

			fread(parentTextureMem, TPAGE_SIZE_8_BIT, 1, fp);

			// SaveTGA(varargs("%s_%d.TGA", skyFileName, i), (ubyte*)colorData, 512, 256, SKY_TEX_CHANNELS);

			// child
			short childFlags;
			short childData;
			fread(&childFlags, sizeof(short), 1, fp);
			fread(&childData, sizeof(short), 1, fp);
			fread(childTextureMem, TPAGE_SIZE_8_BIT, 1, fp);

			if(!palettePresent[parentData])
				MsgInfo("wut\n");

			ConvertIndexedTexImage8(colorData, parentTextureMem, (PalEntry*)palette[parentData]);
			SaveTGA(varargs("%s_par_%d.TGA", fileName, i), (ubyte*)colorData, 256, 256, 4);

			if (!palettePresent[childData])
				MsgInfo("wut\n");

			ConvertIndexedTexImage8(colorData, childTextureMem, (PalEntry*)palette[childData]);
			SaveTGA(varargs("%s_chi_%d.TGA", fileName, i), (ubyte*)colorData, 256, 256, 4);

			child = parent + 1;

			++i;
		}
		else
		{
			MsgInfo("Got 16 bit image\n");

			fread(parentTextureMem, TPAGE_SIZE_16_BIT, 1, fp);

			if (!palettePresent[parentData])
				MsgInfo("wut\n");

			ConvertIndexedTexImage16(colorData, (ushort*)parentTextureMem);
			SaveTGA(varargs("%s_%d.TGA", fileName, i), (ubyte*)colorData, 256, 256, SKY_TEX_CHANNELS);
		}
	}

	free(colorData);
	free(parentTextureMem);
	free(childTextureMem);
	fclose(fp);
}

struct TSD_HEADER 
{
	uint	length;
	short	size;
	short	tsetCount;
};

struct TSET_INFO
{
	int		length;
	int		uvOffset;
	int		bitmapOffset;
	short	numTextures;
	ushort	flags;
	ushort	number;
	short	pad;
};

struct TSD_UV_INFO 
{
	uint	id;
	ubyte	U, V;
	short	width, height;
	short	fileNum;
};

struct TSD_PALETTE
{
	ubyte r,g,b,a;
};

#pragma optimize("", off)

void ConvertTsdIndexedImage8(uint* colorData, const ubyte* srcIndexed, const TSD_PALETTE* palette)
{
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = srcIndexed[y * 256 + x];

			// flip texture by Y
			const int ypos = (256 - y - 1) * 256;

			TVec4D<ubyte> color;
			color.x = palette[clindex].r;
			color.y = palette[clindex].g;
			color.z = palette[clindex].b;
			color.w = palette[clindex].a;
			
			colorData[ypos + x] = *(uint*)(&color);
		}
	}
}

void ConvertTsdImage16(uint* colorData, const ushort* srcIndexed, const TSD_UV_INFO& uv)
{
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ushort clindex = srcIndexed[y * 256 + x];

			// flip texture by Y
			const int ypos = (256 - y - 1) * 256;

			TVec4D<ubyte> color = rgb5a1_ToRGBA8(clindex, false);
			colorData[ypos + x] = *(uint*)(&color);
		}
	}
}

void ConvertTsd(const char* fileName)
{
	FILE* fp = fopen(fileName, "rb");

	if (!fp)
	{
		MsgError("Unable to open '%s' file\n", fileName);
		return;
	}

	TSD_HEADER header;
	TSET_INFO info;

	// Read in ID and make sure its a TSD...
	char ident[4];
	fread(ident, 1, sizeof(ident), fp);
	if ((ident[0] != 'T') || (ident[1] != 'S') || (ident[2] != 'D') || (ident[3] != ' '))
	{
		MsgError("%s has invalid TSD identifier\n", fileName);
		fclose(fp);
		return;
	}

	// floating point version, wtf?
	float version;
	fread(&version, 1, sizeof(float), fp);
	fread(&header, 1, sizeof(TSD_HEADER), fp);

	const short MAX_PALETTES = 50; // might be insufficient
	TSD_PALETTE palettes[MAX_PALETTES][256];

	if (version == 1.0)
	{
		fseek(fp, 256 * sizeof(TSD_PALETTE), SEEK_CUR);
		fseek(fp, 256 * sizeof(TSD_PALETTE), SEEK_CUR);

		// FIXME: HOW TO?
	}
	else
	{
		short numPalettes;
		fread(&numPalettes, 1, sizeof(short), fp);

		Msg("Processing %d palettes\n", numPalettes);
		while (numPalettes--)
		{
			short id, slot;
			fread(&id, sizeof(short), 1, fp);
			fread(&slot, sizeof(short), 1, fp);

			fread(palettes[slot], sizeof(TSD_PALETTE), 256, fp);
		}
	}

	Msg("Processing %d texture sets\n", header.tsetCount);

	ubyte* textureMem = (ubyte*)malloc(TPAGE_SIZE_16_BIT);
	uint* colorData = (uint*)malloc(TPAGE_SIZE_8_BIT * 4);

	for (int i = 0; i < header.tsetCount; ++i)
	{
		// special flag for 16 biut textures
		short paletteIdx;
		fread(&paletteIdx, sizeof(short), 1, fp);

		const long tsetStart = ftell(fp);

		TSET_INFO tsetInfo;
		fread(&tsetInfo, sizeof(tsetInfo), 1, fp);

		Msg("Set %d textures %d flags %d\n", i, tsetInfo.numTextures, tsetInfo.flags);

		// Read indices bitmap
		fseek(fp, tsetStart + tsetInfo.bitmapOffset, SEEK_SET);
		fread(textureMem, 1, TPAGE_SIZE_16_BIT, fp);

		// Go through UVs to make sure it's ok
		fseek(fp, tsetStart + tsetInfo.uvOffset, SEEK_SET);
		for (int j = 0; j < tsetInfo.numTextures; ++j)
		{
			TSD_UV_INFO uvInfo;
			fread(&uvInfo, sizeof(uvInfo), 1, fp);

			Msg("\tUV %d, id %d\n", j, uvInfo.id);

			if (tsetInfo.flags & TEX_8BIT)
			{
				// is that OK? I mean paletteIdx...
				ConvertTsdIndexedImage8(colorData, textureMem, palettes[paletteIdx]);
			}
			else
			{
				ConvertTsdImage16(colorData, (ushort*)textureMem, uvInfo);
			}
		}

		SaveTGA(varargs("%s_%d.TGA", fileName, i), (ubyte*)colorData, 256, 256, SKY_TEX_CHANNELS);

		fseek(fp, tsetStart + tsetInfo.length, SEEK_SET);
	}

	fclose(fp);
	free(colorData);
	free(textureMem);
}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriverImageTool -sky2tim SKY0.RAW\n");
	MsgInfo("\tDriverImageTool -sky2tga SKY1.RAW\n");
	MsgInfo("\tDriverImageTool -skybin2bmp SKY.BIN\n");
	MsgInfo("\tDriverImageTool -bg2tim CARBACK.RAW <CCARS.RAW>\n");
	MsgInfo("\tDriverImageTool -tim2raw GFX.RAW BG.tim <FontAndSelection.tim Image3.tim ..>\n");
	MsgInfo("\tDriverImageTool -tex2tga frontend.tex\n");
	MsgInfo("\tDriverImageTool -tsd2tga frontend.tsd\n");
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	Install_ConsoleSpewFunction();
#endif

	Msg("---------------\nDriverImageTool - PSX Driver image utilities\n---------------\n\n");

	if (argc <= 1)
	{
		PrintCommandLineArguments();
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		if (!stricmp(argv[i], "-sky2tim"))
		{
			if (i + 1 <= argc)
				ConvertSky(argv[i + 1], false);
			else
				MsgWarning("-sky2tim must have an argument!");
		}
		else if (!stricmp(argv[i], "-sky2tga"))
		{
			if (i + 1 <= argc)
				ConvertSky(argv[i + 1], true);
			else
				MsgWarning("-sky2tga must have an argument!");
		}
		else if (!stricmp(argv[i], "-skybin2bmp"))
		{
			if (i + 1 <= argc)
				ConvertSkyD1(argv[i + 1]);
			else
				MsgWarning("-skybin2bmp must have an argument!");
		}
		else if (!stricmp(argv[i], "-bg2tim"))
		{
			if (i + 1 <= argc)
			{
				if (i + 2 <= argc)
					ConvertBackgroundRaw(argv[i + 1], argv[i + 2]);
				else
					ConvertBackgroundRaw(argv[i + 1], nullptr);
			}
			else
				MsgWarning("-bg2tim must have argument!");
		}
		else if (!stricmp(argv[i], "-tim2raw"))
		{
			if (i + 2 <= argc)
			{
				Array<char*> filenames;
				const char* outputFilename = argv[i + 1];

				for (int j = i + 2; j < argc; j++)
				{
					if (*argv[j] == '-' || *argv[j] == '+' || *argv[j] == '/' || *argv[j] == '\\') // encountered new option?
						break;

					filenames.append(argv[j]);
				}
				
				BuildBackgroundRaw(argv[i + 1], filenames);
			}
			else
				MsgWarning("-tim2raw must have at least two arguments!");
		}
		else if (!stricmp(argv[i], "-tex2tga"))
		{
			if (i + 1 <= argc)
				ConvertTex(argv[i + 1]);
			else
				MsgWarning("-tex2tga must have an argument!");
		}
		else if (!stricmp(argv[i], "-tsd2tga"))
		{
			if (i + 1 <= argc)
				ConvertTsd(argv[i + 1]);
			else
				MsgWarning("-tsd2tga must have an argument!");
		}
	}

	return 0;
}
