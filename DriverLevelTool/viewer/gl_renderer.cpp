﻿#include "gl_renderer.h"
#include <SDL.h>
#include "core/cmdlib.h"

#include <assert.h>
#include <string.h>

#include "math/dktypes.h"
#include "math/Matrix.h"

struct GrVAO
{
	GLuint vertexArray;
	GLuint buffers[2];

	int numVertices;
	int numIndices;
};

//-------------------------------------------------------------

SDL_Window* g_window = nullptr;
int			g_windowWidth, g_windowHeight;
int			g_swapInterval = 1;

int			g_CurrentBlendMode = BM_NONE;
int			g_CurrentDepthMode = 0;
int			g_CurrentCullMode = CULL_NONE;
GrVAO*		g_CurrentVAO = nullptr;
TextureID	g_lastBoundTexture = -1;
ShaderID	g_CurrentShader = -1;

GLint		u_MatrixUniforms[MATRIX_MODES];
Matrix4x4	g_matrices[MATRIX_MODES];

GLint		u_World;			// object transform in the world
GLint		u_View;				// view transform in the world
GLint		u_Projection;		// projection
GLint		u_WorldViewProj;

TextureID	g_whiteTexture;

static const GLenum glPrimitiveType[] = {
	GL_TRIANGLES,
	GL_TRIANGLE_FAN,
	GL_TRIANGLE_STRIP,
#ifdef USE_GLES2
	0, // GL_QUADS,
#else
	GL_QUADS,
#endif // USE_GLES2
	GL_LINES,
	GL_LINE_STRIP,
	GL_LINE_LOOP,
	GL_POINTS,
};

static const GLenum glCullModes[] = {
	GL_NONE,
	GL_CW,
	GL_CCW,
};

void GR_GetWVPUniforms(GLuint program)
{
	u_MatrixUniforms[MATRIX_VIEW] = glGetUniformLocation(program, "u_View");
	u_MatrixUniforms[MATRIX_PROJECTION] = glGetUniformLocation(program, "u_Projection");
	u_MatrixUniforms[MATRIX_WORLD] = glGetUniformLocation(program, "u_World");
	u_MatrixUniforms[MATRIX_WORLDVIEWPROJECTION] = glGetUniformLocation(program, "u_WorldViewProj");
}

void GR_UpdateWindowSize(int width, int height)
{
	g_windowWidth = width;
	g_windowHeight = height;

	SDL_GL_SetSwapInterval(g_swapInterval);
}

int GR_InitWindow(char* windowName, int width, int height, int fullscreen)
{
	g_windowWidth = width;
	g_windowHeight = height;

	int windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (fullscreen)
	{
		windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	g_window = SDL_CreateWindow(windowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g_windowWidth, g_windowHeight, windowFlags);

	if (g_window == nullptr)
	{
		MsgError("Failed to initialise SDL window or GL context!\n");
		return 0;
	}

	return 1;
}

//-------------------------------------------------------------
// Loads OpenGL
//-------------------------------------------------------------
int GR_InitGL()
{
#if !defined(RO_DOUBLE_BUFFERED)
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
#endif

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

#if defined(OGLES)

#if defined(__ANDROID__)
	//Override to full screen.
	SDL_DisplayMode displayMode;
	if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0)
	{
		screenWidth = displayMode.w;
		windowWidth = displayMode.w;
		screenHeight = displayMode.h;
		windowHeight = displayMode.h;
	}
#endif
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OGLES_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	int major_version = 3;
	int minor_version = 3;
	int profile = SDL_GL_CONTEXT_PROFILE_CORE;

	// find best OpenGL version
	do
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major_version);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_version);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

		if (SDL_GL_CreateContext(g_window))
			break;

		minor_version--;

	} while (minor_version >= 0);

	if (minor_version == -1)
	{
		MsgError("Failed to initialise - OpenGL 3.x is not supported. Please update video drivers.\n");
		return 0;
	}
#endif

	GLenum err = gladLoadGL();

	if (err == 0)
		return 0;

	//-------------------------------

	const char* rend = (const char*)glGetString(GL_RENDERER);
	const char* vendor = (const char*)glGetString(GL_VENDOR);
	Msg("*Video adapter: %s by %s\n", rend, vendor);

	const char* versionStr = (const char*)glGetString(GL_VERSION);
	Msg("*OpenGL version: %s\n", versionStr);

	const char* glslVersionStr = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	Msg("*GLSL version: %s\n", glslVersionStr);

	return 1;
}

//-------------------------------------------------------------
// Initializes SDL video with subsystem
//-------------------------------------------------------------
int GR_InitSDL()
{
	//Initialise SDL2
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		MsgError("Error: Failed to initialise SDL\n");
		return 0;
	}

	return 1;
}

//-------------------------------------------------------------
// Initializes graphics system with SDL
//-------------------------------------------------------------
int GR_Init(char* windowName, int width, int height, int fullscreen)
{
	if (!GR_InitSDL())
		return 0;

	if (!GR_InitWindow(windowName, width, height, fullscreen))
		return 0;

	if (!GR_InitGL())
		return 0;

	return 1;
}

void GR_Shutdown()
{
	GR_DestroyTexture(g_whiteTexture);

	SDL_DestroyWindow(g_window);
	SDL_Quit();
}

void GR_CheckShaderStatus(GLuint shader)
{
	char info[1024];
	glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
	if (info[0] && strlen(info) > 8)
	{
		MsgError("%s\n", info);
		assert(false);
	}
}

void GR_CheckProgramStatus(GLuint program)
{
	char info[1024];
	glGetProgramInfoLog(program, sizeof(info), nullptr, info);
	if (info[0] && strlen(info) > 8)
	{
		MsgError("%s\n", info);
		assert(false);
	}
}

ShaderID GR_CompileShader(const char* source)
{
#if defined(ES2_SHADERS)
	const char* GLSL_HEADER_VERT =
		"#version 100\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define VERTEX\n";

	const char* GLSL_HEADER_FRAG =
		"#version 100\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define fragColor gl_FragColor\n";
#elif defined(ES3_SHADERS)
	const char* GLSL_HEADER_VERT =
		"#version 300 es\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define VERTEX\n"
		"#define varying   out\n"
		"#define attribute in\n"
		"#define texture2D texture\n";

	const char* GLSL_HEADER_FRAG =
		"#version 300 es\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define varying     in\n"
		"#define texture2D   texture\n"
		"out vec4 fragColor;\n";
#else
	const char* GLSL_HEADER_VERT =
		"#version 330\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define VERTEX\n"
		"#define varying   out\n"
		"#define attribute in\n"
		"#define texture2D texture\n";

	const char* GLSL_HEADER_FRAG =
		"#version 330\n"
		"precision lowp  int;\n"
		"precision highp float;\n"
		"#define varying     in\n"
		"#define texture2D   texture\n"
		"out vec4 fragColor;\n";
#endif

	char extra_vs_defines[1024];
	char extra_fs_defines[1024];
	extra_vs_defines[0] = 0;
	extra_fs_defines[0] = 0;

	const char* vs_list[] = { GLSL_HEADER_VERT, extra_vs_defines, source };
	const char* fs_list[] = { GLSL_HEADER_FRAG, extra_fs_defines, source };

	GLuint program = glCreateProgram();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 3, vs_list, nullptr);
	glCompileShader(vertexShader);
	GR_CheckShaderStatus(vertexShader);
	glAttachShader(program, vertexShader);
	glDeleteShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 3, fs_list, nullptr);
	glCompileShader(fragmentShader);
	GR_CheckShaderStatus(fragmentShader);
	glAttachShader(program, fragmentShader);
	glDeleteShader(fragmentShader);

	glBindAttribLocation(program, a_position_tu, "a_position_tu");
	glBindAttribLocation(program, a_normal_tv, "a_normal_tv");
	glBindAttribLocation(program, a_color, "a_color");

	glLinkProgram(program);
	GR_CheckProgramStatus(program);

	GLint sampler = 0;
	glUseProgram(program);
	glUniform1iv(glGetUniformLocation(program, "s_texture"), 1, &sampler);
	glUseProgram(0);

	return program;
}

void GR_SetShader(const ShaderID& shader)
{
	if (g_CurrentShader == shader)
		return;

	g_CurrentShader = shader;
	
	glUseProgram(shader);
	GR_GetWVPUniforms(shader);
}

//----------------------------------------------------------------

void GR_SetMatrix(GR_MatrixMode mode, const Matrix4x4& matrix)
{
	g_matrices[mode] = matrix;
}

void GR_UpdateMatrixUniforms()
{
	// rebuild WVP
	g_matrices[MATRIX_WORLDVIEWPROJECTION] = identity4() * g_matrices[MATRIX_PROJECTION] * (g_matrices[MATRIX_VIEW] * g_matrices[MATRIX_WORLD]);
	
	for(int i = 0; i < MATRIX_MODES; i++)
		glUniformMatrix4fv(u_MatrixUniforms[i], 1, GL_TRUE, g_matrices[i]);
}

//----------------------------------------------------------------

void GR_GenerateCommonTextures()
{
	unsigned int pixelData = 0xFFFFFFFF;

	glGenTextures(1, &g_whiteTexture);
	glBindTexture(GL_TEXTURE_2D, g_whiteTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixelData);
	glBindTexture(GL_TEXTURE_2D, 0);
}

TextureID GR_CreateRGBATexture(int width, int height, ubyte* data)
{
	TextureID newTexture;
	glGenTextures(1, &newTexture);

	glBindTexture(GL_TEXTURE_2D, newTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glFinish();

	return newTexture;
}

void GR_DestroyTexture(TextureID texture)
{
	glDeleteTextures(1, &texture);
}

void GR_SetTexture(TextureID texture)
{
	//if (g_texturelessMode) 
	//	texture = g_whiteTexture;

	if (g_lastBoundTexture == texture)
		return;
	
	glBindTexture(GL_TEXTURE_2D, texture);

	g_lastBoundTexture = texture;
}


//----------------------------------------------------------------

void GR_ClearColor(int r, int g, int b)
{
	glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GR_ClearDepth(float depth)
{
	glClearDepth(depth);
	glClear(GL_DEPTH_BUFFER_BIT);
	glClear(GL_STENCIL_BUFFER_BIT);
}

void GR_BeginScene()
{
	GR_SetViewPort(0, 0, g_windowWidth, g_windowHeight);
}

void GR_EndScene()
{
	
}

void GR_SwapWindow()
{
	SDL_GL_SwapWindow(g_window);
	glFinish();
}

void GR_SetViewPort(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

//--------------------------------------------------------------------------

void GR_SetPolygonOffset(float ofs)
{
	if (ofs == 0.0f)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
	else
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f, ofs);
	}
}

void GR_SetDepth(int enable)
{
	if (g_CurrentDepthMode == enable)
		return;

	g_CurrentDepthMode = enable;

	if (enable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void GR_SetBlendMode(GR_BlendMode blendMode)
{
	if (g_CurrentBlendMode == blendMode)
		return;

	if (g_CurrentBlendMode == BM_NONE)
		glEnable(GL_BLEND);

	switch (blendMode)
	{
	case BM_NONE:
		glDisable(GL_BLEND);
		break;
	case BM_AVERAGE:
		glBlendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_COLOR);
		glBlendEquation(GL_FUNC_ADD);
		break;
	case BM_ADD:
		glBlendFunc(GL_ONE, GL_ONE);
		glBlendEquation(GL_FUNC_ADD);
		break;
	case BM_SUBTRACT:
		glBlendFunc(GL_ONE, GL_ONE);
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		break;
	case BM_ADD_QUATER_SOURCE:
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
		glBlendEquation(GL_FUNC_ADD);
		break;
	}

	g_CurrentBlendMode = blendMode;
}

void GR_SetCullMode(GR_CullMode cullMode)
{
	if (g_CurrentCullMode == cullMode)
		return;

	if(cullMode == CULL_NONE)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(glCullModes[cullMode]);
	}
	
	g_CurrentCullMode = cullMode;
}

//--------------------------------------------------------------------------

GrVAO* GR_CreateVAO(int numVertices, GrVertex* verts /*= nullptr*/, int dynamic /*= 0*/)
{
	return GR_CreateVAO(numVertices, 0, verts, nullptr, dynamic);
}

GrVAO* GR_CreateVAO(int numVertices, int numIndices, GrVertex* verts /*= nullptr*/, int* indices /*= nullptr*/, int dynamic /*= 0*/)
{
	GLuint buffers[2] = { GL_NONE };
	GLuint vertexArray;

	// gen vertex buffer and index buffer
	glGenVertexArrays(1, &vertexArray);
	{
		glGenBuffers(2, buffers);

		glBindVertexArray(vertexArray);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GrVertex) * numVertices, verts, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

		glEnableVertexAttribArray(a_position_tu);
		glEnableVertexAttribArray(a_normal_tv);
		glEnableVertexAttribArray(a_color);

		glVertexAttribPointer(a_position_tu, 4, GL_FLOAT, GL_FALSE, sizeof(GrVertex), &((GrVertex*)nullptr)->vx);
		glVertexAttribPointer(a_normal_tv, 4, GL_FLOAT, GL_FALSE, sizeof(GrVertex), &((GrVertex*)nullptr)->nx);
		glVertexAttribPointer(a_color, 4, GL_FLOAT, GL_TRUE, sizeof(GrVertex), &((GrVertex*)nullptr)->cr);

		if(numIndices)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * numIndices, indices, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		}

		glBindVertexArray(0);
	}

	GrVAO* newVAO = new GrVAO();
	newVAO->numVertices = numVertices;
	newVAO->numIndices = numIndices;
	newVAO->vertexArray = vertexArray;
	newVAO->buffers[0] = buffers[0];
	newVAO->buffers[1] = buffers[1];

	return newVAO;
}

void GR_SetVAO(GrVAO* vaoPtr)
{
	if (g_CurrentVAO == vaoPtr)
		return;

	g_CurrentVAO = vaoPtr;
	
	if (vaoPtr == nullptr)
	{
		glBindVertexArray(0);
		return;
	}

	glBindVertexArray(vaoPtr->vertexArray);
}

void GR_DestroyVAO(GrVAO* vaoPtr)
{
	if (!vaoPtr)
		return;

	glDeleteVertexArrays(1, &vaoPtr->vertexArray);
	glDeleteBuffers(2, vaoPtr->buffers);
	delete vaoPtr;
}

//--------------------------------------------------------------------------

void GR_DrawNonIndexed(GR_PrimitiveType primitivesType, int firstVertex, int numVertices)
{
	glDrawArrays(glPrimitiveType[primitivesType], firstVertex, numVertices);
}

void GR_DrawIndexed(GR_PrimitiveType primitivesType, int firstIndex, int numIndices)
{
	glDrawElements(glPrimitiveType[primitivesType], numIndices, GL_UNSIGNED_INT, (void*)(intptr_t)(firstIndex*sizeof(int)));
}