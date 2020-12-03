//////////////////////////////////////////////////////////////////////////////////
// Description: Driver 2 LEVel main structures
//////////////////////////////////////////////////////////////////////////////////

#ifndef LEVEL_H
#define LEVEL_H

#include "math/dktypes.h"
#include "math/psx_math_types.h"
#include "d2_types.h"
#include <vector>

// known lumps indexes
#define LUMP_MODELS				1		// level models
#define LUMP_MAP				2		// map info

#define LUMP_TEXTURENAMES		5		// texture name strings

#define LUMP_ROADMAP			7		// unused lump in Driver 2
#define LUMP_ROADS				8		// unused lump in Driver 2
#define LUMP_JUNCTIONS			9		// unused lump in Driver 2
#define LUMP_ROADSURF			10		// unused lump in Driver 2

#define LUMP_MODELNAMES			12		// model name strings

#define LUMP_ROADBOUNDS			16		// unused lump in Driver 2
#define LUMP_JUNCBOUNDS			17		// unused lump in Driver 2

#define LUMP_SUBDIVISION		20		
#define LUMP_LOWDETAILTABLE		21		// LOD tables for models
#define LUMP_MOTIONCAPTURE		22		// motion capture/animation data for peds and Tanner
#define LUMP_OVERLAYMAP			24		// overlay map
#define LUMP_PALLET				25		// car palettes
#define LUMP_SPOOLINFO			26		// level region spooling
#define LUMP_CAR_MODELS			28		// car models

#define LUMP_CHAIR				33
#define LUMP_TEXTUREINFO		34		// texture page info and details (atlases)

#define LUMP_LEVELDESC			35
#define LUMP_LEVELDATA			36
#define LUMP_LUMPDESC			37

#define LUMP_STRAIGHTS2			40		// road straights (AI)
#define LUMP_CURVES2			41
#define LUMP_JUNCTIONS2			42

#define	LUMP_UNKNOWN			43		// unknown lump only appear in release Driver2

//------------------------------------------------------------------------------------------------------------
// globals

extern int g_region_format;
extern int g_format;

//------------------------------------------------------------------------------------------------------------
// functions

class IVirtualStream;
struct RegionModels_t;

void	LoadModelNamesLump(IVirtualStream* pFile, int size);
void	LoadTextureNamesLump(IVirtualStream* pFile, int size);

void	LoadLevelFile(const char* filename);
void	FreeLevelData();

#endif // LEVEL_H
