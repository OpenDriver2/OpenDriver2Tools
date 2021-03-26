#include <malloc.h>
#include <SDL_events.h>

#include <nstd/Array.hpp>
#include <nstd/String.hpp>
#include <nstd/Time.hpp>

#include "gl_renderer.h"
#include "rendermodel.h"

#include "core/cmdlib.h"
#include "core/VirtualStream.h"

#include "math/Volume.h"

#include "driver_routines/d2_types.h"
#include "driver_routines/level.h"
#include "driver_routines/models.h"
#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"
#include "driver_routines/textures.h"

#include "imgui_impl/imgui_impl_opengl3.h"
#include "imgui_impl/imgui_impl_sdl.h"

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
int g_nightMode = 0;
int g_cellsDrawDistance = 441;

int g_currentModel = 0;
int g_renderMode = 0;
int g_noLod = 0;

bool g_holdLeft = false;
bool g_holdRight = false;
bool g_holdShift = false;

Vector3D g_cameraPosition(0);
Vector3D g_cameraAngles(25.0f, 45.0f, 0);

Vector3D g_cameraMoveDir(0);

float g_cameraDistance = 0.5f;
float g_cameraFOV = 75.0f;

//-----------------------------------------------------------------

struct WorldRenderProperties
{
	Vector4D ambientColor;
	Vector4D lightColor;
	Vector3D lightDir;
	
} g_worldRenderProperties;

// sets up lighting properties
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

// compiles model shader
void InitModelShader()
{
	// create shader
	g_modelShader.shader = GR_CompileShader(model_shader);

	g_modelShader.ambientColorConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_ambientColor");
	g_modelShader.lightColorConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_lightColor");

	g_modelShader.lightDirConstantId = GR_GetShaderConstantIndex(g_modelShader.shader, "u_lightDir");
}

// prepares shader for rendering
// used for Models
void SetupModelShader()
{
	GR_SetShader(g_modelShader.shader);
	GR_SetShaderConstatntVector3D(g_modelShader.lightDirConstantId, g_worldRenderProperties.lightDir);

	GR_SetShaderConstatntVector4D(g_modelShader.ambientColorConstantId, g_worldRenderProperties.ambientColor);
	GR_SetShaderConstatntVector4D(g_modelShader.lightColorConstantId, g_worldRenderProperties.lightColor);
}

//-----------------------------------------------------------------

TextureID g_hwTexturePages[128][16];

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

// returns hardware texture
TextureID GetHWTexture(int tpage, int pal)
{
	if (tpage < 0 || tpage >= 128 ||
		pal < 0 || pal >= 32)
		return g_whiteTexture;

	return g_hwTexturePages[tpage][pal];
}

// Dummy texture initilization
void InitHWTextures()
{
	for (int i = 0; i < 128; i++)
	{
		for (int j = 0; j < 32; j++)
			g_hwTexturePages[i][j] = g_whiteTexture;
	}
}

//-----------------------------------------------------------------

// called when model loaded in CDriverLevelModels
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

// called when model freed in CDriverLevelModels
void OnModelFreed(ModelRef_t* ref)
{
	CRenderModel* model = (CRenderModel*)ref->userData;

	if (model)
		model->Destroy();

	delete model;
}

// extern some vars
extern String					g_levname;
extern OUT_CITYLUMP_INFO		g_levInfo;
extern CDriverLevelTextures		g_levTextures;
extern CDriverLevelModels		g_levModels;
extern CBaseLevelMap*			g_levMap;

FILE* g_levFile = nullptr;

//-------------------------------------------------------
// Perorms level loading and renderer data initialization
//-------------------------------------------------------
bool LoadLevelFile()
{
	g_levFile = fopen(g_levname, "rb");
	if (!g_levFile)
	{
		MsgError("Cannot open %s\n", (char*)g_levname);
		return false;
	}

	CFileStream stream(g_levFile);

	// set loading callbacks
	g_levTextures.SetLoadingCallbacks(InitHWTexturePage, nullptr);
	g_levModels.SetModelLoadingCallbacks(OnModelLoaded, OnModelFreed);

	ELevelFormat levFormat = CDriverLevelLoader::DetectLevelFormat(&stream);

	// create map accordingly
	if (levFormat >= LEV_FORMAT_DRIVER2_ALPHA16 || levFormat == LEV_FORMAT_AUTODETECT)
		g_levMap = new CDriver2LevelMap();
	else
		g_levMap = new CDriver1LevelMap();

	CDriverLevelLoader loader;
	loader.Initialize(g_levInfo, &g_levTextures, &g_levModels, g_levMap);

	return loader.LoadFromFile(g_levname);
}

//-------------------------------------------------------
// Frees all data
//-------------------------------------------------------
void FreeLevelData()
{
	MsgWarning("Freeing level data ...\n");

	g_levMap->FreeAll();
	g_levTextures.FreeAll();
	g_levModels.FreeAll();

	delete g_levMap;

	fclose(g_levFile);
}

//-----------------------------------------------------------------

extern int g_windowWidth;
extern int g_windowHeight;

const float MODEL_LOD_HIGH_MIN_DISTANCE = 5.0f;
const float MODEL_LOD_LOW_MIN_DISTANCE  = 40.0f;

//-------------------------------------------------------
// returns specific model or LOD model
// based on the distance from camera
//-------------------------------------------------------
ModelRef_t* GetModelCheckLods(int index, float distSqr)
{
	ModelRef_t* baseRef = g_levModels.GetModelByIndex(index);

	if (g_noLod)
		return baseRef;

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

// stats counters
int g_drawnCells;
int g_drawnModels;
int g_drawnPolygons;

//-------------------------------------------------------
// Draws Driver 2 level region cells
// and spools the world if needed
//-------------------------------------------------------
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

	g_drawnCells = 0;
	g_drawnModels = 0;
	g_drawnPolygons = 0;

	VECTOR_NOPAD cameraPosition;
	cameraPosition.vx = cameraPos.x * 4096;
	cameraPosition.vy = cameraPos.y * 4096;
	cameraPosition.vz = cameraPos.z * 4096;

	CDriver2LevelMap* levMapDriver2 = (CDriver2LevelMap*)g_levMap;
	CFileStream spoolStream(g_levFile);

	SPOOL_CONTEXT spoolContext;
	spoolContext.dataStream = &spoolStream;
	spoolContext.lumpInfo = &g_levInfo;
	spoolContext.models = &g_levModels;
	spoolContext.textures = &g_levTextures;

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
				levMapDriver2->SpoolRegion(spoolContext, icell);
				
				ppco = levMapDriver2->GetFirstPackedCop(&ci, icell.x, icell.z);

				g_drawnCells++;
				
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

					Vector3D absCellPosition(co.pos.vx * RENDER_SCALING, co.pos.vy * -RENDER_SCALING, co.pos.vz * RENDER_SCALING);

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
					
					if (isGround && g_nightMode)
						SetupLightingProperties(0.35f, 0.0f);
					else
						SetupLightingProperties(1.0f, 1.0f);

					CRenderModel* renderModel = (CRenderModel*)ref->userData;
					
					if (renderModel)
					{
						const float boundSphere = ref->model->bounding_sphere * RENDER_SCALING * 2.0f;
						if (frustrumVolume.IsSphereInside(absCellPosition, boundSphere))
						{
							renderModel->Draw();
							g_drawnModels++;
							g_drawnPolygons += ref->model->num_polys;
						}
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

//-------------------------------------------------------
// Draws Driver 2 level region cells
// and spools the world if needed
//-------------------------------------------------------
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

	g_drawnCells = 0;
	g_drawnModels = 0;
	g_drawnPolygons = 0;

	VECTOR_NOPAD cameraPosition;
	cameraPosition.vx = cameraPos.x * 4096;
	cameraPosition.vy = cameraPos.y * 4096;
	cameraPosition.vz = cameraPos.z * 4096;

	CDriver1LevelMap* levMapDriver1 = (CDriver1LevelMap*)g_levMap;
	CFileStream spoolStream(g_levFile);

	SPOOL_CONTEXT spoolContext;
	spoolContext.dataStream = &spoolStream;
	spoolContext.lumpInfo = &g_levInfo;
	spoolContext.models = &g_levModels;
	spoolContext.textures = &g_levTextures;

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
				levMapDriver1->SpoolRegion(spoolContext, icell);

				pco = levMapDriver1->GetFirstCop(&ci, icell.x, icell.z);

				g_drawnCells++;

				// walk each cell object in cell
				while (pco)
				{
					if (pco->type >= MAX_MODELS)
					{
						pco = levMapDriver1->GetNextCop(&ci);
						// WHAT THE FUCK?
						continue;
					}

					Vector3D absCellPosition(pco->pos.vx * RENDER_SCALING, pco->pos.vy * -RENDER_SCALING, pco->pos.vz * RENDER_SCALING);
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

					if (g_nightMode)
						SetupLightingProperties(0.45f, 0.0f);
					else
						SetupLightingProperties(1.0f, 1.0f);

					CRenderModel* renderModel = (CRenderModel*)ref->userData;

					if (renderModel)
					{
						const float boundSphere = ref->model->bounding_sphere * RENDER_SCALING * 2.0f;
						if(frustrumVolume.IsSphereInside(absCellPosition, boundSphere))
						{
							renderModel->Draw();
							g_drawnModels++;
							g_drawnPolygons += ref->model->num_polys;
						}
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

const float CAMERA_MOVEMENT_SPEED_FACTOR = 140 * RENDER_SCALING;
const float CAMERA_MOVEMENT_ACCELERATION = 15 * RENDER_SCALING;
const float CAMERA_MOVEMENT_DECELERATION = 450 * RENDER_SCALING;

Vector3D g_cameraVelocity(0);

//-------------------------------------------------------
// Updates camera movement for level viewer
//-------------------------------------------------------
void UpdateCameraMovement(float deltaTime)
{
	Vector3D forward, right;
	AngleVectors(g_cameraAngles, &forward, &right);
	
	float cameraSpeedModifier = 1.0f;

	if (g_holdShift)
		cameraSpeedModifier = 4.0f;

	const float maxSpeed = CAMERA_MOVEMENT_SPEED_FACTOR * cameraSpeedModifier;
	
	if (lengthSqr(g_cameraMoveDir) > 0.1f && 
		length(g_cameraVelocity) < maxSpeed)
	{
		g_cameraVelocity += g_cameraMoveDir.x * right * deltaTime * CAMERA_MOVEMENT_ACCELERATION * cameraSpeedModifier;
		g_cameraVelocity += g_cameraMoveDir.z * forward * deltaTime * CAMERA_MOVEMENT_ACCELERATION * cameraSpeedModifier;

		//if (length(g_cameraVelocity) > maxSpeed)
		//	g_cameraVelocity = normalize(g_cameraVelocity) * maxSpeed;
	}
	else
	{
		g_cameraVelocity -= g_cameraVelocity * CAMERA_MOVEMENT_DECELERATION * deltaTime;
	}

	g_cameraPosition += g_cameraVelocity * deltaTime;
}

//-------------------------------------------------------
// Render level viewer
//-------------------------------------------------------
void RenderLevelView(float deltaTime)
{
	Vector3D cameraPos = g_cameraPosition;
	Vector3D cameraAngles = VDEG2RAD(g_cameraAngles);

	// calculate view matrices
	Matrix4x4 view, proj;
	Volume frustumVolume;

	proj = perspectiveMatrixY(DEG2RAD(g_cameraFOV), g_windowWidth, g_windowHeight, 0.01f, 1000.0f);
	view = rotateZXY4(-cameraAngles.x, -cameraAngles.y, -cameraAngles.z);
	view.translate(-cameraPos);

	// calculate frustum volume
	frustumVolume.LoadAsFrustum(proj * view);

	GR_SetMatrix(MATRIX_VIEW, view);
	GR_SetMatrix(MATRIX_PROJECTION, proj);

	GR_SetMatrix(MATRIX_WORLD, identity4());

	GR_UpdateMatrixUniforms();
	
	GR_SetDepth(1);
	GR_SetCullMode(CULL_FRONT);

	// reset lighting
	SetupLightingProperties();
	
	if(g_levMap->GetFormat() >= LEV_FORMAT_DRIVER2_ALPHA16)
		DrawLevelDriver2(cameraPos, frustumVolume);
	else
		DrawLevelDriver1(cameraPos, frustumVolume);
}

//-------------------------------------------------------
// Render model viewer
//-------------------------------------------------------
void RenderModelView()
{
	Vector3D forward, right;
	AngleVectors(g_cameraAngles, &forward, &right);

	Vector3D cameraPos = vec3_zero - forward * g_cameraDistance;
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

	ModelRef_t* ref = g_levModels.GetModelByIndex(g_currentModel);

	if(ref && ref->userData)
	{
		CRenderModel* renderModel = (CRenderModel*)ref->userData;

		renderModel->Draw();
	}
}

void SpoolAllAreaDatas()
{
	// Open file stream
	FILE* fp = fopen(g_levname, "rb");
	if (fp)
	{
		CFileStream stream(fp);

		SPOOL_CONTEXT spoolContext;
		spoolContext.dataStream = &stream;
		spoolContext.lumpInfo = &g_levInfo;
		spoolContext.models = &g_levModels;
		spoolContext.textures = &g_levTextures;

		int totalRegions = g_levMap->GetRegionsAcross() * g_levMap->GetRegionsDown();
		
		for (int i = 0; i < totalRegions; i++)
		{
			g_levMap->SpoolRegion(spoolContext, i);
		}

		fclose(fp);
	}
	else
		MsgError("Unable to spool area datas!\n");
}

char g_modelSearchNameBuffer[64];

void PopulateUIModelNames()
{
	memset(g_modelSearchNameBuffer, 0, sizeof(g_modelSearchNameBuffer));
}

void DisplayUI()
{
	if(ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Mode"))
		{
			if (ImGui::MenuItem("Level viewer"))
				g_renderMode = 0;
			
			if (ImGui::MenuItem("Model viewer"))
				g_renderMode = 1;
			
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Level"))
		{
			if (ImGui::MenuItem("Spool all Area Data"))
				SpoolAllAreaDatas();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Night mode", nullptr, g_nightMode))
				g_nightMode ^= 1;

			if (ImGui::MenuItem("Disable LODs", nullptr, g_noLod))
				g_noLod ^= 1;
			
			ImGui::EndMenu();
		}
		
		ImGui::EndMainMenuBar();
	}

	if(ImGui::Begin("HelpFrame", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiInputTextFlags_NoHorizontalScroll |
		ImGuiWindowFlags_NoSavedSettings | ImGuiColorEditFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus))
	{
		ImGui::SetWindowPos(ImVec2(0, 24));

		if(g_renderMode == 0)
		{
			ImGui::SetWindowSize(ImVec2(400, 120));
			
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.25f, 1.0f), "Position: X: %d Y: %d Z: %d",
				int(g_cameraPosition.x * ONE), int(g_cameraPosition.y * ONE), int(g_cameraPosition.z * ONE));

			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Draw distance: %d", g_cellsDrawDistance);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Drawn cells: %d", g_drawnCells);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Drawn models: %d", g_drawnModels);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Drawn polygons: %d", g_drawnPolygons);
		}
		else if (g_renderMode == 1)
		{
			ImGui::SetWindowSize(ImVec2(400, 720));
			
			ModelRef_t* ref = g_levModels.GetModelByIndex(g_currentModel);
			MODEL* model = ref->model;

			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.5f), "Use arrows to change models");
			
			if(model)
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Polygons: %d", model->num_polys);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Vertices: %d", model->num_vertices);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Point normals: %d", model->num_point_normals);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Instance number: %d", model->instance_number);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Bounding sphere: %d", model->bounding_sphere);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Z bias: %d", model->zBias);
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "Collision boxes: %d", model->GetCollisionBoxCount());

				if(ref->lowDetailId != 0xFFFF || ref->highDetailId != 0xFFFF)
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "LODs:");
					ImGui::SameLine();

					if(ref->lowDetailId != 0xFFFF)
					{
						ImGui::SameLine();
						if(ImGui::Button("L", ImVec2(14, 14)))
							g_currentModel = ref->lowDetailId;
					}
					
					if(ref->highDetailId != 0xFFFF)
					{
						ImGui::SameLine();
						if (ImGui::Button("H", ImVec2(14, 14)))
							g_currentModel = ref->highDetailId;
					}
				}
				else
				{
					ImGui::Text("");
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.5f), "MODEL NOT SPOOLED YET");
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
			}

			ImGui::InputText("Filter", g_modelSearchNameBuffer, sizeof(g_modelSearchNameBuffer));

			ImGuiTextFilter filter(g_modelSearchNameBuffer);

			Array<ModelRef_t*> modelRefs;
			
			for (int i = 0; i < MAX_MODELS; i++)
			{
				ModelRef_t* itemRef = g_levModels.GetModelByIndex(i);
				
				if(!filter.IsActive() && !itemRef->name || itemRef->name && filter.PassFilter(itemRef->name))
					modelRefs.append(itemRef);
			}
			
			if (ImGui::ListBoxHeader("", modelRefs.size(), 30))
			{
				ImGuiListClipper clipper(modelRefs.size(), ImGui::GetTextLineHeightWithSpacing());
				
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
						ModelRef_t* itemRef = modelRefs[i];
						const bool item_selected = (itemRef->index == g_currentModel);
						
						String item = String::fromPrintf("%d: %s%s", itemRef->index, itemRef->name ? itemRef->name : "", itemRef->model ? "" : "(empty slot)");

						ImGui::PushID(i);

						if (ImGui::Selectable(item, item_selected))
						{
							g_currentModel = itemRef->index;
						}

						ImGui::PopID();
					}
				}
				ImGui::ListBoxFooter();
			}
		}
		ImGui::End();
	}
}

//-------------------------------------------------------------
// SDL2 event handling
//-------------------------------------------------------------
void SDLPollEvent()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

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

			if (g_holdLeft)
			{
				g_cameraAngles.x += event.motion.yrel * 0.25f;
				g_cameraAngles.y -= event.motion.xrel * 0.25f;
			}
			else if (g_holdRight)
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

			if (nKey == SDL_SCANCODE_LSHIFT || nKey == SDL_SCANCODE_RSHIFT)
				g_holdShift = (event.type == SDL_KEYDOWN);
			else if (nKey == SDL_SCANCODE_LEFT)
			{
				g_cameraMoveDir.x = (event.type == SDL_KEYDOWN) ? -1.0f : 0.0f;
			}
			else if (nKey == SDL_SCANCODE_RIGHT)
			{
				g_cameraMoveDir.x = (event.type == SDL_KEYDOWN) ? 1.0f : 0.0f;
			}
			else if (nKey == SDL_SCANCODE_UP)
			{
				if (g_renderMode == 0)
					g_cameraMoveDir.z = (event.type == SDL_KEYDOWN) ? 1.0f : 0.0f;
				else if (g_renderMode == 1 && (event.type == SDL_KEYDOWN))
				{
					g_currentModel--;
					g_currentModel = MAX(0, g_currentModel);
				}
			}
			else if (nKey == SDL_SCANCODE_DOWN)
			{
				if (g_renderMode == 0)
					g_cameraMoveDir.z = (event.type == SDL_KEYDOWN) ? -1.0f : 0.0f;
				else if (g_renderMode == 1 && (event.type == SDL_KEYDOWN))
				{
					g_currentModel++;
					g_currentModel = MIN(MAX_MODELS, g_currentModel);
				}
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

			break;
		}
		}
	}
}

extern SDL_Window* g_window;

//-------------------------------------------------------------
// Loop
//-------------------------------------------------------------
void ViewerMainLoop()
{
	int64 oldTicks = Time::microTicks();

	// main loop
	do
	{
		// compute time
		const float ticks_to_ms = 1.0f / 10000.0f;
		int64 curTicks = Time::microTicks();
		
		float deltaTime = double(curTicks - oldTicks) * ticks_to_ms;
		
		oldTicks = curTicks;
		
		SDLPollEvent();

		GR_BeginScene();

		ImGui_ImplSDL2_NewFrame(g_window);
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();

		GR_ClearDepth(1.0f);

		if (g_nightMode)
			GR_ClearColor(19 / 255.0f, 23 / 255.0f, 25 / 255.0f);
		else
			GR_ClearColor(128 / 255.0f, 158 / 255.0f, 182 / 255.0f);

		// Render stuff
		if (g_renderMode == 0)
		{
			UpdateCameraMovement(deltaTime);
			RenderLevelView(deltaTime);
		}
		else
		{
			RenderModelView();
		}

		// Do ImGUI interface
		DisplayUI();

		// draw stuff
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		GR_EndScene();
		GR_SwapWindow();
		
	} while (!g_quit);
}

//-------------------------------------------------------------
// Main level viewer
//-------------------------------------------------------------
int ViewerMain()
{
	if(!GR_Init("OpenDriver2 Level viewer", 1280, 720, 0))
	{
		MsgError("Failed to init graphics!\n");
		return -1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(g_window, nullptr);
	ImGui_ImplOpenGL3_Init();

	InitModelShader();
	InitHWTextures();

	// Load level file
	if (!LoadLevelFile())
	{
		GR_Shutdown();
		return -1;
	}

	// this is for filtering purposes
	PopulateUIModelNames();

	// loop and stuff
	ViewerMainLoop();

	// free all
	FreeLevelData();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	GR_Shutdown();

	return 0;
}