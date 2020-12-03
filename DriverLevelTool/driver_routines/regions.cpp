#include "regions.h"
#include "textures.h"

#include "core/IVirtualStream.h"
#include "core/cmdlib.h"

// loaded headers
LEVELINFO				g_levInfo;
OUT_CELL_FILE_HEADER	g_mapInfo;

AreaDataStr*		g_areaData = NULL;			// region model/texture data descriptors
AreaTPage_t*		g_regionPages = NULL;		// region texpage usage table
RegionModels_t*		g_regionModels = NULL;		// cached region models
Spool*				g_regionSpool = NULL;		// region data info
ushort*				g_spoolInfoOffsets = NULL;		// region offset table

int					g_numAreas = 0;
int					g_numSpoolInfoOffsets = 0;
int					g_numRegionSpools = 0;

void*				g_straddlers = NULL;		// cells that placed between regions (transition area)
int					g_numStraddlers = 0;

int					g_cell_slots_add[5] = { 0 };
int					g_cell_objects_add[5] = { 0 };
int					g_PVS_size[4] = { 0 };

//-------------------------------------------------------------
// parses LUMP_MAP and it's straddler objects
//-------------------------------------------------------------
void LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	pFile->Read(&g_mapInfo, 1, sizeof(OUT_CELL_FILE_HEADER));

	Msg("Level dimensions[%d %d], cell size: %d\n", g_mapInfo.cells_across, g_mapInfo.cells_down, g_mapInfo.cell_size);
	Msg(" - num_regions: %d\n", g_mapInfo.num_regions);
	Msg(" - region_size in cells: %d\n", g_mapInfo.region_size);

	int dim_x = g_mapInfo.cells_across / g_mapInfo.region_size;
	int dim_y = g_mapInfo.cells_down / g_mapInfo.region_size;

	Msg("World size:\n [%dx%d] cells\n [%dx%d] regions\n", g_mapInfo.cells_across, g_mapInfo.cells_down, dim_x, dim_y);
	
	Msg(" - num_cell_objects : %d\n", g_mapInfo.num_cell_objects);
	Msg(" - num_cell_data: %d\n", g_mapInfo.num_cell_data);

	// ProcessMapLump
	pFile->Read(&g_numStraddlers, 1, sizeof(g_numStraddlers));
	Msg(" - num straddler cells: %d\n", g_numStraddlers);

	const int pvs_square = 21;
	const int pvs_square_sq = pvs_square*pvs_square;

	// InitCellData actually here, but...

	// read straddlers
	if(g_format >= LEV_FORMAT_DRIVER2_ALPHA16)
	{
		// Driver 2 PCO
		g_straddlers = malloc(sizeof(PACKED_CELL_OBJECT)*g_numStraddlers);
		pFile->Read(g_straddlers, 1, sizeof(PACKED_CELL_OBJECT)*g_numStraddlers);
	}
	else
	{
		// Driver 1 CO
		g_straddlers = malloc(sizeof(CELL_OBJECT)*g_numStraddlers);
		pFile->Read(g_straddlers, 1, sizeof(CELL_OBJECT)*g_numStraddlers);
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//----------------------------------------------------------------------------------------

void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, AreaDataStr* data, AreaTPage_t* pages )
{
	int modelsCountOffset = g_levInfo.spooldata_offset + 2048 * (data->model_offset + data->model_size -1);
	int modelsOffset = g_levInfo.spooldata_offset + 2048 * data->model_offset;
	int texturesOffset = g_levInfo.spooldata_offset + 2048 * data->gfx_offset;

	pFile->Seek(texturesOffset, VS_SEEK_SET);

	// fetch textures
	for(int i = 0; pages->pageIndexes[i] != REGTEXPAGE_EMPTY; i++)
	{
		int pageIdx = pages->pageIndexes[i];

		// region textures are non-compressed due to loading speeds
		LoadTPageAndCluts(pFile, &g_pageDatas[pageIdx], pageIdx, false);

		if(pFile->Tell() % 2048)
			pFile->Seek(2048 - (pFile->Tell() % 2048),VS_SEEK_CUR);
	}

	if(!models) // don't read models
		return;

	// fetch models
	pFile->Seek(modelsCountOffset, VS_SEEK_SET);

	ushort numModels;
	pFile->Read(&numModels, 1, sizeof(ushort));
	MsgInfo("	model count: %d\n", numModels);

	// read model indexes
	short* model_indexes = new short[numModels];
	pFile->Read(model_indexes, numModels, sizeof(short));

	pFile->Seek(modelsOffset,VS_SEEK_SET);

	bool needModels = !models->modelRefs.size();

	for(int i = 0; i < numModels; i++)
	{
		int modelSize;
		pFile->Read(&modelSize, sizeof(int), 1);

		if(modelSize > 0)
		{
			char* data = (char*)malloc(modelSize);
			pFile->Read(data, modelSize, 1);

			int idx = model_indexes[i];

			if( needModels )
			{
				ModelRef_t ref;
				ref.index = idx;
				ref.model = (MODEL*)data;
				ref.size = modelSize;
				
				models->modelRefs.push_back(ref);
			}
		}
	}

	delete [] model_indexes;
}

//-------------------------------------------------------------
// parses LUMP_SPOOLINFO, and also loads region data
//-------------------------------------------------------------
void LoadSpoolInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int model_spool_buffer_size;
	pFile->Read(&model_spool_buffer_size, 1, sizeof(int));
	Msg("model_spool_buffer_size = %d * 2048\n", model_spool_buffer_size);

	int Music_And_AmbientOffsetsSize;
	pFile->Read(&Music_And_AmbientOffsetsSize, 1, sizeof(int));
	Msg("Music_And_AmbientOffsetsSize = %d\n", Music_And_AmbientOffsetsSize);

	// move further
	pFile->Seek(Music_And_AmbientOffsetsSize, VS_SEEK_CUR);

	pFile->Read(&g_numAreas, 1, sizeof(int));
	Msg("NumAreas = %d\n", g_numAreas);

	g_areaData = new AreaDataStr[g_numAreas];
	g_regionPages = new AreaTPage_t[g_numAreas];
	g_regionModels = new RegionModels_t[g_numAreas];

	// read local geometry and texture pages
	pFile->Read(g_areaData, g_numAreas, sizeof(AreaDataStr));
	pFile->Read(g_regionPages, g_numAreas, sizeof(AreaTPage_t));

	for (int i = 0; i < 5; i++)
	{
		g_cell_slots_add[i] = 0;
		g_cell_objects_add[i] = 0;
	}

	// read slots count
	for (int i = 0; i < 4; i++)
	{
		int slots_count;
		pFile->Read(&slots_count, 1, sizeof(int));
		g_cell_slots_add[i] = g_cell_slots_add[4];
		g_cell_slots_add[4] += slots_count;
	}

	// read objects count
	for (int i = 0; i < 4; i++)
	{
		int objects_count;
		pFile->Read(&objects_count, 1, sizeof(int));
		g_cell_objects_add[i] = g_cell_objects_add[4];
		g_cell_objects_add[4] += objects_count;
	}

	// read pvs sizes
	for (int i = 0; i < 4; i++)
	{
		int pvs_size;
		pFile->Read(&pvs_size, 1, sizeof(int));

		g_PVS_size[i] = pvs_size + 0x7ff & 0xfffff800;		
	}
 
	Msg("cell_slots_add = {%d,%d,%d,%d}\n", g_cell_slots_add[0], g_cell_slots_add[1], g_cell_slots_add[2], g_cell_slots_add[3]);
	Msg("cell_objects_add = {%d,%d,%d,%d}\n", g_cell_objects_add[0], g_cell_objects_add[1], g_cell_objects_add[2], g_cell_objects_add[3]);
	Msg("PVS_size = {%d,%d,%d,%d}\n", g_PVS_size[0], g_PVS_size[1], g_PVS_size[2], g_PVS_size[3]);

	// ... but InitCellData is here
	{
		int maxCellData = g_numStraddlers + g_cell_slots_add[4];
		Msg("*** MAX cell slots = %d\n", maxCellData);

		// I don't have any idea
		pFile->Read(&g_numSpoolInfoOffsets, 1, sizeof(int));
		Msg("numRegionOffsets: %d\n", g_numSpoolInfoOffsets);
	}

	g_spoolInfoOffsets = new ushort[g_numSpoolInfoOffsets];
	pFile->Read(g_spoolInfoOffsets, g_numSpoolInfoOffsets, sizeof(short));

	int regionsInfoSize;
	pFile->Read(&regionsInfoSize, 1, sizeof(int));
	g_numRegionSpools = regionsInfoSize / sizeof(AreaDataStr);

	//ASSERT(regionsInfoSize % sizeof(REGIONINFO) == 0);

	Msg("Region spool count %d (size=%d bytes)\n", g_numRegionSpools, regionsInfoSize);

	g_regionSpool = (Spool*)malloc(regionsInfoSize);
	pFile->Read(g_regionSpool, 1, regionsInfoSize);

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

void FreeSpoolData()
{
	if(g_straddlers)
		free(g_straddlers);
	g_straddlers = NULL;

	if(g_regionSpool)
		free(g_regionSpool);
	g_regionSpool = NULL;

	if(g_spoolInfoOffsets)
		delete [] g_spoolInfoOffsets;
	g_spoolInfoOffsets = NULL;

	if(g_regionPages)
		delete [] g_regionPages;
	g_regionPages = NULL;

	if(g_areaData)
		delete [] g_areaData;
	g_areaData = NULL;

	if(g_regionModels)
	{
		for(int i = 0; i < g_regionModels[i].modelRefs.size(); i++)
		{
			for(int j = 0; j < g_regionModels[i].modelRefs.size(); j++)
				free(g_regionModels[i].modelRefs[j].model);
		}

		delete [] g_regionModels;
		g_regionModels = NULL;
	}
}
