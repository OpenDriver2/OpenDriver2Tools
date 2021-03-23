#ifndef REGIONS_H
#define REGIONS_H

#include "math/dktypes.h"
#include "models.h"

//------------------------------------------------------------------------------------------------------------

// forward
class IVirtualStream;

class CBaseLevelRegion;
class CBaseLevelMap;

typedef void (*OnRegionLoaded_t)(CBaseLevelRegion* region);
typedef void (*OnRegionFreed_t)(CBaseLevelRegion* region);

//----------------------------------------------------------------------------------

class CBaseLevelRegion
{
	friend class CBaseLevelMap;
	friend class CDriver1LevelMap;
	friend class CDriver2LevelMap;
public:
	CBaseLevelRegion();
	virtual ~CBaseLevelRegion();

	virtual void			FreeAll();
	virtual void			LoadRegionData(IVirtualStream* pFile) = 0;
	void					LoadAreaData(IVirtualStream* pFile);

	bool					IsEmpty() const;
	int						GetNumber() const;

protected:
	static int				UnpackCellPointers(ushort* dest_ptrs, char* src_data, int cell_slots_add, int targetRegion = 0);
	
	CBaseLevelMap*			m_owner;

	Spool*					m_spoolInfo{ nullptr };
	
	ushort*					m_cellPointers{ nullptr };		// cell pointers - pointing to CELL_DATA

	ushort*					m_pvsData{ nullptr };			// potentially visibile set of cells
	short*					m_roadmapData{ nullptr };		// heightfield with planes and BSP

	int						m_regionX{ -1 };
	int						m_regionZ{ -1 };
	int						m_regionNumber{ -1 };
	int						m_regionBarrelNumber{ -1 };
	bool					m_loaded{ false };
};

//----------------------------------------------------------------------------------

class CBaseLevelMap
{
	friend class CBaseLevelRegion;
	friend class CDriver1LevelRegion;
	friend class CDriver2LevelRegion;
public:
	CBaseLevelMap();
	virtual ~CBaseLevelMap();

	virtual void				FreeAll();

	//----------------------------------------

	void						SetLoadingCallbacks(OnRegionLoaded_t onLoaded, OnRegionFreed_t onFreed);
	
	//----------------------------------------

	virtual void				LoadMapLump(IVirtualStream* pFile);
	virtual void				LoadSpoolInfoLump(IVirtualStream* pFile);

	virtual int					GetAreaDataCount() const;
	virtual void				LoadInAreaTPages(IVirtualStream* pFile, int areaDataNum) const;
	virtual void				LoadInAreaModels(IVirtualStream* pFile, int areaDataNum) const;

	virtual void				SpoolRegion(const XZPAIR& cell) = 0;
	virtual void				SpoolRegion(int regionIdx) = 0;

	virtual CBaseLevelRegion*	GetRegion(const XZPAIR& cell) const = 0;
	virtual CBaseLevelRegion*	GetRegion(int regionIdx) const = 0;

	// converters
	void						WorldPositionToCellXZ(XZPAIR& cell, const VECTOR_NOPAD& position) const;

	const OUT_CELL_FILE_HEADER&	GetMapInfo() const;
	
	int							GetCellsAcross() const;
	int							GetCellsDown() const;

	int							GetRegionsAcross() const;
	int							GetRegionsDown() const;

protected:

	void						InitRegion(CBaseLevelRegion* region, int index) const;

	void						OnRegionLoaded(CBaseLevelRegion* region);
	void						OnRegionFreed(CBaseLevelRegion* region);

	// shared
	OUT_CELL_FILE_HEADER		m_mapInfo;
	
	Spool*						m_regionSpoolInfo{ nullptr };			// region data info
	ushort*						m_regionSpoolInfoOffsets{ nullptr };	// region offset table
	
	AreaDataStr*				m_areaData{ nullptr };					// region model/texture data descriptors
	AreaTPage_t*				m_areaTPages{ nullptr };				// region texpage usage table
	bool*						m_areaDataStates{ nullptr };			// area data loading states

	int							m_numStraddlers{ 0 };
	
	int							m_cell_slots_add[5] { 0 };
	int							m_cell_objects_add[5] { 0 };
	int							m_PVS_size[4] { 0 };
	
	int							m_numAreas{ 0 };
	int							m_numSpoolInfoOffsets{ 0 };
	int							m_numRegionSpools{ 0 };

	int							m_regions_across{ 0 };
	int							m_regions_down{ 0 };

	OnRegionLoaded_t			m_onRegionLoaded{ nullptr };
	OnRegionFreed_t				m_onRegionFreed{ nullptr };
};

//-----------------------------------------------------------------------------------------

extern OUT_CITYLUMP_INFO				g_levInfo;

//-----------------------------------------------------------------------------------------


#endif // REGIONS_H
