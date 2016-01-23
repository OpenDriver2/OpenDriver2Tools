
#include "driver_level.h"

#include "core/cmdlib.h"
#include "core/IVirtualStream.h"
#include "math/Vector.h"



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
	out->rsize = UnpackTexture(compressedData, out4bitData);

	// seek to the right position
	pFile->Seek(imageStart + out->rsize, VS_SEEK_SET);
}

void LoadTexturePageData(IVirtualStream* pFile, texdata_t* out, int nPage, bool isCompressed)
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

	Msg("PAGE %d (%s) datasize=%d\n", nPage, isCompressed ? "preload" : "region_dyn", readSize);
}

//
// loads global textures (pre-loading stage)
//
void LoadGlobalTextures(IVirtualStream* pFile)
{
	Msg("loading global texture pages\n");
	g_levStream->Seek( g_levInfo.texdata_offset, VS_SEEK_SET );

	// allocate page data
	g_pageDatas = new texdata_t[g_numTexPages];
	memset(g_pageDatas, 0, sizeof(texdata_t)*g_numTexPages);

	short mode = PAGE_FLAG_PRELOAD;

	int numTex = 0;
	int nPageIndex = 0;

	do
	{
		if(!(g_texPageInfos[nPageIndex].flags & mode))
		{
			// goto next page
			nPageIndex++;

			// if we exceeded, switch mode and reset page index
			if(nPageIndex >= g_numTexPages)
			{
				nPageIndex = 0;
				mode = PAGE_FLAG_GLOBAL2;
			}

			continue;
		}

		// abort reading
		if(nPageIndex >= g_numTexPages)
			break;

		// preloaded textures are compressed
		LoadTexturePageData(pFile, &g_pageDatas[nPageIndex], nPageIndex, true);

		numTex++;
		nPageIndex++;

		if(pFile->Tell() % 4096)
			pFile->Seek(2048 - (pFile->Tell() % 2048),VS_SEEK_CUR);

	}while(pFile->Tell()-g_levInfo.texdata_offset < g_levInfo.texdata_size);

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
