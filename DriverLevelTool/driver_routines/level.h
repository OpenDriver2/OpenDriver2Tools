//////////////////////////////////////////////////////////////////////////////////
// Description: Driver 2 LEVel main structures
//////////////////////////////////////////////////////////////////////////////////

#ifndef LEVEL_H
#define LEVEL_H

// known lumps indexes
enum LevLumpType
{
	// known lumps indexes
	LUMP_MODELS = 1,			// level models
	LUMP_MAP = 2,				// map info

	LUMP_TEXTURENAMES = 5,		// texture name strings

	LUMP_ROADMAP = 7,			// unused lump in Driver 2
	LUMP_ROADS = 8,				// unused lump in Driver 2
	LUMP_JUNCTIONS = 9,			// unused lump in Driver 2
	LUMP_ROADSURF = 10,			// unused lump in Driver 2

	LUMP_MODELNAMES = 12,		// model name strings

	LUMP_ROADBOUNDS = 16,		// unused lump in Driver 2
	LUMP_JUNCBOUNDS = 17,		// unused lump in Driver 2

	LUMP_SUBDIVISION = 20,
	LUMP_LOWDETAILTABLE = 21,	// LOD tables for models
	LUMP_MOTIONCAPTURE = 22,	// motion capture/animation data for peds and Tanner
	LUMP_OVERLAYMAP = 24,		// overlay map
	LUMP_PALLET = 25,			// car palettes
	LUMP_SPOOLINFO = 26,		// level region spooling
	LUMP_CAR_MODELS = 28,		// car models

	LUMP_CHAIR = 33,
	LUMP_TEXTUREINFO = 34,		// texture page info and details (atlases)

	LUMP_LEVELDESC = 35,
	LUMP_LEVELDATA = 36,
	LUMP_LUMPDESC = 37,

	LUMP_STRAIGHTS2 = 40,		// road straights (AI)
	LUMP_CURVES2 = 41,

	LUMP_JUNCTIONS2 = 42,		// previously LUMP_JUNCTIONS2
	LUMP_JUNCTIONS2_NEW = 43,	// Only appear in release Driver2
};

enum ELevelFormat
{
	LEV_FORMAT_AUTODETECT = -1,

	LEV_FORMAT_DRIVER1 = 0,			// driver 1
	LEV_FORMAT_DRIVER2_ALPHA16,		// driver 2 alpha 1.6 format
	LEV_FORMAT_DRIVER2_RETAIL,		// driver 2 retail format
};

// forward
class IVirtualStream;
struct RegionModels_t;

//------------------------------------------------------------------------------------------------------------
// globals

extern ELevelFormat g_format;

//------------------------------------------------------------------------------------------------------------
// functions



void	LoadModelNamesLump(IVirtualStream* pFile, int size);

void	LoadLevelFile(const char* filename);
void	FreeLevelData();

#endif // LEVEL_H
