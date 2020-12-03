#ifndef REGIONS_H
#define REGIONS_H

#include "math/dktypes.h"
#include "math/psx_math_types.h"
#include "models.h"
#include <vector>

//------------------------------------------------------------------------------------------------------------

struct CELL_DATA {
	ushort num; // size=0, offset=0
};
struct CELL_DATA_D1 {
	ushort num; // size=0, offset=0
	ushort next_ptr;
};

struct PACKED_CELL_OBJECT {
	USVECTOR_NOPAD			pos;
	ushort					value; // packed angle and model param
};

struct CELL_OBJECT {
	struct VECTOR_NOPAD		pos;
	ubyte					pad; // just to be aligned in PSX memory
	ubyte					yang;
	ushort					type;
};

struct AreaDataStr {
	uint16	gfx_offset;
	uint16	model_offset;
	uint16	music_offset;
	uint16	ambient_offset;
	uint8	model_size;

	uint8	pad;

	uint8	num_tpages;

	uint8	ambient_size;

	uint8	music_size;
	uint8	music_samples_size;
	uint8	music_id;

	uint8	ambient_id;
};

#define SUPERREGION_NONE	(0xFF)

// bundle texture page list. paired with regiondata_t
struct AreaTPage_t
{
	uint8	pageIndexes[16];
};

#define REGTEXPAGE_EMPTY	(0xFF)

struct Spool {
	uint16	offset;
	uint8	connected_areas[2];
	uint8	pvs_size;
	uint8	cell_data_size[3];
	uint8	super_region;
	uint8	num_connected_areas;
	uint8	roadm_size;
	uint8	roadh_size;
};

#define REGION_EMPTY	(0xFFFF)

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
