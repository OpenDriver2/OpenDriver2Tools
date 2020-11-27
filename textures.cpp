
#include "driver_level.h"

#include "core/cmdlib.h"
#include "core/IVirtualStream.h"
#include "math/Vector.h"



//-------------------------------------------------------------------------------


TEXPAGE_POS*		g_texPagePos = NULL;

int					g_numTexPages = 0;
int					g_numTexDetails = 0;

int					g_numPermanentPages = 0;
int					g_numSpecPages = 0;

XYPAIR				g_permsList[16];
XYPAIR				g_specList[16];
texdata_t*			g_pageDatas = NULL;
TexPage_t*			g_texPages = NULL;
extclutdata_t*		g_extraPalettes = NULL;
int					g_numExtraPalettes = 0;

// legacy format converter
TVec4D<ubyte> bgr5a1_ToRGBA8(ushort color)
{
	ubyte b = (color & 0x1F) * 8;
	ubyte g = ((color >> 5) & 0x1F) * 8;
	ubyte r = ((color >> 10) & 0x1F) * 8;
	ubyte a = (color >> 15)*255;

	return TVec4D<ubyte>(r,g,b,a);
}

//-------------------------------------------------------------------------------

// unpacks texture, returns byte count
// there is something like RLE used
int UnpackTexture(ubyte* pSrc, ubyte* pDst)
{
	ubyte* pSrcData = pSrc;
	ubyte* pDstData = pDst;

	do
	{
		int8 c = *pSrcData++;

		int count = c;// (c & 0x7f) + 1;

		if(c & 0x80)
		{
			// Repeat 2 + |count| times

			for(int i = 2; i > count; i--)
				*pDstData++ = *pSrcData;

			pSrcData++;
		}
		else
		{
			// Copy 1 + count bytes
			for(int i = -1; i < count; i++)
				*pDstData++ = *pSrcData++;
		}
	}while((pDstData-pDst) < TEXPAGE_4BIT_SIZE);

	return (pSrcData - pSrc);
}

//-------------------------------------------------------------------------------

#define MAX_PAGE_CLUTS 63

struct texturedata_t
{
	int			numPalettes;
	TEXCLUT		palettes[63];
	ubyte		compTable[28];		// padding for 512-B alignment if uncompressed
	ubyte		texels[128 * 256];	// 256×256 four-bit indices
};

void LoadCompressedTexture(IVirtualStream* pFile, texdata_t* out, ubyte* out4bitData)
{
	int rStart = pFile->Tell();

	pFile->Read( &out->numPalettes, 1, sizeof(int) );

	Msg("cluts: %d\n", out->numPalettes);

	// allocate palettes
	out->clut = new TEXCLUT[out->numPalettes];

	for(int i = 0; i < out->numPalettes; i++)
	{
		// read 16 palettes
		pFile->Read(&out->clut[i].colors, 1, sizeof(ushort)*16);
	}

	int imageStart = pFile->Tell();

	// read compression data
	ubyte* compressedData = new ubyte[TEXPAGE_4BIT_SIZE + 28];
	pFile->Read(compressedData, 1, TEXPAGE_4BIT_SIZE + 28);

	// unpack
	out->rsize = UnpackTexture(compressedData, out4bitData);

	// seek to the right position
	pFile->Seek(imageStart + out->rsize, VS_SEEK_SET);
}

void LoadTPageAndCluts(IVirtualStream* pFile, texdata_t* out, int nPage, bool isCompressed)
{
	int rStart = pFile->Tell();

	if(out->data)
	{
		// skip already loaded data
		pFile->Seek(out->rsize, VS_SEEK_CUR);
		return;
	}

	// skip zero padding (usually 2048 bytes)
	int check_numpal;
	pFile->Read(&check_numpal, 1, sizeof(int));
	pFile->Seek(-(int)sizeof(int), VS_SEEK_CUR); // seek back

	if(check_numpal == 0)
	{
		pFile->Seek(2048, VS_SEEK_CUR);

		/*
		ubyte check_zero;
		do
		{
			pFile->Read(&check_zero, 1, sizeof(ubyte));
		}while(check_zero == 0);

		pFile->Seek(-1, VS_SEEK_CUR); // seek back
		*/
	}

	ubyte* tex4bitData = new ubyte[TEXPAGE_4BIT_SIZE];

	if( isCompressed )
	{
		LoadCompressedTexture(pFile, out, tex4bitData);
	}
	else
	{
		// non-compressed textures loads different way, with a fixed size
		texturedata_t* texData = new texturedata_t;
		pFile->Read( texData, 1, sizeof(texturedata_t) );

		out->numPalettes = texData->numPalettes;

		out->clut = new TEXCLUT[out->numPalettes];
		memcpy(out->clut, texData->palettes, sizeof(TEXCLUT)*out->numPalettes);

		memcpy(tex4bitData, texData->texels, TEXPAGE_4BIT_SIZE);

		// not need anymore
		delete texData;
	}

	// convert 4 bit indexed paletted to 8 bit indexed paletted
	ubyte* indexed = new ubyte[TEXPAGE_SIZE];

	for(int y = 0; y < 256; y++)
	{
		for(int x = 0; x < 256; x++)
		{
			// The color index from the file.
			ubyte colorIndex = tex4bitData[y * 128 + x / 2];

			if(isCompressed) // since compressed textures are flipped
				colorIndex = tex4bitData[(TEXPAGE_4BIT_SIZE-1) - (y * 128 + x / 2)];

			if(0 != (x & 1))
				colorIndex >>= 4;

			colorIndex = colorIndex & 0xF;

			indexed[y * 256 + x] = colorIndex;
		}
	}

	delete [] tex4bitData;

	int readSize = pFile->Tell()-rStart;

	out->rsize = readSize;
	out->data = indexed;

	Msg("PAGE %d (%s) datasize=%d\n", nPage, isCompressed ? "compr" : "spooled", readSize);
}

//
// loads global textures (pre-loading stage)
//
void LoadPermanentTPages(IVirtualStream* pFile)
{
	g_levStream->Seek( g_levInfo.texdata_offset, VS_SEEK_SET );

	// allocate page data
	g_pageDatas = new texdata_t[g_numTexPages];
	memset(g_pageDatas, 0, sizeof(texdata_t)*g_numTexPages);

	//-----------------------------------

	Msg("Loading permanent texture pages (%d)\n", g_numPermanentPages);

	// load permanent pages
	for(int i = 0; i < g_numPermanentPages; i++)
	{
		int tpage = g_permsList[i].x;

		// permanents are also compressed
		LoadTPageAndCluts(pFile, &g_pageDatas[tpage], tpage, true);

		if (pFile->Tell() % 4096)
			pFile->Seek(2048 - (pFile->Tell() % 2048), VS_SEEK_CUR);
	}

	// Driver 2 - special cars only
	// Driver 1 - only player cars
	Msg("Loading special/car texture pages (%d)\n", g_numSpecPages);

	// load spec pages
	for (int i = 0; i < g_numSpecPages; i++)
	{
		int tpage = g_specList[i].x;
		
		// permanents are compressed
		LoadTPageAndCluts(pFile, &g_pageDatas[tpage], tpage, true);

		if (pFile->Tell() % 4096)
			pFile->Seek(2048 - (pFile->Tell() % 2048), VS_SEEK_CUR);
	}
}

//-------------------------------------------------------------
// parses texture info lumps. Quite simple
//-------------------------------------------------------------
void LoadTextureInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int numPages;
	pFile->Read(&numPages, 1, sizeof(int));

	int numTextures;
	pFile->Read(&numTextures, 1, sizeof(int));

	Msg("TPage count: %d\n", numPages);
	Msg("Texture amount: %d\n", numTextures);

	// read array of texutre page info
	g_texPagePos = new TEXPAGE_POS[numPages+1];
	g_texPages = new TexPage_t[numPages];

	pFile->Read(g_texPagePos, numPages+1, sizeof(TEXPAGE_POS));

	// read page details
	g_numTexPages = numPages;

	for(int i = 0; i < numPages; i++) 
	{
		TexPage_t& tp = g_texPages[i];

		pFile->Read(&tp.numDetails, 1, sizeof(int));

		// read texture detail info
		tp.details = new TEXINF[tp.numDetails];
		pFile->Read(tp.details, tp.numDetails, sizeof(TEXINF));
	}

	pFile->Read(&g_numPermanentPages, 1, sizeof(int));
	Msg("Permanent TPages = %d\n", g_numPermanentPages);
	pFile->Read(g_permsList, 16, sizeof(XYPAIR));

	// Driver 2 - special cars only
	// Driver 1 - only player cars
	pFile->Read(&g_numSpecPages, 1, sizeof(int));
	pFile->Read(g_specList, 16, sizeof(XYPAIR));

	Msg("Special/Car TPages = %d\n", g_numSpecPages);

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//----------------------------------------------------------------------------------------------------

TEXINF* FindTextureDetail(const char* name)
{
	for(int i = 0; i < g_numTexPages; i++)
	{
		for(int j = 0; j < g_texPages[i].numDetails; j++)
		{
			char* pTexName = g_textureNamesData + g_texPages[i].details[j].nameoffset;

			if(!strcmp(pTexName, name))
			{
				return &g_texPages[i].details[j];
			}
		}
	}

	return NULL;
}
