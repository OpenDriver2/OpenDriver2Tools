#ifndef COMPILER_H
#define COMPILER_H

#include "obj_loader.h"

//----------------------------------------------------------

void OptimizeModel(smdmodel_t& model);

//----------------------------------------------------------

void CompileOBJModelToDMODEL(const char* filename, const char* outputName);

#endif