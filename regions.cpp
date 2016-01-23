#include "regions.h"
#include "textures.h"

#include "core/IVirtualStream.h"
#include "core/cmdlib.h"

// loaded headers
LEVELINFO			g_levInfo;
MAPINFO				g_mapInfo;

regiondata_t*		g_regionDataDescs = NULL;	// region model/texture data descriptors
regionpages_t*		g_regionPages = NULL;		// region texpage usage table
RegionModels_t*		g_regionModels = NULL;		// cached region models
Spool*				g_regionSpool = NULL;		// region data info
ushort*				g_regionOffsets = NULL;		// region offset table

int					g_numRegionDatas = 0;
int					g_numRegionOffsets = 0;
int					g_numRegionSpool = 0;

CELL_OBJECT*		g_straddlers = NULL;		// cells that placed between regions (transition area)

//-------------------------------------------------------------
// parses LUMP_MAP and it's straddler objects
//-------------------------------------------------------------
void LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	pFile->Read(&g_mapInfo, 1, sizeof(MAPINFO));

	Msg("Level dimensions[%d %d], tile size: %d\n", g_mapInfo.width, g_mapInfo.height, g_mapInfo.tileSize);
	Msg("numRegions: %d\n", g_mapInfo.numRegions);
	Msg("visTableWidth: %d\n", g_mapInfo.visTableWidth);

	Msg("numAllObjects : %d\n", g_mapInfo.numAllObjects);
	Msg("NumStraddlers: %d\n", g_mapInfo.numStraddlers);

	g_straddlers = new CELL_OBJECT[g_mapInfo.numStraddlers];
	
	// straddler objects are placed where region transition comes
	// FIXME: HOW to use them in regions?
	for(int i = 0; i < g_mapInfo.numStraddlers; i++)
	{
		PACKED_CELL_OBJECT object;
		
		pFile->Read(&object, 1, sizeof(PACKED_CELL_OBJECT));

		UnpackCellObject(object, g_straddlers[i]);
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//----------------------------------------------------------------------------------------

void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, regiondata_t* data, regionpages_t* pages )
{
	int modelsCountOffset = g_levInfo.spooldata_offset + 2048 * (data->modelsOffset + data->modelsSize-1);
	int modelsOffset = g_levInfo.spooldata_offset + 2048 * data->modelsOffset;
	int texturesOffset = g_levInfo.spooldata_offset + 2048 * data->textureOffset;

	pFile->Seek(texturesOffset, VS_SEEK_SET);

	// fetch textures
	for(int i = 0; pages->pageIndexes[i] != REGTEXPAGE_EMPTY; i++)
	{
		int pageIdx = pages->pageIndexes[i];

		// region textures are non-compressed due to loading speeds
		LoadTexturePageData(pFile, &g_pageDatas[pageIdx], pageIdx, false);

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

	int unk;
	pFile->Read(&unk, 1, sizeof(int));

	Msg("unk = %d\n", unk);

	int numSomething; // i think it's always 32
	pFile->Read(&numSomething, 1, sizeof(int));

	Msg("numSomething = %d\n", numSomething);
	pFile->Seek(numSomething, VS_SEEK_CUR);
	
	pFile->Read(&g_numRegionDatas, 1, sizeof(int));

	Msg("numRegionDatas = %d\n", g_numRegionDatas);
	g_regionModels = new RegionModels_t[g_numRegionDatas];

	g_regionDataDescs = new regiondata_t[g_numRegionDatas];
	g_regionPages = new regionpages_t[g_numRegionDatas];

	// read local geometry and texture pages
	pFile->Read(g_regionDataDescs, g_numRegionDatas, sizeof(regiondata_t));
	pFile->Read(g_regionPages, g_numRegionDatas, sizeof(regionpages_t));

	// something decompled, 48 bytes
	struct V17Data
	{
		int a[4];
		int b[4];
		int c[4];
	};

	V17Data unknStruct;

	pFile->Read(&unknStruct, 1, sizeof(unknStruct));

	Msg("unk a = [%d %d %d %d]\n", unknStruct.a[0], unknStruct.a[1], unknStruct.a[2], unknStruct.a[3]);
	Msg("unk b = [%d %d %d %d]\n", unknStruct.b[0], unknStruct.b[1], unknStruct.b[2], unknStruct.b[3]);
	Msg("unk c = [%d %d %d %d]\n", unknStruct.c[0]+2047 & -2048, unknStruct.c[1]+2047 & -2048, unknStruct.c[2]+2047 & -2048, unknStruct.c[3]+2047 & -2048);

	pFile->Read(&g_numRegionOffsets, 1, sizeof(int));
	Msg("numRegionOffsets: %d\n", g_numRegionOffsets);

	g_regionOffsets = new ushort[g_numRegionOffsets];
	pFile->Read(g_regionOffsets, g_numRegionOffsets, sizeof(short));

	int regionsInfoSize;
	pFile->Read(&regionsInfoSize, 1, sizeof(int));
	g_numRegionSpool = regionsInfoSize/sizeof(REGIONINFO);

	//ASSERT(regionsInfoSize % sizeof(REGIONINFO) == 0);

	Msg("Region spool count %d (size=%d bytes)\n", g_numRegionSpool, regionsInfoSize);

	g_regionSpool = (Spool*)malloc(regionsInfoSize);
	pFile->Read(g_regionSpool, 1, regionsInfoSize);

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

void FreeSpoolData()
{
	if(g_straddlers)
		delete [] g_straddlers;
	g_straddlers = NULL;

	if(g_regionSpool)
		free(g_regionSpool);
	g_regionSpool = NULL;

	if(g_regionOffsets)
		delete [] g_regionOffsets;
	g_regionOffsets = NULL;

	if(g_regionPages)
		delete [] g_regionPages;
	g_regionPages = NULL;

	if(g_regionDataDescs)
		delete [] g_regionDataDescs;
	g_regionDataDescs = NULL;

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
