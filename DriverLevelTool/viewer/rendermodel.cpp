#include "driver_routines/models.h"
#include "rendermodel.h"

#include "driver_level.h"
#include "gl_renderer.h"
#include "core/cmdlib.h"
#include "util/DkList.h"
#include <assert.h>

CRenderModel::CRenderModel()
{
}

CRenderModel::~CRenderModel()
{
	Destroy();
}

bool CRenderModel::Initialize(ModelRef_t* model)
{
	if (!model)
		return false;

	if (!model->model)
		return false;
	
	m_sourceModel = model;
	GenerateBuffers();

	return true;
}

void CRenderModel::Destroy()
{
	GR_DestroyVAO(m_vao);
	m_vao = nullptr;
	m_sourceModel = nullptr;
	m_batches.clear();
}

struct vertexTuple_t
{
	int		flags;				// store few face flags here

	short	grVertexIndex;

	short	vertexIndex;
	short	normalIndex;		// point normal index
	ushort	uvs;
};

int FindGrVertexIndex(const DkList<vertexTuple_t>& whereFind, int flags, int vertexIndex, int normalIndex, ushort uvs)
{
	for(int i = 0; i < whereFind.numElem(); i++)
	{
		if (whereFind[i].flags != flags)
			continue;
		
		if (flags & FACE_VERT_NORMAL)
		{
			if (whereFind[i].normalIndex != normalIndex)
				continue;
		}

		if (flags & FACE_TEXTURED)
		{
			if (whereFind[i].uvs != uvs)
				continue;
		}

		if (whereFind[i].vertexIndex == vertexIndex)
			return whereFind[i].grVertexIndex;
	}

	return -1;
}

modelBatch_t* FindBatch(DkList<modelBatch_t*>& batches, int tpageId)
{
	for(int i = 0; i < batches.numElem(); i++)
	{
		if (batches[i]->tpage == tpageId)
			return batches[i];
	}
	return nullptr;
}

void CRenderModel::GenerateBuffers()
{
	DkList<vertexTuple_t>	verticesMap;
	
	DkList<GrVertex>		vertices;
	DkList<int>				indices;
	
	MODEL* model = m_sourceModel->model;
	MODEL* vertex_ref = model;

	if (model->instance_number > 0) // car models have vertex_ref=0
	{
		ModelRef_t* ref = FindModelByIndex(model->instance_number, m_regModelData);

		if (!ref)
		{
			Msg("vertex ref not found %d\n", model->instance_number);
			return;
		}

		vertex_ref = ref->model;
	}

	modelBatch_t* batch = nullptr;
	
	int modelSize = m_sourceModel->size;
	int face_ofs = 0;
	dpoly_t dec_face;

	// go throught all polygons
	for (int i = 0; i < model->num_polys; i++)
	{
		char* facedata = model->pPolyAt(face_ofs);

		// check offset
		if ((ubyte*)facedata >= (ubyte*)model + modelSize)
		{
			MsgError("poly id=%d type=%d ofs=%d bad offset!\n", i, *facedata & 31, model->poly_block + face_ofs);
			break;
		}

		int poly_size = decode_poly(facedata, &dec_face);

		// check poly size
		if (poly_size == 0)
		{
			MsgError("poly id=%d type=%d ofs=%d zero size!\n", i, *facedata & 31, model->poly_block + face_ofs);
			break;
		}	

		int numPolyVerts = (dec_face.flags & FACE_IS_QUAD) ? 4 : 3;
		bool bad_face = false;

		// perform vertex checks
		for (int v = 0; v < numPolyVerts; v++)
		{
			if (dec_face.vindices[v] >= vertex_ref->num_vertices)
			{
				bad_face = true;
				break;
			}

			// also check normals
			if (dec_face.flags & FACE_VERT_NORMAL)
			{
				if (dec_face.nindices[v] >= vertex_ref->num_point_normals)
				{
					bad_face = true;
					break;
				}
			}
		}

		if (bad_face)
		{
			MsgError("poly id=%d type=%d ofs=%d has invalid indices (or format is unknown)\n", i, *facedata & 31, model->poly_block + face_ofs);
			face_ofs += poly_size;
			continue;
		}

		// find or create new batch
		int tpageId = (dec_face.flags & FACE_TEXTURED) ? dec_face.page : -1;

		batch = FindBatch(m_batches, tpageId);
		if (!batch)
		{
			batch = new modelBatch_t;
			batch->startIndex = indices.numElem();
			batch->numIndices = 0;
			batch->tpage = tpageId;
	
			m_batches.append(batch);
		}
		
		// Gouraud-shaded poly smoothing
		bool smooth = (dec_face.flags & FACE_VERT_NORMAL);

		int faceIndices[4];

		// add vertices and generate faces
		for (int v = 0; v < numPolyVerts; v++)
		{
			// NOTE: Vertex indexes is reversed here
#define VERT_IDX numPolyVerts - 1 - v

			int vflags = dec_face.flags & ~(FACE_IS_QUAD | FACE_RGB);
			
			// try searching for vertex
			int index = FindGrVertexIndex(verticesMap, 
				vflags,
				dec_face.vindices[VERT_IDX], 
				dec_face.nindices[VERT_IDX], 
				*(ushort*)dec_face.uv[VERT_IDX]);

			// add new vertex
			if(index == -1)
			{
				GrVertex newVert;
				vertexTuple_t vertMap;

				vertMap.flags = vflags;
				vertMap.normalIndex = -1;
				
				// get the vertex
				SVECTOR* vert = vertex_ref->pVertex(dec_face.vindices[VERT_IDX]);
				
				(*(Vector3D*)&newVert.vx) = Vector3D(vert->x * -EXPORT_SCALING, vert->y * -EXPORT_SCALING, vert->z * EXPORT_SCALING);

				if (smooth)
				{
					vertMap.normalIndex = dec_face.nindices[VERT_IDX];
					
					SVECTOR* norm = vertex_ref->pPointNormal(vertMap.normalIndex);
					*(Vector3D*)&newVert.nx = Vector3D(norm->x * -EXPORT_SCALING, norm->y * -EXPORT_SCALING, norm->z * EXPORT_SCALING);
				}
				
				if (dec_face.flags & FACE_TEXTURED)
				{
					UV_INFO uv = *(UV_INFO*)dec_face.uv[VERT_IDX];

					// map to 0..1
					newVert.tc_u = float(uv.u / 2) / 128.0f;		// do /2 and *2 as textures were already 4 bit
					newVert.tc_v = float(uv.v) / 256.0f;
				}

				vertMap.grVertexIndex = vertices.append(newVert);
				vertMap.vertexIndex = dec_face.vindices[VERT_IDX];

				// add vertex and a map
				index = verticesMap.append(vertMap);

				// vertices and verticesMap should be equal
				_ASSERT(verticesMap.numElem() == vertices.numElem());
			}
			
			// add index
			faceIndices[v] = index;
		}

		// if not gouraud shaded we just compute face normal
		// FIXME: make it like game does?
		if(!smooth)
		{
			// it takes only triangle
			Vector3D v0 = *(Vector3D*)&vertices[faceIndices[0]].vx;
			Vector3D v1 = *(Vector3D*)&vertices[faceIndices[1]].vx;
			Vector3D v2 = *(Vector3D*)&vertices[faceIndices[2]].vx;
			
			Vector3D normal = normalize(cross(v2 - v1, v0 - v1));

			// set to each vertex
			for (int v = 0; v < numPolyVerts; v++)
				*(Vector3D*)&vertices[faceIndices[0]].nx = normal;
		}

		// triangulate quads
		if(numPolyVerts == 4)
		{
			indices.append(faceIndices[0]);
			indices.append(faceIndices[1]);
			indices.append(faceIndices[2]);

			indices.append(faceIndices[0]);
			indices.append(faceIndices[2]);
			indices.append(faceIndices[3]);
			batch->numIndices += 6;
		}
		else
		{
			indices.append(faceIndices[0]);
			indices.append(faceIndices[1]);
			indices.append(faceIndices[2]);
			batch->numIndices += 6;
		}

		face_ofs += poly_size;
	}
	
	m_vao = GR_CreateVAO(vertices.numElem(), indices.numElem(), vertices.ptr(), indices.ptr(), 0);

	if(!m_vao)
	{
		MsgError("Cannot create Model VAO!\n");
	}
}

void CRenderModel::Draw()
{
	extern TextureID g_hwTexturePages[128][32];
	extern ShaderID g_modelShader;
	
	GR_SetVAO(m_vao);
	
	for(int i = 0; i < m_batches.numElem(); i++)
	{
		modelBatch_t* batch = m_batches[i];
		GR_SetShader(g_modelShader);
		GR_SetTexture(g_hwTexturePages[batch->tpage][0]);
		GR_DrawIndexed(PRIM_TRIANGLES, batch->startIndex, batch->numIndices);
	}
}