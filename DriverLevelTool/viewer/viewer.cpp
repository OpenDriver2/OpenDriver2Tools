#include <malloc.h>
#include <SDL_events.h>

#include "driver_level.h"
#include "gl_renderer.h"
#include "rendermodel.h"
#include "core/cmdlib.h"

#include "math/Volume.h"

#include "driver_routines/d2_types.h"
#include "driver_routines/level.h"
#include "driver_routines/models.h"
#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"
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
	"		if(color.a <= 0.5f) discard;\n"\
	"		lighting = vec4(color.rgb * u_ambientColor.rgb * u_ambientColor.a, color.a);\n"\
	"		lighting.rgb += u_lightColor.rgb * u_lightColor.a * color.rgb * max(1.0 - dot(v_normal, u_lightDir), 0);\n"\
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
int g_night = 0;
int g_cellsDrawDistance = 441;

int g_currentModel = 191;
bool g_holdLeft = false;
bool g_holdRight = false;

Vector3D g_cameraPosition(0);
Vector3D g_cameraAngles(25.0f, 45.0f, 0);

Vector3D g_cameraMoveDir(0);

float g_cameraDistance = 0.5f;
float g_cameraFOV = 75.0f;

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
				else if (nKey == SDL_SCANCODE_N && event.type == SDL_KEYDOWN)
				{
					g_night = !g_night;
				}
				else if (nKey == SDL_SCANCODE_PAGEUP && event.type == SDL_KEYDOWN)
				{
					g_cellsDrawDistance += 441;
				}
				else if (nKey == SDL_SCANCODE_PAGEDOWN && event.type == SDL_KEYDOWN)
				{
					g_cellsDrawDistance -= 441;
					if (g_cellsDrawDistance < 441)
						g_cellsDrawDistance = 441;
				}
				

				//Emulator_DoDebugKeys(nKey, (event.type == SDL_KEYUP) ? false : true);
				break;
			}
		}
	}
}

TextureID g_hwTexturePages[128][16];

//-----------------------------------------------------------------

struct WorldRenderProperties
{
	Vector4D ambientColor;
	Vector4D lightColor;
	Vector3D lightDir;
	
} g_worldRenderProperties;

void SetupLightingProperties(float ambientScale = 1.0f, float lightScale = 1.0f)
{
	g_worldRenderProperties.ambientColor = ColorRGBA(0.95f, 0.9f, 1.0f, 0.45f * ambientScale) ;
	g_worldRenderProperties.lightColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.4f * lightScale);
	g_worldRenderProperties.lightDir = normalize(Vector3D(1, -0.5, 0));
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
		tpage->ConvertIndexedTextureToRGBA(color_data, i, &bitmap.clut[i], false, false);

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
				tpage->ConvertIndexedTextureToRGBA(color_data, j, detail->extraCLUTs[pal], false, false);
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

extern TextureID g_whiteTexture;

TextureID GetHWTexture(int tpage, int pal)
{
	if (tpage < 0 || tpage >= 128 ||
		pal < 0 || pal >= 32)
		return g_whiteTexture;

	return g_hwTexturePages[tpage][pal];
}

void InitHWTextures()
{
	for (int i = 0; i < 128; i++)
	{
		for (int j = 0; j < 32; j++)
			g_hwTexturePages[i][j] = g_whiteTexture;
	}

}

//-----------------------------------------------------------------

extern int g_windowWidth;
extern int g_windowHeight;

const float MODEL_LOD_HIGH_MIN_DISTANCE = 5.0f;
const float MODEL_LOD_LOW_MIN_DISTANCE  = 40.0f;

ModelRef_t* GetModelCheckLods(int index, float distSqr)
{
	ModelRef_t* baseRef = g_levModels.GetModelByIndex(index);

	ModelRef_t* retRef = baseRef;
	if (baseRef->highDetailId != 0xFFFF)
	{
		if(distSqr < MODEL_LOD_HIGH_MIN_DISTANCE)
			return g_levModels.GetModelByIndex(baseRef->highDetailId);
	}

	if (baseRef->lowDetailId != 0xFFFF)
	{
		if (distSqr > MODEL_LOD_HIGH_MIN_DISTANCE)
			return g_levModels.GetModelByIndex(baseRef->lowDetailId);
	}

	return retRef;
}

void DrawLevelDriver2(const Vector3D& cameraPos, const Volume& frustrumVolume)
{
	CELL_ITERATOR ci;
	PACKED_CELL_OBJECT* ppco;

	int i = g_cellsDrawDistance;
	int vloop = 0;
	int hloop = 0;
	int dir = 0;
	XZPAIR cell;
	XZPAIR icell;

	VECTOR_NOPAD cameraPosition;
	cameraPosition.vx = cameraPos.x * 4096;
	cameraPosition.vy = cameraPos.y * 4096;
	cameraPosition.vz = cameraPos.z * 4096;

	CDriver2LevelMap* levMapDriver2 = (CDriver2LevelMap*)g_levMap;
	
	levMapDriver2->WorldPositionToCellXZ(cell, cameraPosition);

	// walk through all cells
	while (i >= 0)
	{
		if (abs(hloop) + abs(vloop) < 256)
		{
			// clamped vis values
			int vis_h = MIN(MAX(hloop, -9), 10);
			int vis_v = MIN(MAX(vloop, -9), 10);

			icell.x = cell.x + hloop;
			icell.z = cell.z + vloop;

			if(icell.x > -1 && icell.x < levMapDriver2->GetCellsAcross() &&
			   icell.z > -1 && icell.z < levMapDriver2->GetCellsDown())
			{
				levMapDriver2->SpoolRegion(icell);
				
				ppco = levMapDriver2->GetFirstPackedCop(&ci, icell.x, icell.z);

				// walk each cell object in cell
				while (ppco)
				{
					CELL_OBJECT co;
					CDriver2LevelMap::UnpackCellObject(co, ppco, ci.nearCell);

					if(co.type >= MAX_MODELS)
					{
						ppco = levMapDriver2->GetNextPackedCop(&ci);
						// WHAT THE FUCK?
						continue;
					}

					Vector3D absCellPosition(co.pos.vx * EXPORT_SCALING, co.pos.vy * -EXPORT_SCALING, co.pos.vz * EXPORT_SCALING);

					float distanceFromCamera = lengthSqr(absCellPosition - cameraPos);

					ModelRef_t* ref = GetModelCheckLods(co.type, distanceFromCamera);

					MODEL* model = ref->model;
					
					float cellRotationRad = -co.yang / 64.0f * PI_F * 2.0f;

					bool isGround = false;
					
					if(model)
					{
						if(model->shape_flags & SHAPE_FLAG_SMASH_SPRITE)
						{
							cellRotationRad = DEG2RAD(g_cameraAngles.y);
						}

						if ((model->shape_flags & (SHAPE_FLAG_SUBSURFACE | SHAPE_FLAG_ALLEYWAY)) ||
							(model->flags2 & (MODEL_FLAG_SIDEWALK | MODEL_FLAG_GRASS)))
						{
							isGround = true;
						}
					}

					
					Matrix4x4 objectMatrix = translate(absCellPosition) * rotateY4(cellRotationRad);
					GR_SetMatrix(MATRIX_WORLD, objectMatrix);
					GR_UpdateMatrixUniforms();
					
					if (isGround && g_night)
						SetupLightingProperties(0.5f, 0.5f);
					else
						SetupLightingProperties(1.0f, 1.0f);

					CRenderModel* renderModel = (CRenderModel*)ref->userData;
					
					if (renderModel)
					{
						const float boundSphere = ref->model->bounding_sphere * EXPORT_SCALING * 2.0f;
						if (frustrumVolume.IsSphereInside(absCellPosition, boundSphere))
							renderModel->Draw();
					}

					
					ppco = levMapDriver2->GetNextPackedCop(&ci);
				}
			}
		}

		if (dir == 0)
		{
			hloop++;

			if (hloop + vloop == 1)
				dir = 1;
		}
		else if (dir == 1)
		{
			vloop++;
			if (hloop == vloop)
				dir = 2;
		}
		else if (dir == 2)
		{
			hloop--;
			if (hloop + vloop == 0)
				dir = 3;
		}
		else
		{
			vloop--;
			if (hloop == vloop)
				dir = 0;
		}

		i--;
	}
}

void DrawLevelDriver1(const Vector3D& cameraPos, const Volume& frustrumVolume)
{
	CELL_ITERATOR_D1 ci;
	CELL_OBJECT* pco;

	int i = g_cellsDrawDistance;
	int vloop = 0;
	int hloop = 0;
	int dir = 0;
	XZPAIR cell;
	XZPAIR icell;

	VECTOR_NOPAD cameraPosition;
	cameraPosition.vx = cameraPos.x * 4096;
	cameraPosition.vy = cameraPos.y * 4096;
	cameraPosition.vz = cameraPos.z * 4096;

	CDriver1LevelMap* levMapDriver1 = (CDriver1LevelMap*)g_levMap;

	levMapDriver1->WorldPositionToCellXZ(cell, cameraPosition);

	// walk through all cells
	while (i >= 0)
	{
		if (abs(hloop) + abs(vloop) < 256)
		{
			// clamped vis values
			int vis_h = MIN(MAX(hloop, -9), 10);
			int vis_v = MIN(MAX(vloop, -9), 10);

			icell.x = cell.x + hloop;
			icell.z = cell.z + vloop;


			if (icell.x > -1 && icell.x < levMapDriver1->GetCellsAcross() &&
				icell.z > -1 && icell.z < levMapDriver1->GetCellsDown())
			{
				levMapDriver1->SpoolRegion(icell);

				pco = levMapDriver1->GetFirstCop(&ci, icell.x, icell.z);

				// walk each cell object in cell
				while (pco)
				{
					if (pco->type >= MAX_MODELS)
					{
						pco = levMapDriver1->GetNextCop(&ci);
						// WHAT THE FUCK?
						continue;
					}

					Vector3D absCellPosition(pco->pos.vx * EXPORT_SCALING, pco->pos.vy * -EXPORT_SCALING, pco->pos.vz * EXPORT_SCALING);
					float distanceFromCamera = lengthSqr(absCellPosition - cameraPos);

					ModelRef_t* ref = GetModelCheckLods(pco->type, distanceFromCamera);
					MODEL* model = ref->model;

					float cellRotationRad = -pco->yang / 64.0f * PI_F * 2.0f;

					bool isGround = false;

					if (model)
					{
						if (model->shape_flags & SHAPE_FLAG_SMASH_SPRITE)
						{
							cellRotationRad = DEG2RAD(g_cameraAngles.y);
						}

						if ((model->shape_flags & (SHAPE_FLAG_SUBSURFACE | SHAPE_FLAG_ALLEYWAY)) ||
							(model->flags2 & (MODEL_FLAG_SIDEWALK | MODEL_FLAG_GRASS)))
						{
							isGround = true;
						}
					}

					
					Matrix4x4 objectMatrix = translate(absCellPosition) * rotateY4(cellRotationRad);
					GR_SetMatrix(MATRIX_WORLD, objectMatrix);
					GR_UpdateMatrixUniforms();

					if (g_night)
						SetupLightingProperties(0.5f, 0.5f);
					else
						SetupLightingProperties(1.0f, 1.0f);

					CRenderModel* renderModel = (CRenderModel*)ref->userData;

					if (renderModel)
					{
						const float boundSphere = ref->model->bounding_sphere * EXPORT_SCALING * 2.0f;
						if(frustrumVolume.IsSphereInside(absCellPosition, boundSphere))
							renderModel->Draw();
					}

					pco = levMapDriver1->GetNextCop(&ci);
				}
			}
		}

		if (dir == 0)
		{
			hloop++;

			if (hloop + vloop == 1)
				dir = 1;
		}
		else if (dir == 1)
		{
			vloop++;
			if (hloop == vloop)
				dir = 2;
		}
		else if (dir == 2)
		{
			hloop--;

			if (hloop + vloop == 0)
				dir = 3;
		}
		else
		{
			vloop--;
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
	Volume frustumVolume;

	proj = perspectiveMatrixY(DEG2RAD(g_cameraFOV), g_windowWidth, g_windowHeight, 0.01f, 1000.0f);
	view = rotateZXY4(-cameraAngles.x, -cameraAngles.y, -cameraAngles.z);
	view.translate(-cameraPos);

	frustumVolume.LoadAsFrustum(proj * view);

	SetupLightingProperties();

	GR_SetMatrix(MATRIX_VIEW, view);
	GR_SetMatrix(MATRIX_PROJECTION, proj);

	GR_SetMatrix(MATRIX_WORLD, identity4());

	GR_UpdateMatrixUniforms();
	GR_SetDepth(1);
	GR_SetCullMode(CULL_FRONT);

	if(g_format >= LEV_FORMAT_DRIVER2_ALPHA16)
		DrawLevelDriver2(cameraPos, frustumVolume);
	else
		DrawLevelDriver1(cameraPos, frustumVolume);
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

void OnModelFreed(ModelRef_t* ref)
{
	CRenderModel* model = (CRenderModel*)ref->userData;

	if(model)
		model->Destroy();

	delete model;
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
	InitHWTextures();

	// set loading callbacks
	g_levTextures.SetLoadingCallbacks(InitHWTexturePage, nullptr);
	g_levModels.SetModelLoadingCallbacks(OnModelLoaded, OnModelFreed);

	// Load level file
	if (!LoadLevelFile(filename))
	{
		GR_Shutdown();
		return -1;
	}

	do
	{
		SDLPollEvent();

		GR_BeginScene();

		GR_ClearDepth(1.0f);

		if(g_night)
			GR_ClearColor(19 / 255.0f, 23 / 255.0f, 25 / 255.0f);
		else
			GR_ClearColor(128 / 255.0f, 158 / 255.0f, 182 / 255.0f);

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