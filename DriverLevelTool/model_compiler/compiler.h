#ifndef COMPILER_H
#define COMPILER_H

#include "obj_loader.h"

enum EPolyExtraFlags
{
	POLY_EXTRA_DENTING = (1 << 0)
};

//----------------------------------------------------------

void OptimizeModel(smdmodel_t& model);
void GenerateDenting(smdmodel_t& model, const char* outputName);

//----------------------------------------------------------

void CompileOBJModelToDMODEL(const char* filename, const char* outputName, bool generate_denting);

#endif