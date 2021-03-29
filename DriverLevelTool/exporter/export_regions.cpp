#include "driver_level.h"
#include "driver_routines/level.h"

#include "core/cmdlib.h"
#include "core/VirtualStream.h"
#include "util/util.h"
#include <string.h>
#include <nstd/Directory.hpp>
#include <nstd/File.hpp>


#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"

#include "math/Matrix.h"

static const char* s_UnityScriptTemplate =
"using System.Collections;\n\
using System.Collections.Generic;\n\
using UnityEngine;\n\
public class %s : MonoBehaviour\n\
{\n\
	void Start()\n\
	{\n\
%s\n\
	}\n\
	void Update()\n\
	{\n\
	}\n\
}";

extern bool				g_export_models;
extern bool				g_export_worldUnityScript;

extern String			g_levname_moddir;
extern String			g_levname;

char g_packed_cell_pointers[8192];
ushort g_cell_ptrs[8192];

CELL_DATA g_cells[16384];
CELL_DATA_D1 g_cells_d1[16384];

// Driver 2
PACKED_CELL_OBJECT g_packed_cell_objects[16384];

// Driver 1
CELL_OBJECT g_cell_objects[16384];

//-------------------------------------------------------------
// Processes Driver 1 region
//-------------------------------------------------------------
int ExportRegionDriver1(CDriver1LevelRegion* region, IVirtualStream* levelFileStream, int& lobj_first_v, int& lobj_first_t)
{
	CDriver1LevelMap* levMapDriver1 = (CDriver1LevelMap*)g_levMap;
	const OUT_CELL_FILE_HEADER& mapInfo = levMapDriver1->GetMapInfo();

	int numRegionObjects = 0;

	if (g_export_worldUnityScript)
		levelFileStream->Print("// Region %d\n", region->GetNumber());
	
	// walk through all cell data
	for(int i = 0; i < mapInfo.region_size * mapInfo.region_size; i++)
	{	
		CELL_ITERATOR_D1 iterator;
		CELL_OBJECT* co = region->StartIterator(&iterator, i);

		if (!co)
			continue;

		while(co)
		{
			Vector3D absCellPosition(co->pos.vx * -EXPORT_SCALING, co->pos.vy * -EXPORT_SCALING, co->pos.vz * EXPORT_SCALING);
			float cellRotationRad = co->yang / 64.0f * PI_F * 2.0f;

			ModelRef_t* ref = g_levModels.GetModelByIndex(co->type);

			if (ref)
			{
				if (g_export_worldUnityScript)
				{
					String cellModelName = ref->name ? String::fromCString(ref->name) : String::fromPrintf("MOD_%d", ref->index);

					float cellRotationDeg = RAD2DEG(cellRotationRad);// +180;
					
					levelFileStream->Print("var reg%d_o%d = Instantiate(%s, new Vector3(%gf,%gf,%gf), Quaternion.Euler(0.0f,%gf,0.0f)) as GameObject;\n",
						region->GetNumber(), numRegionObjects, (char*)cellModelName, absCellPosition.x, absCellPosition.y, absCellPosition.z, cellRotationDeg);
				}
				else
				{
					// transform objects and save
					Matrix4x4 transform = translate(absCellPosition);
					transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

					WriteMODELToObjStream(levelFileStream, ref->model, ref->size, co->type,
						String::fromPrintf("reg%d", region->GetNumber()),
						false, transform, &lobj_first_v, &lobj_first_t);
				}
			}

			numRegionObjects++;
			
			co = levMapDriver1->GetNextCop(&iterator);
		}
	}

	if (g_export_worldUnityScript)
		levelFileStream->Print("// total region cells %d\n", numRegionObjects);

	return numRegionObjects;
}

//-------------------------------------------------------------
// Processes Driver 2 region
//-------------------------------------------------------------
int ExportRegionDriver2(CDriver2LevelRegion* region, IVirtualStream* levelFileStream, int& lobj_first_v, int& lobj_first_t)
{
	CDriver2LevelMap* levMapDriver2 = (CDriver2LevelMap*)g_levMap;
	const OUT_CELL_FILE_HEADER& mapInfo = levMapDriver2->GetMapInfo();

	int numRegionObjects = 0;

	if (g_export_worldUnityScript)
		levelFileStream->Print("// Region %d\n", region->GetNumber());

	// walk through all cell data
	for (int i = 0; i < mapInfo.region_size * mapInfo.region_size; i++)
	{
		CELL_ITERATOR iterator;
		PACKED_CELL_OBJECT* pco = region->StartIterator(&iterator, i);

		if (!pco)
			continue;

		while (pco)
		{
			CELL_OBJECT co;
			CDriver2LevelMap::UnpackCellObject(co, pco, iterator.nearCell);
			
			Vector3D absCellPosition(co.pos.vx * -EXPORT_SCALING, co.pos.vy * -EXPORT_SCALING, co.pos.vz * EXPORT_SCALING);
			float cellRotationRad = co.yang / 64.0f * PI_F * 2.0f;

			ModelRef_t* ref = g_levModels.GetModelByIndex(co.type);

			if (ref)
			{
				if (g_export_worldUnityScript)
				{
					String modelName = strlen(ref->name) > 0 ? String::fromCString(ref->name) : String::fromPrintf("MOD_%d", ref->index);

					float cellRotationDeg = RAD2DEG(cellRotationRad) + 180;

					levelFileStream->Print("var reg%d_o%d = Instantiate(%s, new Vector3(%gf,%gf,%gf), Quaternion.Euler(0.0f,%gf,0.0f)) as GameObject;\n",
						region->GetNumber(), numRegionObjects, (char*)modelName, absCellPosition.x, absCellPosition.y, absCellPosition.z, cellRotationDeg);
				}
				else
				{
					// transform objects and save
					Matrix4x4 transform = translate(absCellPosition);
					transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

					WriteMODELToObjStream(levelFileStream, ref->model, ref->size, co.type,
						String::fromPrintf("reg%d", region->GetNumber()),
						false, transform, &lobj_first_v, &lobj_first_t);
				}
			}

			numRegionObjects++;

			pco = levMapDriver2->GetNextPackedCop(&iterator);
		}
	}

	if (g_export_worldUnityScript)
		levelFileStream->Print("// total region cells %d\n", numRegionObjects);

	return numRegionObjects;
}

void PrintRegionHeader(IVirtualStream* levelStream)
{
	int numModels = 0;

	levelStream->Print("var modelsPath = \"%s/\";\n", (char*)File::basename(g_levname_moddir));

	levelStream->Print("// Load resources\n");
	levelStream->Print("// You must have them placed to Assets/Resources/ folder\n");
	for (int i = 0; i < MAX_MODELS; i++)
	{
		ModelRef_t* ref = g_levModels.GetModelByIndex(i);

		if (!ref || ref && !ref->model)
			continue;

		String modelName = strlen(ref->name) > 0 ? String::fromCString(ref->name) : String::fromPrintf("MOD_%d", ref->index);

		// load the resource
		levelStream->Print("var %s = Resources.Load(modelsPath + \"%s\") as GameObject;\n", (char*)modelName, (char*)modelName);
		numModels++;
	}

	levelStream->Print("// total %d models\n", numModels);
}

//-------------------------------------------------------------
// Exports all level regions to OBJ file
//-------------------------------------------------------------
void ExportRegions()
{
	MsgInfo("Exporting cell points and world model...\n");
	const OUT_CELL_FILE_HEADER& mapInfo = g_levMap->GetMapInfo();

	int counter = 0;

	int dim_x = g_levMap->GetRegionsAcross();
	int dim_y = g_levMap->GetRegionsDown();

	Msg("World size:\n [%dx%d] cells\n [%dx%d] regions\n", g_levMap->GetCellsAcross(), g_levMap->GetCellsDown(), dim_x, dim_y);

	// Debug information: Print the map to the console.
	for (int i = 0; i < dim_x* dim_y; i++, counter++)
	{
		char str[512];
		if(counter < dim_x)
		{
			str[counter] = g_levMap->GetRegion(i)->IsEmpty() ? '.' : 'O';
		}
		else
		{
			str[counter] = 0;
			Msg("%s\n", str);
			counter = 0;
		}
	}

	Msg("\n");

	int numCellObjectsRead = 0;
	
	FILE* levelFile;
	IVirtualStream* levelStream;

	CMemoryStream memStream;
	
	if(g_export_worldUnityScript)
	{
		memStream.Open(nullptr, VS_OPEN_WRITE | VS_OPEN_TEXT, 65535 * 2048);
		Directory::create(File::dirname(g_levname) + "/regions");
	}
	else
	{
		levelFile = fopen(String::fromPrintf("%s_LEVELMODEL.obj", (char*)g_levname), "wb");
	}

	CFileStream fileStream(levelFile);

	if (g_export_worldUnityScript)
	{
		// use memory stream at first
		levelStream = &memStream;
	}
	else
	{
		// use file stream directly
		levelStream = &fileStream;
		levelStream->Print("mtllib %s_LEVELMODEL.mtl\r\n", (char*)g_levname);
	}

	int lobj_first_v = 0;
	int lobj_first_t = 0;

	String levNameOnly = File::basename(g_levname, File::extension(g_levname));

	// Open file stream
	FILE* fp = fopen(g_levname, "rb");
	if (fp)
	{
		CFileStream stream(fp);

		SPOOL_CONTEXT spoolContext;
		spoolContext.dataStream = &stream;
		spoolContext.lumpInfo = &g_levInfo;
		spoolContext.models = &g_levModels;
		spoolContext.textures = &g_levTextures;

		int totalRegions = g_levMap->GetRegionsAcross() * g_levMap->GetRegionsDown();
		
		for (int i = 0; i < totalRegions; i++)
		{
			// load region
			// it will also load area data models for it
			g_levMap->SpoolRegion(spoolContext, i);

			CBaseLevelRegion* region = g_levMap->GetRegion(i);

			if (region->IsEmpty())
				continue;

			if(g_export_worldUnityScript)
			{
				levelFile = fopen(String::fromPrintf("%s/regions/%s_reg%d.cs", (char*)File::dirname(g_levname), (char*)levNameOnly, i), "wb");
				PrintRegionHeader(&memStream);
			}

			Msg("Exporting region %d...", i);
			
			if (g_levMap->GetFormat() >= LEV_FORMAT_DRIVER2_ALPHA16)
			{
				numCellObjectsRead += ExportRegionDriver2((CDriver2LevelRegion*)region, levelStream, lobj_first_v, lobj_first_t);
			}
			else
			{
				numCellObjectsRead += ExportRegionDriver1((CDriver1LevelRegion*)region, levelStream, lobj_first_v, lobj_first_t);
			}

			// format into Unity CS script
			if (g_export_worldUnityScript)
			{
				String regionName = String::fromPrintf("%s_reg%d", (char*)levNameOnly, i);
				
				int zero = 0;
				memStream.Write(&zero, 1, sizeof(zero));
				
				fprintf(levelFile, s_UnityScriptTemplate, (char*)regionName, memStream.GetBasePointer());

				memStream.Seek(0, VS_SEEK_SET);
				
				fclose(levelFile);
				levelFile = nullptr;
			}

			Msg("DONE\n");
		}

		// @FIXME: it doesn't really match up but still correct
		//int numCellsObjectsFile = mapInfo.num_cell_objects;

		//if (numCellObjectsRead != numCellsObjectsFile)
		//	MsgError("numAllObjects mismatch: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);

		MsgAccept("Successfully exported world\n", (char*)g_levname);

		if(levelFile)
			fclose(levelFile);

		fclose(fp);
	}
	else
		MsgError("Unable to export regions - cannot open level file!\n");

	g_export_worldUnityScript = false;
}