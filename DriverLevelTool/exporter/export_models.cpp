#include <stdio.h>

#include "driver_routines/models.h"
#include "util/util.h"
#include "util/DkList.h"
#include "core/VirtualStream.h"
#include <string>

#include "driver_level.h"
#include "math/Matrix.h"

extern bool						g_extract_dmodels;
extern std::string				g_levname_moddir;
extern std::string				g_levname_texdir;

//-------------------------------------------------------------
// writes Wavefront OBJ into stream
//-------------------------------------------------------------
void WriteMODELToObjStream(IVirtualStream* pStream, MODEL* model, int modelSize, int model_index, const char* name_prefix,
	bool debugInfo,
	const Matrix4x4& translation,
	int* first_v,
	int* first_t,
	RegionModels_t* regModels)
{
	if (!model)
	{
		MsgError("no model %d!!!\n", model_index);
		return;
	}

	// export OBJ with points
	if (debugInfo)
		pStream->Print("#vert count %d\r\n", model->num_vertices);

	pStream->Print("g %s_%d\r\n", name_prefix, model_index);
	pStream->Print("o %s_%d\r\n", name_prefix, model_index);

	MODEL* vertex_ref = model;

	if (model->instance_number > 0) // car models have vertex_ref=0
	{
		pStream->Print("#vertex data ref model: %d (count = %d)\r\n", model->instance_number, model->num_vertices);

		ModelRef_t* ref = g_levModels.GetModelByIndex(model->instance_number);//, regModels);

		if (!ref)
		{
			Msg("vertex ref not found %d\n", model->instance_number);
			return;
		}

		vertex_ref = ref->model;
	}

	// store vertices
	for (int i = 0; i < vertex_ref->num_vertices; i++)
	{
		SVECTOR* vert = vertex_ref->pVertex(i);
		Vector3D sfVert = Vector3D(vert->x * -EXPORT_SCALING, vert->y * -EXPORT_SCALING, vert->z * EXPORT_SCALING);

		sfVert = (translation * Vector4D(sfVert, 1.0f)).xyz();

		pStream->Print("v %g %g %g\r\n", 
			sfVert.x,
			sfVert.y,
			sfVert.z);
	}

	// store GT3/GT4 vertex normals
	for (int i = 0; i < vertex_ref->num_point_normals; i++)
	{
		SVECTOR* norm = vertex_ref->pPointNormal(i);
		Vector3D sfNorm = Vector3D(norm->x * -EXPORT_SCALING, norm->y * -EXPORT_SCALING, norm->z * EXPORT_SCALING);

		pStream->Print("vn %g %g %g\r\n", 
			sfNorm.x,
			sfNorm.y,
			sfNorm.z);
	}

	int face_ofs = 0;

	if (debugInfo)
	{
		pStream->Print("#poly ofs %d\r\n", model->poly_block);
		pStream->Print("#poly count %d\r\n", model->num_polys);
	}

	pStream->Print("usemtl none\r\n");

	char formatted_vertex[512];
	int numVertCoords = 0;
	int numVerts = 0;

	if (first_t)
		numVertCoords = *first_t;

	if (first_v)
		numVerts = *first_v;

	bool prevSmooth = false;
	int prev_tpage = -1;
	dpoly_t dec_face;

	// go throught all polygons
	for (int i = 0; i < model->num_polys; i++)
	{
		char* facedata = model->pPolyAt(face_ofs);

		// check offset
		if((ubyte*)facedata >= (ubyte*)model+modelSize)
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
		
		if (debugInfo)
			pStream->Print("# ft=%d ofs=%d size=%d\r\n", *facedata & 31, model->poly_block + face_ofs, poly_size);

		volatile int numPolyVerts = (dec_face.flags & FACE_IS_QUAD) ? 4 : 3;
		bool bad_face = false;

		// perform vertex checks
		for(int v = 0; v < numPolyVerts; v++)
		{
			if(dec_face.vindices[v] >= vertex_ref->num_vertices)
			{
				
				bad_face = true;
				break;
			}

			// also check normals
			if(dec_face.flags & FACE_VERT_NORMAL)
			{
				if(dec_face.nindices[v] >= vertex_ref->num_point_normals)
				{
					bad_face = true;
					break;
				}
			}
		}

		if(bad_face)
		{
			MsgError("poly id=%d type=%d ofs=%d has invalid indices (or format is unknown)\n", i, *facedata & 31, model->poly_block + face_ofs);
			face_ofs += poly_size;
			continue;
		}

		if (dec_face.flags & FACE_TEXTURED)
		{
			if(prev_tpage != dec_face.page)
				pStream->Print("usemtl page_%d\r\n", dec_face.page);

			prev_tpage = dec_face.page;
		}
		else
		{
			if(prev_tpage != -1)
				pStream->Print("usemtl none\r\n");

			prev_tpage = -1;
		}

		bool smooth = (dec_face.flags & FACE_VERT_NORMAL);

		// Gouraud-shaded poly smoothing
		if(smooth != prevSmooth)
		{
			pStream->Print("s %s\r\n", smooth ? "1" : "off");
			prevSmooth = smooth;
		}

		// start new fresh face
		strcpy(formatted_vertex, "f ");

		for(int v = 0; v < numPolyVerts; v++)
		{
			char temp[64] = {0};
			char vertex_value[64] = {0};

			// NOTE: Vertex indexes is reversed here
#define VERT_IDX numPolyVerts - 1 - v

			// starting with vertex index
			sprintf(temp, "%d", dec_face.vindices[VERT_IDX] + 1 + numVerts);
			strcat(vertex_value, temp);

			// dump texture coordinate
			if (dec_face.flags & FACE_TEXTURED)
			{
				UV_INFO uv = *(UV_INFO*)dec_face.uv[VERT_IDX];

				float fsU, fsV;
				
				// map to 0..1
				fsU = ((float)uv.u + 0.5f) / 256.0f;
				fsV = ((float)uv.v + 0.5f) / 256.0f;

				pStream->Print("vt %g %g\r\n", fsU, 1.0f - fsV);

				// add texture coordinate to face value
				sprintf(temp, "/%d", numVertCoords + 1);
				strcat(vertex_value, temp);

				numVertCoords++;
			}

			// dump vertex normal
			if(dec_face.flags & FACE_VERT_NORMAL)
			{
				// add vertex normal to face value
				sprintf(temp, "/%d", dec_face.nindices[VERT_IDX] + 1 + numVerts);
				strcat(vertex_value, temp);
			}

			// add a space
			strcat(vertex_value, " ");

			// concat the value
			strcat(formatted_vertex, vertex_value);
		}

		// end the vertex
		pStream->Print("%s\r\n", formatted_vertex);

		face_ofs += poly_size;
	}

	if (first_t)
		*first_t = numVertCoords;

	if (first_v)
		*first_v = numVerts + vertex_ref->num_vertices;

	PrintUnknownPolys();
}

//-------------------------------------------------------------
// Saves raw MODEL to file
//-------------------------------------------------------------
void ExtractDMODEL(MODEL* model, const char* model_name, int modelSize)
{
	FILE* mdlFile = fopen(varargs("%s.dmodel", model_name), "wb");

	if (mdlFile)
	{
		CFileStream fstr(mdlFile);

		fwrite(model, 1, modelSize, mdlFile);

		Msg("----------\nSaving model %s\n", model_name);

		// success
		fclose(mdlFile);
	}
}

//-------------------------------------------------------------
// exports model to single file
//-------------------------------------------------------------
void ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize)
{
	if (!model)
		return;

	if (g_extract_dmodels)
	{
		ExtractDMODEL(model, model_name, modelSize);
		return;
	}

	const char* selFilename = model_name;

	if (strchr(model_name, '.') == nullptr)
		selFilename = varargs("%s.obj", model_name);
	
	FILE* mdlFile = fopen(selFilename, "wb");

	if (mdlFile)
	{
		CFileStream fstr(mdlFile);

		Msg("----------\nModel %s (%d)\n", model_name, model_index);

		fstr.Print("mtllib MODELPAGES.mtl\r\n");

		WriteMODELToObjStream(&fstr, model, modelSize, model_index, model_name, true);

		// success
		fclose(mdlFile);
	}
}

//-------------------------------------------------------------
// exports car model. Car models are typical MODEL structures
//-------------------------------------------------------------
void ExportCarModel(MODEL* model, int size, int index, const char* name_suffix)
{
	std::string model_name(varargs("%s/CARMODEL_%d_%s", g_levname_moddir.c_str(), index, name_suffix));

	// export model
	ExportDMODELToOBJ(model, model_name.c_str(), index, size);
}

//-------------------------------------------------------------
// Saves OBJ material file
//-------------------------------------------------------------
void SaveModelPagesMTL()
{
	// create material file
	FILE* pMtlFile = fopen(varargs("%s/MODELPAGES.mtl", g_levname_moddir.c_str()), "wb");

	if (pMtlFile)
	{
		for (int i = 0; i < g_levTextures.GetTPageCount(); i++)
		{
			fprintf(pMtlFile, "newmtl page_%d\r\n", i);
			fprintf(pMtlFile, "map_Kd ../../%s/PAGE_%d.tga\r\n", g_levname_texdir.c_str(), i);
		}

		fclose(pMtlFile);
	}
}

//-------------------------------------------------------------
// Exports all models from level
//-------------------------------------------------------------
void ExportAllModels()
{
	MsgInfo("Exporting all models...\n");

	for (int i = 0; i < MAX_MODELS; i++)
	{
		ModelRef_t* modelRef = g_levModels.GetModelByIndex(i);
		const char* modelName = g_levModels.GetModelName(modelRef);
		
		if (!modelRef->model)
			continue;

		std::string modelFileName(varargs("%s/ZMOD_%d", g_levname_moddir.c_str(), i));

		if (modelName && modelName[0] != 0)
			modelFileName = varargs("%s/%d_%s", g_levname_moddir.c_str(), i, modelName);

		// export model
		ExportDMODELToOBJ(modelRef->model, modelFileName.c_str(), i, modelRef->size);

		// save original dmodel2
		FILE* dFile = fopen(varargs("%s.dmodel", modelFileName.c_str()), "wb");
		if (dFile)
		{
			fwrite(modelRef->model, modelRef->size, 1, dFile);
			fclose(dFile);
		}
	}
}

//-------------------------------------------------------------
// Exports all CAR models from level
//-------------------------------------------------------------
void ExportAllCarModels()
{
	MsgInfo("Exporting car models...\n");

	for (int i = 0; i < MAX_CAR_MODELS; i++)
	{
		CarModelData_t* modelRef = g_levModels.GetCarModel(i);
		
		ExportCarModel(modelRef->cleanmodel, modelRef->cleanSize, i, "clean");
		ExportCarModel(modelRef->dammodel, modelRef->cleanSize, i, "damaged");
		ExportCarModel(modelRef->lowmodel, modelRef->lowSize, i, "low");
	}
}