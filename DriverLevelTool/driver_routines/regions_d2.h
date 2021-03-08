#ifndef REGIONS_D2
#define REGIONS_D2

#include "regions.h"

//----------------------------------------------------------------------------------
// DRIVER 2 regions and map
//----------------------------------------------------------------------------------

struct CELL_ITERATOR
{
	CDriver2LevelRegion*	region;
	CELL_DATA*				pcd;
	PACKED_CELL_OBJECT*		ppco;
	XZPAIR					nearCell;
};


// Driver 2 region
class CDriver2LevelRegion : public CBaseLevelRegion
{
	friend class CDriver2LevelMap;
public:
	void					FreeAll() override;
	void					LoadRegionData(IVirtualStream* pFile, Spool* spool) override;

	PACKED_CELL_OBJECT*		GetCellObject(int num) const;

protected:
	CELL_DATA* m_cells{ nullptr };				// cell data that holding information about cell pointers. 3D world seeks cells first here
	PACKED_CELL_OBJECT* m_cellObjects{ nullptr };		// cell objects that represents objects placed in the world
};

// Driver 2 level map
class CDriver2LevelMap : public CBaseLevelMap
{
	friend class CDriver2LevelRegion;
public:
	void					FreeAll() override;
	
	//----------------------------------------

	void 					LoadMapLump(IVirtualStream* pFile) override;
	void					LoadSpoolInfoLump(IVirtualStream* pFile) override;

	void					SpoolRegion(const XZPAIR& cell) override;
	void					SpoolRegion(int regionIdx) override;

	CBaseLevelRegion*		GetRegion(const XZPAIR& cell) const override;
	CBaseLevelRegion*		GetRegion(int regionIdx) const override;
	
	//----------------------------------------
	// cell iterator
	PACKED_CELL_OBJECT*		GetFirstPackedCop(CELL_ITERATOR* iterator, int cellx, int cellz) const;
	PACKED_CELL_OBJECT*		GetNextPackedCop(CELL_ITERATOR* iterator) const;
	static bool				UnpackCellObject(CELL_OBJECT& co, PACKED_CELL_OBJECT* pco, const XZPAIR& nearCell);

protected:
	
	// Driver 2 - specific
	CDriver2LevelRegion*	m_regions{ nullptr };					// map of regions
	PACKED_CELL_OBJECT*		m_straddlers { nullptr };				// cell objects between regions
};


#endif