#ifndef REGIONS_H
#define REGIONS_H

#include "math/dktypes.h"
#include "models.h"
#include "util/DkList.h"

//------------------------------------------------------------------------------------------------------------

struct RegionModels_t
{
	DkList<ModelRef_t> modelRefs;
};

//-----------------------------------------------------------------------------------------

extern LEVELINFO				g_levInfo;
extern OUT_CELL_FILE_HEADER		g_mapInfo;

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

//-----------------------------------------------------------------------------------------

class IVirtualStream;

void LoadMapLump(IVirtualStream* pFile);
void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, AreaDataStr* data, AreaTPage_t* pages );
void LoadSpoolInfoLump(IVirtualStream* pFile);

void FreeSpoolData();

#endif // REGIONS_H
