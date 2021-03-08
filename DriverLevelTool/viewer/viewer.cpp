#include <malloc.h>
#include <SDL_events.h>

#include "driver_level.h"
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
	"		v_normal = mat3(u_World) * a_normal_tv.xyz;\n"\
	"		gl_Position = u_WorldViewProj * vec4(a_position_tu.xyz, 1.0);\n"\
	"	}\n"

#define MODEL_FRAGMENT_SHADER \
	"	uniform sampler2D s_texture;\n"\
	"	uniform vec3 u_lightDir;\n"\
	"	uniform vec4 u_ambientColor;\n"\
	"	uniform vec4 u_lightColor;\n"\
	"	void main() {\n"\
	"		vec4 lighting;\n"\
	"		vec4 color = texture2D(s_texture, v_texcoord.xy);\n"\
	"		lighting = vec4(color.rgb * u_ambientColor.rgb * u_ambientColor.a, color.a);\n"\
	"		lighting.rgb += u_lightColor.rgb * u_lightColor.a * color.rgb * saturate(1.0 - dot(v_normal, u_lightDir));\n"\
	"		fragColor = lighting;\n"\
	"	}\n"

const char* model_shader =
	"varying vec2 v_texcoord;\n"
	"varying vec3 v_normal;\n"
	"varying vec4 v_color;\n"
	"#ifdef VERTEX\n"
	MODEL_VERTEX_SHADER
	"#else\n"
	MODEL_FRAGMENT_SHADER
	"#endif\n";

int g_quit = 0;

int g_currentModel = 191;
bool g_holdLeft = false;
bool g_holdRight = false;

Vector3D g_cameraPosition(0);
Vector3D g_cameraAngles(25.0f, 45.0f, 0);

Vector3D g_cameraMoveDir(0);

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
					g_cameraAngles.x += event.motion.yrel * 0.25f;
					g_cameraAngles.y -= event.motion.xrel * 0.25f;
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

				if(nKey == SDL_SCANCODE_LEFT)
				{
					g_cameraMoveDir.x = (event.type == SDL_KEYDOWN) ? -1.0f : 0.0f;
				}
				else if (nKey == SDL_SCANCODE_RIGHT)
				{
					g_cameraMoveDir.x = (event.type == SDL_KEYDOWN) ? 1.0f : 0.0f;
				}
				else if (nKey == SDL_SCANCODE_UP)
				{
					g_cameraMoveDir.z = (event.type == SDL_KEYDOWN) ? 1.0f : 0.0f;
				}
				else if (nKey == SDL_SCANCODE_DOWN)
				{
					g_cameraMoveDir.z = (event.type == SDL_KEYDOWN) ? -1.0f : 0.0f;
				}

				//Emulator_DoDebugKeys(nKey, (event.type == SDL_KEYUP) ? false : true);
				break;
			}
		}
	}
}

TextureID g_hwTexturePages[128][32];

//-----------------------------------------------------------------

struct WorldRenderProperties
{
	Vector4D ambientColor;
	Vector4D lightColor;
	Vector3D lightDir;
	
} g_worldRenderProperties;

void SetupLightingProperties()
{
	g_worldRenderProperties.ambientColor = ColorRGBA(0.95f, 0.9f, 1.0f, 0.25f);
	g_worldRenderProperties.lightColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f);
	g_worldRenderProperties.lightDir = normalize(Vector3D(1, -1, -1));
}

//-----------------------------------------------------------------


struct ModelShaderInfo
{
	ShaderID	shader{ 0 };

	int			ambientColorConstantId{ -1 };
	int			lightColorConstantId{ -1 };

	int			lightDirConstantId{ -1 };
} g_modelShader;

void InitModelShader()
{
	// create shader
	g_modelShader.shader = GR_CompileShader(model_shader);

	g_modelShader.ambientColorConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_ambientColor");
	g_modelShader.lightColorConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_lightColor");

	g_modelShader.lightDirConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_lightDir");
}

void SetupModelShader()
{
	GR_SetShader(g_modelShader.shader);
	GR_SetShaderConstatntVector3D(g_modelShader.lightDirConstantId, g_worldRenderProperties.lightDir);

	GR_SetShaderConstatntVector4D(g_modelShader.ambientColorConstantId, g_worldRenderProperties.ambientColor);
	GR_SetShaderConstatntVector4D(g_modelShader.lightColorConstantId, g_worldRenderProperties.lightColor);
}

//-----------------------------------------------------------------

// Creates hardware texture
void InitHWTexturePage(CTexturePage* tpage)
{
	const TexBitmap_t& bitmap = tpage->GetBitmap();

	if (bitmap.data == nullptr)
		return;
	
	int imgSize = TEXPAGE_SIZE * 4;
	uint* color_data = (uint*)malloc(imgSize);

	memset(color_data, 0, imgSize);

	// Dump whole TPAGE indexes
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = bitmap.data[y * 128 + (x >> 1)];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			int ypos = (TEXPAGE_SIZE_Y - y - 1) * TEXPAGE_SIZE_Y;

			color_data[ypos + x] = clindex * 32;
		}
	}

	int numDetails = tpage->GetDetailCount();

	// FIXME: load indexes instead?

	for (int i = 0; i < numDetails; i++)
		tpage->ConvertIndexedTextureToRGBA(color_data, i, &bitmap.clut[i]);

	int tpageId = tpage->GetId();
	
	g_hwTexturePages[tpageId][0] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);

	// also load different palettes
	int numPalettes = 0;
	for (int pal = 0; pal < 16; pal++)
	{
		bool anyMatched = false;

		for (int j = 0; j < numDetails; j++)
		{
			TexDetailInfo_t* detail = tpage->GetTextureDetail(j);

			if (detail->extraCLUTs[pal])
			{
				tpage->ConvertIndexedTextureToRGBA(color_data, j, detail->extraCLUTs[pal]);
				anyMatched = true;
			}
		}

		if (anyMatched)
		{
			g_hwTexturePages[tpageId][numPalettes + 1] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);
			numPalettes++;
		}
	}
	
	
	// no longer need in RGBA data
	free(color_data);
}

TextureID GetHWTexture(int tpage, int pal)
{
	extern TextureID g_whiteTexture;
	
	if (tpage < 0)
		return g_whiteTexture;

	return g_hwTexturePages[tpage][pal];
}

//-----------------------------------------------------------------

extern int g_windowWidth;
extern int g_windowHeight;

void DrawLevelDriver2(const Vector3D& cameraPos)
{
	CELL_ITERATOR ci;
	PACKED_CELL_OBJECT* ppco;

	int i = 441 * 2;
	int vloop = 0;
	int hloop = 0;
	int dir = 0;
	XZPAIR cell;
	XZPAIR icell;

	VECTOR_NOPAD cameraPosition;
	cameraPosition.vx = cameraPos.x * 4096;
	cameraPosition.vy = cameraPos.y * 4096;
	cameraPosition.vz = cameraPos.z * 4096;
	
	g_levMap.WorldPositionToCellXZ(cell, cameraPosition);

	// walk through all cells
	while (i >= 0)
	{
		if (abs(hloop) + abs(vloop) < 64)
		{
			// clamped vis values
			int vis_h = MIN(MAX(hloop, -9), 10);
			int vis_v = MIN(MAX(vloop, -9), 10);

			icell.x = cell.x + hloop;
			icell.z = cell.z + vloop;

#if 0
			if (rightPlane < 0 &&
				leftPlane > 0 &&
				backPlane < farClipLimit &&  // check planes
				PVS_ptr[vis_v * pvs_square + vis_h]) // check PVS table
#endif
			if(icell.x > -1 && icell.x < g_levMap.GetCellsAcross() && 
			   icell.z > -1 && icell.z < g_levMap.GetCellsDown())
			{
				g_levMap.SpoolRegion(icell);
				
				ppco = g_levMap.GetFirstPackedCop(&ci, icell.x, icell.z);

				// walk each cell object in cell
				while (ppco)
				{
					CELL_OBJECT co;
					CDriver2LevelMap::UnpackCellObject(co, ppco, ci.nearCell);

					if(co.type >= MAX_MODELS)
					{
						ppco = g_levMap.GetNextPackedCop(&ci);
						// WHAT THE FUCK?
						continue;
					}

					float cellRotationRad = -co.yang / 64.0f * PI_F * 2.0f;

					Vector3D absCellPosition(co.pos.vx * EXPORT_SCALING, co.pos.vy * -EXPORT_SCALING, co.pos.vz * EXPORT_SCALING);
					Matrix4x4 objectMatrix = translate(absCellPosition) * rotateY4(cellRotationRad);
					GR_SetMatrix(MATRIX_WORLD, objectMatrix);
					GR_UpdateMatrixUniforms();
					
					ModelRef_t* ref = g_levModels.GetModelByIndex(co.type);

					CRenderModel* renderModel = (CRenderModel*)ref->userData;
					
					if (renderModel)
						renderModel->Draw();
					
					ppco = g_levMap.GetNextPackedCop(&ci);
				}
			}
		}

		if (dir == 0)
		{
			//leftPlane += leftcos;
			//backPlane += backcos;
			//rightPlane += rightcos;

			hloop++;

			if (hloop + vloop == 1)
				dir = 1;
		}
		else if (dir == 1)
		{
			//leftPlane += leftsin;
			//backPlane += backsin;
			//rightPlane += rightsin;
			vloop++;

			//PVS_ptr += pvs_square;

			if (hloop == vloop)
				dir = 2;
		}
		else if (dir == 2)
		{
			hloop--;
			//leftPlane -= leftcos;
			//backPlane -= backcos;
			//rightPlane -= rightcos;

			if (hloop + vloop == 0)
				dir = 3;
		}
		else
		{
			//leftPlane -= leftsin;
			//backPlane -= backsin;
			//rightPlane -= rightsin;
			vloop--;

			//PVS_ptr -= pvs_square;

			if (hloop == vloop)
				dir = 0;
		}

		i--;
	}
}

void RenderView()
{
	Vector3D forward, right;
	AngleVectors(g_cameraAngles, &forward, &right);

	// TODO: tie to framerate
	g_cameraPosition += g_cameraMoveDir.x * right * 0.15f;
	g_cameraPosition += g_cameraMoveDir.z * forward * 0.15f;

	Vector3D cameraPos = g_cameraPosition;// -forward * g_cameraDistance;
	Vector3D cameraAngles = VDEG2RAD(g_cameraAngles);

	Matrix4x4 view, proj;

	proj = perspectiveMatrixY(DEG2RAD(g_cameraFOV), g_windowWidth, g_windowHeight, 0.01f, 1000.0f);
	view = rotateZXY4(-cameraAngles.x, -cameraAngles.y, -cameraAngles.z);

	view.translate(-cameraPos);

	SetupLightingProperties();

	GR_SetMatrix(MATRIX_VIEW, view);
	GR_SetMatrix(MATRIX_PROJECTION, proj);

	GR_SetMatrix(MATRIX_WORLD, identity4());

	GR_UpdateMatrixUniforms();
	GR_SetDepth(1);
	GR_SetCullMode(CULL_FRONT);

	DrawLevelDriver2(cameraPos);
}

void OnRegionLoaded(CDriver2LevelRegion* region)
{
	
}

void OnModelLoaded(ModelRef_t* ref)
{
	if (!ref->model)
		return;

	CRenderModel* renderModel = new CRenderModel();

	if (renderModel->Initialize(ref))
		ref->userData = renderModel;
	else
		delete renderModel;
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

	InitModelShader();

	// set loading callbacks
	g_levMap.SetLoadingCallbacks(OnRegionLoaded, nullptr);
	g_levTextures.SetLoadingCallbacks(InitHWTexturePage, nullptr);
	g_levModels.SetModelLoadingCallbacks(OnModelLoaded, nullptr);

	// Load level file
	LoadLevelFile(filename);

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