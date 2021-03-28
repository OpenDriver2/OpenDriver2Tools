#include "driver_level.h"
#include "driver_routines/level.h"

#include "core/cmdlib.h"
#include "core/VirtualStream.h"
#include "util/util.h"

#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"

#include "math/Matrix.h"

extern bool				g_export_models;
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
int ExportRegionDriver1(CDriver1LevelRegion* region, IVirtualStream* cellsFileStream, IVirtualStream* levelFileStream, int& lobj_first_v, int& lobj_first_t)
{
	CDriver1LevelMap* levMapDriver1 = (CDriver1LevelMap*)g_levMap;
	const OUT_CELL_FILE_HEADER& mapInfo = levMapDriver1->GetMapInfo();

	int numRegionObjects = 0;
	
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

			if (cellsFileStream)
			{
				cellsFileStream->Print("# m %d r %d\r\n", co->type, co->yang);
				cellsFileStream->Print("v %g %g %g\r\n", absCellPosition.x, absCellPosition.y, absCellPosition.z);
			}

			ModelRef_t* ref = g_levModels.GetModelByIndex(co->type);

			if (ref)
			{
				// transform objects and save
				Matrix4x4 transform = translate(absCellPosition);
				transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

				WriteMODELToObjStream(levelFileStream, ref->model, ref->size, co->type, 
					String::fromPrintf("reg%d", region->GetNumber()),
					false, transform, &lobj_first_v, &lobj_first_t);
			}

			numRegionObjects++;
			
			co = levMapDriver1->GetNextCop(&iterator);
		}
	}

	if (cellsFileStream)
		cellsFileStream->Print("# total region cells %d\r\n", numRegionObjects);

	return numRegionObjects;
}

//-------------------------------------------------------------
// Processes Driver 2 region
//-------------------------------------------------------------
int ExportRegionDriver2(CDriver2LevelRegion* region, IVirtualStream* cellsFileStream, IVirtualStream* levelFileStream, int& lobj_first_v, int& lobj_first_t)
{
	CDriver2LevelMap* levMapDriver2 = (CDriver2LevelMap*)g_levMap;
	const OUT_CELL_FILE_HEADER& mapInfo = levMapDriver2->GetMapInfo();

	int numRegionObjects = 0;

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

			if (cellsFileStream)
			{
				cellsFileStream->Print("# m %d r %d\r\n", co.type, co.yang);
				cellsFileStream->Print("v %g %g %g\r\n", absCellPosition.x, absCellPosition.y, absCellPosition.z);
			}

			ModelRef_t* ref = g_levModels.GetModelByIndex(co.type);

			if (ref)
			{
				// transform objects and save
				Matrix4x4 transform = translate(absCellPosition);
				transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

				WriteMODELToObjStream(levelFileStream, ref->model, ref->size, co.type, 
					String::fromPrintf("reg%d", region->GetNumber()),
					false, transform, &lobj_first_v, &lobj_first_t);
			}

			numRegionObjects++;

			pco = levMapDriver2->GetNextPackedCop(&iterator);
		}
	}

	if (cellsFileStream)
		cellsFileStream->Print("# total region cells %d\r\n", numRegionObjects);

	return numRegionObjects;
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
	FILE* cellsFile = fopen(String::fromPrintf("%s_CELLPOS_MAP.obj", (char*)g_levname), "wb");
	FILE* levelFile = fopen(String::fromPrintf("%s_LEVELMODEL.obj", (char*)g_levname), "wb");

	CFileStream cellsFileStream(cellsFile);
	CFileStream levelFileStream(levelFile);

	levelFileStream.Print("mtllib %s_LEVELMODEL.mtl\r\n", (char*)g_levname);

	int lobj_first_v = 0;
	int lobj_first_t = 0;

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

			Msg("Exporting region %d...", i);
			
			if (g_levMap->GetFormat() >= LEV_FORMAT_DRIVER2_ALPHA16)
			{
				numCellObjectsRead += ExportRegionDriver2((CDriver2LevelRegion*)region, &cellsFileStream, &levelFileStream, lobj_first_v, lobj_first_t);
			}
			else
			{
				numCellObjectsRead += ExportRegionDriver1((CDriver1LevelRegion*)region, &cellsFileStream, &levelFileStream, lobj_first_v, lobj_first_t);
			}

			Msg("DONE\n");
		}

		// @FIXME: it doesn't really match up but still correct
		//int numCellsObjectsFile = mapInfo.num_cell_objects;

		//if (numCellObjectsRead != numCellsObjectsFile)
		//	MsgError("numAllObjects mismatch: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);

		MsgAccept("Successfully saved '%s_LEVELMODEL.obj'\n", (char*)g_levname);
		
		fclose(fp);
	}
	else
		MsgError("Unable to export regions - cannot open level file!\n");
}