#include "regions.h"

#include <malloc.h>


#include "driver_level.h"
#include "textures.h"

#include "core/IVirtualStream.h"
#include "core/cmdlib.h"

// loaded headers
LEVELINFO				g_levInfo;

CDriver2LevelRegion::CDriver2LevelRegion()
{
}

CDriver2LevelRegion::~CDriver2LevelRegion()
{
	FreeAll();
}

void CDriver2LevelRegion::FreeAll()
{
	if(m_models)
	{
		for (int i = 0; i < MAX_MODELS; i++)
		{
			if (m_models[i].model)
				free(m_models[i].model);
		}
	}

	delete[] m_models;
	m_models = nullptr;

	// do I need that?
	if(m_areaDataNum != -1)
	{
		int numAreaTpages = m_owner->m_areaData[m_areaDataNum].num_tpages;
		AreaTPage_t& areaTPages = m_owner->m_areaTPages[m_areaDataNum];

		for (int i = 0; numAreaTpages; i++)
		{
			if (areaTPages.pageIndexes[i] == 0xFF)
				break;
			
			CTexturePage* tpage = g_levTextures.GetTPage(areaTPages.pageIndexes[i]);
			tpage->FreeBitmap();
		}
	}
	m_areaDataNum = -1;

	if(m_cells)
		free(m_cells);
	m_cells = nullptr;

	delete [] m_cellPointers;
	m_cellPointers = nullptr;

	if (m_cellObjects)
		free(m_cellObjects);
	m_cellObjects = nullptr;

	m_pvsData = nullptr;	
	m_roadmapData = nullptr;

	m_loaded = false;
}

//-------------------------------------------------------------
// Region unpacking function
//-------------------------------------------------------------
int CDriver2LevelRegion::UnpackCellPointers(ushort* dest_ptrs, char* src_data, int cell_slots_add, int targetRegion)
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
		numCellPointers = -1;
	}

	return numCellPointers;
}

void CDriver2LevelRegion::LoadRegionData(IVirtualStream* pFile, Spool* spool)
{
	//Msg("---------\nSpool %d %d (offset: %d)\n", x, y, g_regionSpoolInfoOffsets[sPosIdx]);
	Msg(" - offset: %d\n", spool->offset);

	for (int i = 0; i < spool->num_connected_areas && i < 2; i++)
		Msg(" - connected area %d: %d\n", i, spool->connected_areas[i]);

	Msg(" - pvs_size: %d\n", spool->pvs_size);
	Msg(" - cell_data_size: %d %d %d\n", spool->cell_data_size[0], spool->cell_data_size[1], spool->cell_data_size[2]);

	Msg(" - super_region: %d\n", spool->super_region);

	// LoadRegionData - calculate offsets
	Msg(" - cell pointers size: %d\n", spool->cell_data_size[1]);
	Msg(" - cell data size: %d\n", spool->cell_data_size[0]);
	Msg(" - cell objects size: %d\n", spool->cell_data_size[2]);
	Msg(" - PVS data size: %d\n", spool->roadm_size);

	//
	// Driver 2 use PACKED_CELL_OBJECTS - 8 bytes. not wasting, but tricky
	//

	int cellPointersOffset;
	int cellDataOffset;
	int cellObjectsOffset;
	int pvsDataOffset;

	if (g_format == LEV_FORMAT_DRIVER2_RETAIL) // retail
	{
		cellPointersOffset = spool->offset; // SKIP PVS buffers, no need rn...
		cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
		cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
		pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
	}
	else // 1.6 alpha
	{
		cellPointersOffset = spool->offset + spool->roadm_size;
		cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
		cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
		pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
	}

	char* packed_cell_pointers = new char[spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE];
	m_cellPointers = new ushort[m_owner->m_mapInfo.region_size * m_owner->m_mapInfo.region_size];

	memset(m_cellPointers, 0xFF, sizeof(ushort) * m_owner->m_mapInfo.region_size * m_owner->m_mapInfo.region_size);
	
	// read packed cell pointers
	g_levStream->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
	g_levStream->Read(packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

	// unpack cell pointers so we can use them
	if(UnpackCellPointers(m_cellPointers, packed_cell_pointers, 0, 0) != -1)
	{
		// read cell data
		m_cells = (CELL_DATA*)malloc(spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE);
		g_levStream->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
		g_levStream->Read(m_cells, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

		// read cell objects
		m_cellObjects = (PACKED_CELL_OBJECT*)malloc(spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE);
		g_levStream->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
		g_levStream->Read(m_cellObjects, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));
	}
	else
		MsgError("BAD PACKED CELL POINTER DATA, region = %d\n", m_regionNumber);

	delete packed_cell_pointers;

	// even if error occured we still need it to be here
	m_loaded = true;

	// TODO: PVS and heightmap data

#if 0
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
#endif
}

void CDriver2LevelRegion::LoadAreaData(IVirtualStream* pFile)
{
	// load in-area textures
	m_owner->LoadInAreaTPages(pFile, m_areaDataNum);

	// load in-area models
	m_models = m_owner->LoadInAreaModels(pFile, m_areaDataNum);
}

PACKED_CELL_OBJECT* CDriver2LevelRegion::GetCellObject(int num) const
{
	int numStraddlers = m_owner->m_numStraddlers;

	if (num >= numStraddlers)
	{
		num -= m_owner->m_cell_objects_add[m_regionBarrelNumber] + numStraddlers;
		return &m_cellObjects[num];
	}

	return &m_owner->m_straddlers[num];
}

ModelRef_t* CDriver2LevelRegion::GetModel(int num) const
{
	if (!m_models)
		return nullptr;
	
	ModelRef_t* ref = &m_models[num];

	if (!ref->model)
		return nullptr;
	
	return ref;
}

//-------------------------------------------------------------------------------------------

CDriver2LevelMap::CDriver2LevelMap()
{
}

CDriver2LevelMap::~CDriver2LevelMap()
{
}

void CDriver2LevelMap::FreeAll()
{
	int total_regions = m_regions_across * m_regions_down;

	if(m_regions)
	{
		for (int i = 0; i < total_regions; i++)
			m_regions[i].FreeAll();
	}

	delete[] m_regions;
	m_regions = nullptr;
	
	delete [] m_straddlers;
	m_straddlers = nullptr;

	if (m_regionSpoolInfo)
		free(m_regionSpoolInfo);
	m_regionSpoolInfo = nullptr;

	delete[] m_regionSpoolInfoOffsets;
	m_regionSpoolInfoOffsets = nullptr;

	delete[] m_areaTPages;
	m_areaTPages = nullptr;

	delete[] m_areaData;
	m_areaData = nullptr;
}


//-------------------------------------------------------------
// parses LUMP_MAP and it's straddler objects
//-------------------------------------------------------------
void CDriver2LevelMap::LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	pFile->Read(&m_mapInfo, 1, sizeof(OUT_CELL_FILE_HEADER));

	Msg("Level dimensions[%d %d], cell size: %d\n", m_mapInfo.cells_across, m_mapInfo.cells_down, m_mapInfo.cell_size);
	Msg(" - num_regions: %d\n", m_mapInfo.num_regions);
	Msg(" - region_size in cells: %d\n", m_mapInfo.region_size);

	int dim_x = m_mapInfo.cells_across / m_mapInfo.region_size;
	int dim_y = m_mapInfo.cells_down / m_mapInfo.region_size;

	Msg("World size:\n [%dx%d] cells\n [%dx%d] regions\n", m_mapInfo.cells_across, m_mapInfo.cells_down, dim_x, dim_y);
	
	Msg(" - num_cell_objects : %d\n", m_mapInfo.num_cell_objects);
	Msg(" - num_cell_data: %d\n", m_mapInfo.num_cell_data);

	// ProcessMapLump
	pFile->Read(&m_numStraddlers, 1, sizeof(m_numStraddlers));
	Msg(" - num straddler cells: %d\n", m_numStraddlers);
	
	const int pvs_square = 21;
	const int pvs_square_sq = pvs_square*pvs_square;

	// InitCellData actually here, but...

	// read straddlers
	// Driver 2 PCO
	m_straddlers = new PACKED_CELL_OBJECT[m_numStraddlers];
	pFile->Read(m_straddlers, m_numStraddlers, sizeof(PACKED_CELL_OBJECT));
	
#if 0
	// Driver 1 CO
	m_straddlers = malloc(sizeof(CELL_OBJECT) * m_numStraddlers);
	pFile->Read(m_straddlers, 1, sizeof(CELL_OBJECT) * m_numStraddlers);
#endif

	pFile->Seek(l_ofs, VS_SEEK_SET);

	// initialize regions
	m_regions_across = m_mapInfo.cells_across / m_mapInfo.region_size;
	m_regions_down = m_mapInfo.cells_down / m_mapInfo.region_size;
}

//-------------------------------------------------------------
// parses LUMP_SPOOLINFO, and also loads region data
//-------------------------------------------------------------
void CDriver2LevelMap::LoadSpoolInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int model_spool_buffer_size;
	pFile->Read(&model_spool_buffer_size, 1, sizeof(int));
	Msg("model_spool_buffer_size = %d * SPOOL_CD_BLOCK_SIZE\n", model_spool_buffer_size);

	int Music_And_AmbientOffsetsSize;
	pFile->Read(&Music_And_AmbientOffsetsSize, 1, sizeof(int));
	Msg("Music_And_AmbientOffsetsSize = %d\n", Music_And_AmbientOffsetsSize);

	// move further
	// this was probably used in early D1 level files for sound banks
	pFile->Seek(Music_And_AmbientOffsetsSize, VS_SEEK_CUR);

	pFile->Read(&m_numAreas, 1, sizeof(int));
	Msg("NumAreas = %d\n", m_numAreas);

	m_areaData = new AreaDataStr[m_numAreas];
	m_areaTPages = new AreaTPage_t[m_numAreas];

	// read local geometry and texture pages
	pFile->Read(m_areaData, m_numAreas, sizeof(AreaDataStr));
	pFile->Read(m_areaTPages, m_numAreas, sizeof(AreaTPage_t));

	for (int i = 0; i < 5; i++)
	{
		m_cell_slots_add[i] = 0;
		m_cell_objects_add[i] = 0;
	}

	// read slots count
	for (int i = 0; i < 4; i++)
	{
		int slots_count;
		pFile->Read(&slots_count, 1, sizeof(int));
		m_cell_slots_add[i] = m_cell_slots_add[4];
		m_cell_slots_add[4] += slots_count;
	}

	// read objects count
	for (int i = 0; i < 4; i++)
	{
		int objects_count;
		pFile->Read(&objects_count, 1, sizeof(int));
		m_cell_objects_add[i] = m_cell_objects_add[4];
		m_cell_objects_add[4] += objects_count;
	}

	// read pvs sizes
	for (int i = 0; i < 4; i++)
	{
		int pvs_size;
		pFile->Read(&pvs_size, 1, sizeof(int));

		m_PVS_size[i] = pvs_size + 0x7ff & 0xfffff800;
	}
 
	Msg("cell_slots_add = {%d,%d,%d,%d}\n", m_cell_slots_add[0], m_cell_slots_add[1], m_cell_slots_add[2], m_cell_slots_add[3]);
	Msg("cell_objects_add = {%d,%d,%d,%d}\n", m_cell_objects_add[0], m_cell_objects_add[1], m_cell_objects_add[2], m_cell_objects_add[3]);
	Msg("PVS_size = {%d,%d,%d,%d}\n", m_PVS_size[0], m_PVS_size[1], m_PVS_size[2], m_PVS_size[3]);

	// ... but InitCellData is here
	{
		int maxCellData = m_numStraddlers + m_cell_slots_add[4];
		Msg("*** MAX cell slots = %d\n", maxCellData);

		// I don't have any idea
		pFile->Read(&m_numSpoolInfoOffsets, 1, sizeof(int));
		Msg("numRegionOffsets: %d\n", m_numSpoolInfoOffsets);
	}

	m_regionSpoolInfoOffsets = new ushort[m_numSpoolInfoOffsets];
	pFile->Read(m_regionSpoolInfoOffsets, m_numSpoolInfoOffsets, sizeof(short));

	int regionsInfoSize;
	pFile->Read(&regionsInfoSize, 1, sizeof(int));
	m_numRegionSpools = regionsInfoSize / sizeof(AreaDataStr);

	//ASSERT(regionsInfoSize % sizeof(REGIONINFO) == 0);

	Msg("Region spool count %d (size=%d bytes)\n", m_numRegionSpools, regionsInfoSize);

	m_regionSpoolInfo = (Spool*)malloc(regionsInfoSize);
	pFile->Read(m_regionSpoolInfo, 1, regionsInfoSize);

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);

	// Init regions
	int total_regions = m_regions_across * m_regions_down;

	m_regions = new CDriver2LevelRegion[total_regions];

	for (int i = 0; i < total_regions; i++)
	{
		ushort spoolInfoOffset = m_regionSpoolInfoOffsets[i];

		const int region_x = i % m_regions_across;
		const int region_z = (i - region_x) / m_regions_across;
		
		m_regions[i].m_owner = this;
		m_regions[i].m_regionX = region_x;
		m_regions[i].m_regionZ = region_z;
		m_regions[i].m_regionNumber = i;
		m_regions[i].m_regionBarrelNumber = (region_x & 1) + (region_z & 1) * 2;

		if (spoolInfoOffset != REGION_EMPTY)
		{
			Spool* spool = (Spool*)((ubyte*)m_regionSpoolInfo + m_regionSpoolInfoOffsets[i]);
			m_regions[i].m_areaDataNum = spool->super_region;
		}
		else
			m_regions[i].m_areaDataNum = -1;
	}
}

int	CDriver2LevelMap::GetAreaDataCount() const
{
	return m_numAreas;
}

void CDriver2LevelMap::LoadInAreaTPages(IVirtualStream* pFile, int areaDataNum) const
{
	if (areaDataNum == -1)
		return;
	
	AreaDataStr& areaData = m_areaData[areaDataNum];
	AreaTPage_t& areaTPages = m_areaTPages[areaDataNum];
	
	const int texturesOffset = g_levInfo.spooldata_offset + SPOOL_CD_BLOCK_SIZE * areaData.gfx_offset;

	pFile->Seek(texturesOffset, VS_SEEK_SET);

	for (int i = 0; areaData.num_tpages; i++)
	{
		if (areaTPages.pageIndexes[i] == 0xFF)
			break;

		CTexturePage* tpage = g_levTextures.GetTPage(areaTPages.pageIndexes[i]);
		tpage->LoadTPageAndCluts(pFile, true);

		if (pFile->Tell() % SPOOL_CD_BLOCK_SIZE)
			pFile->Seek(SPOOL_CD_BLOCK_SIZE - (pFile->Tell() % SPOOL_CD_BLOCK_SIZE), VS_SEEK_CUR);
	}
}

ModelRef_t* CDriver2LevelMap::LoadInAreaModels(IVirtualStream* pFile, int areaDataNum) const
{
	if (areaDataNum == -1)
		return nullptr;
	
	// allocate model reference slots
	ModelRef_t* newModels = new ModelRef_t[MAX_MODELS];
	memset(newModels, 0, sizeof(ModelRef_t) * MAX_MODELS);

	const int modelsCountOffset = g_levInfo.spooldata_offset + SPOOL_CD_BLOCK_SIZE * (m_areaData->model_offset + m_areaData->model_size - 1);
	const int modelsOffset = g_levInfo.spooldata_offset + SPOOL_CD_BLOCK_SIZE * m_areaData->model_offset;

	ushort numModels;

	pFile->Seek(modelsCountOffset, VS_SEEK_SET);
	pFile->Read(&numModels, 1, sizeof(ushort));

	// read model indexes
	ushort* new_model_numbers = new ushort[numModels];
	pFile->Read(new_model_numbers, numModels, sizeof(short));

	MsgInfo("	model count: %d\n", numModels);
	pFile->Seek(modelsOffset, VS_SEEK_SET);

	for (int i = 0; i < numModels; i++)
	{
		int modelSize;
		pFile->Read(&modelSize, sizeof(int), 1);

		if (modelSize > 0)
		{
			ModelRef_t& ref = newModels[new_model_numbers[i]];

			ref.index = new_model_numbers[i];
			ref.model = (MODEL*)malloc(modelSize);
			ref.size = modelSize;

			pFile->Read(ref.model, modelSize, 1);
		}
	}

	delete[] new_model_numbers;

	return newModels;
}

CDriver2LevelRegion* CDriver2LevelMap::GetRegion(const XZPAIR& cell) const
{
	// lookup region
	const int region_x = cell.x / m_mapInfo.region_size;
	const int region_z = cell.z / m_mapInfo.region_size;

	return &m_regions[region_x + region_z * m_regions_across];
}

CDriver2LevelRegion* CDriver2LevelMap::GetRegion(int regionIdx) const
{
	return &m_regions[regionIdx];
}

void CDriver2LevelMap::SpoolRegion(const XZPAIR& cell)
{
	CDriver2LevelRegion* region = GetRegion(cell);

	if (!region->m_loaded)
	{
		if (m_regionSpoolInfoOffsets[region->m_regionNumber] != REGION_EMPTY)
		{
			Spool* spool = (Spool*)((ubyte*)m_regionSpoolInfo + m_regionSpoolInfoOffsets[region->m_regionNumber]);
			region->LoadRegionData(g_levStream, spool);
			region->LoadAreaData(g_levStream);
		}
		else
			region->m_loaded = true;
	}
}

void CDriver2LevelMap::SpoolRegion(int regionIdx)
{
	CDriver2LevelRegion* region = GetRegion(regionIdx);

	if(!region->m_loaded)
	{
		if (m_regionSpoolInfoOffsets[region->m_regionNumber] != REGION_EMPTY)
		{
			Spool* spool = (Spool*)((ubyte*)m_regionSpoolInfo + m_regionSpoolInfoOffsets[region->m_regionNumber]);
			region->LoadRegionData(g_levStream, spool);
			region->LoadAreaData(g_levStream);
		}
		else
			region->m_loaded = true;
	}
}

//-------------------------------------------------------------
// returns first cell object of cell
//-------------------------------------------------------------
PACKED_CELL_OBJECT* CDriver2LevelMap::GetFirstPackedCop(CELL_ITERATOR* iterator, int cellx, int cellz) const
{
	// lookup region
	const int region_x = cellx / m_mapInfo.region_size;
	const int region_z = cellz / m_mapInfo.region_size;
	
	int regionIdx = region_x + region_z * m_regions_across;

	CDriver2LevelRegion& region = m_regions[regionIdx];
	
	iterator->region = &region;

	// don't do anything on empty or non-spooled regions
	if(!region.m_cells)
		return nullptr;

	// get cell index on the region
	const int region_square = m_mapInfo.region_size * m_mapInfo.region_size;
	const int region_cell_x = cellx % m_mapInfo.region_size;
	const int region_cell_z = cellz % m_mapInfo.region_size;

	// FIXME: might be incorrect
	int cell_index = region_cell_x + region_cell_z * m_mapInfo.region_size;

	ushort cell_ptr = region.m_cellPointers[cell_index];

	if (cell_ptr == 0xFFFF)
		return nullptr;
	
	// get the near cell position in the world
	iterator->nearCell.x = (cellx - (m_mapInfo.cells_across / 2)) * m_mapInfo.cell_size;
	iterator->nearCell.z = (cellz - (m_mapInfo.cells_down / 2)) * m_mapInfo.cell_size;

	// get the packed cell data start and near cell
	CELL_DATA& cell = region.m_cells[cell_ptr];

	if (cell.num & 0x4000)
		return nullptr;

	PACKED_CELL_OBJECT* ppco = region.GetCellObject(cell.num & 0x3fff);

	iterator->pcd = &cell;
	
	if (ppco->value == 0xffff && (ppco->pos.vy & 1))
		ppco = GetNextPackedCop(iterator);
	
	iterator->ppco = ppco;
	
	return ppco;
}

//-------------------------------------------------------------
// iterates cell objects
//-------------------------------------------------------------
PACKED_CELL_OBJECT* CDriver2LevelMap::GetNextPackedCop(CELL_ITERATOR* iterator) const
{
	ushort num;
	PACKED_CELL_OBJECT* ppco;

	do {
		if (iterator->pcd->num & 0x8000)
			return nullptr;

		iterator->pcd++;
		num = iterator->pcd->num;

		if (num & 0x4000)
			return nullptr;

		ppco = iterator->region->GetCellObject(num & 0x3fff);
		if (!ppco)
			return nullptr;
	} while (ppco->value == 0xffff && (ppco->pos.vy & 1));

	iterator->ppco = ppco;

	return ppco;
}

//-------------------------------------------------------------
// Unpacks cell object (Driver 2 ONLY)
//-------------------------------------------------------------
bool CDriver2LevelMap::UnpackCellObject(CELL_OBJECT& co, PACKED_CELL_OBJECT* pco, const XZPAIR& nearCell)
{
	if (!pco)
		return false;

	co.pos.vx = nearCell.x + (((pco->pos.vx - nearCell.x) << 0x10) >> 0x10);
	co.pos.vz = nearCell.z + (((pco->pos.vz - nearCell.z) << 0x10) >> 0x10);

	co.pos.vy = (pco->pos.vy << 0x10) >> 0x11;
	co.yang = pco->value & 0x3f;
	co.type = (pco->value >> 6) | ((pco->pos.vy & 1) << 10);

	return true;
}

int	CDriver2LevelMap::GetCellsAcross() const
{
	return m_mapInfo.cells_across;
}

int	CDriver2LevelMap::GetCellsDown() const
{
	return m_mapInfo.cells_down;
}

void CDriver2LevelMap::WorldPositionToCellXZ(XZPAIR& cell, const VECTOR_NOPAD& position) const
{
	// @TODO: constants
	int units_across_halved = m_mapInfo.cells_across / 2 * m_mapInfo.cell_size;
	int units_down_halved = m_mapInfo.cells_down / 2 * m_mapInfo.cell_size;
	
	cell.x = (position.vx + units_across_halved) / m_mapInfo.cell_size;
	cell.z = (position.vz + units_down_halved) / m_mapInfo.cell_size;
}