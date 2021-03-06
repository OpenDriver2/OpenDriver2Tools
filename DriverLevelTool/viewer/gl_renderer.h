#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include "glad.h"
#include "math/dktypes.h"
#include "math/Matrix.h"

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

enum MatrixMode
{
	MATRIX_VIEW = 0,
	MATRIX_PROJECTION,
	MATRIX_WORLD,

	MATRIX_WORLDVIEWPROJECTION,

	MATRIX_MODES,
};

int			GR_Init(char* windowName, int width, int height, int fullscreen);
void		GR_Shutdown();

void		GR_UpdateWindowSize(int width, int height);
void		GR_SwapWindow();

ShaderID	GR_CompileShader(const char* source);

//--------------------------------------------------

void		GR_ClearColor(int r, int g, int b);
void		GR_ClearDepth(float depth);

void		GR_SetViewPort(int x, int y, int width, int height);

//--------------------------------------------------

TextureID	GR_CreateRGBATexture(int width, int height, ubyte* data = nullptr);
void		GR_DestroyTexture(TextureID texture);

//--------------------------------------------------

GrVAO*		GR_CreateVAO(int numVertices, GrVertex* verts = nullptr, int dynamic = 0);
void		GR_DestroyVAO(GrVAO* vaoPtr);

//--------------------------------------------------

void		GR_SetShader(const ShaderID& shader);
void		GR_SetVAO(GrVAO* vaoPtr);
void		GR_SetTexture(TextureID texture);
void		GR_SetMatrix(MatrixMode mode, const Matrix4x4& matrix);

void		GR_SetPolygonOffset(float ofs);
void		GR_SetDepth(int enable);

void		GR_UpdateMatrixUniforms();



//--------------------------------------------------



#endif