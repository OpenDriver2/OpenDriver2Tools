#ifndef COMPILER_H
#define COMPILER_H

#include "obj_loader.h"
#include "math/psx_math_types.h"

enum EPolyExtraFlags
{
	POLY_EXTRA_DENTING = (1 << 0),
	POLY_DAMAGE_SPLIT = (1 << 1)
};

enum DamageZoneIds
{
	ZONE_FRONT_LEFT = 0,
	ZONE_FRONT_RIGHT = 1,

	ZONE_SIDE_LEFT = 5,
	ZONE_SIDE_RIGHT = 2,
	
	ZONE_REAR_LEFT = 4,
	ZONE_REAR_RIGHT = 3,

	// multi-types
	ZONE_FRONT = 6,
	ZONE_REAR = 7,
	ZONE_SIDE = 8,

	NUM_ZONE_TYPES,		// DO NOT USE when saving denting!
};

static const char* s_DamageZoneNames[] = {
	"front_left",
	"front_right",
	"side_left",
	"side_right",
	"rear_left",
	"rear_right",

	"front",
	"rear",
	"side"
};

//----------------------------------------------------------

void ConvertVertexToDriver(SVECTOR* dest, Vector3D* src);
void OptimizeModel(smdmodel_t& model);
void GenerateDenting(smdmodel_t& model, const char* outputName);

//----------------------------------------------------------

void CompileOBJModelToMDL(const char* filename, const char* outputName, bool generate_denting);

#endif