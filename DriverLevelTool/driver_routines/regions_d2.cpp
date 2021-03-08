
#include "regions_d2.h"


#include "driver_level.h"
#include "level.h"
#include "core/cmdlib.h"
#include "core/IVirtualStream.h"

void CDriver2LevelRegion::FreeAll()
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

void CDriver2LevelRegion::LoadRegionData(IVirtualStream* pFile, Spool* spool)
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
	pFile->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
	pFile->Read(packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

	// unpack cell pointers so we can use them
	if (UnpackCellPointers(m_cellPointers, packed_cell_pointers, 0, 0) != -1)
	{
		// read cell data
		m_cells = (CELL_DATA*)malloc(spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE);
		pFile->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
		pFile->Read(m_cells, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

		// read cell objects
		m_cellObjects = (PACKED_CELL_OBJECT*)malloc(spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE);
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

PACKED_CELL_OBJECT* CDriver2LevelRegion::GetCellObject(int num) const
{
	CDriver2LevelMap* owner = (CDriver2LevelMap*)m_owner;

	int numStraddlers = owner->m_numStraddlers;

	if (num >= numStraddlers)
	{
		num -= owner->m_cell_objects_add[m_regionBarrelNumber] + numStraddlers;
		return &m_cellObjects[num];
	}

	return &owner->m_straddlers[num];
}


//-------------------------------------------------------------------------------------------

void CDriver2LevelMap::FreeAll()
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
void CDriver2LevelMap::LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	CBaseLevelMap::LoadMapLump(pFile);

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
}

//-------------------------------------------------------------
// Load spool info, Driver 2 version
//-------------------------------------------------------------
void CDriver2LevelMap::LoadSpoolInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	CBaseLevelMap::LoadSpoolInfoLump(pFile);

	// Init regions
	int total_regions = m_regions_across * m_regions_down;

	m_regions = new CDriver2LevelRegion[total_regions];

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

CBaseLevelRegion* CDriver2LevelMap::GetRegion(const XZPAIR& cell) const
{
	// lookup region
	const int region_x = cell.x / m_mapInfo.region_size;
	const int region_z = cell.z / m_mapInfo.region_size;

	return &m_regions[region_x + region_z * m_regions_across];
}

CBaseLevelRegion* CDriver2LevelMap::GetRegion(int regionIdx) const
{
	return &m_regions[regionIdx];
}

void CDriver2LevelMap::SpoolRegion(const XZPAIR& cell)
{
	CDriver2LevelRegion* region = (CDriver2LevelRegion*)GetRegion(cell);

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
	CDriver2LevelRegion* region = (CDriver2LevelRegion*)GetRegion(regionIdx);

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
PACKED_CELL_OBJECT* CDriver2LevelMap::GetFirstPackedCop(CELL_ITERATOR* iterator, int cellx, int cellz) const
{
	// lookup region
	const int region_x = cellx / m_mapInfo.region_size;
	const int region_z = cellz / m_mapInfo.region_size;

	int regionIdx = region_x + region_z * m_regions_across;

	CDriver2LevelRegion& region = m_regions[regionIdx];

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