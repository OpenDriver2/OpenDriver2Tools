#ifndef MODEL_H
#define MODEL_H

#include "dktypes.h"

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

typedef struct dface_t
{
	ubyte	flags;
	ubyte	page;
	uint16	detail;

	ubyte	vindex[4];
	ubyte	texcoord[4][2];
	ubyte	nindex[4];
	ubyte	color[4];
	// something more?
}FACE;

enum EFaceFlags_e
{
	FACE_IS_QUAD			= (1 << 0),
	FACE_RGB				= (1 << 1),	// this face has a color data
	FACE_TEXTURED			= (1 << 2),	// this face is textured and has custom texture coordinates
	FACE_TEXTURED2			= (1 << 3),	// this face is textured, and texcoords are taken from texture atlas

	FACE_HAS_VERTEXNORMAL	= (1 << 4),

	FACE_UNK6				= (1 << 5),
	FACE_UNK7				= (1 << 7),
	FACE_UNK8				= (1 << 8),
};

//------------------------------------------------------------------------------------------------------------

// model vertex
typedef struct dvertex_t
{
	short x;
	short y;
	short z;
	short w;
}SVERTEX;

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

typedef struct dmodel_t	// in original sources declared as MODEL (thanks Driver for iOS)
{
	short	collFlags;	// collision flags? ModelFlags1
	short	propFlags;	// property	flags? ModelFlags2
	short	vertex_ref;	// it points on other model if not negative
	short	unk3;
	int		unk4;

	short	numverts;
	short	numfaces;
	int		vertexOffset;
	int		faceOffset;

	int		faceOffset2;
	int		unk6;			// may be also boundingOffsets
	int		boundingsOffset;

	inline dvertex_t* pVertex(int i) const 
	{
		return (dvertex_t *)(((ubyte *)this) + vertexOffset) + i; 
	}

	inline char* pFaceAt(int ofs) const 
	{
		return (char *)(((ubyte *)this) + faceOffset + ofs); 
	}

	inline dbounding_t* pBoundingData() const 
	{
		if(!boundingsOffset)
			return NULL;

		return (dbounding_t *)(((ubyte *)this) + boundingsOffset); 
	}
}MODEL;

int decode_face(const char* face, dface_t* out);

//-------------------------------------------------------------------------------

// research function
void DumpFaceTypes();

#endif // MODEL_H