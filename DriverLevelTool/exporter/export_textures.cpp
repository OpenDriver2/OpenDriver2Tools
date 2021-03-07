#include "driver_level.h"
#include "core/cmdlib.h"

#include "util/image.h"
#include "util/util.h"
#include "util/rnc2.h"

extern std::string			g_levname_moddir;
extern std::string			g_levname_texdir;
extern std::string			g_levname;

extern bool g_export_textures;
extern bool g_export_overmap;
extern int g_overlaymap_width;
extern bool g_explode_tpages;
extern bool g_export_world;
extern bool g_originalTransparencyKey = true;

int clutSortFunc(ExtClutData_t* const& i, ExtClutData_t* const& j)
{
	return (i->palette - j->palette);
}

void GetTPageDetailPalettes(DkList<TEXCLUT*>& out, int nPage, int detail)
{
	const TexBitmap_t& bitmap = g_texPages[nPage].GetBitmap();

	// ofc, add default
	out.append(&bitmap.clut[detail]);

	DkList<ExtClutData_t*> extra_cluts;

	for (int i = 0; i < g_numExtraPalettes; i++)
	{
		if (g_extraPalettes[i].tpage != nPage)
			continue;

		bool found = false;

		for (int j = 0; j < g_extraPalettes[i].texcnt; j++)
		{
			if (g_extraPalettes[i].texnum[j] == detail)
			{
				found = true;
				break;
			}
		}

		if (found)
			extra_cluts.append(&g_extraPalettes[i]);
	}

	extra_cluts.sort(clutSortFunc);

	for (int i = 0; i < extra_cluts.numElem(); i++)
		out.append(&extra_cluts[i]->clut);
}

//-------------------------------------------------------------
// writes 4-bit TIM image file from TPAGE
//-------------------------------------------------------------
void ExportTIM(int nPage, int detail)
{
	if (!(detail < g_texPages[nPage].GetDetailCount()))
	{
		MsgError("Cannot apply palette to non-existent detail! Programmer error?\n");
		return;
	}

	const TexBitmap_t& bitmap = g_texPages[nPage].GetBitmap();
	TEXINF* texdetail = g_texPages[nPage].GetTextureDetail(detail);
	
	int ox = texdetail->x;
	int oy = texdetail->y;
	int w = texdetail->width;
	int h = texdetail->height;

	if (w == 0)
		w = 256;

	if (h == 0)
		h = 256;

	DkList<TEXCLUT*> palettes;
	GetTPageDetailPalettes(palettes, nPage, detail);

	char* textureName = g_textureNamesData + texdetail->nameoffset;

	Msg("Saving %s' %d (xywh: %d %d %d %d)\n", textureName, detail, ox, oy, w, h);

	int tp_wx = ox + w;
	int tp_hy = oy + h;

	int half_w = (w >> 1);
	int img_size = (half_w + ((half_w & 1) ? 0 : 1)) * h;

	half_w -= half_w & 1;

	// copy image
	ubyte* image_data = new ubyte[img_size];
	TEXCLUT* clut_data = new TEXCLUT[palettes.numElem()];

	for (int y = oy; y < tp_hy; y++)
	{
		for (int x = ox; x < tp_wx; x++)
		{
			int px, py;
			px = (x - ox);
			py = (y - oy);

			int half_x = (x >> 1);
			int half_px = (px >> 1);
			//half_px -= half_px & 1;

			if (py * half_w + half_px < img_size)
			{
				image_data[py * half_w + half_px] = bitmap.data[y * 128 + half_x];
			}
			else
			{
				if (py * half_w > h)
					MsgWarning("Y offset exceeds %d (%d)\n", py, h);

				if (half_px > half_w)
					MsgWarning("X offset exceeds %d (%d)\n", half_px, half_w);
			}
		}
	}

	// copy cluts
	for (int i = 0; i < palettes.numElem(); i++)
	{
		memcpy(&clut_data[i], palettes[i], sizeof(TEXCLUT));
	}

	// compose TIMs
	SaveTIM_4bit(varargs("%s/PAGE_%d/%s_%d.TIM", g_levname_texdir.c_str(), nPage, textureName, detail), 
		image_data, img_size, ox, oy, w, h, 
		(ubyte*)clut_data, palettes.numElem() );

	delete[] image_data;
	delete[] clut_data;
}

//-------------------------------------------------------------
// Exports entire texture page
//-------------------------------------------------------------
void ExportTexturePage(int nPage)
{
	//
	// This is where texture is converted from paletted BGR5A1 to BGRA8
	// Also saving to LEVEL_texture/PAGE_*
	//

	const TexBitmap_t& bitmap = g_texPages[nPage].GetBitmap();

	if (!bitmap.data)
		return;		// NO DATA

	int numDetails = g_texPages[nPage].GetDetailCount();

	// Write an INI file with texture page info
	{
		FILE* pIniFile = fopen(varargs("%s/PAGE_%d.ini", g_levname_texdir.c_str(), nPage), "wb");

		if (pIniFile)
		{
			fprintf(pIniFile, "[tpage]\r\n");
			fprintf(pIniFile, "details=%d\r\n", numDetails);
			fprintf(pIniFile, "\r\n");

			for (int i = 0; i < numDetails; i++)
			{
				TEXINF* detail = g_texPages[nPage].GetTextureDetail(i);
				
				int x = detail->x;
				int y = detail->y;
				int w = detail->width;
				int h = detail->height;

				if (w == 0)
					w = 256;

				if (h == 0)
					h = 256;

				fprintf(pIniFile, "[detail_%d]\r\n", i);
				fprintf(pIniFile, "id=%d\r\n", detail->id);
				fprintf(pIniFile, "name=%s\r\n", g_textureNamesData + detail->nameoffset);
				fprintf(pIniFile, "xywh=%d,%d,%d,%d\r\n",x, y, w, h);
				fprintf(pIniFile, "\r\n");
			}

			fclose(pIniFile);
		}
	}

	if (g_explode_tpages)
	{
		MsgInfo("Exploding texture '%s/PAGE_%d'\n", g_levname_texdir.c_str(), nPage);

		// make folder and place all tims in there
		mkdirRecursive(varargs("%s/PAGE_%d", g_levname_texdir.c_str(), nPage));

		for (int i = 0; i < numDetails; i++)
		{
			ExportTIM(nPage, i);
		}

		return;
	}

#define TEX_CHANNELS 4

	int imgSize = TEXPAGE_SIZE * TEX_CHANNELS;

	uint* color_data = (uint*)malloc(imgSize);

	memset(color_data, 0, imgSize);

	// Dump whole TPAGE indexes
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = bitmap.data[y * 128 + (x >> 1)];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			int ypos = (TEXPAGE_SIZE_Y - y - 1) * TEXPAGE_SIZE_Y;

			color_data[ypos + x] = clindex * 32;
		}
	}

	for (int i = 0; i < numDetails; i++)
	{
		g_texPages[nPage].ConvertIndexedTextureToRGBA(color_data, i, &bitmap.clut[i], true, g_originalTransparencyKey);
	}

	MsgInfo("Writing texture '%s/PAGE_%d.tga'\n", g_levname_texdir.c_str(), nPage);
	SaveTGA(varargs("%s/PAGE_%d.tga", g_levname_texdir.c_str(), nPage), (ubyte*)color_data, TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, TEX_CHANNELS);

	int numPalettes = 0;
	for (int pal = 0; pal < 16; pal++)
	{
		bool anyMatched = false;
		for (int i = 0; i < g_numExtraPalettes; i++)
		{
			if (g_extraPalettes[i].tpage != nPage)
				continue;

			if (g_extraPalettes[i].palette != pal)
				continue;

			for (int j = 0; j < g_extraPalettes[i].texcnt; j++)
			{
				g_texPages[nPage].ConvertIndexedTextureToRGBA(color_data, g_extraPalettes[i].texnum[j], &g_extraPalettes[i].clut, true, g_originalTransparencyKey);
			}

			anyMatched = true;
		}

		if (anyMatched)
		{
			MsgInfo("Writing texture %s/PAGE_%d_%d.tga\n", g_levname_texdir.c_str(), nPage, numPalettes);
			SaveTGA(varargs("%s/PAGE_%d_%d.tga", g_levname_texdir.c_str(), nPage, numPalettes), (ubyte*)color_data, TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, TEX_CHANNELS);
			numPalettes++;
		}
	}

	free(color_data);
}

//-------------------------------------------------------------
// Exports all texture pages
//-------------------------------------------------------------
void ExportAllTextures()
{
	// preload region data if needed
	if (!g_export_world)
	{
		MsgInfo("Preloading region datas (%d)\n", g_numAreas);

		for (int i = 0; i < g_numAreas; i++)
		{
			LoadRegionData(g_levStream, nullptr, &g_areaData[i], &g_regionPages[i]);
		}
	}

	MsgInfo("Exporting texture data\n");
	for (int i = 0; i < g_numTexPages; i++)
	{
		ExportTexturePage(i);
	}

	// create material file
	FILE* pMtlFile = fopen(varargs("%s_LEVELMODEL.mtl", g_levname.c_str()), "wb");

	std::string justLevFilename = g_levname.substr(g_levname.find_last_of("/\\") + 1);

	size_t lastindex = justLevFilename.find_last_of(".");
	justLevFilename = justLevFilename.substr(0, lastindex);

	if (pMtlFile)
	{
		for (int i = 0; i < g_numTexPages; i++)
		{
			fprintf(pMtlFile, "newmtl page_%d\r\n", i);
			fprintf(pMtlFile, "map_Kd %s_textures/PAGE_%d.tga\r\n", justLevFilename.c_str(), i);
		}

		fclose(pMtlFile);
	}
}

//-------------------------------------------------------------
// converts and writes TGA file of overlay map
//-------------------------------------------------------------
void ExportOverlayMap()
{
	ushort* offsets = (ushort*)g_overlayMapData;

	char* mapBuffer = new char[32768];

	int numValid = 0;

	// max offset count for overlay map is 256, next is palette
	for (int i = 0; i < 256; i++)
	{
		char* rncData = g_overlayMapData + offsets[i];
		if (rncData[0] == 'R' && rncData[1] == 'N' && rncData[2] == 'C')
		{
			numValid++;
		}
		else
		{
			MsgWarning("overlay map segment count: %d\n", numValid);
			break;
		}
	}

	int overmapWidth = g_overlaymap_width * 32;
	int overmapHeight = (numValid / g_overlaymap_width) * 32;

	TVec4D<ubyte>* rgba = new TVec4D<ubyte>[overmapWidth * overmapHeight];

	ushort* clut = (ushort*)(g_overlayMapData + 512);

	int x, y, xx, yy;
	int wide, tall;

	wide = overmapWidth / 32;
	tall = overmapHeight / 32;

	int numTilesProcessed = 0;

	for (x = 0; x < wide; x++)
	{
		for (y = 0; y < tall; y++)
		{
			int idx = x + y * wide;

			UnpackRNC(g_overlayMapData + offsets[idx], mapBuffer);
			numTilesProcessed++;

			// place segment
			for (yy = 0; yy < 32; yy++)
			{
				for (xx = 0; xx < 32; xx++)
				{
					int px, py;

					ubyte colorIndex = (ubyte)mapBuffer[yy * 16 + xx / 2];

					if (0 != (xx & 1))
						colorIndex >>= 4;

					colorIndex &= 0xf;

					TVec4D<ubyte> color = bgr5a1_ToRGBA8(clut[colorIndex]);

					px = x * 32 + xx;
					py = (tall - 1 - y) * 32 + (31-yy);

					rgba[px + py * overmapWidth] = color;
				}
			}
		}
	}

	if(numTilesProcessed != numValid)
	{
		MsgWarning("Missed tiles: %d\n", numValid-numTilesProcessed);
	}

	SaveTGA(varargs("%s/MAP.tga", g_levname_texdir.c_str()), (ubyte*)rgba, overmapWidth, overmapHeight, TEX_CHANNELS);

	delete[] rgba;
	delete[] mapBuffer;
}