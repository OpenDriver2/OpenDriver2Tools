#ifndef DRIVER_LEVEL_H
#define DRIVER_LEVEL_H

#include "driver_routines/models.h"
#include "driver_routines/textures.h"
#include "driver_routines/regions.h"
#include "driver_routines/level.h"

#include "math/Matrix.h"

//----------------------------------------------------------

#define EXPORT_SCALING			(1.0f / ONE_F)

//----------------------------------------------------------

// extern some vars
extern OUT_CITYLUMP_INFO		g_levInfo;
extern CDriverLevelTextures		g_levTextures;
extern CDriverLevelModels		g_levModels;
extern CBaseLevelMap*			g_levMap;

//----------------------------------------------------------

void	ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize);
void	WriteMODELToObjStream(IVirtualStream* pStream, MODEL* model, int modelSize, int model_index, const char* name_prefix,
			bool debugInfo = true,
			const Matrix4x4& translation = identity4(),
			int* first_v = nullptr,
			int* first_t = nullptr);

//----------------------------------------------------------
// main functions

struct ModelExportFilters
{
	enum {
		MODE_DISABLE = 0,
		MODE_EXCLUDE_ALL_INCLUDE_FLAGS,
		MODE_INCLUDE_ALL_EXCLUDE_FLAGS
	};

	int shapeFlags{ 0 };
	int flags2{ 0 };

	int mode{ MODE_DISABLE };

	bool Check(MODEL* model) const
	{
		switch (mode)
		{
		case MODE_EXCLUDE_ALL_INCLUDE_FLAGS:
			if (model->shape_flags & shapeFlags)
				return true;

			if (model->flags2 & flags2)
				return true;
			return false;
		case MODE_INCLUDE_ALL_EXCLUDE_FLAGS:
			if (model->shape_flags & shapeFlags)
				return false;

			if (model->flags2 & flags2)
				return false;

			return true;
		}

		return true;
	}
};

void SaveModelPagesMTL();
void ExportAllModels();
void ExportAllCarModels();

void ExportRegions(const ModelExportFilters& filters, bool* regionsToExport = nullptr);

void ExportAllTextures();
void ExportOverlayMap();

#endif