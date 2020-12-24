#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include "glad.h"
#include "math/dktypes.h"

typedef GLuint TextureID;
typedef GLuint ShaderID;
typedef GLuint RenderTargetID;

enum ShaderAttrib
{
	a_position,
	a_texcoord,
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

void		GR_ClearColor(int r, int g, int b);
void		GR_ClearDepth(float depth);
void		GR_SwapWindow();

TextureID	GR_CreateRGBATexture(int width, int height, ubyte* data = NULL);
void		GR_DestroyTexture(TextureID texture);

void		GR_SetPolygonOffset(float ofs);
void		GR_EnableDepth(int enable);

void		GR_SetViewPort(int x, int y, int width, int height);

#endif