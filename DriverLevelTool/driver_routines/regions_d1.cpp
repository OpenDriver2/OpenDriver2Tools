#include "regions_d1.h"

#include "driver_level.h"
#include "level.h"
#include "core/cmdlib.h"
#include "core/IVirtualStream.h"

void CDriver1LevelRegion::FreeAll()
{
	if (!m_loaded)
		return;

	CBaseLevelRegion::FreeAll();

	if (m_cells)
		free(m_cells);
	m_cells = nullptr;

	if (m_cellObjects)
		free(m_cellObjects);
	m_cellObjects = nullptr;
}

void CDriver1LevelRegion::LoadRegionData(IVirtualStream* pFile, Spool* spool)
{
	m_spoolInfo = spool;

	Msg("---------\nSpool %d %d\n", m_regionX, m_regionZ);
	
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
	Msg(" - PVS data size: %d\n", spool->pvs_size);
	Msg(" - roadmap data size: %dx%d\n", spool->roadm_size, spool->roadh_size);

	//
	// Driver 1 use CELL_OBJECTS directly - 16 bytes, wasteful in RAM
	//

	int cellPointersOffset = spool->offset + spool->roadm_size + spool->roadh_size; // SKIP road map
	int cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
	int cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
	int pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2]; // FIXME: is it even there in Driver 1?

	char* packed_cell_pointers = new char[spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE];
	
	m_cellPointers = new ushort[m_owner->m_cell_objects_add[2]];
	memset(m_cellPointers, 0xFF, sizeof(ushort) * m_owner->m_cell_objects_add[2]);

	// read packed cell pointers
	pFile->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
	pFile->Read(packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

	// unpack cell pointers so we can use them
	if (UnpackCellPointers(m_cellPointers, packed_cell_pointers, 0, 0) != -1)
	{
		// read cell data
		m_cells = (CELL_DATA_D1*)malloc(spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE);
		pFile->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
		pFile->Read(m_cells, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

		// read cell objects
		m_cellObjects = (CELL_OBJECT*)malloc(spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE * 2);
		pFile->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
		pFile->Read(m_cellObjects, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));
	}
	else
		MsgError("BAD PACKED CELL POINTER DATA, region = %d\n", m_regionNumber);

	delete [] packed_cell_pointers;

	// even if error occured we still need it to be here
	m_loaded = true;

	m_owner->OnRegionLoaded(this);

	// TODO: PVS and heightmap data
}

CELL_OBJECT* CDriver1LevelRegion::GetCellObject(int num) const
{
	CDriver1LevelMap* owner = (CDriver1LevelMap*)m_owner;

	int numStraddlers = owner->m_numStraddlers;

	if (num >= numStraddlers)
	{
		num -= owner->m_cell_objects_add[m_regionBarrelNumber] + numStraddlers;
		return &m_cellObjects[num];
	}

	return &owner->m_straddlers[num];
}


//-------------------------------------------------------------------------------------------

void CDriver1LevelMap::FreeAll()
{
	int total_regions = m_regions_across * m_regions_down;

	if (m_regions)
	{
		for (int i = 0; i < total_regions; i++)
			m_regions[i].FreeAll();
	}

	delete[] m_regions;
	m_regions = nullptr;

	delete[] m_straddlers;
	m_straddlers = nullptr;

	CBaseLevelMap::FreeAll();
}

//-------------------------------------------------------------
// Loads map lump, Driver 2 version
//-------------------------------------------------------------
void CDriver1LevelMap::LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	CBaseLevelMap::LoadMapLump(pFile);

	// read straddlers
	// Driver 1 CO
	m_straddlers = new CELL_OBJECT[m_numStraddlers];
	pFile->Read(m_straddlers, m_numStraddlers, sizeof(CELL_OBJECT));

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// Load spool info, Driver 2 version
//-------------------------------------------------------------
void CDriver1LevelMap::LoadSpoolInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	CBaseLevelMap::LoadSpoolInfoLump(pFile);

	// Init regions
	int total_regions = m_regions_across * m_regions_down;

	m_regions = new CDriver1LevelRegion[total_regions];

	for (int i = 0; i < total_regions; i++)
	{
		const int region_x = i % m_regions_across;
		const int region_z = (i - region_x) / m_regions_across;

		m_regions[i].m_owner = this;
		m_regions[i].m_regionX = region_x;
		m_regions[i].m_regionZ = region_z;
		m_regions[i].m_regionNumber = i;
		m_regions[i].m_regionBarrelNumber = (region_x & 1) + (region_z & 1) * 2;
	}

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

CBaseLevelRegion* CDriver1LevelMap::GetRegion(const XZPAIR& cell) const
{
	// lookup region
	const int region_x = cell.x / m_mapInfo.region_size;
	const int region_z = cell.z / m_mapInfo.region_size;

	return &m_regions[region_x + region_z * m_regions_across];
}

CBaseLevelRegion* CDriver1LevelMap::GetRegion(int regionIdx) const
{
	return &m_regions[regionIdx];
}

void CDriver1LevelMap::SpoolRegion(const XZPAIR& cell)
{
	CDriver1LevelRegion* region = (CDriver1LevelRegion*)GetRegion(cell);

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

void CDriver1LevelMap::SpoolRegion(int regionIdx)
{
	CDriver1LevelRegion* region = (CDriver1LevelRegion*)GetRegion(regionIdx);

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

//-------------------------------------------------------------
// returns first cell object of cell
//-------------------------------------------------------------
CELL_OBJECT* CDriver1LevelMap::GetFirstCop(CELL_ITERATOR_D1* iterator, int cellx, int cellz) const
{
	// lookup region
	const int region_x = cellx / m_mapInfo.region_size;
	const int region_z = cellz / m_mapInfo.region_size;

	int regionIdx = region_x + region_z * m_regions_across;

	CDriver1LevelRegion& region = m_regions[regionIdx];

	iterator->region = &region;

	// don't do anything on empty or non-spooled regions
	if (!region.m_cells)
		return nullptr;

	// get cell index on the region
	const int region_cell_x = cellx % m_mapInfo.region_size;
	const int region_cell_z = cellz % m_mapInfo.region_size;

	// FIXME: might be incorrect
	int cell_index = region_cell_x + region_cell_z * m_mapInfo.region_size;

	ushort cell_ptr = region.m_cellPointers[cell_index];

	if (cell_ptr == 0xFFFF)
		return nullptr;

	// get the packed cell data start and near cell
	CELL_DATA_D1& cell = region.m_cells[cell_ptr];
	
	CELL_OBJECT* pco = region.GetCellObject(cell.num & 0x3fff);

	iterator->pcd = &cell;
	iterator->pco = pco;

	return pco;
}

//-------------------------------------------------------------
// iterates cell objects
//-------------------------------------------------------------
CELL_OBJECT* CDriver1LevelMap::GetNextCop(CELL_ITERATOR_D1* iterator) const
{
	ushort cell_ptr = iterator->pcd->next_ptr;

	if(cell_ptr != 0xFFFF)
	{
		CDriver1LevelRegion* region = iterator->region;
		cell_ptr -= m_cell_slots_add[region->m_regionBarrelNumber];
		
		// get the packed cell data start and near cell
		CELL_DATA_D1& cell = region->m_cells[cell_ptr];
		iterator->pcd = &cell;
		
		CELL_OBJECT* pco = region->GetCellObject(cell.num & 0x3fff);
		iterator->pco = pco;

		return pco;
	}

	return nullptr;
}