#ifndef DRIVER_LEVEL_H
#define DRIVER_LEVEL_H

#include "utils/dklist.h"
#include "utils/eqstring.h"

#include "level.h"

#include "models.h"
#include "textures.h"
#include "regions.h"

//----------------------------------------------------------

struct ModelRef_t
{
	MODEL*	model;
	int		index;
	int		size;
	bool	swap;
};

struct RegionModels_t
{
	DkList<ModelRef_t> modelRefs;
};

//----------------------------------------------------------

// extern some vars

extern TEXPAGEINFO*			g_texPageInfos;

extern int					g_numTexPages;
extern int					g_numTexDetail;

extern LEVELINFO			g_levInfo;

extern texdata_t*			g_pageDatas;
extern TexPage_t*			g_texPages;

extern DkList<EqString>		g_model_names;
extern DkList<EqString>		g_texture_names;
extern EqString				g_levname_moddir;
extern EqString				g_levname_texdir;

extern DkList<ModelRef_t>	g_models;

extern char*				g_textureNamesData;

#endif