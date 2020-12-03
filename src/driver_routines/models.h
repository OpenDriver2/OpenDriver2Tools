#ifndef MODEL_H
#define MODEL_H

#include "math/dktypes.h"

//-----------------------------------------------------------------

// oriented bbox
typedef struct dboundingbox_t
{
	short	type;

	short	position[3];
	short	rotation[3];
	short	size[3];
}BOUNDINGBOX;

typedef struct dbounding_t
{
	int numBounds;

	inline dboundingbox_t *pBox(int i) const 
	{
		return (dboundingbox_t *)(((ubyte *)this) + sizeof(dbounding_t)) + i; 
	}
}BOUNDINGDATA;

//------------------------------------------------------------------------------------------------------------

struct CVECTOR
{
	ubyte r;
	ubyte g;
	ubyte b;
	ubyte pad;
};


struct UV_INFO
{
	unsigned char u;
	unsigned char v;
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

typedef struct dpoly_t
{
	ubyte	flags;
	ubyte	page;
	ubyte	detail;

	ubyte	vindices[4];
	ubyte	uv[4][2];
	ubyte	nindices[4];
	CVECTOR	color[4];
	// something more?
}FACE;

enum EFaceFlags_e
{
	FACE_IS_QUAD			= (1 << 0),
	FACE_RGB				= (1 << 1),	// this face has a color data
	FACE_TEXTURED			= (1 << 2),	// face is textured
	FACE_VERT_NORMAL		= (1 << 3),
};

//------------------------------------------------------------------------------------------------------------

// model vertex
typedef struct dvertex_t
{
	short x;
	short y;
	short z;
	short w;
}SVECTOR;

enum ModelFlags1	// collision flags?
{
   everything    = 1,
   dontKnow0     = 2,
   walls         = 4,
   dontKnow1     = 8,
   dontKnow2     = 16,
   planarGround  = 32,
   dontKnow3     = 64,
   dontKnow4     = 128,
   dontKnow5     = 256,
   dontKnow6     = 512,
   ground        = 1024,
   dontKnow7     = 2048,
   dontKnow8     = 4096,
   street        = 8192,
   tree          = 16384,
   anythingToHit = 32768 // walls, poles, fences; but not trees or buildings
};

enum ModelFlags2	// effect flags?
{
   dontKnow9    = 1,     // empty in Chicago
   sandy        = 2,     // ??? lots of sandy stuff, but not everything
   dontKnow10   = 4,     // ??? (just one roof in Chicago)
   dontKnow11   = 8,     // ??? (empty in Chicago)
   dontKnow12   = 16,    // many sidewalks, some grass
   medianStrips = 32,
   crossings    = 64,
   dirt         = 128,   // lots but not everything
   dontKnow13   = 256,   // parking lot in Chicago
   chair        = 512,
   walls2       = 1024,  // don't know the difference to 'walls'
   destructable = 2048,
   dontKnow14   = 4096,  // ??? (empty in Chicago)
   greenTree    = 8192,  // don't know the difference to 'tree'
   dontKnow15   = 16384, // ??? again: many sidewalks, some grass
   sidewalk     = 32768
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

	dvertex_t* pVertex(int i) const 
	{
		return (dvertex_t *)(((ubyte *)this) + vertices) + i; 
	}

	dvertex_t* pNormal(int i) const 
	{
		return (dvertex_t *)(((ubyte *)this) + point_normals) + i; 
	}

	char* pPolyAt(int ofs) const 
	{
		return (char *)(((ubyte *)this) + poly_block + ofs); 
	}
};

assert_sizeof(MODEL, 36);

void PrintUnknownPolys();
int decode_poly(const char* face, dpoly_t* out);

//-------------------------------------------------------------------------------

// research function
void DumpFaceTypes();

#endif // MODEL_H
