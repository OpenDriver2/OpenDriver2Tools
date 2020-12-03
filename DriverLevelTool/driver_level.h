#ifndef DRIVER_LEVEL_H
#define DRIVER_LEVEL_H

#include <vector>
#include <string>

#include "driver_routines/level.h"
#include "driver_routines/models.h"
#include "driver_routines/textures.h"
#include "driver_routines/regions.h"

#include "math/Matrix.h"

//----------------------------------------------------------

#define ONE						(4096.0f)
#define EXPORT_SCALING			(1.0f / ONE)


//----------------------------------------------------------

// extern some vars
extern IVirtualStream*			g_levStream;

//----------------------------------------------------------

void							ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize);
void							WriteMODELToObjStream(IVirtualStream* pStream, MODEL* model, int modelSize, int model_index, const char* name_prefix,
									bool debugInfo = true,
									const Matrix4x4& translation = identity4(),
									int* first_v = NULL,
									int* first_t = NULL,
									RegionModels_t* regModels = NULL);

//----------------------------------------------------------
// main functions

void SaveModelPagesMTL();
void ExportAllModels();
void ExportAllCarModels();

void ExportRegions();

void ExportAllTextures();
void ExportOverlayMap();


#endif