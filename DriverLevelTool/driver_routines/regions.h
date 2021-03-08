#ifndef REGIONS_H
#define REGIONS_H

#include "math/dktypes.h"
#include "models.h"
#include "util/DkList.h"

//------------------------------------------------------------------------------------------------------------

// forward
class IVirtualStream;
class CDriver2LevelRegion;

// OBSOLETE
struct RegionModels_t
{
	DkList<ModelRef_t> modelRefs;
};

typedef void (*OnRegionLoaded_t)(CDriver2LevelRegion* region);
typedef void (*OnRegionFreed_t)(CDriver2LevelRegion* region);

struct CELL_ITERATOR
{
	CDriver2LevelRegion* region;
	CELL_DATA* pcd;
	PACKED_CELL_OBJECT* ppco;
	XZPAIR nearCell;
};

//----------------------------------------------------------------------------------

// Driver 2 region
class CDriver2LevelRegion
{
	friend class CDriver2LevelMap;
public:
	CDriver2LevelRegion();
	virtual ~CDriver2LevelRegion();

	void					FreeAll();

	void					LoadRegionData(IVirtualStream* pFile, Spool* spool);
	void					LoadAreaData(IVirtualStream* pFile);

	PACKED_CELL_OBJECT*		GetCellObject(int num) const;
	ModelRef_t*				GetModel(int num) const;
	
protected:
	static int				UnpackCellPointers(ushort* dest_ptrs, char* src_data, int cell_slots_add, int targetRegion = 0);
	
	ModelRef_t*				m_models{ nullptr };			// loaded region models

	CDriver2LevelMap*		m_owner;

	CELL_DATA*				m_cells{ nullptr };			// cell data that holding information about cell pointers. 3D world seeks cells first here
	ushort*					m_cellPointers{ nullptr };		// cell pointers that holding indexes of cell objects. Designed for cell iterator
	PACKED_CELL_OBJECT*		m_cellObjects{ nullptr };		// cell objects that represents objects placed in the world

	ushort*					m_pvsData{ nullptr };			// potentially visibile set of cells
	short*					m_roadmapData{ nullptr };		// heightfield with planes and BSP

	int						m_regionX{ -1 };
	int						m_regionZ{ -1 };
	int						m_regionNumber{ -1 };
	int						m_areaDataNum{ -1 };
	int						m_regionBarrelNumber{ -1 };
	bool					m_loaded{ false };
};

//----------------------------------------------------------------------------------

// Driver 2 level map iterator
class CDriver2LevelMap
{
	friend class CDriver2LevelRegion;
public:
	CDriver2LevelMap();
	virtual ~CDriver2LevelMap();

	void					FreeAll();

	//----------------------------------------
	void					SetLoadingCallbacks(OnRegionLoaded_t onLoaded, OnRegionFreed_t onFreed);
	
	//----------------------------------------

	void					LoadMapLump(IVirtualStream* pFile);
	void					LoadSpoolInfoLump(IVirtualStream* pFile);

	int						GetAreaDataCount() const;
	void					LoadInAreaTPages(IVirtualStream* pFile, int areaDataNum) const;
	ModelRef_t*				LoadInAreaModels(IVirtualStream* pFile, int areaDataNum) const;

	
	
	//----------------------------------------
	// cell iterator
	PACKED_CELL_OBJECT*		GetFirstPackedCop(CELL_ITERATOR* iterator, int cellx, int cellz) const;
	PACKED_CELL_OBJECT*		GetNextPackedCop(CELL_ITERATOR* iterator) const;

	static bool				UnpackCellObject(CELL_OBJECT& co, PACKED_CELL_OBJECT* pco, const XZPAIR& nearCell);

	CDriver2LevelRegion*	GetRegion(const XZPAIR& cell) const;
	CDriver2LevelRegion*	GetRegion(int regionIdx) const;

	void					SpoolRegion(const XZPAIR& cell);
	void					SpoolRegion(int regionIdx);
	
	// converters
	void					WorldPositionToCellXZ(XZPAIR& cell, const VECTOR_NOPAD& position) const;

	int						GetCellsAcross() const;
	int						GetCellsDown() const;

protected:

	void					OnRegionLoaded(CDriver2LevelRegion* region);
	void					OnRegionFreed(CDriver2LevelRegion* region);

	OUT_CELL_FILE_HEADER	m_mapInfo;

	CDriver2LevelRegion*	m_regions{ nullptr };					// map of regions
	
	PACKED_CELL_OBJECT*		m_straddlers { nullptr };				// cell objects between regions
	
	Spool*					m_regionSpoolInfo{ nullptr };			// region data info
	ushort*					m_regionSpoolInfoOffsets{ nullptr };	// region offset table
	
	AreaDataStr*			m_areaData{ nullptr };					// region model/texture data descriptors
	AreaTPage_t*			m_areaTPages{ nullptr };				// region texpage usage table

	int						m_numStraddlers{ 0 };
	
	int						m_cell_slots_add[5] { 0 };
	int						m_cell_objects_add[5] { 0 };
	int						m_PVS_size[4] { 0 };
	
	int						m_numAreas{ 0 };
	int						m_numSpoolInfoOffsets{ 0 };
	int						m_numRegionSpools{ 0 };

	int						m_regions_across{ 0 };
	int						m_regions_down{ 0 };

	OnRegionLoaded_t		m_onRegionLoaded{ nullptr };
	OnRegionFreed_t			m_onRegionFreed{ nullptr };
};

//-----------------------------------------------------------------------------------------

extern LEVELINFO				g_levInfo;

//-----------------------------------------------------------------------------------------


#endif // REGIONS_H
