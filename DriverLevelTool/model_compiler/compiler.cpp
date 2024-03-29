#include "compiler.h"

#include <string.h>

#include "driver_routines/d2_types.h"

#include "math/TriangleUtil.inl"

#include "core/VirtualStream.h"
#include "util/ini.h"
#include "util/util.h"

#include <nstd/File.hpp>
#include <nstd/Directory.hpp>
#include <nstd/Array.hpp>

#include "core/cmdlib.h"

extern String	g_levname;

struct CompilerTPage
{
	int		id{ -1 };
	TEXINF* details{ nullptr };
	int		numDetails{ 0 };
};

CompilerTPage*			g_compilerTPages = nullptr;
int						g_numCompilerTPages = 0;

//--------------------------------------------------------------------------

static int GetDamageZoneId(const char* zoneName)
{
	if (zoneName == nullptr)
		return 0xFFFF;

	for(int i = 0; i < NUM_ZONE_TYPES; i++)
	{
		if(!stricmp(s_DamageZoneNames[i], zoneName))
		{
			return i;
		}
	}

	return 0xFFFF;
}

void ConvertVertexToDriver(SVECTOR* dest, Vector3D* src)
{
	dest->x = -src->x * ONE_F;
	dest->y = -src->y * ONE_F;
	dest->z = src->z * ONE_F;
}

//--------------------------------------------------------------------------
// Free work memory
//--------------------------------------------------------------------------
static void FreeTextureDetails()
{
	for(int i = 0; i < g_numCompilerTPages; i++)
	{
		delete[] g_compilerTPages[i].details;
	}

	delete[] g_compilerTPages;
	g_compilerTPages = nullptr;
	g_numCompilerTPages = 0;
}

//--------------------------------------------------------------------------
// Loads INI files to get a clue of texture details
//--------------------------------------------------------------------------
static void InitTextureDetailsForModel(smdmodel_t* model)
{
	// get texture pages from source model
	Array<int> tpage_ids;
	Array<smdgroup_t*> textured_groups;

	for(usize i = 0; i < model->groups.size(); i++)
	{
		smdgroup_t* group = model->groups[i];
	
		int tpage_number = -1;
		sscanf(group->texture, "page_%d", &tpage_number);

		if (tpage_number != -1 && tpage_ids.find(tpage_number) == tpage_ids.end())
		{
			tpage_ids.append(tpage_number);
			textured_groups.append(group);
		}
	}

	g_compilerTPages = new CompilerTPage[textured_groups.size()];
	g_numCompilerTPages = textured_groups.size();

	String lev_no_ext = File::dirname(g_levname) + "/" + File::basename(g_levname, File::extension(g_levname));

	// load INI files
	for(usize i = 0; i < textured_groups.size(); i++)
	{
		ini_t* tpage_ini = ini_load(String::fromPrintf("%s_textures/%s.ini", (char*)lev_no_ext, textured_groups[i]->texture));

		if (!tpage_ini)
		{
			g_compilerTPages[i].details = nullptr;
			g_compilerTPages[i].numDetails = 0;
			
			MsgError("Unable to open '%s_textures/%s.ini'! Texture coordinates might be messed up\n", (char*)lev_no_ext, textured_groups[i]->texture);
			continue;
		}
		
		int numDetails;
		ini_sget(tpage_ini, "tpage", "details", "%d", &numDetails);

		g_compilerTPages[i].id = tpage_ids[i];
		g_compilerTPages[i].numDetails = numDetails;
		g_compilerTPages[i].details = new TEXINF[numDetails];
		
		for(int j = 0; j < numDetails; j++)
		{
			TEXINF& detail = g_compilerTPages[i].details[j];

			String detailName = String::fromPrintf("detail_%d", j);

			int x, y, w, h;
			const char* xywh_values = ini_get(tpage_ini, detailName, "xywh");
			sscanf(xywh_values, "%d,%d,%d,%d", &x, &y, &w, &h);

			// this is how game handles it
			if (w >= 256)
				w -= 256;
			
			if (h >= 256)
				h -= 256;

			detail.x = x;
			detail.y = y;
			detail.width = w;
			detail.height = h;

			int damage_level = 0;
			int damage_split = 0;
			
			const char* damageZoneName = ini_get(tpage_ini, detailName, "damagezone");

			ini_sget(tpage_ini, detailName, "damagelevel", "%d", &damage_level);
			ini_sget(tpage_ini, detailName, "damagesplit", "%d", &damage_split);

			// store damage zone in ID and damage level in nameoffset
			detail.id = GetDamageZoneId(damageZoneName);
			detail.nameoffset = damage_level | (damage_split ? 0x8000 : 0);

			//Msg("Damage zone detail_%d %s id: %d\n", j, damageZoneName, detail.id);
		}

		ini_free(tpage_ini);
	}
}

//--------------------------------------------------------------------------
// searches for texture detail by checking UV coordinates ownership to each
// texture detail rectangle
//--------------------------------------------------------------------------
static int FindTextureDetailByUV(int tpage, UV_INFO* uvs, int num_uv, TEXINF** detail)
{
	// first find tpage
	CompilerTPage* tpinfo = nullptr;
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
			if (detail)
				*detail = &tpinfo->details[i];
			
			// return our detail
			return i;
		}
	}

	return 255;
}

//--------------------------------------------------------------------------
// Converts polygons to Driver PSX coordinate system
//--------------------------------------------------------------------------
static void ConvertPolyUVs(UV_INFO* dest, Vector2D* src, const smdpoly_t& poly)
{
	for(int i = 0; i < poly.vcount; i++)
	{
		int x = roundf(src[poly.tindices[i]].x * 255.0f);
		int y = roundf(src[poly.tindices[i]].y * 255.0f + 0.5f);
		
		dest[i].u = x;
		dest[i].v = y;
	}
}

//--------------------------------------------------------------------------
// Writes MDL polygons
//--------------------------------------------------------------------------
static int WriteGroupPolygons(IVirtualStream* dest, smdmodel_t* model, smdgroup_t* group)
{
	int tpage_number = -1;

	char* texture_name = strchr(group->texture, '/');

	if (!texture_name)
		texture_name = group->texture;
	else
		texture_name++;
	
	sscanf(texture_name, "page_%d", &tpage_number);

	int numPolys = group->polygons.size();
	for(int i = 0; i < numPolys; i++)
	{
		smdpoly_t& poly = group->polygons[i];
		
		UV_INFO uvs[4];
		ConvertPolyUVs(uvs, (Vector2D*)model->texcoords, poly);

		int detail_id = 255;

		if(tpage_number != -1)
		{
			TEXINF* detail;
			detail_id = FindTextureDetailByUV(tpage_number, uvs, poly.vcount, &detail);

			// Add polygon denting information
			if(detail_id != -1 && detail->id != 0xFFFF)
			{
				poly.flags = POLY_EXTRA_DENTING | ((detail->nameoffset & 0x8000) ? POLY_DAMAGE_SPLIT : 0);
				poly.extraData = detail->id | ((detail->nameoffset & 0xf) << 4);	// damage zone + damage level
			}
		}
		
		if(poly.vcount == 4)
		{
			if(tpage_number != -1)
			{
				if(poly.smooth)
				{
					POLYGT4 pgt4;
					pgt4.id = 23;
					pgt4.texture_set = tpage_number;
					pgt4.texture_id = detail_id;
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
					pft4.texture_id = detail_id;
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
					pgt3.texture_id = detail_id;
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
					pft3.texture_id = detail_id;
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
static void WritePolygonNormals(IVirtualStream* stream, MODEL* model, smdgroup_t* group)
{
	int numPolys = group->polygons.size();
	for (int i = 0; i < numPolys; i++)
	{
		const smdpoly_t& poly = group->polygons[i];

		SVECTOR* v1 = model->pVertex(poly.vindices[0]);
		SVECTOR* v2 = model->pVertex(poly.vindices[1]);
		SVECTOR* v3 = model->pVertex(poly.vindices[2]);
		
		Vector3D normal;
		Vector3D fv1, fv2, fv3;

		fv1 = Vector3D((float)v1->x, (float)v1->y, (float)v1->z);
		fv2 = Vector3D((float)v2->x, (float)v2->y, (float)v2->z);
		fv3 = Vector3D((float)v3->x, (float)v3->y, (float)v3->z);
		
		ComputeTriangleNormal(fv1,fv2,fv3,normal);

		// Invert normals (REQUIRED)
		normal *= -1.0f;
		
		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &normal);

		stream->Write(&dstvert, 1, sizeof(SVECTOR));
	}
}

//--------------------------------------------------------------------------
// Computes bounding sphere, primarily takes MAX vertex coordinate values
//--------------------------------------------------------------------------
static int CalculateBoundingSphere(SVECTOR* verts, int count)
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
// Compiles MDL from source model (primarily OBJ)
//--------------------------------------------------------------------------
static MODEL* CompileMDL(CMemoryStream* stream, smdmodel_t* model, int& resultSize)
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

	modelData->num_vertices = model->verts.size();
	modelData->num_point_normals = model->normals.size();
	modelData->num_polys = 0;
	modelData->collision_block = 0;	// TODO: boxes
	// 
	// write vertices
	modelData->vertices = stream->Tell();
	for(usize i = 0; i < model->verts.size(); i++)
	{
		Vector3D& srcvert = model->verts[i];
		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &srcvert);
		
		stream->Write(&dstvert, 1 ,sizeof(SVECTOR));
	}

	Msg("Total vertices: %d\n", modelData->num_vertices);

	modelData->bounding_sphere = CalculateBoundingSphere(modelData->pVertex(0), modelData->num_vertices);
	Msg("Bounding sphere: %d\n", modelData->bounding_sphere);

	Msg("Writing polygon normals...\n");
	// polygon normals
	modelData->normals = stream->Tell();
	for (usize i = 0; i < model->groups.size(); i++)
	{
		WritePolygonNormals(stream, modelData, model->groups[i]);
	}

	Msg("Writing point normals...\n");
	// write point normals
	modelData->point_normals = stream->Tell();
	for(usize i = 0; i < model->normals.size(); i++)
	{
		Vector3D normal = model->normals[i];

		// Invert normals (REQUIRED)
		normal *= -1.0f;

		SVECTOR dstvert;
		ConvertVertexToDriver(&dstvert, &normal);
		
		stream->Write(&dstvert, 1 ,sizeof(SVECTOR));
	}

	modelData->poly_block = stream->Tell();

	Msg("Generating polygons\n");

	// save polygons
	for(usize i = 0; i < model->groups.size(); i++)
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
void CompileOBJModelToMDL(const char* filename, const char* outputName, bool generate_denting)
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

	if(model.verts.size() > 256)
	{
		MsgError("Cannot continue - model exceeds 256 vertices\n");
		return;
	}

	if (model.normals.size() > 256)
	{
		MsgError("Cannot continue - model exceeds 256 vertex normals\n");
		return;
	}

	InitTextureDetailsForModel(&model);

	CMemoryStream stream;
	stream.Open(nullptr, VS_OPEN_WRITE, 512 * 1024);
	
	int resultSize = 0;
	MODEL* resultModel = CompileMDL(&stream, &model, resultSize);

	if(resultModel)
	{
		String outputNameStr(outputName, strlen(outputName));
		Directory::create(File::dirname(outputNameStr));
		FILE* fp = fopen(outputName, "wb");

		if(fp)
		{
			stream.WriteToFileStream(fp);
		}
			fclose(fp);

		// additionally generate denting
		if (generate_denting)
			GenerateDenting(model, outputName);
	}

	FreeOBJ(&model);
	FreeTextureDetails();
}