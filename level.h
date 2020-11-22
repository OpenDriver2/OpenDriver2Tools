//////////////////////////////////////////////////////////////////////////////////
// Description: Driver 2 LEVel main structures
//////////////////////////////////////////////////////////////////////////////////

#ifndef LEVEL_H
#define LEVEL_H

#include "math/dktypes.h"
#include "psx_math_types.h"

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

//-----------------------------------------------------------------

#define MAX_MODELS				2048	// maximum models (this is limited by PACKED_CELL_OBJECT)

//-----------------------------------------------------------------

typedef struct slump_t
{
	uint	type;
    int		size;
}LUMP;

typedef struct dlevinfo_t
{
	uint	levdesc_offset;
	uint	levdesc_size;

	uint	texdata_offset;
	uint	texdata_size;

	uint	levdata_offset;
	uint	levdata_size;

	uint	spooldata_offset;
	uint	spooldata_size;
}LEVELINFO;

//------------------------------------------------------------------------------------------------------------

struct OUT_CELL_FILE_HEADER {
	int cells_across; // size=0, offset=0
	int cells_down; // size=0, offset=4
	int cell_size; // size=0, offset=8

	int num_regions; // size=0, offset=12
	int region_size; // size=0, offset=16

	int num_cell_objects; // size=0, offset=20
	int num_cell_data; // size=0, offset=24

	int ambient_light_level; // size=0, offset=28
	struct VECTOR_NOPAD light_source; // size=12, offset=32
};

//assert_sizeof(OUT_CELL_FILE_HEADER, 48);

//------------------------------------------------------------------------------------------------------------

struct CELL_DATA {
	ushort num; // size=0, offset=0
};
struct CELL_DATA_D1 {
	ushort num; // size=0, offset=0
	ushort next_ptr;
};

struct PACKED_CELL_OBJECT {
	struct USVECTOR_NOPAD	pos;
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

//------------------------------------------------------------------------------------------------------------

// car models
#define MAX_CAR_MODELS		13

struct carmodelentry_t
{
	int cleanOffset;		// -1 if no model
	int damOffset;
	int lowOffset;
};

//------------------------------------------------------------------------------------------------------------

#define TEXPAGE_SIZE_X	(128)
#define TEXPAGE_SIZE_Y	(256)

#define TEXPAGE_4BIT_SIZE	(TEXPAGE_SIZE_X*TEXPAGE_SIZE_Y)
#define TEXPAGE_SIZE		(TEXPAGE_SIZE_Y*TEXPAGE_SIZE_Y)

struct TEXINF {
	uint16		id;
	uint16		nameoffset;
	uint8		x;
	uint8		y;
	uint8		width; 
	uint8		height;
};

enum EPageStorage
{
	TPAGE_PERMANENT		= (1 << 0),		// pre-loaded page.
	TPAGE_AREADATA	= (1 << 1),		// demand-loaded
	TPAGE_SPECPAGES		= (1 << 2),		// pre-loaded page  However, it doesn't indicate texture compression
	PAGE_FLAG_UNK1		= (1 << 3),
	PAGE_FLAG_UNK2		= (1 << 4),
};

typedef struct texpage_pos_t
{
	unsigned long flags; // size=0, offset=0
	unsigned long offset; // size=0, offset=4
}TEXPAGE_POS;	// originally TP

typedef struct dclut_t
{
	ushort colors[16];
}TEXCLUT;

//------------------------------------------------------------------------------------------------------------

// functions

class IVirtualStream;

int GetCarPalIndex(int tpage);

void LoadModelNamesLump(IVirtualStream* pFile, int size);
void LoadTextureNamesLump(IVirtualStream* pFile, int size);

void LoadLevelFile(const char* filename);
void FreeLevelData();

#endif // LEVEL_H
