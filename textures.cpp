
#include "driver_level.h"

#include "IVirtualStream.h"
#include "Math/Vector.h"

#include "core_base_header.h"
#include "DebugInterface.h"

//-------------------------------------------------------------------------------


TEXPAGEINFO*		g_texPageInfos = NULL;

int					g_numTexPages = 0;
int					g_numTexDetails = 0;

texdata_t*			g_pageDatas = NULL;
TexPage_t*			g_texPages = NULL;

// legacy format converter
TVec4D<ubyte> bgr5a1_ToRGBA8(ushort color)
{
	ubyte b = (color & 0x1F) * 8;
	ubyte g = ((color >> 5) & 0x1F) * 8;
	ubyte r = ((color >> 10) & 0x1F) * 8;
	ubyte a = (color >> 15)*255;

	return TVec4D<ubyte>(b,g,r,a);
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
		int8 count = *pSrcData++;

		if(count < 0)
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
	out->size = UnpackTexture(compressedData, out4bitData);

	// seek to the right position
	pFile->Seek(imageStart + out->size, VS_SEEK_SET);
}

void LoadTexturePageData(IVirtualStream* pFile, texdata_t* out, int nPage)
{
	int rStart = pFile->Tell();

	Msg("PAGE %d (%s)\n", nPage, (g_texPageInfos[nPage].flags & PAGE_FLAG_PRELOAD) ? "preload" : "region_dyn");

	ubyte* tex4bitData = new ubyte[TEXPAGE_4BIT_SIZE];

	bool isCompressed = (g_texPageInfos[nPage].flags & PAGE_FLAG_PRELOAD);// | (g_texPageInfos[nPage].flags & PAGE_FLAG_GLOBAL2);

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

	out->size = TEXPAGE_SIZE;
	out->data = indexed;
}

//
// loads global textures (pre-loading stage)
//
void LoadGlobalTextures(IVirtualStream* pFile)
{
	Msg("loading global texture pages\n");

	g_pageDatas = new texdata_t[g_numTexPages];
	memset(g_pageDatas, 0, sizeof(texdata_t)*g_numTexPages);

	int ofs = pFile->Tell();

	short mode = PAGE_FLAG_PRELOAD;

	int numTex = 0;

//read_again:

	for(int i = 0; i < g_numTexPages; i++)
	{
		if(!((g_texPageInfos[i].flags & mode)))
			continue;

		// read data
		LoadTexturePageData(pFile, &g_pageDatas[i], i);

		if(pFile->Tell() % 2048)
			pFile->Seek(2048 - (pFile->Tell() % 2048),VS_SEEK_CUR);
	}

	//if(g_texPageInfos[i].flags != PAGE_FLAG_PRELOAD)
	//	mode = PAGE_FLAG_GLOBAL2;
	
	//if(mode == PAGE_FLAG_PRELOAD)
	//	mode = PAGE_FLAG_GLOBAL2;
	//else
	//	return;
	
	//goto read_again;

	Msg("global textures: %d\n", numTex);
}

//-------------------------------------------------------------
// parses texture info lumps. Quite simple
//-------------------------------------------------------------
void LoadTextureInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int numPages;
	pFile->Read(&numPages, sizeof(int), 1);

	int numTotalDetails;
	pFile->Read(&numTotalDetails, sizeof(int), 1);

	Msg("Texture page count: %d\n", numPages);
	Msg("Texture detail (atlas) count: %d\n", numTotalDetails);

	// read array of texutre page info
	g_texPageInfos = new TEXPAGEINFO[numPages];
	pFile->Read(g_texPageInfos, numPages, sizeof(TEXPAGEINFO));

	int unk1, unk2 = 0;
	pFile->Read(&unk1, sizeof(int), 1);
	pFile->Read(&unk2, sizeof(int), 1);

	//Msg("unk1 = %d\n", unk1); // special?
	//Msg("unk2 = %d\n", unk2);

	// read page details
	g_numTexPages = numPages;

	g_texPages = new TexPage_t[numPages];

	for(int i = 0; i < numPages; i++) 
	{
		pFile->Read(&g_texPages[i].numDetails, sizeof(int), 1);

		// read texture detail info
		g_texPages[i].details = new TEXTUREDETAIL[g_texPages[i].numDetails];
		pFile->Read(g_texPages[i].details, g_texPages[i].numDetails, sizeof(TEXTUREDETAIL));
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//----------------------------------------------------------------------------------------------------

TEXTUREDETAIL* FindTextureDetail(const char* name)
{
	for(int i = 0; i < g_numTexPages; i++)
	{
		for(int j = 0; j < g_texPages[i].numDetails; j++)
		{
			char* pTexName = g_textureNamesData + g_texPages[i].details[j].texNameOffset;

			if(!strcmp(pTexName, name))
			{
				return &g_texPages[i].details[j];
			}
		}
	}

	return NULL;
}
