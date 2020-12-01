#include "models.h"

#include <corecrt_math.h>
#include <malloc.h>
#include <unordered_set>

#include "core/cmdlib.h"

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
	*(uint*)&out->color[0] = 0;
	*(uint*)&out->color[1] = 0;
	*(uint*)&out->color[2] = 0;
	*(uint*)&out->color[3] = 0;

	switch (ptype)
	{
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
		case 1:
		case 19:
		{
			// F4
			*(uint*)out->vindices = *(uint*)&polyList[1];

			// FIXME: read colours

			out->flags = FACE_RGB; // RGB?

			break;
		}
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

			//SwapValues(out->vindices[2], out->vindices[3]);
			//SwapValues(out->nindices[2], out->nindices[3]);

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