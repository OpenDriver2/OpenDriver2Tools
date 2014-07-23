#ifndef REGIONS_H
#define REGIONS_H

#include "driver_level.h"

struct RegionModels_t;

void LoadRegionData(IVirtualStream* pFile, RegionModels_t* models, regiondata_t* data, regionpages_t* pages );

#endif // REGIONS_H