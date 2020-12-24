#include "gl_renderer.h"
#include <SDL.h>
#include "core/cmdlib.h"

#include <assert.h>
#include <string.h>

#include "math/dktypes.h"

SDL_Window* g_window = NULL;
int g_windowWidth, g_windowHeight;
int g_swapInterval = 1;

TextureID g_whiteTexture;

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

    if (g_window == NULL)
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif

    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    SDL_GL_CreateContext(g_window);
	
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
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    if (info[0] && strlen(info) > 8)
    {
        MsgError("%s\n", info);
        assert(false);
    }
}

void GR_CheckProgramStatus(GLuint program)
{
    char info[1024];
    glGetProgramInfoLog(program, sizeof(info), NULL, info);
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
    glShaderSource(vertexShader, 3, vs_list, NULL);
    glCompileShader(vertexShader);
    GR_CheckShaderStatus(vertexShader);
    glAttachShader(program, vertexShader);
    glDeleteShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 3, fs_list, NULL);
    glCompileShader(fragmentShader);
    GR_CheckShaderStatus(fragmentShader);
    glAttachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    glBindAttribLocation(program, a_position, "a_position");
    glBindAttribLocation(program, a_texcoord, "a_texcoord");
    glBindAttribLocation(program, a_color, "a_color");

    glLinkProgram(program);
    GR_CheckProgramStatus(program);

    GLint sampler = 0;
    glUseProgram(program);
    glUniform1iv(glGetUniformLocation(program, "s_texture"), 1, &sampler);
    glUseProgram(0);

    return program;
}

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glFinish();
	
    return newTexture;
}

void GR_DestroyTexture(TextureID texture)
{
    glDeleteTextures(1, &texture);
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

void GR_SwapWindow()
{
    SDL_GL_SwapWindow(g_window);
    glFinish();
}

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

void GR_EnableDepth(int enable)
{
    if (enable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void GR_SetViewPort(int x, int y, int width, int height)
{
    glViewport(x, y, width, height);
}
