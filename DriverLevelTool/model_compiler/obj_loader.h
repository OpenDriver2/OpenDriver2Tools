#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math/Vector.h"
#include "util/DkList.h"

// polygon data
struct smdpoly_t
{
	int vcount;
	int vindices[4];
	int tindices[4];
	int nindices[4];

	int smooth;		// if smooth is not off - uses nindices
};

// texture group
struct smdgroup_t
{
	smdgroup_t()
	{
		texture[0] = '\0';
	}

	char					texture[256];
	DkList<smdpoly_t>		polygons;
};

// simple polygon model
struct smdmodel_t
{
	char					name[64];

	DkList<Vector3D>		verts;
	DkList<Vector3D>		normals;
	DkList<Vector2D>		texcoords;
	
	DkList<smdgroup_t*>		groups;

	smdgroup_t*				FindGroupByName(const char* pszGroupname);
};

// Loads OBJ model, as DSM
bool LoadOBJ(smdmodel_t* model, const char* filename);
bool SaveOBJ(smdmodel_t* model, const char* filename);

#endif // DSM_OBJ_LOADER_H