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

extern int g_format;

//----------------------------------------------------------

// extern some vars
extern IVirtualStream*			g_levStream;

extern TEXPAGE_POS*				g_texPagePos;

extern int						g_numTexPages;
extern int						g_numTexDetail;

extern char*					g_overlayMapData;

extern LEVELINFO				g_levInfo;
extern OUT_CELL_FILE_HEADER		g_mapInfo;

extern texdata_t*				g_pageDatas;
extern TexPage_t*				g_texPages;
extern extclutdata_t*			g_extraPalettes;
extern int						g_numExtraPalettes;

extern std::vector<std::string>	g_model_names;
extern std::vector<std::string>	g_texture_names;

extern std::string				g_levname;

extern char*					g_textureNamesData;

extern ModelRef_t				g_levelModels[MAX_MODELS];

extern CarModelData_t			g_carModels[MAX_CAR_MODELS];

extern AreaDataStr*				g_areaData;	// region model/texture data descriptors
extern AreaTPage_t*				g_regionPages;		// region texpage usage table
extern RegionModels_t*			g_regionModels;		// cached region models
extern Spool*					g_regionSpool;		// region data info
extern ushort*					g_spoolInfoOffsets;	// region offset table

extern void*					g_straddlers;
extern int						g_numStraddlers;

extern int						g_cell_slots_add[5];
extern int						g_cell_objects_add[5];
extern int						g_PVS_size[4];

extern int						g_numAreas;
extern int						g_numSpoolInfoOffsets;
extern int						g_numRegionSpools;

//----------------------------------------------------------

void							ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize);
void							WriteMODELToObjStream(IVirtualStream* pStream, MODEL* model, int model_index, const char* name_prefix,
									bool debugInfo = true,
									const Matrix4x4& translation = identity4(),
									int* first_v = NULL,
									int* first_t = NULL,
									RegionModels_t* regModels = NULL);

//----------------------------------------------------------
// main functions

void ExportAllModels();
void ExportAllCarModels();

void ExportRegions();

void ExportAllTextures();
void ExportOverlayMap();


#endif