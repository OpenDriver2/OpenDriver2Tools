#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math/Vector.h"
#include <nstd/Array.hpp>

// polygon data
struct smdpoly_t
{
	int vcount;
	int vindices[4];
	int tindices[4];
	int nindices[4];

	int smooth;		// if smooth is not off - uses nindices
	int flags;		// extra poly flags
	int extraData;
};

// texture group
struct smdgroup_t
{
	smdgroup_t()
	{
		name[0] = '\0';
		texture[0] = '\0';
	}

	char				name[256];
	char				texture[256];
	Array<smdpoly_t>	polygons;
};

// simple polygon model
struct smdmodel_t
{
	char					name[64];

	Array<Vector3D>		verts;
	Array<Vector3D>		normals;
	Array<Vector2D>		texcoords;
	
	Array<smdgroup_t*>	groups;

	smdgroup_t*			FindGroupByName(const char* pszGroupname);
};

// Loads OBJ model, as DSM
bool LoadOBJ(smdmodel_t* model, const char* filename);
void FreeOBJ(smdmodel_t* model);

#endif // DSM_OBJ_LOADER_H