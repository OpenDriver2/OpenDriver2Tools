#include "driver_level.h"
#include "driver_routines/level.h"

#include "core/cmdlib.h"
#include "core/VirtualStream.h"
#include "util/util.h"

#include "math/Matrix.h"

#define SPOOL_CD_BLOCK_SIZE 2048

extern int			g_region_format;
extern int			g_format;
extern bool			g_export_models;
extern std::string	g_levname_moddir;

char g_packed_cell_pointers[8192];
ushort g_cell_ptrs[8192];

CELL_DATA g_cells[16384];
CELL_DATA_D1 g_cells_d1[16384];

// Driver 2
PACKED_CELL_OBJECT g_packed_cell_objects[16384];

// Driver 1
CELL_OBJECT g_cell_objects[16384];

//-------------------------------------------------------------
// Region unpacking function
//-------------------------------------------------------------
int UnpackCellPointers(int region, int targetRegion, int cell_slots_add, ushort* dest_ptrs, char* src_data)
{
	ushort cell;
	ushort* short_ptr;
	ushort* source_packed_data;
	int loop;
	uint bitpos;
	uint pcode;
	int numCellPointers;

	int packtype;

	packtype = *(int*)(src_data + 4);
	source_packed_data = (ushort*)(src_data + 8);

	numCellPointers = 0;

	if (packtype == 0)
	{
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
			*short_ptr++ = 0xffff;
	}
	else if (packtype == 1)
	{
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
		{
			cell = *source_packed_data++;

			if (cell != 0xffff)
				cell += cell_slots_add;

			*short_ptr++ = cell;
			numCellPointers++;
		}
	}
	else if (packtype == 2)
	{
		bitpos = 0x8000;
		pcode = (uint)*source_packed_data;
		source_packed_data++;
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
		{
			if (pcode & bitpos)
			{
				cell = *source_packed_data++;
				cell += cell_slots_add;
			}
			else
				cell = 0xffff;

			bitpos >>= 1;
			*short_ptr++ = cell;
			numCellPointers++;

			if (bitpos == 0)
			{
				bitpos = 0x8000;
				pcode = *source_packed_data++;
			}
		}
	}
	else
	{
		MsgError("BAD PACKED CELL POINTER DATA, region = %d\n", region);
	}

	return numCellPointers;
}

//-------------------------------------------------------------
// returns first cell object of cell
//-------------------------------------------------------------
PACKED_CELL_OBJECT* GetFirstPackedCop(CELL_DATA** cell, ushort cell_ptr, int barrel_region)
{
	*cell = &g_cells[cell_ptr];

	if ((*cell)->num & 0x8000)
		return NULL;

	return &g_packed_cell_objects[((*cell)->num & 0x3fff) - g_cell_objects_add[barrel_region]];
}

//-------------------------------------------------------------
// iterates cell objects
//-------------------------------------------------------------
PACKED_CELL_OBJECT* GetNextPackedCop(CELL_DATA** cell, int barrel_region)
{
	ushort num;
	PACKED_CELL_OBJECT* ppco;

	do {
		if ((*cell)->num & 0x8000)
			return NULL;

		(*cell) = (*cell) + 1;
		num = (*cell)->num;

		if (num & 0x4000)
			return NULL;

		ppco = &g_packed_cell_objects[((*cell)->num & 0x3fff) - g_cell_objects_add[barrel_region]];
	} while (ppco->value == 0xffff && (ppco->pos.vy & 1) != 0);

	return ppco;
}

//-------------------------------------------------------------
// Unpacks cell object (Driver 2 ONLY)
//-------------------------------------------------------------
void UnpackCellObject(PACKED_CELL_OBJECT* pco, CELL_OBJECT& co, VECTOR_NOPAD nearCell)
{
	//bool isStraddler = pco - cell_objects < g_numStraddlers;

	co.pos.vy = (pco->pos.vy << 0x10) >> 0x11;

	//if(isStraddler)
	{
		co.pos.vx = nearCell.vx + ((nearCell.vx - pco->pos.vx << 0x10) >> 0x10);
		co.pos.vz = nearCell.vz + ((nearCell.vz - pco->pos.vz << 0x10) >> 0x10);
	}
	//else
	{
		co.pos.vx = nearCell.vx + pco->pos.vx;// ((pco.pos.vx - nearCell.vx << 0x10) >> 0x10);
		co.pos.vz = nearCell.vz + pco->pos.vz;// ((pco.pos.vz - nearCell.vz << 0x10) >> 0x10);
	}

	// unpack
	co.yang = (pco->value & 0x3f);
	co.type = (pco->value >> 6) + ((pco->pos.vy & 0x1) ? 1024 : 0);
}

//-------------------------------------------------------------
// Exports all level regions to OBJ file
//-------------------------------------------------------------
void ExportRegions()
{
	MsgInfo("Exporting cell points and world model...\n");
	
	int counter = 0;

	int dim_x = g_mapInfo.cells_across / g_mapInfo.region_size;
	int dim_y = g_mapInfo.cells_down / g_mapInfo.region_size;

	Msg("World size:\n [%dx%d] cells\n [%dx%d] regions\n", g_mapInfo.cells_across, g_mapInfo.cells_down, dim_x, dim_y);

	// Debug information: Print the map to the console.
	for (int i = 0; i < g_numSpoolInfoOffsets; i++, counter++)
	{
		char str[512];
		if(counter < dim_x)
		{
			str[counter] = g_spoolInfoOffsets[i] == REGION_EMPTY ? '.' : 'O';
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

	FILE* cellsFile = fopen(varargs("%s_CELLPOS_MAP.obj", g_levname.c_str()), "wb");
	FILE* levelFile = fopen(varargs("%s_LEVELMODEL.obj", g_levname.c_str()), "wb");

	CFileStream cellsFileStream(cellsFile);
	CFileStream levelFileStream(levelFile);

	levelFileStream.Print("mtllib %s_LEVELMODEL.mtl\r\n", g_levname.c_str());

	int lobj_first_v = 0;
	int lobj_first_t = 0;

	// copy straddlers to cell objects
	if (g_format == 2)
	{
		memcpy(g_packed_cell_objects, g_straddlers, sizeof(PACKED_CELL_OBJECT) * g_numStraddlers);
	}
	else
	{
		memcpy(g_cell_objects, g_straddlers, sizeof(CELL_OBJECT) * g_numStraddlers);
	}

	// dump 2d region map
	// easy seeking
	for (int y = 0; y < dim_y; y++)
	{
		for (int x = 0; x < dim_x; x++)
		{
			int sPosIdx = y * dim_x + x;

			if (g_spoolInfoOffsets[sPosIdx] == REGION_EMPTY)
				continue;

			int barrel_region = (x & 1) + (y & 1) * 2;

			// determine region position
			VECTOR_NOPAD regionPos;
			regionPos.vx = (x - dim_x / 2) * g_mapInfo.region_size * g_mapInfo.cell_size;
			regionPos.vz = (y - dim_y / 2) * g_mapInfo.region_size * g_mapInfo.cell_size;
			regionPos.vy = 0;

			// region at offset
			Spool* spool = (Spool*)((ubyte*)g_regionSpool + g_spoolInfoOffsets[sPosIdx]);

			Msg("---------\nSpool %d %d (offset: %d)\n", x, y, g_spoolInfoOffsets[sPosIdx]);
			Msg(" - offset: %d\n", spool->offset);

			for (int i = 0; i < spool->num_connected_areas; i++)
				Msg(" - connected area %d: %d\n", i, spool->connected_areas[i]);

			Msg(" - pvs_size: %d\n", spool->pvs_size);
			Msg(" - cell_data_size: %d %d %d\n", spool->cell_data_size[0], spool->cell_data_size[1], spool->cell_data_size[2]);

			Msg(" - super_region: %d\n", spool->super_region);

			// LoadRegionData - calculate offsets
			Msg(" - cell pointers size: %d\n", spool->cell_data_size[1]);
			Msg(" - cell data size: %d\n", spool->cell_data_size[0]);
			Msg(" - cell objects size: %d\n", spool->cell_data_size[2]);
			Msg(" - PVS data size: %d\n", spool->roadm_size);

			// prepare
			int cellPointerCount = 0;

			memset(g_packed_cell_pointers, 0, sizeof(g_packed_cell_pointers));

			memset(g_cells, 0, sizeof(g_cells));

			memset(g_cells_d1, 0, sizeof(g_cells_d1));

			if (g_format == 2)	// Driver 2
			{
				//
				// Driver 2 use PACKED_CELL_OBJECTS - 8 bytes. not wasting, but tricky
				//

				memset(g_packed_cell_objects + g_numStraddlers, 0, sizeof(g_packed_cell_objects) - g_numStraddlers * sizeof(PACKED_CELL_OBJECT));

				int cellPointersOffset;
				int cellDataOffset;
				int cellObjectsOffset;
				int pvsDataOffset;

				if (g_region_format == 3) // retail
				{
					cellPointersOffset = spool->offset; // SKIP PVS buffers, no need rn...
					cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
					cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
					pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
				}
				else if (g_region_format == 2) // 1.6 alpha
				{
					cellPointersOffset = spool->offset + spool->roadm_size;
					cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
					cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
					pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
				}

				// read packed cell pointers
				g_levStream->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// unpack cell pointers so we can use them
				cellPointerCount = UnpackCellPointers(sPosIdx, 0, 0, g_cell_ptrs, g_packed_cell_pointers);

				// read cell data
				g_levStream->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cells, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// read cell objects
				g_levStream->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_objects + g_numStraddlers, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));
			}
			else if (g_format == 1) // Driver 1
			{
				//
				// Driver 1 use CELL_OBJECTS directly - 16 bytes, wasteful in RAM
				//

				memset(g_cell_objects + g_numStraddlers, 0, sizeof(g_cell_objects) - g_numStraddlers * sizeof(CELL_OBJECT));

				int cellPointersOffset = spool->offset + spool->roadm_size + spool->roadh_size; // SKIP road map
				int cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
				int cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
				int pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2]; // FIXME: is it even there in Driver 1?

																				  // read packed cell pointers
				g_levStream->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// unpack cell pointers so we can use them
				cellPointerCount = UnpackCellPointers(sPosIdx, 0, 0, g_cell_ptrs, g_packed_cell_pointers);

				// read cell data
				g_levStream->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cells_d1, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// read cell objects
				g_levStream->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cell_objects + g_numStraddlers, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));
			}

			// read cell objects after we spool additional area data
			RegionModels_t* regModels = NULL;

			// if region data is available, load models and textures
			if (spool->super_region != SUPERREGION_NONE)
			{
				regModels = &g_regionModels[spool->super_region];

				bool isNew = (regModels->modelRefs.size() == 0);

				LoadRegionData(g_levStream, regModels, &g_areaData[spool->super_region], &g_regionPages[spool->super_region]);

				if (g_export_models && isNew)
				{
					// export region models
					for (int i = 0; i < regModels->modelRefs.size(); i++)
					{
						int mod_indxex = regModels->modelRefs[i].index;

						std::string modelFileName = varargs("%s/ZREG%d_MOD_%d", g_levname_moddir.c_str(), spool->super_region, mod_indxex);

						if (g_model_names[mod_indxex].length())
							modelFileName = varargs("%s/ZREG%d_%d_%s", g_levname_moddir.c_str(), spool->super_region, i, g_model_names[mod_indxex].c_str());

						// export model
						ExportDMODELToOBJ(regModels->modelRefs[i].model, modelFileName.c_str(), regModels->modelRefs[i].index, regModels->modelRefs[i].size);
					}
				}
			}

			int numRegionObjects = 0;

			if (g_format == 2)	// Driver 2
			{
				CELL_DATA* celld;
				PACKED_CELL_OBJECT* pcop;

				// go through all cell pointers
				for (int i = 0; i < cellPointerCount; i++)
				{
					ushort cell_ptr = g_cell_ptrs[i];
					if (cell_ptr == 0xffff)
						continue;

					// iterate just like game would
					if (pcop = GetFirstPackedCop(&celld, cell_ptr, barrel_region))
					{
						do
						{
							// unpack cell object
							CELL_OBJECT co;
							UnpackCellObject(pcop, co, regionPos);

							{
								Vector3D absCellPosition(co.pos.vx * -EXPORT_SCALING, co.pos.vy * -EXPORT_SCALING, co.pos.vz * EXPORT_SCALING);
								float cellRotationRad = co.yang / 64.0f * PI_F * 2.0f;

								if (cellsFile)
								{
									cellsFileStream.Print("# m %d r %d\r\n", co.type, co.yang);
									cellsFileStream.Print("v %g %g %g\r\n", absCellPosition.x, absCellPosition.y, absCellPosition.z);
								}

								MODEL* model = FindModelByIndex(co.type, regModels);

								if (model)
								{
									// transform objects and save
									Matrix4x4 transform = translate(absCellPosition);
									transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

									const char* modelNamePrefix = varargs("reg%d", sPosIdx);

									WriteMODELToObjStream(&levelFileStream, model, co.type, modelNamePrefix, false, transform, &lobj_first_v, &lobj_first_t, regModels);
								}

								numRegionObjects++;
							}
						} while (pcop = GetNextPackedCop(&celld, barrel_region));
					}
				}
			}
			else // Driver 1
			{
				CELL_DATA_D1* celld;

				// go through all cell pointers
				for (int i = 0; i < cellPointerCount; i++)
				{
					ushort cell_ptr = g_cell_ptrs[i];

					// iterate just like game would
					while (cell_ptr != 0xffff)
					{
						celld = &g_cells_d1[cell_ptr];

						int number = (celld->num & 0x3fff);// -g_cell_objects_add[barrel_region];

						{
							// barrel_region doesn't work here
							int val = 0;
							for (int j = 0; j < 4; j++)
							{
								if (number >= g_cell_objects_add[j])
									val = g_cell_objects_add[j];
							}

							number -= val;
						}

						CELL_OBJECT& co = g_cell_objects[number];

						{
							Vector3D absCellPosition(co.pos.vx * -EXPORT_SCALING, co.pos.vy * -EXPORT_SCALING, co.pos.vz * EXPORT_SCALING);
							float cellRotationRad = co.yang / 64.0f * PI_F * 2.0f;

							if (cellsFile)
							{
								cellsFileStream.Print("# m %d r %d\r\n", co.type, co.yang);
								cellsFileStream.Print("v %g %g %g\r\n", absCellPosition.x, absCellPosition.y, absCellPosition.z);
							}

							MODEL* model = FindModelByIndex(co.type, regModels);

							if (model)
							{
								// transform objects and save
								Matrix4x4 transform = translate(absCellPosition);
								transform = transform * rotateY4(cellRotationRad) * scale4(1.0f, 1.0f, 1.0f);

								const char* modelNamePrefix = varargs("reg%d", sPosIdx);

								WriteMODELToObjStream(&levelFileStream, model, co.type, modelNamePrefix, false, transform, &lobj_first_v, &lobj_first_t, regModels);
							}

							numRegionObjects++;
						}

						cell_ptr = celld->next_ptr;

						if (cell_ptr != 0xffff)
						{
							// barrel_region doesn't work here
							int val = 0;
							for (int j = 0; j < 4; j++)
							{
								if (cell_ptr >= g_cell_slots_add[j])
									val = g_cell_slots_add[j];
							}

							cell_ptr -= val;
						}
					}
				}
			}

			if (cellsFile)
			{
				cellsFileStream.Print("# total region cells %d\r\n", numRegionObjects);
			}

			numCellObjectsRead += numRegionObjects;
		}
	}

	fclose(cellsFile);
	fclose(levelFile);

	int numCellsObjectsFile = g_mapInfo.num_cell_objects;

	if (numCellObjectsRead != numCellsObjectsFile)
		MsgError("numAllObjects mismatch: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);
}