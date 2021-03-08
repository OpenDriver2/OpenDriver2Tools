#ifndef REGION_D1
#define REGION_D1
#include "regions.h"


//----------------------------------------------------------------------------------
// DRIVER 1
//----------------------------------------------------------------------------------

// Driver 1 region
class CDriver1LevelRegion : public CBaseLevelRegion
{
	friend class CDriver1LevelMap;
public:
	void					FreeAll() override;
	void					LoadRegionData(IVirtualStream* pFile, Spool* spool) override;

	CELL_OBJECT*			GetCellObject(int num) const;

protected:
	CELL_DATA*				m_cells{ nullptr };				// cell data that holding information about cell pointers. 3D world seeks cells first here
	CELL_OBJECT*			m_cellObjects{ nullptr };		// cell objects that represents objects placed in the world
};

struct CELL_ITERATOR_D1
{
	CDriver1LevelRegion*	region;
	CELL_DATA_D1*			pcd;
	CELL_OBJECT*			pco;
	XZPAIR					nearCell;
};

// Driver 1 level map
class CDriver1LevelMap : public CBaseLevelMap
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
	PACKED_CELL_OBJECT*		GetFirstPackedCop(CELL_ITERATOR_D1* iterator, int cellx, int cellz) const;
	PACKED_CELL_OBJECT*		GetNextPackedCop(CELL_ITERATOR_D1* iterator) const;

protected:

	// Driver 2 - specific
	CDriver1LevelRegion*	m_regions{ nullptr };					// map of regions
	CELL_OBJECT*			m_straddlers{ nullptr };				// cell objects between regions
};


#endif