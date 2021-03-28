#include <math.h>

#include <malloc.h>
#include "core/VirtualStream.h"
#include "core/cmdlib.h"
#include "core/dktypes.h"
#include "util/image.h"
#include "util/util.h"

#include <string.h>

struct RECT16
{
	short x, y, w, h;
};

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
		imageCopy, SKY_SIZE_W * SKY_SIZE_H, 0, 0, SKY_SIZE_W*2, SKY_SIZE_H, (ubyte*)imageClut, 1);
}

void ConvertSky(const char* skyFileName)
{
	FILE* fp = fopen(skyFileName, "rb");
	if(!fp)
	{
		MsgError("Unable to open '%s'\n", skyFileName);
		return;
	}

	MsgInfo("Converting '%s' to separate TIM files...", skyFileName);

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	ubyte* data = (ubyte*)malloc(size);

	fread(data, size, 1, fp);
	fclose(fp);

	const int OFFSET_STEP = 0x10000;

	for(int i = 0; i < 5; i++)
	{
		int n = 0;
		ubyte* skyImage = data + i * OFFSET_STEP;

		for (int y = 0; y < 3; y++)
		{
			for (int x = 0; x < 4; x++, n++)
				ExportSkyImage(skyFileName, skyImage, x, y, i, n);
		}
	}

	MsgInfo("Done!");
}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriverImageTool -sky2tim SKY0.RAW\n");
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
				ConvertSky(argv[i + 1]);
			else
				MsgWarning("-sky2tim must have an argument!");
		}
	}

	return 0;
}