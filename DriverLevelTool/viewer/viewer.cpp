#include <malloc.h>
#include <SDL_events.h>
#include "gl_renderer.h"
#include "rendermodel.h"
#include "core/cmdlib.h"

#include "driver_routines/d2_types.h"
#include "driver_routines/level.h"
#include "driver_routines/models.h"
#include "driver_routines/textures.h"

#define MODEL_VERTEX_SHADER \
	"	attribute vec4 a_position_tu;\n"\
	"	attribute vec4 a_normal_tv;\n"\
	"	uniform mat4 u_View;\n"\
	"	uniform mat4 u_Projection;\n"\
	"	uniform mat4 u_World;\n"\
	"	uniform mat4 u_WorldViewProj;\n"\
	"	void main() {\n"\
	"		v_texcoord = vec2(a_position_tu.w, 1.0-a_normal_tv.w);\n"\
	"		gl_Position = u_WorldViewProj * vec4(a_position_tu.xyz, 1.0);\n"\
	"	}\n"

#define MODEL_FRAGMENT_SHADER \
	"	uniform sampler2D s_texture;\n"\
	"	void main() {\n"\
	"		fragColor = texture2D(s_texture, v_texcoord.xy);\n"\
	"	}\n"

const char* model_shader =
	"varying vec2 v_texcoord;\n"
	"varying vec4 v_color;\n"
	"varying vec4 v_page_clut;\n"
	"varying float v_z;\n"
	"#ifdef VERTEX\n"
	MODEL_VERTEX_SHADER
	"#else\n"
	MODEL_FRAGMENT_SHADER
	"#endif\n";

int g_quit = 0;

int g_currentModel = 0;
bool g_holdLeft = false;
bool g_holdRight = false;

Vector3D g_cameraPosition(0);
Vector3D g_cameraAngles(25.0f, 45.0f, 0);
float g_cameraDistance = 0.5f;
float g_cameraFOV = 90.0f;

void SDLPollEvent()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
			{
				g_quit = 1;
				break;
			}
			case SDL_WINDOWEVENT:
			{
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_RESIZED:
					GR_UpdateWindowSize(event.window.data1, event.window.data2);

					break;
				case SDL_WINDOWEVENT_CLOSE:
					g_quit = 1;
					break;
				}
				break;
			}
			case SDL_MOUSEMOTION:
			{
				//Emulator_DoDebugMouseMotion(event.motion.x, event.motion.y);

				if(g_holdLeft)
				{
					g_cameraAngles.x += event.motion.yrel * 0.8f;
					g_cameraAngles.y -= event.motion.xrel * 0.8f;
				}
				else if(g_holdRight)
				{
					g_cameraDistance += event.motion.yrel * 0.01f;
				}
				
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				bool down = (event.type == SDL_MOUSEBUTTONDOWN);

				if (event.button.button == 1)
					g_holdLeft = down;
				else if (event.button.button == 3)
					g_holdRight = down;
				break;
			}
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				int nKey = event.key.keysym.scancode;

				// lshift/right shift
				if (nKey == SDL_SCANCODE_RSHIFT)
					nKey = SDL_SCANCODE_LSHIFT;
				else if (nKey == SDL_SCANCODE_RCTRL)
					nKey = SDL_SCANCODE_LCTRL;
				else if (nKey == SDL_SCANCODE_RALT)
					nKey = SDL_SCANCODE_LALT;

				if (event.type == SDL_KEYDOWN)
				{
					if(nKey == SDL_SCANCODE_LEFT)
					{
						g_currentModel--;
						g_currentModel = MAX(0, g_currentModel);

						Msg("Current: %d\n", g_currentModel);
					}
					else if (nKey == SDL_SCANCODE_RIGHT)
					{
						g_currentModel++;
						g_currentModel = MIN(MAX_MODELS, g_currentModel);

						Msg("Current: %d\n", g_currentModel);
					}
				}

				//Emulator_DoDebugKeys(nKey, (event.type == SDL_KEYUP) ? false : true);
				break;
			}
		}
	}
}

CRenderModel* g_renderModels[MAX_MODELS];
TextureID g_hwTexturePages[128][32];
ShaderID g_modelShader = -1;
extern bool g_originalTransparencyKey;

// Creates hardware texture
void InitHWTexturePage(int nPage)
{
	g_originalTransparencyKey = false;	// off
	
	texdata_t* page = &g_texPageData[nPage];

	if (page->data == nullptr)
		return;
	
	int imgSize = TEXPAGE_SIZE * 4;
	uint* color_data = (uint*)malloc(imgSize);

	memset(color_data, 0, imgSize);

	// Dump whole TPAGE indexes
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = page->data[y * 128 + (x >> 1)];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			int ypos = (TEXPAGE_SIZE_Y - y - 1) * TEXPAGE_SIZE_Y;

			color_data[ypos + x] = clindex * 32;
		}
	}

	int numDetails = g_texPages[nPage].numDetails;

	// FIXME: load indexes instead?

	for (int i = 0; i < numDetails; i++)
		ConvertIndexedTextureToRGBA(nPage, color_data, i, &page->clut[i], false);

	g_hwTexturePages[nPage][0] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);

	// also load different palettes
	int numPalettes = 0;
	for (int pal = 0; pal < 16; pal++)
	{
		bool anyMatched = false;
		for (int i = 0; i < g_numExtraPalettes; i++)
		{
			if (g_extraPalettes[i].tpage != nPage)
				continue;

			if (g_extraPalettes[i].palette != pal)
				continue;

			for (int j = 0; j < g_extraPalettes[i].texcnt; j++)
				ConvertIndexedTextureToRGBA(nPage, color_data, g_extraPalettes[i].texnum[j], &g_extraPalettes[i].clut, false);

			anyMatched = true;
		}

		if (anyMatched)
		{
			g_hwTexturePages[nPage][numPalettes+1] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);
			numPalettes++;
		}
	}

	// no longer need in RGBA data
	free(color_data);
}

extern int g_windowWidth;
extern int g_windowHeight;

void RenderView()
{
	Vector3D forward, right;
	AngleVectors(g_cameraAngles, &forward, &right);

	Vector3D cameraPos = g_cameraPosition - forward * g_cameraDistance;
	Vector3D cameraAngles = VDEG2RAD(g_cameraAngles);

	Matrix4x4 view, proj;

	proj = perspectiveMatrixY(DEG2RAD(g_cameraFOV), g_windowWidth, g_windowHeight, 0.01f, 1000.0f);
	view = rotateZXY4(-cameraAngles.x, -cameraAngles.y, -cameraAngles.z);

	view.translate(-cameraPos);

	GR_SetMatrix(MATRIX_VIEW, view);
	GR_SetMatrix(MATRIX_PROJECTION, proj);

	GR_SetMatrix(MATRIX_WORLD, identity4());

	GR_UpdateMatrixUniforms();
	GR_SetDepth(1);
	GR_SetCullMode(CULL_FRONT);

	if(g_renderModels[g_currentModel])
		g_renderModels[g_currentModel]->Draw();
}

//-------------------------------------------------------------
// Main level viewer
//-------------------------------------------------------------
int ViewerMain(const char* filename)
{
	if(!GR_Init("OpenDriver2 Level Tool - Viewer", 1280, 720, 0))
	{
		MsgError("Failed to init graphics!\n");
		return -1;
	}

	// create shader
	g_modelShader = GR_CompileShader(model_shader);

	// Load level file
	LoadLevelFile(filename);

	// Load permanent textures
	for(int i = 0; i < g_numTexPages; i++)
	{
		memset(g_hwTexturePages[i], 0, sizeof(g_hwTexturePages[0]));

		if(g_texPagePos[i].flags & TPAGE_PERMANENT)
		{
			InitHWTexturePage(i);
		}
	}

	// Load models
	for(int i = 0; i < MAX_MODELS; i++)
	{
		g_renderModels[i] = nullptr;

		CRenderModel* model = new CRenderModel();
		if (model->Initialize(&g_levelModels[i]))
			g_renderModels[i] = model;
		else
			delete model;
	}

	// Load car models
	for (int i = 0; i < MAX_CAR_MODELS; i++)
	{

	}

	do
	{
		SDLPollEvent();

		GR_BeginScene();
		
		GR_ClearColor(32, 32, 32);
		GR_ClearDepth(1.0f);
	
		// Control and map

		// Spool map
		
		// Render stuff
		RenderView();

		GR_EndScene();

		GR_SwapWindow();
	} while (!g_quit);

	// free all
	FreeLevelData();
	
	GR_Shutdown();

	return 0;
}