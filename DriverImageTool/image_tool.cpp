#include <math.h>

#include <malloc.h>
#include "core/VirtualStream.h"
#include "core/cmdlib.h"
#include "core/dktypes.h"
#include "util/image.h"
#include "util/util.h"

#include <string.h>
#include <nstd/String.hpp>

#include "math/Vector.h"

const int SKY_CLUT_START_Y = 252;
const int SKY_SIZE_W = 256 / 4;
const int SKY_SIZE_H = 84;

void CopyTpageImage(ushort* tp_src, ushort* dst, int x, int y, int dst_w, int dst_h)
{
	ushort* src = tp_src + x + 128 * y;

	for (int i = 0; i < dst_h; i++) 
	{
		memcpy(dst, src, dst_w * sizeof(short));
		dst += dst_w;
		src += 128;
	}
}

void ExportSkyImage(const char* skyFileName, ubyte* data, int x_idx, int y_idx, int sky_id, int n)
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

void ConvertIndexedSkyImage(uint* color_data, ubyte* src_indexed, int x_idx, int y_idx, bool outputBGR, bool originalTransparencyKey)
{
	ushort imageClut[16];

	int clut_x = x_idx * 16;
	int clut_y = SKY_CLUT_START_Y + y_idx;

	CopyTpageImage((ushort*)src_indexed, (ushort*)imageClut, clut_x, clut_y, 16, 1);
	
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
			ubyte clindex = src_indexed[y * 256 + x / 2];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			// flip texture by Y
			int ypos = (256 - y - 1) * 512;

			if (outputBGR)
			{
				TVec4D<ubyte> color = rgb5a1_ToBGRA8(imageClut[clindex], originalTransparencyKey);
				color_data[ypos + x] = *(uint*)(&color);
			}
			else
			{
				TVec4D<ubyte> color = rgb5a1_ToRGBA8(imageClut[clindex], originalTransparencyKey);
				color_data[ypos + x] = *(uint*)(&color);
			}
		}
	}
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
					ConvertIndexedSkyImage(color_data, skyImage, x, y, true, true);
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

	ubyte* imageClut = bgData + 11 * 0x8000;
	
	// convert background image and store
	{
		ubyte timData[64*6 * 2 * 512];
		
		int rect_w;
		int rect_h;
		rect_w = 64;
		rect_h = 256;

		for (int i = 0; i < 6; i++)
		{
			ubyte* bgImagePiece = bgData + i * 0x8000;

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
			timData, 64 * 6 * 512, 0, 0, 384*2, 512, (ubyte*)imageClut, 1);
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
			ubyte* imageAddr = data + i * 0x8000;

			if ((i * 0x8000) < size)
				ExportExtraImage(extraFilename, imageAddr, imageClut, i);
		}

		free(data);
	}

	free(bgData);

	MsgInfo("Done!\n");
}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriverImageTool -sky2tim SKY0.RAW\n");
	MsgInfo("\tDriverImageTool -sky2tga SKY1.RAW\n");
	MsgInfo("\tDriverImageTool -bg2tim CARBACK.RAW <CCARS.RAW>\n");
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
				MsgWarning("-bg2tim must have an argument!");
		}
	}

	return 0;
}