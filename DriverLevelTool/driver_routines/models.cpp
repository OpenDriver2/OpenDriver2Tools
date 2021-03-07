#include "models.h"
#include "regions.h"
#include "util/DkList.h"
#include <unordered_set>
#include <string>

#include "core/cmdlib.h"
#include "core/IVirtualStream.h"

#include "d2_types.h"

//--------------------------------------------------------------------------------

CDriverLevelModels::CDriverLevelModels()
{
}

CDriverLevelModels::~CDriverLevelModels()
{
}

void CDriverLevelModels::FreeAll()
{
	for (int i = 0; i < MAX_MODELS; i++)
	{
		if (m_levelModels[i].model)
			free(m_levelModels[i].model);
	}

	for (int i = 0; i < MAX_CAR_MODELS; i++)
	{
		if (m_carModels[i].cleanmodel)
			free(m_carModels[i].cleanmodel);

		if (m_carModels[i].dammodel)
			free(m_carModels[i].dammodel);

		if (m_carModels[i].lowmodel)
			free(m_carModels[i].lowmodel);
	}
}

ModelRef_t* CDriverLevelModels::GetModelByIndex(int nIndex, RegionModels_t* models) const
{
	if (nIndex >= 0 && nIndex < 1536)
	{
		// try searching in region datas
		if (m_levelModels[nIndex].swap && models)
		{
			for (int i = 0; i < models->modelRefs.numElem(); i++)
			{
				if (models->modelRefs[i].index == nIndex)
					return &models->modelRefs[i];
			}
		}

		return (ModelRef_t*)&m_levelModels[nIndex];
	}

	return nullptr;
}

int CDriverLevelModels::FindModelIndexByName(const char* name) const
{
	for (int i = 0; i < 1536; i++)
	{
		if (!strcmp(m_model_names[i].c_str(), name))
			return i;
	}

	return -1;
}

const char* CDriverLevelModels::GetModelName(ModelRef_t* model) const
{
	if (!model)
		return nullptr;

	return m_model_names[model->index].c_str();
}

CarModelData_t* CDriverLevelModels::GetCarModel(int index) const
{
	return (CarModelData_t*)&m_carModels[index];
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void CDriverLevelModels::LoadCarModelsLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	Msg("	all car models count: %d\n", modelCount);

	// read entries
	carmodelentry_t model_entries[MAX_CAR_MODELS];
	pFile->Read(&model_entries, sizeof(carmodelentry_t), MAX_CAR_MODELS);

	// position
	int r_ofs = pFile->Tell();

	int pad; // really padding?
	pFile->Read(&pad, sizeof(int), 1);

	// load car models
	for (int i = 0; i < MAX_CAR_MODELS; i++)
	{
		Msg("car model: %d %d %d\n", model_entries[i].cleanOffset != -1, model_entries[i].damOffset != -1, model_entries[i].lowOffset != -1);

		if (model_entries[i].cleanOffset != -1)
		{
			pFile->Seek(r_ofs + model_entries[i].cleanOffset, VS_SEEK_SET);

			pFile->Read(&m_carModels[i].cleanSize, 1, sizeof(int));

			m_carModels[i].cleanmodel = (MODEL*)malloc(m_carModels[i].cleanSize);
			pFile->Read(m_carModels[i].cleanmodel, 1, m_carModels[i].cleanSize);
		}
		else
			m_carModels[i].cleanmodel = nullptr;

		if (model_entries[i].damOffset != -1)
		{
			pFile->Seek(r_ofs + model_entries[i].damOffset, VS_SEEK_SET);

			pFile->Read(&m_carModels[i].damSize, 1, sizeof(int));

			m_carModels[i].dammodel = (MODEL*)malloc(m_carModels[i].damSize);
			pFile->Read(m_carModels[i].dammodel, 1, m_carModels[i].damSize);
		}
		else
			m_carModels[i].dammodel = nullptr;

		if (model_entries[i].lowOffset != -1)
		{
			pFile->Seek(r_ofs + model_entries[i].lowOffset, VS_SEEK_SET);

			pFile->Read(&m_carModels[i].lowSize, 1, sizeof(int));

			m_carModels[i].lowmodel = (MODEL*)malloc(m_carModels[i].lowSize);
			pFile->Read(m_carModels[i].lowmodel, 1, m_carModels[i].lowSize);
		}
		else
			m_carModels[i].lowmodel = nullptr;
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// load model names
//-------------------------------------------------------------
void CDriverLevelModels::LoadModelNamesLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	char* modelnames = new char[size];
	pFile->Read(modelnames, size, 1);

	int len = strlen(modelnames);
	int sz = 0;

	do
	{
		char* str = modelnames + sz;

		len = strlen(str);

		m_model_names.append(str);

		sz += len + 1;
	} while (sz < size);

	delete[] modelnames;

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void CDriverLevelModels::LoadLevelModelsLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	MsgInfo("	model count: %d\n", modelCount);

	for (int i = 0; i < modelCount; i++)
	{
		int modelSize;

		pFile->Read(&modelSize, sizeof(int), 1);

		if (modelSize > 0)
		{
			char* data = (char*)malloc(modelSize);

			pFile->Read(data, modelSize, 1);

			ModelRef_t& ref = m_levelModels[i];
			ref.index = i;
			ref.model = (MODEL*)data;
			ref.size = modelSize;
			ref.swap = false;
		}
		else // leave empty as swap
		{
			ModelRef_t& ref = m_levelModels[i];
			ref.index = i;
			ref.model = nullptr;
			ref.size = modelSize;
			ref.swap = true;
		}
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//--------------------------------------------------------------------------------

// From Car Poly code of D2
int PolySizes[56] = {
	8,12,16,24,20,20,28,32,8,12,16,16,
	0,0,0,0,12,12,12,16,20,20,24,24,
	0,0,0,0,0,0,0,0,8,12,16,24,
	20,20,28,32,0,0,0,0,0,0,0,0,
	12,12,12,16,20,20,24,24
};

std::unordered_set<int> g_UnknownPolyTypes;

void PrintUnknownPolys()
{
	if (!g_UnknownPolyTypes.size())
		return;

	MsgError("Unknown polygons: \n");
	std::unordered_set<int>::iterator itr;
	for (itr = g_UnknownPolyTypes.begin(); itr != g_UnknownPolyTypes.end(); itr++)
	{
		MsgWarning("%d (sz=%d), ", *itr, PolySizes[*itr]);
	}
	MsgError("\n\n");
	g_UnknownPolyTypes.clear();
}

// 5 (sz=20), 11 (sz=16), 10 (sz=16), 17 (sz=12), 9 (sz=12), 16 (sz=12), 8 (sz=8),

template <typename T>
void SwapValues(T& a, T& b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

// returns size of face and fills dface_t struct
// TODO: rework, few variants of faces still looks bad
int decode_poly(const char* polyList, dpoly_t* out)
{
	int ptype = *polyList & 31;

	out->page = 0xFF;
	out->detail = 0xFF;
	out->flags = 0;

	*(uint*)&out->color[0] = 0;
	*(uint*)&out->color[1] = 0;
	*(uint*)&out->color[2] = 0;
	*(uint*)&out->color[3] = 0;

	switch (ptype)
	{
		case 1:
			// what a strange face type
			*(uint*)out->vindices = *(uint*)&polyList[3];
			break;
		case 0:
		case 8:
		case 16:
		case 18:
		{
			// F3
			*(uint*)out->vindices = *(uint*)&polyList[1];

			// FIXME: read colours

			out->flags = FACE_RGB; // RGB?
			break;
		}
		case 19:
		{
			// F4
			*(uint*)out->vindices = *(uint*)&polyList[1];

			// FIXME: read colours

			out->flags = FACE_RGB; // RGB?

			break;
		}
		case 4:
		case 10:
		case 20:
		{
			POLYFT3* pft3 = (POLYFT3*)polyList;

			*(uint*)out->vindices = *(uint*)&pft3->v0;
			*(ushort*)out->uv[0] = *(uint*)&pft3->uv0;
			*(ushort*)out->uv[1] = *(uint*)&pft3->uv1;
			*(ushort*)out->uv[2] = *(uint*)&pft3->uv2;
			*(uint*)out->color = *(uint*)&pft3->color;
			out->page = pft3->texture_set;
			out->detail = pft3->texture_id;

			out->flags = FACE_TEXTURED;

			break;
		}
		case 5:
		case 9:
		case 11:
		case 17:
		case 21:
		{
			POLYFT4* pft4 = (POLYFT4*)polyList;

			*(uint*)out->vindices = *(uint*)&pft4->v0;
			*(ushort*)out->uv[0] = *(uint*)&pft4->uv0;
			*(ushort*)out->uv[1] = *(uint*)&pft4->uv1;
			*(ushort*)out->uv[2] = *(uint*)&pft4->uv2;
			*(ushort*)out->uv[3] = *(uint*)&pft4->uv3;

			*(uint*)out->color = *(uint*)&pft4->color;
			out->page = pft4->texture_set;
			out->detail = pft4->texture_id;

			//SwapValues(out->vindices[2], out->vindices[3]);

			out->flags = FACE_IS_QUAD | FACE_TEXTURED;

			break;
		}
		case 22:
		{
			POLYGT3* pgt3 = (POLYGT3*)polyList;

			*(uint*)out->vindices = *(uint*)&pgt3->v0;
			*(uint*)out->nindices = *(uint*)&pgt3->n0;
			*(ushort*)out->uv[0] = *(uint*)&pgt3->uv0;
			*(ushort*)out->uv[1] = *(uint*)&pgt3->uv1;
			*(ushort*)out->uv[2] = *(uint*)&pgt3->uv2;

			*(uint*)out->color = *(uint*)&pgt3->color;
			out->page = pgt3->texture_set;
			out->detail = pgt3->texture_id;

			out->flags = FACE_VERT_NORMAL | FACE_TEXTURED;
			break;
		}
		case 7:
		case 23:
		{
			POLYGT4* pgt4 = (POLYGT4*)polyList;

			*(uint*)out->vindices = *(uint*)&pgt4->v0;
			*(uint*)out->nindices = *(uint*)&pgt4->n0;
			*(ushort*)out->uv[0] = *(uint*)&pgt4->uv0;
			*(ushort*)out->uv[1] = *(uint*)&pgt4->uv1;
			*(ushort*)out->uv[2] = *(uint*)&pgt4->uv2;
			*(ushort*)out->uv[3] = *(uint*)&pgt4->uv3;

			*(uint*)out->color = *(uint*)&pgt4->color;
			out->page = pgt4->texture_set;
			out->detail = pgt4->texture_id;
			out->flags = FACE_IS_QUAD | FACE_VERT_NORMAL | FACE_TEXTURED;
			break;
		}
		default:
		{
			g_UnknownPolyTypes.insert(ptype);
		}
	}
	
	return PolySizes[ptype];
}