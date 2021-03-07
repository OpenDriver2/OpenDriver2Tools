#ifndef D2_TYPES
#define D2_TYPES

#include "math/dktypes.h"
#include "math/psx_math_types.h"

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
	TPAGE_PERMANENT = (1 << 0),		// permanently loaded into VRAM
	TPAGE_AREADATA = (1 << 1),		// spooled and uncompressed
	TPAGE_SPECPAGES = (1 << 2),		// special car texture page
};

typedef struct texpage_pos_t
{
	uint flags; // size=0, offset=0
	uint offset; // size=0, offset=4
}TEXPAGE_POS;	// originally TP

typedef struct dclut_t
{
	ushort colors[16];
}TEXCLUT;

//---------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------------------

// car models
#define MAX_CAR_MODELS		13

struct carmodelentry_t
{
	int cleanOffset;		// -1 if no model
	int damOffset;
	int lowOffset;
};

// FIXME: it's guessed
struct POLYF3
{
	unsigned char id;
	unsigned char v0;
	unsigned char v1;
	unsigned char v2;
	unsigned char pad;
	CVECTOR_NOPAD color;
	char pad2[4];
};

// FIXME: it's guessed
struct POLYF4
{
	unsigned char id; // 0
	unsigned char pad1;
	unsigned char pad2;
	unsigned char spare;
	unsigned char v0; //4
	unsigned char v1;
	unsigned char v2;
	unsigned char v3;
	CVECTOR_NOPAD color;
	char pad[5];
};

struct POLYFT3
{
	unsigned char id;
	unsigned char texture_set;
	unsigned char texture_id;
	unsigned char spare;
	unsigned char v0;
	unsigned char v1;
	unsigned char v2;
	unsigned char pad;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO pad2;
	CVECTOR color;
};

struct POLYGT3
{
	unsigned char id;
	unsigned char texture_set;
	unsigned char texture_id;
	unsigned char spare;
	unsigned char v0;
	unsigned char v1;
	unsigned char v2;
	unsigned char pad;
	unsigned char n0;
	unsigned char n1;
	unsigned char n2;
	unsigned char pad2;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO pad3;
	CVECTOR color;
};

struct POLYFT4
{
	unsigned char id;
	unsigned char texture_set;
	unsigned char texture_id;
	unsigned char spare;
	unsigned char v0;
	unsigned char v1;
	unsigned char v2;
	unsigned char v3;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	CVECTOR color;
};

struct POLYGT4
{
	unsigned char id;
	unsigned char texture_set;
	unsigned char texture_id;
	unsigned char spare;
	unsigned char v0;
	unsigned char v1;
	unsigned char v2;
	unsigned char v3;
	unsigned char n0;
	unsigned char n1;
	unsigned char n2;
	unsigned char n3;
	UV_INFO uv0;
	UV_INFO uv1;
	UV_INFO uv2;
	UV_INFO uv3;
	CVECTOR color;
};

enum ModelFlags1	// collision flags?
{
	everything = 1,
	dontKnow0 = 2,
	walls = 4,
	dontKnow1 = 8,
	dontKnow2 = 16,
	planarGround = 32,
	dontKnow3 = 64,
	dontKnow4 = 128,
	dontKnow5 = 256,
	dontKnow6 = 512,
	ground = 1024,
	dontKnow7 = 2048,
	dontKnow8 = 4096,
	street = 8192,
	tree = 16384,
	anythingToHit = 32768 // walls, poles, fences; but not trees or buildings
};

enum ModelFlags2	// effect flags?
{
	dontKnow9 = 1,     // empty in Chicago
	sandy = 2,     // ??? lots of sandy stuff, but not everything
	dontKnow10 = 4,     // ??? (just one roof in Chicago)
	dontKnow11 = 8,     // ??? (empty in Chicago)
	dontKnow12 = 16,    // many sidewalks, some grass
	medianStrips = 32,
	crossings = 64,
	dirt = 128,   // lots but not everything
	dontKnow13 = 256,   // parking lot in Chicago
	chair = 512,
	walls2 = 1024,  // don't know the difference to 'walls'
	destructable = 2048,
	dontKnow14 = 4096,  // ??? (empty in Chicago)
	greenTree = 8192,  // don't know the difference to 'tree'
	dontKnow15 = 16384, // ??? again: many sidewalks, some grass
	sidewalk = 32768
};

struct MODEL
{
	short	shape_flags;
	short	flags2;
	short	instance_number;

	ubyte	tri_verts;
	ubyte	zBias;

	short	bounding_sphere;
	short	num_point_normals;

	short	num_vertices;
	short	num_polys;

	int		vertices;
	int		poly_block;
	int		normals;
	int		point_normals;
	int		collision_block;

	SVECTOR* pVertex(int i) const
	{
		return (SVECTOR *)(((ubyte *)this) + vertices) + i;
	}

	SVECTOR* pNormal(int i) const
	{
		return (SVECTOR*)(((ubyte*)this) + normals) + i;
	}
	
	SVECTOR* pPointNormal(int i) const
	{
		return (SVECTOR *)(((ubyte *)this) + point_normals) + i;
	}

	char* pPolyAt(int ofs) const
	{
		return (char *)(((ubyte *)this) + poly_block + ofs);
	}
};

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

struct PALLET_INFO
{
	int palette;
	int texnum;
	int tpage;
	int clut_number;
};

struct PALLET_INFO_D1
{
	int palette;
	int texnum;
	int tpage;
};

#endif