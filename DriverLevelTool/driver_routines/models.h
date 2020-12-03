#ifndef MODEL_H
#define MODEL_H

#include "math/dktypes.h"
#include "math/psx_math_types.h"
#include "d2_types.h"


//------------------------------------------------------------------------------------------------------------

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

struct ModelRef_t
{
	ModelRef_t()
	{
		model = NULL;
		userData = NULL;
	}

	MODEL*	model;
	int		index;
	int		size;
	bool	swap;

	void*	userData;
};

//------------------------------------------------------------------------------------------------------------

#define MAX_MODELS				1536	// maximum models (this is limited by PACKED_CELL_OBJECT)

struct CarModelData_t
{
	MODEL* cleanmodel;
	MODEL* dammodel;
	MODEL* lowmodel;

	int cleanSize;
	int damSize;
	int lowSize;
};


extern ModelRef_t				g_levelModels[MAX_MODELS];
extern CarModelData_t			g_carModels[MAX_CAR_MODELS];

//------------------------------------------------------------------------------------------------------------

struct RegionModels_t;
MODEL*	FindModelByIndex(int nIndex, RegionModels_t* models);
int		GetModelIndexByName(const char* name);

void PrintUnknownPolys();
int decode_poly(const char* face, dpoly_t* out);

//-------------------------------------------------------------------------------

// research function
void DumpFaceTypes();

#endif // MODEL_H
