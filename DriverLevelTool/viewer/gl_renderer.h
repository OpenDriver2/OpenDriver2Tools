#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include "glad.h"
#include "math/dktypes.h"

#define RO_DOUBLE_BUFFERED

typedef GLuint TextureID;
typedef GLuint ShaderID;
typedef GLuint RenderTargetID;

struct GrVertex
{
	float vx, vy, vz, tc_u;
	float nx, ny, nz, tc_v;
	float cr, cg, cb, _cpad;
};

struct GrVAO;

enum ShaderAttrib
{
	a_position_tu,
	a_normal_tv,
	a_color,
};

enum BlendMode
{
	BM_NONE,
	BM_AVERAGE,
	BM_ADD,
	BM_SUBTRACT,
	BM_ADD_QUATER_SOURCE
};

int			GR_Init(char* windowName, int width, int height, int fullscreen);
void		GR_Shutdown();
void		GR_UpdateWindowSize(int width, int height);

ShaderID	GR_CompileShader(const char* source);

//--------------------------------------------------

void		GR_ClearColor(int r, int g, int b);
void		GR_ClearDepth(float depth);
void		GR_SwapWindow();
void		GR_SetViewPort(int x, int y, int width, int height);

//--------------------------------------------------

TextureID	GR_CreateRGBATexture(int width, int height, ubyte* data = NULL);
void		GR_SetTexture(TextureID texture);
void		GR_DestroyTexture(TextureID texture);

//--------------------------------------------------

void		GR_SetPolygonOffset(float ofs);
void		GR_EnableDepth(int enable);

//--------------------------------------------------

GrVAO*		GR_CreateVAO(int numVertices, GrVertex* verts = NULL, int dynamic = 0);
void		GR_SetVAO(GrVAO* vaoPtr);
void		GR_DestroyVAO(GrVAO* vaoPtr);

#endif