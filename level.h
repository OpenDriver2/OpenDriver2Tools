//////////////////////////////////////////////////////////////////////////////////
// Description: Driver 2 LEVel main structures
//////////////////////////////////////////////////////////////////////////////////

#ifndef LEVEL_H
#define LEVEL_H

#include "dktypes.h"

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

typedef struct mapinfo_t // TODO: undone, for spool info for now
{
	int width;
	int height;

	int tileSize;
	int numRegions;
	int visTableWidth;

	int numAllObjects;
	int unk2;

	int ambientColor;
	int sunAngle[3];

	int numStraddlers;
}MAPINFO;

//------------------------------------------------------------------------------------------------------------

typedef struct dpacked_cell_t
{
	ushort		pos_x;
	short		addidx_pos_y;
	ushort		pos_z;

	ushort		rotation_model;	// 6 bit rotation and 10 bit model index
}PACKED_CELL_OBJECT;

typedef struct dcell_t
{
	ushort		x;
	short		y;
	ushort		z;

	ushort		rotation;
	ushort		modelindex;		// max 2048

	// TODO: visibility info
}CELL_OBJECT;

inline void UnpackCellObject(const PACKED_CELL_OBJECT& object, CELL_OBJECT& cell)
{
	cell.x = object.pos_x;
	cell.y = object.addidx_pos_y >> 1;
	cell.z = object.pos_z;

	// unpack
	cell.rotation = (object.rotation_model & 0x3f);
	cell.modelindex = (object.rotation_model >> 6) + ((object.addidx_pos_y & 0x1) ? 1024 : 0);
}

// region data info
struct regiondata_t
{
	ushort	textureOffset;	// where additional textures placed (*2048)
	ushort	modelsOffset;	// where models placed (*2048)
	ushort	unknown;
	ushort	unknown2;
	uint8	modelsSize;		// size of models
	uint8	unk2;
	uint8	numberOfTextures;
	uint8	unk3;
	ushort	unk4[2];
};

#define REGDATA_EMPTY	(0xFF)

// bundle texture page list. paired with regiondata_t
struct regionpages_t
{
	uint8	pageIndexes[16];
};

#define REGTEXPAGE_EMPTY	(0xFF)

// sector info
typedef struct regioninfo_t
{
	ushort	offset;
	ubyte	unk1;
	ubyte	unk2;
	ubyte	cellsSize;	// always zero in retail Driver 2, in Driver 1/Driver 2 demo not zero
	uint8	contentsNodesSize;
	uint8	contentsTableSize;
	uint8	modelsSize;
	uint8	dataIndex;
	uint8	unk3;
	uint8	unk4;
	uint8	unk5;
}REGIONINFO;

#define REGION_EMPTY	(0xFFFF)

//------------------------------------------------------------------------------------------------------------

// car models
#define MAX_CAR_MODELS		13

struct carmodelentry_t
{
	int cleanOffset;		// -1 if no model
	int damagedOffset;
	int lowOffset;
};

//------------------------------------------------------------------------------------------------------------

#define TEXPAGE_SIZE_X	(128)
#define TEXPAGE_SIZE_Y	(256)

#define TEXPAGE_COMPRESSED_SIZE (TEXPAGE_SIZE_X*TEXPAGE_SIZE_Y)
#define TEXPAGE_SIZE			(TEXPAGE_SIZE_Y*TEXPAGE_SIZE_Y)

typedef struct texturedetail_t
{
	short	editorIndex;		// unknown thing
	short	texNameOffset;		// texture name offset in LUMP_TEXTURENAMES data (for fast searching)

	uint8	x;
	uint8	y;
	uint8	w;
	uint8	h;
}TEXTUREDETAIL;

enum EPageStorage
{
	PAGE_FLAG_PRELOAD	= (1 << 0),		// pre-loaded page
	PAGE_FLAG_REGION	= (1 << 1),		// demand-loaded
	PAGE_FLAG_GLOBAL2	= (1 << 2),		// pre-loaded page (really unknown for me)
	PAGE_FLAG_UNK1		= (1 << 3),
};

typedef struct texpageinfo_t
{
	short	flags;			// EPageStorage
	short	unk1;
	int		endoffset;			// really offset?
}TEXPAGEINFO;

typedef struct dclut_t
{
	ushort colors[16];
}TEXCLUT;

//------------------------------------------------------------------------------------------------------------

#endif // LEVEL_H