#include "regions.h"
#include "textures.h"

#include "IVirtualStream.h"
#include "DebugInterface.h"

//----------------------------------------------------------------------------------------

void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, regiondata_t* data, regionpages_t* pages )
{
	int modelsCountOffset = g_levInfo.spooldata_offset + 2048 * (data->modelsOffset + data->modelsSize-1);

	int modelsOffset = g_levInfo.spooldata_offset + 2048 * data->modelsOffset;

	int texturesOffset = g_levInfo.spooldata_offset + 2048 * data->textureOffset;

	pFile->Seek(texturesOffset, VS_SEEK_SET);

	// try load textures
	for(int i = 0; pages->pageIndexes[i] != REGTEXPAGE_EMPTY; i++)
	{
		// load texture pages
		LoadTexturePageData(pFile, &g_pageDatas[pages->pageIndexes[i]], pages->pageIndexes[i]);

		if(pFile->Tell() % 2048)
			pFile->Seek(2048 - (pFile->Tell() % 2048),VS_SEEK_CUR);
	}

	// we try to load models
	pFile->Seek(modelsCountOffset, VS_SEEK_SET);

	ushort numModels;
	pFile->Read(&numModels, 1, sizeof(ushort));
	MsgInfo("	model count: %d\n", numModels);

	// read model indexes
	short* model_indexes = new short[numModels];
	pFile->Read(model_indexes, numModels, sizeof(short));

	pFile->Seek(modelsOffset,VS_SEEK_SET);

	bool needModels = !models->modelRefs.numElem();

	for(int i = 0; i < numModels; i++)
	{
		int modelSize;
		pFile->Read(&modelSize, sizeof(int), 1);

		if(modelSize > 0)
		{
			char* data = (char*)malloc(modelSize);
			pFile->Read(data, modelSize, 1);

			int idx = model_indexes[i];

			if(needModels)
			{
				ModelRef_t ref;
				ref.index = idx;
				ref.model = (MODEL*)data;
				ref.size = modelSize;
				
				models->modelRefs.append(ref);
			}

			// replace model globally
			if(g_models[idx].model)
			{
				ASSERT(g_models[idx].swap); // assume that empty models are used only for swapping

				g_models[idx].model = NULL;
			}

			g_models[idx].model = (MODEL*)data;
				
		}
	}
}