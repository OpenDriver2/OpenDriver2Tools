#ifndef DRIVER_LEVEL_H
#define DRIVER_LEVEL_H

#include <vector>
#include <string>

#include "level.h"

#include "models.h"
#include "textures.h"
#include "regions.h"

//----------------------------------------------------------

struct ModelRef_t
{
	ModelRef_t()
	{
		model = NULL;
		userData = NULL;
	}

	MODEL*	model;
	int		index;
	int		size;
	bool	swap;

	void*	userData;
};

struct RegionModels_t
{
	std::vector<ModelRef_t> modelRefs;
};

struct CarModelData_t
{
	MODEL* cleanmodel;
	MODEL* dammodel;
	MODEL* lowmodel;

	int cleanSize;
	int damSize;
	int lowSize;

	// TODO: denting and car params
};

extern CarModelData_t			g_carModels[MAX_CAR_MODELS];

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

extern ModelRef_t				g_levelModels[1536];

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

MODEL*							FindModelByIndex(int nIndex, RegionModels_t* models);
int								GetModelIndexByName(const char* name);

#endif