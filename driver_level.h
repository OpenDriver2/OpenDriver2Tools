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

//----------------------------------------------------------

// extern some vars
extern IVirtualStream*			g_levStream;

extern TEXPAGEINFO*				g_texPageInfos;

extern int						g_numTexPages;
extern int						g_numTexDetail;

extern LEVELINFO				g_levInfo;
extern MAPINFO					g_mapInfo;

extern texdata_t*				g_pageDatas;
extern TexPage_t*				g_texPages;

extern std::vector<std::string>	g_model_names;
extern std::vector<std::string>	g_texture_names;

extern std::string				g_levname;

extern char*					g_textureNamesData;

extern std::vector<ModelRef_t>	g_levelModels;

extern regiondata_t*			g_regionDataDescs;	// region model/texture data descriptors
extern regionpages_t*			g_regionPages;		// region texpage usage table
extern RegionModels_t*			g_regionModels;		// cached region models
extern Spool*					g_regionSpool;		// region data info
extern ushort*					g_regionOffsets;	// region offset table

extern CELL_OBJECT*				g_straddlers;

extern int						g_numRegionDatas;
extern int						g_numRegionOffsets;
extern int						g_numRegionSpool;

//----------------------------------------------------------

MODEL*							FindModelByIndex(int nIndex, RegionModels_t* models);
int								GetModelIndexByName(const char* name);

#endif