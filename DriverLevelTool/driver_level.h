#ifndef DRIVER_LEVEL_H
#define DRIVER_LEVEL_H

#include "driver_routines/models.h"
#include "driver_routines/textures.h"
#include "driver_routines/regions.h"
#include "driver_routines/level.h"

#include "math/Matrix.h"

//----------------------------------------------------------

#define EXPORT_SCALING			(1.0f / ONE_F)

//----------------------------------------------------------

// extern some vars
extern OUT_CITYLUMP_INFO		g_levInfo;
extern CDriverLevelTextures		g_levTextures;
extern CDriverLevelModels		g_levModels;
extern CBaseLevelMap*			g_levMap;

//----------------------------------------------------------

void	ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize);
void	WriteMODELToObjStream(IVirtualStream* pStream, MODEL* model, int modelSize, int model_index, const char* name_prefix,
			bool debugInfo = true,
			const Matrix4x4& translation = identity4(),
			int* first_v = nullptr,
			int* first_t = nullptr);

//----------------------------------------------------------
// main functions

void SaveModelPagesMTL();
void ExportAllModels();
void ExportAllCarModels();

void ExportRegions();

void ExportAllTextures();
void ExportOverlayMap();

#endif