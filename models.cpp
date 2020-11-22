#include "models.h"
#include <malloc.h>

//--------------------------------------------------------------------------------

// From Car Poly code of D2
int PolySizes[56] = {
	8,12,16,24,20,20,28,32,8,12,16,16,
	0,0,0,0,12,12,12,16,20,20,24,24,
	0,0,0,0,0,0,0,0,8,12,16,24,
	20,20,28,32,0,0,0,0,0,0,0,0,
	12,12,12,16,20,20,24,24 
};


// returns size of face and fills dface_t struct
// TODO: rework, few variants of faces still looks bad
int decode_poly(const char* face, dface_t* out)
{
	out->page = 0xFF;
	out->detail = 0xFFFF;

	char* data = (char*)face;

	// determining poly type
	uint polyType = *((uint*)data) & 31;

	out->flags = polyType;

	/*
	switch (polyType)
	{
		case 0:
		case 18:
			out->vindex[0] = data[1];
			out->vindex[1] = data[2];
			out->vindex[2] = data[3];
			out->flags = 0;
			break;
		case 1:
		case 19:
			out->vindex[0] = data[4];
			out->vindex[1] = data[5];
			out->vindex[2] = data[6];
			out->vindex[3] = data[7];
			out->flags = FACE_IS_QUAD;
			break;
		case 21:
			out->vindex[0] = data[4];
			out->vindex[1] = data[5];
			out->vindex[2] = data[6];
			out->vindex[3] = data[7];
			out->detail = data[1];
			//out->texcoord[0][0] = data[2];
			out->page = data[3];
			//out->texcoord[1][0] = data[3];
			break;
	}

	return PolySizes[polyType];
	*/
	
	*data++;

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

	return PolySizes[polyType];
}
