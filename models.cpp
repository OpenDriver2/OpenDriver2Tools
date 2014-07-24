#include "models.h"
#include "DebugInterface.h"

struct face_type_t
{
	ubyte flags;

	char*	data;
	int		size;
};

face_type_t g_facetypeLib[256];

void InitFaceTypeLib()
{
	memset(&g_facetypeLib, 0, sizeof(face_type_t)*256);
}

void AddUniqueFaceType(ubyte flags, const char* data, int size)
{
	if(g_facetypeLib[flags].data == NULL)
	{
		g_facetypeLib[flags].data = (char*)malloc(size);
		memcpy(g_facetypeLib[flags].data, data, size);

		g_facetypeLib[flags].size = size;
	}
}

void DumpFaceTypes()
{
	IFile* pFaceDumpFile = GetFileSystem()->Open("faceTypeLib.binstr.txt", "wb", SP_ROOT);

	if(pFaceDumpFile)
	{
		pFaceDumpFile->Print("Face dump file. Open with hex viewer\r\n");
		pFaceDumpFile->Print("Face begins after !!!! and ends with END!\r\n");

		for(int i = 0; i < 256; i++)
		{
			if(!g_facetypeLib[i].data)
				continue;

			pFaceDumpFile->Print("\r\n---------\r\nface flags=%d size=%d\r\n\r\n", i, g_facetypeLib[i].size);
			static char zeroes[4] = {0,0,0,0};

			pFaceDumpFile->Print("!!!!");
			pFaceDumpFile->Write(g_facetypeLib[i].data, 1, g_facetypeLib[i].size );
			pFaceDumpFile->Write(zeroes, 1, 4 );
			pFaceDumpFile->Print("END!");
		}

		GetFileSystem()->Close(pFaceDumpFile);
	}
};

//--------------------------------------------------------------------------------

// returns size of face and fills dface_t struct
// TODO: rework, few variants of faces still looks bad
int decode_face(const char* face, dface_t* out)
{
	/*
	char* data = (char*)face;

	out->flags = *data++;

	int numComp = 4;
	
	if((out->flags & FACE_TEXTURED) || (out->flags & FACE_TEXTURED2) || (out->flags & FACE_IS_QUAD))
	{
		out->palette = *data++;
		out->texture = *data++;
		*data++;
	}
	else
		numComp = (out->flags & FACE_IS_QUAD) ? 4 : 3;

	for(int i = 0; i < numComp; i++)
	{
		out->vindex[i] = *data++;
	}

	if(out->flags & FACE_TEXTURED)
	{
		if(out->flags & FACE_RGB)
		{
			for(int i = 0; i < 4; i++)
			{
				out->color[i] = *data++;
			}
		}

		for(int i = 0; i < 4; i++)
		{
			out->texcoord[i][0] = *data++;
			out->texcoord[i][1] = *data++;
		}
	}
	else if(out->flags & FACE_TEXTURED2)
	{
		for(int i = 0; i < 4; i++)
		{
			out->texcoord[i][0] = *data++;
			out->texcoord[i][1] = *data++;
		}
	}
	else if(out->flags & FACE_RGB)
	{
		for(int i = 0; i < 4; i++)
		{
			out->color[i] = *data++;
		}
	}

	if( (out->flags & FACE_HAS_VERTEXNORMAL) && numComp == 3 && !(out->flags & FACE_RGB))
		data += 4;

	if(!(out->flags & FACE_TEXTURED2))
		data += 4;

	return data-(char*)face;
	*/

	//----------------------------------------------------------------------------------------------------------------------
	/*
	char* data = (char*)face;

	out->flags = *data++;

	int numComp = 4;

	if((out->flags & FACE_TEXTURED) || ((out->flags & FACE_TEXTURED2) && (out->flags & FACE_RGB)) || (out->flags & FACE_IS_QUAD))
	{
		out->page	= *data++;
		out->detail = *data++;
		*data++;
	}
	else
		numComp = (out->flags & FACE_IS_QUAD) ? 4 : 3;

	for(int i = 0; i < numComp; i++)
	{
		out->vindex[i] = *data++;
	}

	if(out->flags & FACE_RGB)
	{
		for(int i = 0; i < 4; i++)
		{
			out->color[i] = *data++;
		}
	}

	// this is fucking ugliest code ever
	if((out->flags & FACE_TEXTURED2) && (out->flags & FACE_IS_QUAD))
	{
		out->texcoord[0][0] = out->color[0];
		out->texcoord[0][1] = out->color[1];
		out->texcoord[1][0] = out->color[2];
		out->texcoord[1][1] = out->color[3];

		if((out->flags & FACE_HAS_VERTEXNORMAL) && numComp == 3 && !(out->flags & FACE_RGB))
		{
			out->texcoord[2][0] = *data++;
			out->texcoord[2][1] = *data++;
			out->texcoord[3][0] = *data++;
			out->texcoord[3][1] = *data++;
		}
	}
	else
	{
		if(out->flags & FACE_TEXTURED)
		{
			for(int i = 0; i < 4; i++)
			{
				out->texcoord[i][0] = *data++;
				out->texcoord[i][1] = *data++;
			}
		}

		if((out->flags & FACE_HAS_VERTEXNORMAL) && numComp == 3 && !(out->flags & FACE_RGB))
			data += 4;
	}

	data += 4;
	*/
	//----------------------------------------------------------------------------------------------------------------------

	char* data = (char*)face;

	out->flags = *data++;

	int numComp = 4;

	if((out->flags & FACE_TEXTURED) || ((out->flags & FACE_TEXTURED2) && (out->flags & FACE_RGB)) || (out->flags & FACE_IS_QUAD))
	{
		out->page	= *data++;
		out->detail = *data++;
		*data++;
	}
	else
		numComp = (out->flags & FACE_IS_QUAD) ? 4 : 3;

	for(int i = 0; i < numComp; i++)
	{
		out->vindex[i] = *data++;
	}

	// this is fucking ugliest code ever
	if((out->flags & FACE_TEXTURED2) && (out->flags & FACE_IS_QUAD))
	{
		if(!(out->flags & FACE_RGB))
			data += 4;

		for(int i = 0; i < 4; i++)
		{
			out->texcoord[i][0] = *data++;
			out->texcoord[i][1] = *data++;
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

		if((out->flags & FACE_HAS_VERTEXNORMAL) && numComp == 3 && !(out->flags & FACE_RGB))
			data += 4;

		data += 4;
	}

	AddUniqueFaceType(out->flags, face, data-(char*)face);

	return data-(char*)face;
}
