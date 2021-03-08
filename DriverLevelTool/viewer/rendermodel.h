#ifndef DRAWMODEL_H
#define DRAWMODEL_H

#include "util/DkList.h"

struct RegionModels_t;
struct ModelRef_t;
struct GrVAO;

struct modelBatch_t
{
	int tpage;
	int startIndex;
	int numIndices;
};

class CRenderModel
{
public:
					CRenderModel();
	virtual			~CRenderModel();

	bool			Initialize(ModelRef_t* model);
	void			Destroy();

	void			Draw();

protected:
	void			GenerateBuffers();
	
	ModelRef_t*				m_sourceModel { nullptr };
	GrVAO*					m_vao { nullptr };
	DkList<modelBatch_t>	m_batches;
	int						m_numVerts;
};

#endif