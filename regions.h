#ifndef REGIONS_H
#define REGIONS_H

#include "driver_level.h"

struct RegionModels_t;

void LoadMapLump(IVirtualStream* pFile);
void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, AreaDataStr* data, AreaTPage_t* pages );
void LoadSpoolInfoLump(IVirtualStream* pFile);

void FreeSpoolData();

#endif // REGIONS_H
