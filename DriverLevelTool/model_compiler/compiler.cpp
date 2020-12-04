#include "compiler.h"

#include "obj_loader.h"
#include "driver_routines/d2_types.h"
#include <malloc.h>

#include "driver_level.h"
#include "core/VirtualStream.h"
#include "util/ini.h"
#include "util/util.h"

extern std::string		g_levname;

TexPage_t*				g_compilerTPages = NULL;
int						g_numCompilerTPages = 0;

void ConvertVertexToDriver(SVECTOR* dest, Vector3D* src)
{
	dest->x = -src->x * ONE;
	dest->y = -src->y * ONE;
	dest->z = src->z * ONE;
}

//--------------------------------------------------------------------------
// Free work memory
//--------------------------------------------------------------------------
void FreeTextureDetails()
{
	for(int i = 0; i < g_numCompilerTPages; i++)
	{
		delete[] g_compilerTPages[i].details;
	}

	delete[] g_compilerTPages;
	g_compilerTPages = NULL;
	g_numCompilerTPages = 0;
}

//--------------------------------------------------------------------------
// Loads INI files to get a clue of texture details
//--------------------------------------------------------------------------
void InitTextureDetailsForModel(smdmodel_t* model)
{
	// get texture pages from source model
	DkList<int> tpage_ids;
	DkList<smdgroup_t*> textured_groups;

	for(int i = 0; i < model->groups.numElem(); i++)
	{
		smdgroup_t* group = model->groups[i];
	
		int tpage_number = -1;
		sscanf(group->texture, "page_%d", &tpage_number);

		if (tpage_number != -1 && tpage_ids.findIndex(tpage_number) == -1)
		{
			tpage_ids.append(tpage_number);
			textured_groups.append(group);
		}
	}

	g_compilerTPages = new TexPage_t[textured_groups.numElem()];
	g_numCompilerTPages = textured_groups.numElem();

	size_t lastindex = g_levname.find_last_of(".");
	g_levname = g_levname.substr(0, lastindex);
	
	// load INI files
	for(int i = 0; i < textured_groups.numElem(); i++)
	{
		ini_t* tpage_ini = ini_load(varargs("%s_textures/%s.ini", g_levname.c_str(), textured_groups[i]->texture));

		if (!tpage_ini)
		{
			g_compilerTPages[i].details = NULL;
			g_compilerTPages[i].numDetails = 0;
			
			MsgError("Unable to open '%s_textures/%s.ini'! Texture coordinates might be messed up\n", g_levname.c_str(), textured_groups[i]->texture);
			continue;
		}
		
		int numDetails;
		ini_sget(tpage_ini, "tpage", "details", "%d", &numDetails);

		g_compilerTPages[i].id = tpage_ids[i];
		g_compilerTPages[i].numDetails = numDetails;
		g_compilerTPages[i].details = new TEXINF[numDetails];
		
		for(int j = 0; j < numDetails; j++)
		{
			const char* xywh_values = ini_get(tpage_ini, varargs("detail_%d", j), "xywh");

			int x, y, w, h;
			
			sscanf(xywh_values, "%d,%d,%d,%d", &x, &y, &w, &h);

			// this is how game handles it
			if (w >= 256)
				w -= 256;
			
			if (h >= 256)
				h -= 256;
			
			g_compilerTPages[i].details[j].x = x;
			g_compilerTPages[i].details[j].y = y;
			g_compilerTPages[i].details[j].width = w;
			g_compilerTPages[i].details[j].height = h;
		}

		ini_free(tpage_ini);
	}
}

//--------------------------------------------------------------------------
// searches for texture detail by checking UV coordinates ownership to each
// texture detail rectangle
//--------------------------------------------------------------------------
int FindTextureDetailByUV(int tpage, UV_INFO* uvs, int num_uv)
{
	// first find tpage
	TexPage_t* tpinfo = NULL;
	for(int i = 0; i < g_numCompilerTPages; i++)
	{
		if(g_compilerTPages[i].id == tpage)
		{
			tpinfo = &g_compilerTPages[i];
			break;
		}
	}

	if (!tpinfo)
		return 255;

#define UV_ASSIGN_TOLERANCE 1
	
	for(int i = 0; i < tpinfo->numDetails; i++)
	{
		// make rectangle coords
		int rx1, ry1, rx2, ry2;
		rx1 = tpinfo->details[i].x;
		ry1 = tpinfo->details[i].y;

		rx2 = tpinfo->details[i].width;
		ry2 = tpinfo->details[i].height;

		rx2 += rx1;
		ry2 += ry1;

		int numInside = 0;
		
		// check each UV
		// FIXME: check mid point inside too?
		for (int j = 0; j < num_uv; j++)
		{
			int x, y;
			x = uvs[j].u;
			y = uvs[j].v;
			
			if (x >= rx1-UV_ASSIGN_TOLERANCE && x <= rx2+UV_ASSIGN_TOLERANCE && 
				y >= ry1-UV_ASSIGN_TOLERANCE && y <= ry2+UV_ASSIGN_TOLERANCE)
			{
				numInside++;
			}
		}
		
		// definitely inside
		if(numInside > 2)
		{
			// return our detail
			return i;
		}
	}

	return 255;
}

//--------------------------------------------------------------------------
// Converts polygons to Driver PSX coordinate system
//--------------------------------------------------------------------------
void ConvertPolyUVs(UV_INFO* dest, Vector2D* src, const smdpoly_t& poly)
{
	//float tpage_texel = 1.0f / 256.0f * 0.5f;
	
	for(int i = 0; i < poly.vcount; i++)
	{
		int x = src[poly.tindices[i]].x * 255.0f + 1;
		int y = src[poly.tindices[i]].y * 255.0f + 1;
		
		dest[i].u = x;
		dest[i].v = y;
	}
}

//--------------------------------------------------------------------------
// Writes DMODEL polygons
//--------------------------------------------------------------------------
int WriteGroupPolygons(IVirtualStream* dest, smdmodel_t* model, smdgroup_t* group)
{
	int tpage_number = -1;

	char* texture_name = strchr(group->texture, '/');

	if (!texture_name)
		texture_name = group->texture;
	else
		texture_name++;
	
	sscanf(texture_name, "page_%d", &tpage_number);
	
	int numPolys = group->polygons.numElem();
	for(int i = 0; i < numPolys; i++)
	{
		const smdpoly_t& poly = group->polygons[i];

		UV_INFO uvs[4];

		ConvertPolyUVs(uvs, model->texcoords.ptr(), poly);
		
		if(poly.vcount == 4)
		{
			if(tpage_number != -1)
			{
				if(poly.smooth)
				{
					POLYGT4 pgt4;
					pgt4.id = 23;
					pgt4.texture_set = tpage_number;
					pgt4.texture_id = FindTextureDetailByUV(tpage_number, uvs, 4);
					pgt4.v0 = poly.vindices[3];
					pgt4.v1 = poly.vindices[2];
					pgt4.v2 = poly.vindices[1];
					pgt4.v3 = poly.vindices[0];
					pgt4.uv0 = uvs[3];
					pgt4.uv1 = uvs[2];
					pgt4.uv2 = uvs[1];
					pgt4.uv3 = uvs[0];
					pgt4.n0 = poly.nindices[3];
					pgt4.n1 = poly.nindices[2];
					pgt4.n2 = poly.nindices[1];
					pgt4.n3 = poly.nindices[0];

					pgt4.color = { 128,128,128,0 };
					
					dest->Write(&pgt4, 1, sizeof(pgt4));
				}
				else
				{
					POLYFT4 pft4;
					pft4.id = 21;
					pft4.texture_set = tpage_number;
					pft4.texture_id = FindTextureDetailByUV(tpage_number, uvs, 4);
					pft4.v0 = poly.vindices[3];
					pft4.v1 = poly.vindices[2];
					pft4.v2 = poly.vindices[1];
					pft4.v3 = poly.vindices[0];
					pft4.uv0 = uvs[3];
					pft4.uv1 = uvs[2];
					pft4.uv2 = uvs[1];
					pft4.uv3 = uvs[0];
					pft4.color = { 128,128,128,0 };
					
					dest->Write(&pft4, 1, sizeof(pft4));
				}
			}
			else
			{
				POLYF4 pf4;
				pf4.id = 19;
				pf4.v0 = poly.vindices[3];
				pf4.v1 = poly.vindices[2];
				pf4.v2 = poly.vindices[1];
				pf4.v3 = poly.vindices[0];
				pf4.color = { 0,0,0 };

				dest->Write(&pf4, 1, sizeof(pf4));
			}
		}
		else
		{
			if(tpage_number != -1)
			{
				if(poly.smooth)
				{
					POLYGT3 pgt3;
					pgt3.id = 22;
					pgt3.texture_set = tpage_number;
					pgt3.texture_id = FindTextureDetailByUV(tpage_number, uvs, 3);
					pgt3.v0 = poly.vindices[2];
					pgt3.v1 = poly.vindices[1];
					pgt3.v2 = poly.vindices[0];
					
					pgt3.n0 = poly.nindices[2];
					pgt3.n1 = poly.nindices[1];
					pgt3.n2 = poly.nindices[0];
					pgt3.uv0 = uvs[2];
					pgt3.uv1 = uvs[1];
					pgt3.uv2 = uvs[0];
					pgt3.color = { 128,128,128,0 };
					
					dest->Write(&pgt3, 1, sizeof(pgt3));
				}
				else
				{
					POLYFT3 pft3;
					pft3.id = 20;
					pft3.texture_set = tpage_number;
					pft3.texture_id = FindTextureDetailByUV(tpage_number, uvs, 3);
					pft3.v0 = poly.vindices[2];
					pft3.v1 = poly.vindices[1];
					pft3.v2 = poly.vindices[0];
					pft3.uv0 = uvs[2];
					pft3.uv1 = uvs[1];
					pft3.uv2 = uvs[0];
					pft3.color = { 128,128,128,0 };
					
					dest->Write(&pft3, 1, sizeof(pft3));
				}
			}
			else
			{
				POLYF3 pf3;
				pf3.id = 18;
				pf3.v0 = poly.vindices[2];
				pf3.v1 = poly.vindices[1];
				pf3.v2 = poly.vindices[0];
				pf3.color = { 0,0,0 };

				dest->Write(&pf3, 1, sizeof(pf3));
			}
		}
	}
	
	return numPolys;
}

//--------------------------------------------------------------------------
// Writes polygon normals
//--------------------------------------------------------------------------
void WritePolygonNormals(IVirtualStream* stream, smdmodel_t* model, smdgroup_t* group)
{
	int numPolys = group->polygons.numElem();
	for (int i = 0; i < numPolys; i++)
	{
		const smdpoly_t& poly = group->polygons[i];

		Vector3D normal = NormalOfTriangle(model->verts[poly.vindices[0]], model->verts[poly.vindices[1]], model->verts[poly.vindices[2]]);

		// Invert normals
		normal *= -1.0f;
		
		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &normal);

		stream->Write(&dstvert, 1, sizeof(SVECTOR));
	}
}

//--------------------------------------------------------------------------
// Computes bounding sphere, primarily takes MAX vertex coordinate values
//--------------------------------------------------------------------------
int CalculateBoundingSphere(SVECTOR* verts, int count)
{
	int radii = 0;
	for(int i = 0; i < count; i++)
	{
		int vx = verts[i].x;
		int vy = verts[i].y;
		int vz = verts[i].z;

		if(vx > radii)
			radii = vx;

		if(vy > radii)
			radii = vy;
		
		if(vz > radii)
			radii = vz;
	}

	return radii;
}

//--------------------------------------------------------------------------
// Compiles DMODEL from source model (primarily OBJ)
//--------------------------------------------------------------------------
MODEL* CompileDMODEL(CMemoryStream* stream, smdmodel_t* model, int& resultSize)
{
	MODEL* modelData = (MODEL*)stream->GetCurrentPointer();

	// advance to the model data
	stream->Seek(sizeof(MODEL), VS_SEEK_CUR);

	// fill the header
	// TODO: proper values
	modelData->flags2 = 0;
	modelData->shape_flags = 0;

	modelData->zBias = 0;
	modelData->instance_number = -1;
	modelData->tri_verts = 3;

	modelData->num_vertices = model->verts.numElem();
	modelData->num_point_normals = model->normals.numElem();
	modelData->num_polys = 0;
	modelData->collision_block = 0;	// TODO: boxes
	// 
	// write vertices
	modelData->vertices = stream->Tell();
	for(int i = 0; i < model->verts.numElem(); i++)
	{
		Vector3D& srcvert = model->verts[i];
		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &srcvert);
		
		stream->Write(&dstvert, 1 ,sizeof(SVECTOR));
	}

	Msg("Total vertices: %d\n", modelData->num_vertices);

	modelData->bounding_sphere = CalculateBoundingSphere(modelData->pVertex(0), modelData->num_vertices);
	Msg("Bounding sphere: %d\n", modelData->bounding_sphere);
	
	// polygon normals
	modelData->normals = stream->Tell();
	for (int i = 0; i < model->groups.numElem(); i++)
	{
		WritePolygonNormals(stream, model, model->groups[i]);
	}

	// write point normals
	modelData->point_normals = stream->Tell();
	for(int i = 0; i < model->normals.numElem(); i++)
	{
		Vector3D normal = model->normals[i];

		// Invert normals
		normal *= -1.0f;

		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &normal);
		
		stream->Write(&dstvert, 1 ,sizeof(SVECTOR));
	}

	modelData->poly_block = stream->Tell();

	Msg("Generating polygons\n");

	// save polygons
	for(int i = 0; i < model->groups.numElem(); i++)
	{
		int numPolygons = WriteGroupPolygons(stream, model, model->groups[i]);
		Msg("Group %d num polygons: %d\n", i, numPolygons);
		modelData->num_polys += numPolygons;
	}

	// save to output file
	resultSize = stream->Tell();

	MsgWarning("Final model size: %d bytes\n", resultSize);
	
	// dun
	return modelData;
}

//--------------------------------------------------------------------------
// Compiler function
//--------------------------------------------------------------------------
void CompileOBJModelToDMODEL(const char* filename, const char* outputName)
{
	smdmodel_t model;

	if(g_levname.length() == 0)
	{
		MsgError("Level name must be specified!\n");
		return;
	}

	MsgInfo("Compiling '%s' to '%s'...\n", filename, outputName);
	
	if (!LoadOBJ(&model, filename))
		return;

	OptimizeModel(model);

	if(model.verts.numElem() > 256)
	{
		MsgError("Cannot continue - model exceeds 256 vertices\n");
		return;
	}

	if (model.normals.numElem() > 256)
	{
		MsgError("Cannot continue - model exceeds 256 vertex normals\n");
		return;
	}

	InitTextureDetailsForModel(&model);

	CMemoryStream stream;
	stream.Open(NULL, VS_OPEN_WRITE, 512 * 1024);
	
	int resultSize = 0;
	MODEL* resultModel = CompileDMODEL(&stream, &model, resultSize);

	if(resultModel)
	{
		FILE* fp = fopen(outputName, "wb");

		if(fp)
		{
			stream.WriteToFileStream(fp);
			fclose(fp);
		}
	}

	FreeTextureDetails();
}