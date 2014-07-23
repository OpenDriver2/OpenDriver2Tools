
#include "driver_level.h"

#include "IVirtualStream.h"
#include "Math/Vector.h"

#include "core_base_header.h"
#include "DebugInterface.h"

#include "ImageLoader.h"

ConVar g_export_textures("textures", "0");

//-------------------------------------------------------------------------------

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
	}while((pDstData-pDst) < TEXPAGE_COMPRESSED_SIZE);

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

int LoadTexturePageData(IVirtualStream* pFile, texdata_t* out, int nPage)
{
	int rStart = pFile->Tell();

	texturedata_t* data = new texturedata_t;
	pFile->Read( data, 1, sizeof(texturedata_t) );

	pFile->Seek( rStart, VS_SEEK_SET );

	pFile->Read( &out->numPalettes, 1, sizeof(int) );

	Msg("cluts: %d\n", out->numPalettes);

	if( out->numPalettes == 0 )
	{
		MsgInfo("non-palettized texture?\n");

		// alloc page
		out->size = TEXPAGE_COMPRESSED_SIZE;
		out->data = new ubyte[TEXPAGE_COMPRESSED_SIZE];

		pFile->Read(out->data, 1, TEXPAGE_COMPRESSED_SIZE);

		return out->numPalettes;
	}

	if(out->numPalettes > 0)
	{
		out->clut = new TEXCLUT[out->numPalettes];

		for(int i = 0; i < out->numPalettes; i++)
		{
			// read 16 palettes
			pFile->Read(&out->clut[i].colors, 1, sizeof(ushort)*16);
		}
	}

	int imageStart = pFile->Tell();

	// read compression data
	ubyte* compressedData = new ubyte[TEXPAGE_COMPRESSED_SIZE + 28];
	pFile->Read(compressedData, 1, TEXPAGE_COMPRESSED_SIZE + 28);

	// alloc page
	out->size = TEXPAGE_COMPRESSED_SIZE;
	out->data = new ubyte[out->size];

	// check for compression
	if(!compressedData[0] && !compressedData[1] && !compressedData[2])
	{
		Msg("non-compressed texture %d\n", g_texPageInfos[nPage].flags);

		// copy
		memcpy(out->data, data->texels, TEXPAGE_COMPRESSED_SIZE);
	}
	else
	{
		Msg("compressed texture\n");

		// Decompress the data. This requires padding as we don't know when the extraction finishes.
		ubyte* tempDecomp = new ubyte[TEXPAGE_COMPRESSED_SIZE];

		// unpack
		out->size = UnpackTexture(compressedData, tempDecomp);

		// seek to the right position
		pFile->Seek(imageStart + out->size, VS_SEEK_SET);

		// The compressed data was reversed. Revert to the correct order.
		for(int i = 0; i < 128 * 256; i++)
		{
			out->data[128 * 256 - 1 - i] = tempDecomp[i];
		}

		delete [] tempDecomp;
	}

	delete data;

	Msg("read %d bytes (total is %d)\n", out->size, pFile->Tell()-rStart);

	delete [] compressedData;

	//
	// This is where texture is converted from paletted BGR5A1 to BGRA8
	// Also saving to LEVEL_texture/PAGE_*
	//

	if(g_export_textures.GetBool())
	{
		// make indexed image
		ubyte* indexed = new ubyte[TEXPAGE_SIZE];
		for(int y = 0; y < 256; y++)
		{
			for(int x = 0; x < 256; x++)
			{
				// The color index from the file.
				ubyte colorIndex = out->data[y * 128 + x / 2];
				if(0 != (x & 1))
					colorIndex >>= 4;

				colorIndex = colorIndex & 0xF;

				indexed[y * 256 + x] = colorIndex;
			}
		}

		CImage img;
		uint* color_data = (uint*)img.Create(FORMAT_RGBA8, 256,256,1,1);

		int clut = max(out->numPalettes, g_texPages[nPage].numDetails);

		for(int i = 0; i < g_texPages[nPage].numDetails; i++, clut++)
		{
			int ox = g_texPages[nPage].details[i].x;
			int oy = g_texPages[nPage].details[i].y;
			int w = g_texPages[nPage].details[i].w;
			int h = g_texPages[nPage].details[i].h;

			char* name = g_textureNamesData + g_texPages[nPage].details[i].texNameOffset;
		
			/*
			Msg("Texture detail %d (%s) [%d %d %d %d]\n", i,name,
															g_texPages[nPage].details[i].x,
															g_texPages[nPage].details[i].y,
															g_texPages[nPage].details[i].w,
															g_texPages[nPage].details[i].h);
			*/
			for(int y = oy; y < oy+h; y++)
			{
				for(int x = ox; x < ox+w; x++)
				{
					ubyte clindex = indexed[y*256 + x];

					TVec4D<ubyte> color = bgr5a1_ToRGBA8( out->clut[i].colors[clindex] );

					color_data[y*256 + x] = *(uint*)(&color);
				}
			}
		}

		img.SaveTGA(varargs("%s/PAGE_%d.tga", g_levname_texdir.c_str(), nPage));
	
		delete [] indexed;
	}

	return out->numPalettes;
}

//
// loads global textures (pre-loading stage)
//
void LoadGlobalTextures(IVirtualStream* pFile)
{
	Msg("loading global texture pages\n");
	g_pageDatas = new texdata_t[g_numTexPages];

	int ofs = pFile->Tell();

	short mode = PAGE_FLAG_PRELOAD;

	int numTex = 0;

read_again:

	for(int i = 0; i < g_numTexPages; i++)
	{
		if(!((g_texPageInfos[i].flags & mode)))
			continue;

		Msg("--------\npage: %d flags=%p offset: %d (%d in file), unk=%d\n", i, g_texPageInfos[i].flags, g_texPageInfos[i].endoffset, pFile->Tell(), g_texPageInfos[i].unk1);

		// read data
		LoadTexturePageData(pFile, &g_pageDatas[i], i);

		if(pFile->Tell() % 2048)
			pFile->Seek(2048 - (pFile->Tell() % 2048),VS_SEEK_CUR);
	}

	
	if(mode == PAGE_FLAG_PRELOAD)
		mode = PAGE_FLAG_GLOBAL2;
	else
		return;
	
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
		//Msg("page flags=%d offset=%d\n", g_texPageInfos[i].flags, g_texPageInfos[i].endoffset);

		pFile->Read(&g_texPages[i].numDetails, sizeof(int), 1);

		//Msg("Texture page %d detail count: %d\n", i, g_texPages[i].numDetails);

		// read texture detail info
		g_texPages[i].details = new TEXTUREDETAIL[g_texPages[i].numDetails];
		pFile->Read(g_texPages[i].details, g_texPages[i].numDetails, sizeof(TEXTUREDETAIL));

		/*
		// dump textures
		for(int j = 0; j < g_texPages[i].numDetails; j++)
		{
			char* name = g_textureNamesData + g_texPages[i].details[j].texNameOffset;
			
			Msg("Texture detail %d (%s) [%d %d %d %d]\n", j,name,
															g_texPages[i].details[j].x,
															g_texPages[i].details[j].y,
															g_texPages[i].details[j].w,
															g_texPages[i].details[j].h);
		}*/
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}