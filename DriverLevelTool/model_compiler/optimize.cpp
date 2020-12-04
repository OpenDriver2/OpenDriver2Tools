#include "obj_loader.h"
#include "core/cmdlib.h"

#define VERTEX_LINK_TOLERANCE 0.0001f

int FindVertexInRefList(const Vector3D& vert, const DkList<Vector3D>& newVerts, float tolerance)
{
	for (int i = 0; i < newVerts.numElem(); i++)
	{
		if (fsimilar(newVerts[i].x, vert.x, tolerance) &&
			fsimilar(newVerts[i].y, vert.y, tolerance) &&
			fsimilar(newVerts[i].z, vert.z, tolerance))
			return i;
	}

	return -1;
}

void OptimizeVectorArray(DkList<Vector3D>& targetArray, DkList<int>& index_remap)
{
	DkList<Vector3D> newVectors;

	int num_verts = targetArray.numElem();
	index_remap.setNum(num_verts);
	
	for (int i = 0; i < num_verts; i++)
	{
		Vector3D& vert = targetArray[i];
		int index = FindVertexInRefList(vert, newVectors, VERTEX_LINK_TOLERANCE);

		if (index == -1)
			index = newVectors.append(vert);

		index_remap[i] = index;
	}

	// replace with new list
	targetArray.swap(newVectors);
}

void OptimizeModel(smdmodel_t& model)
{
	MsgInfo("Optimizing model...\n");

	MsgWarning("Vertex count before: %d\n", model.verts.numElem());
	
	DkList<int> vertex_remap;
	OptimizeVectorArray(model.verts, vertex_remap);

	MsgWarning("\t\tafter: %d\n", model.verts.numElem());

	MsgWarning("Normal count before: %d\n", model.normals.numElem());

	DkList<int> normals_remap;
	OptimizeVectorArray(model.normals, normals_remap);

	MsgWarning("\t\tafter: %d\n", model.normals.numElem());

	MsgInfo("Remapping polys...");

	// now process all polys
	for (int i = 0; i < model.groups.numElem(); i++)
	{
		smdgroup_t* group = model.groups[i];
		
		for (int j = 0; j < group->polygons.numElem(); j++)
		{
			smdpoly_t& poly = group->polygons[j];
			
			for (int k = 0; k < poly.vcount; k++)
			{
				poly.vindices[k] = vertex_remap[poly.vindices[k]];
				poly.nindices[k] = normals_remap[poly.nindices[k]];
			}
		}
	}

	MsgInfo("DONE\n");
}