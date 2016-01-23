#include "models.h"
#include <malloc.h>

//--------------------------------------------------------------------------------

// returns size of face and fills dface_t struct
// TODO: rework, few variants of faces still looks bad
int decode_poly(const char* face, dface_t* out)
{
	out->page = 0xFF;
	out->detail = 0xFFFF;

	char* data = (char*)face;

	out->flags = *data++;

	int numComp = 4;

	bool has_texcoord = false;

	if((out->flags & FACE_TEXTURED) || ((out->flags & FACE_TEXTURED2) && ((out->flags & FACE_RGB) || (out->flags & FACE_UNK6))) || (out->flags & FACE_IS_QUAD))
	{
		out->page	= *data++;
		out->detail = *data++;
		*data++;

		has_texcoord = true;
	}
	else
		numComp = (out->flags & FACE_IS_QUAD) ? 4 : 3;

	for(int i = 0; i < numComp; i++)
	{
		out->vindex[i] = *data++;
	}

	// this is fucking ugliest code ever
	if((out->flags & FACE_TEXTURED2) && (out->flags & FACE_IS_QUAD))// && (out->flags & FACE_UNK6))
	{
		for(int i = 0; i < 4; i++)
		{
			out->texcoord[i][0] = *data++;
			out->texcoord[i][1] = *data++;
		}

		if(!(out->flags & FACE_RGB) && (out->flags & FACE_UNK6))
			data += 4;

		if(out->flags == 9)
		{
			return 12;
		}
	}
	else
	{
		if(out->flags & FACE_RGB)
		{
			for(int i = 0; i < 4; i++)
			{
				out->color[i] = *data++;
			}
		}

		if(out->flags & FACE_TEXTURED)
		{
			for(int i = 0; i < 4; i++)
			{
				out->texcoord[i][0] = *data++;
				out->texcoord[i][1] = *data++;
			}
		}

		if((out->flags & FACE_NOCULL) && numComp == 3 && !(out->flags & FACE_RGB))
			data += 4;

		data += 4;
	}

	return data-(char*)face;
}
