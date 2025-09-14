#include "debug_overlay.h"
#include "gl_renderer.h"

#include <string.h>

#include "core/VirtualStream.h"
#include "driver_routines/models.h"
#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"
#include "driver_routines/textures.h"
#include "math/Volume.h"
#include "math/isin.h"
#include "rendermodel.h"
#include "core/cmdlib.h"

#include "convert.h"
#include "camera.h"

// extern some vars
extern String					g_levname;
extern String					g_levname_moddir;
extern String					g_levname_texdir;
extern OUT_CITYLUMP_INFO		g_levInfo;
extern CDriverLevelTextures		g_levTextures;
extern CDriverLevelModels		g_levModels;
extern CBaseLevelMap*			g_levMap;

extern FILE* g_levFile;

extern bool g_nightMode;
extern bool g_displayCollisionBoxes;
extern bool g_displayHeightMap;
extern bool g_displayAllCellLevels;
extern bool g_displayRoads;
extern bool g_displayRoadConnections;
extern bool g_noLod;
extern int g_cellsDrawDistance;

//-----------------------------------------------------------------

const float MODEL_LOD_HIGH_MIN_DISTANCE = 1.0f;
const float MODEL_LOD_LOW_MIN_DISTANCE = 5.0f;

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
		if (distSqr < MODEL_LOD_HIGH_MIN_DISTANCE * MODEL_LOD_HIGH_MIN_DISTANCE)
			return g_levModels.GetModelByIndex(baseRef->highDetailId);
	}

	if (baseRef->lowDetailId != 0xFFFF)
	{
		if (distSqr > MODEL_LOD_LOW_MIN_DISTANCE * MODEL_LOD_LOW_MIN_DISTANCE)
			return g_levModels.GetModelByIndex(baseRef->lowDetailId);
	}

	return retRef;
}

// stats counters
int g_drawnCells;
int g_drawnModels;
int g_drawnPolygons;

void DrawCellObject(const CELL_OBJECT& co, const Vector3D& cameraPos, float cameraAngleY, const Volume& frustrumVolume, bool buildingLighting)
{
	if (co.type >= MAX_MODELS)
	{
		// WHAT THE FUCK?
		return;
	}

	Vector3D absCellPosition = FromFixedVector(co.pos);
	absCellPosition.y *= -1.0f;

	const float distanceFromCamera = lengthSqr(absCellPosition - cameraPos);

	ModelRef_t* ref = GetModelCheckLods(co.type, distanceFromCamera);

	if (!ref->model)
		return;

	MODEL* model = ref->model;

	CRenderModel* renderModel = (CRenderModel*)ref->userData;

	if (!renderModel)
		return;

	// check if it is in view
	const float boundSphere = model->bounding_sphere * RENDER_SCALING * 2.0f;

	if (!frustrumVolume.IsSphereInside(absCellPosition, boundSphere))
		return;

	bool isGround = false;

	if ((model->shape_flags & (SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) ||
		(model->flags2 & (MODEL_FLAG_PATH | MODEL_FLAG_GRASS)))
	{
		isGround = true;
	}

	// compute world matrix
	{
		Matrix4x4 objectMatrix;

		if (model->shape_flags & SHAPE_FLAG_SPRITE)
		{
			objectMatrix = rotateY4(DEG2RAD(cameraAngleY));
		}
		else 
		{
			const float cellRotationRad = -co.yang / 64.0f * PI_F * 2.0f;
			objectMatrix = rotateY4(cellRotationRad);
		}
			
		objectMatrix.setTranslationTransposed(absCellPosition);

		GR_SetMatrix(MATRIX_WORLD, objectMatrix);
		GR_UpdateMatrixUniforms();
	}

	const float nightAmbientScale = 0.35f;
	const float nightLightScale = 0.0f;

	const float ambientScale = 1.0f;
	const float lightScale = 1.0;

	// apply lighting
	if ((isGround || !buildingLighting) && g_nightMode)
		CRenderModel::SetupLightingProperties(nightAmbientScale, nightLightScale);
	else
		CRenderModel::SetupLightingProperties(ambientScale, lightScale);

	renderModel->SetupModelShader();
	renderModel->SetDrawBuffer();
	renderModel->DrawBatchs();

	extern TextureID g_whiteTexture;
	extern bool g_displayWireframeBackfaces;

	if (g_displayWireframeBackfaces)
	{
		GR_SetFillMode(FILL_WIREFRAME);
		GR_SetCullMode(CULL_BACK);

		GR_SetTexture(g_whiteTexture);
		renderModel->DrawBatchs(true);

		GR_SetFillMode(FILL_SOLID);
		GR_SetCullMode(CULL_FRONT);
	}

	g_drawnModels++;
	g_drawnPolygons += ref->model->num_polys;

	// debug
	if (g_displayCollisionBoxes)
		CRenderModel::DrawModelCollisionBox(ref, co.pos, co.yang);
}

struct PCO_PAIR_D2
{
	PACKED_CELL_OBJECT* pco;
	XZPAIR nearCell;
};

//-------------------------------------------------------
// Draws Driver 2 level region cells
// and spools the world if needed
//-------------------------------------------------------
void DrawLevelDriver2(const Vector3D& cameraPos, float cameraAngleY, const Volume& frustrumVolume)
{
	CELL_ITERATOR_CACHE iteratorCache;
	int i = g_cellsDrawDistance;
	int vloop = 0;
	int hloop = 0;
	int dir = 0;
	XZPAIR cell;
	XZPAIR icell;

	g_drawnCells = 0;
	g_drawnModels = 0;
	g_drawnPolygons = 0;

	VECTOR_NOPAD cameraPosition = ToFixedVector(cameraPos);

	CDriver2LevelMap* levMapDriver2 = (CDriver2LevelMap*)g_levMap;
	CFileStream spoolStream(g_levFile);

	SPOOL_CONTEXT spoolContext;
	spoolContext.dataStream = &spoolStream;
	spoolContext.lumpInfo = &g_levInfo;

	levMapDriver2->WorldPositionToCellXZ(cell, cameraPosition);

	static Array<PCO_PAIR_D2> drawObjects;
	drawObjects.reserve(g_cellsDrawDistance * 2);
	drawObjects.clear();

	CBaseLevelRegion* currentRegion = nullptr;

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

			CBaseLevelRegion* reg = g_levMap->GetRegion(icell);

			if (currentRegion != reg)
				memset(&iteratorCache, 0, sizeof(iteratorCache));
			currentRegion = reg;

			if ( //rightPlane < 0 &&
				//leftPlane > 0 &&
				//backPlane < farClipLimit &&  // check planes
				icell.x > -1 && icell.x < levMapDriver2->GetCellsAcross() &&
				icell.z > -1 && icell.z < levMapDriver2->GetCellsDown())
			{
				CELL_ITERATOR_D2 ci;
				ci.cache = &iteratorCache;
				PACKED_CELL_OBJECT* ppco;

				levMapDriver2->SpoolRegion(spoolContext, icell);

				ppco = levMapDriver2->GetFirstPackedCop(&ci, icell);

				if (ppco)
					g_drawnCells++;

				// walk each cell object in cell
				while (ppco)
				{
					if (ci.listType != -1 && !g_displayAllCellLevels)
						break;
					
					PCO_PAIR_D2 pair;
					pair.nearCell = ci.nearCell;
					pair.pco = ppco;

					drawObjects.append(pair);

					ppco = levMapDriver2->GetNextPackedCop(&ci);
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

			if (hloop == vloop)
				dir = 2;
		}
		else if (dir == 2)
		{
			//leftPlane -= leftcos;
			//backPlane -= backcos;
			//rightPlane -= rightcos;

			hloop--;

			if (hloop + vloop == 0)
				dir = 3;
		}
		else
		{
			//leftPlane -= leftsin;
			//backPlane -= backsin;
			//rightPlane -= rightsin;

			vloop--;

			if (hloop == vloop)
				dir = 0;
		}

		i--;
	}

	// at least once we should do that
	CRenderModel::SetupModelShader();

	// draw object list
	for (uint i = 0; i < drawObjects.size(); i++)
	{
		CELL_OBJECT co;
		CDriver2LevelMap::UnpackCellObject(co, drawObjects[i].pco, drawObjects[i].nearCell);

		DrawCellObject(co, cameraPos, cameraAngleY, frustrumVolume, true);
	}

	if (g_displayRoadConnections)
	{
#define IS_STRAIGHT_SURFACE(surfid)			(((surfid) > -1) && ((surfid) & 0xFFFFE000) == 0 && ((surfid) & 0x1FFF) < levMapDriver2->GetNumStraights())
#define IS_CURVED_SURFACE(surfid)			(((surfid) > -1) && ((surfid) & 0xFFFFE000) == 0x4000 && ((surfid) & 0x1FFF) < levMapDriver2->GetNumCurves())
#define IS_JUNCTION_SURFACE(surfid)			(((surfid) > -1) && ((surfid) & 0xFFFFE000) == 0x2000 && ((surfid) & 0x1FFF) < levMapDriver2->GetNumJunctions())

		for (int i = 0; i < levMapDriver2->GetNumStraights(); i++)
		{
			DRIVER2_STRAIGHT* straight = levMapDriver2->GetStraight(i);

			VECTOR_NOPAD positionA{ straight->Midx, 0, straight->Midz };

			for (int j = 0; j < 4; j++)
			{
				if (straight->ConnectIdx[j] == -1)
					continue;
				
				if (IS_STRAIGHT_SURFACE(straight->ConnectIdx[j]))
				{
					DRIVER2_STRAIGHT* conn = levMapDriver2->GetStraight(straight->ConnectIdx[j]);
					VECTOR_NOPAD positionB{ conn->Midx, 0, conn->Midz };

					DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1,1,0,1));
				}
				else if (IS_CURVED_SURFACE(straight->ConnectIdx[j]))
				{
					DRIVER2_CURVE* conn = levMapDriver2->GetCurve(straight->ConnectIdx[j]);

					const int radius = conn->inside * 1024 + 256 + 512 * ROAD_LANES_COUNT(conn);
					const int curveLength = conn->end - conn->start & 4095;
					const int distAlongPath = conn->start + curveLength / 2;

					VECTOR_NOPAD positionB{ conn->Midx + (radius * isin(distAlongPath)) / ONE, 0, conn->Midz + (radius * icos(distAlongPath)) / ONE };

					DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 1, 0, 1));
				}
				else if (IS_JUNCTION_SURFACE(straight->ConnectIdx[j]))
				{
					DRIVER2_JUNCTION* connJ = levMapDriver2->GetJunction(straight->ConnectIdx[j]);

					for (int k = 0; k < 4; k++)
					{
						if (IS_STRAIGHT_SURFACE(connJ->ExitIdx[k]))
						{
							DRIVER2_STRAIGHT* connK = levMapDriver2->GetStraight(connJ->ExitIdx[k]);
							VECTOR_NOPAD positionB{ connK->Midx, 0, connK->Midz };

							DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 0, 0, 1));
						}
						else if (IS_CURVED_SURFACE(connJ->ExitIdx[k]))
						{
							DRIVER2_CURVE* connK = levMapDriver2->GetCurve(connJ->ExitIdx[k]);
							const int radius = connK->inside * 1024 + 256 + 512 * ROAD_LANES_COUNT(connK);
							const int curveLength = connK->end - connK->start & 4095;
							const int distAlongPath = connK->start + curveLength / 2;

							VECTOR_NOPAD positionB{ connK->Midx + (radius * isin(distAlongPath)) / ONE, 0, connK->Midz + (radius * icos(distAlongPath)) / ONE };

							DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 0, 0, 1));
						}
					}
				}
			} // for j
		} // for i

		for (int i = 0; i < levMapDriver2->GetNumCurves(); i++)
		{
			DRIVER2_CURVE* curve = levMapDriver2->GetCurve(i | 0x4000);

			VECTOR_NOPAD positionA{ curve->Midx, 0, curve->Midz };

			for (int j = 0; j < 4; j++)
			{
				if (curve->ConnectIdx[j] == -1)
					continue;

				if (IS_STRAIGHT_SURFACE(curve->ConnectIdx[j]))
				{
					DRIVER2_STRAIGHT* conn = levMapDriver2->GetStraight(curve->ConnectIdx[j]);
					VECTOR_NOPAD positionB{ conn->Midx, 0, conn->Midz };

					DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 1, 0, 1));
				}
				else if (IS_CURVED_SURFACE(curve->ConnectIdx[j]))
				{
					DRIVER2_CURVE* conn = levMapDriver2->GetCurve(curve->ConnectIdx[j]);

					const int radius = conn->inside * 1024 + 256 + 512 * ROAD_LANES_COUNT(conn);
					const int curveLength = conn->end - conn->start & 4095;
					const int distAlongPath = conn->start + curveLength / 2;

					VECTOR_NOPAD positionB{ conn->Midx + (radius * isin(distAlongPath)) / ONE, 0, conn->Midz + (radius * icos(distAlongPath)) / ONE };

					DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 1, 0, 1));
				}
				else if (IS_JUNCTION_SURFACE(curve->ConnectIdx[j]))
				{
					DRIVER2_JUNCTION* connJ = levMapDriver2->GetJunction(curve->ConnectIdx[j]);

					for (int k = 0; k < 4; k++)
					{
						if (IS_STRAIGHT_SURFACE(connJ->ExitIdx[k]))
						{
							DRIVER2_STRAIGHT* connK = levMapDriver2->GetStraight(connJ->ExitIdx[k]);
							VECTOR_NOPAD positionB{ connK->Midx, 0, connK->Midz };

							DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 0, 0, 1));
						}
						else if (IS_CURVED_SURFACE(connJ->ExitIdx[k]))
						{
							DRIVER2_CURVE* connK = levMapDriver2->GetCurve(connJ->ExitIdx[k]);
							const int radius = connK->inside * 1024 + 256 + 512 * ROAD_LANES_COUNT(connK);
							const int curveLength = connK->end - connK->start & 4095;
							const int distAlongPath = connK->start + curveLength / 2;

							VECTOR_NOPAD positionB{ connK->Midx + (radius * isin(distAlongPath)) / ONE, 0, connK->Midz + (radius * icos(distAlongPath)) / ONE };

							DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), ColorRGBA(1, 0, 0, 1));
						}
					}
				}
			} // for j
		} // for i
	}

	if (g_displayRoads)
	{
		// TODO: road at their surface height!

		for (int i = 0; i < levMapDriver2->GetNumStraights(); i++)
		{
			DRIVER2_STRAIGHT* straight = levMapDriver2->GetStraight(i);

			int sn, cs;
			sn = isin(straight->angle);
			cs = icos(straight->angle);

			int numLanes = ROAD_WIDTH_IN_LANES(straight);
			for (int j = 0; j < numLanes; j++)
			{
				VECTOR_NOPAD offset{ 
					((straight->length / 2) * sn) / ONE, 
					0, 
					((straight->length / 2) * cs) / ONE 
				};
				VECTOR_NOPAD lane_offset{ 
					((512 * j - numLanes * 256 + 256) * -cs) / ONE,
					0, 
					((512 * j - numLanes * 256 + 256) * sn) / ONE
				};

				VECTOR_NOPAD positionA{ 
					straight->Midx - offset.vx + lane_offset.vx, 
					0, 
					straight->Midz - offset.vz + lane_offset.vz 
				};
				VECTOR_NOPAD positionB{
					straight->Midx + offset.vx + lane_offset.vx, 
					0, 
					straight->Midz + offset.vz + lane_offset.vz 
				};

				ColorRGBA color(0.0f, 1.0f, 0.0f, 1.0f);

				if (ROAD_LANE_DIR(straight, j))
					color.z = 1.0f;

				if (!ROAD_IS_AI_LANE(straight, j))
				{
					color.x = 1.0f;
					color.y = 0.0f;
					color.w = 0.5f;
				}

				if (ROAD_IS_PARKING_ALLOWED_AT(straight, j))
				{
					color.x = 1.0f;
					color.y = 1.0f;
				}

				DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), color);
			}
		}

		for (int i = 0; i < levMapDriver2->GetNumCurves(); i++)
		{
			DRIVER2_CURVE* curve = levMapDriver2->GetCurve(i | 0x4000);

			int numLanes = ROAD_WIDTH_IN_LANES(curve);

			for (int j = 0; j < numLanes; j++)
			{
				VECTOR_NOPAD positionA;
				VECTOR_NOPAD positionB;

				int distAlongPath = curve->start;
				int curveLength = curve->end - curve->start & 4095;
				int radius = curve->inside * 1024 + 256 + 512 * j;

				ColorRGBA color(0.0f, 1.0f, 0.0f, 1.0f);

				if (ROAD_LANE_DIR(curve, j))
					color.z = 1.0f;

				if (!ROAD_IS_AI_LANE(curve, j))
				{
					color.x = 1.0f;
					color.y = 0.0f;
					color.w = 0.5f;
				}

				if (ROAD_IS_PARKING_ALLOWED_AT(curve, j))
				{
					color.x = 1.0f;
					color.y = 1.0f;
				}

				for (int k = 0; k < 8; k++)
				{
					positionB = VECTOR_NOPAD{ 
						curve->Midx + (radius * isin(distAlongPath)) / ONE, 
						0, 
						curve->Midz + (radius * icos(distAlongPath)) / ONE
					};

					distAlongPath += curveLength / 7; // 8 - 1

					if (k > 0)
						DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), color);

					positionA = positionB;
				}
			}
		}
	}
}

//-------------------------------------------------------
// Draws Driver 2 level region cells
// and spools the world if needed
//-------------------------------------------------------
void DrawLevelDriver1(const Vector3D& cameraPos, float cameraAngleY, const Volume& frustrumVolume)
{
	CELL_ITERATOR_CACHE iteratorCache;
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

	VECTOR_NOPAD cameraPosition = ToFixedVector(cameraPos);

	CDriver1LevelMap* levMapDriver1 = (CDriver1LevelMap*)g_levMap;
	CFileStream spoolStream(g_levFile);

	SPOOL_CONTEXT spoolContext;
	spoolContext.dataStream = &spoolStream;
	spoolContext.lumpInfo = &g_levInfo;

	levMapDriver1->WorldPositionToCellXZ(cell, cameraPosition);

	static Array<CELL_OBJECT*> drawObjects;
	drawObjects.reserve(g_cellsDrawDistance * 2);
	drawObjects.clear();

	ci.cache = &iteratorCache;

	CBaseLevelRegion* currentRegion = nullptr;

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

			CBaseLevelRegion* reg = g_levMap->GetRegion(icell);

			if (currentRegion != reg)
				memset(&iteratorCache, 0, sizeof(iteratorCache));
			currentRegion = reg;

			if (icell.x > -1 && icell.x < levMapDriver1->GetCellsAcross() &&
				icell.z > -1 && icell.z < levMapDriver1->GetCellsDown())
			{
				levMapDriver1->SpoolRegion(spoolContext, icell);

				pco = levMapDriver1->GetFirstCop(&ci, icell);

				if(pco)
					g_drawnCells++;

				// walk each cell object in cell
				while (pco)
				{
					drawObjects.append(pco);

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

	// at least once we should do that
	CRenderModel::SetupModelShader();

	for (uint i = 0; i < drawObjects.size(); i++)
	{
		DrawCellObject(*drawObjects[i], cameraPos, cameraAngleY, frustrumVolume, true);
	}

	if (g_displayRoads)
	{
		ROUTE_DATA route;
		VECTOR_NOPAD cameraPosition = ToFixedVector(g_cameraPosition);
		levMapDriver1->GetRoadInfo(route, cameraPosition);

		const ROAD_MAP_LUMP_DATA& lumpData = levMapDriver1->GetRoadMapLumpData();

		const int px = lumpData.width / 2 * 1500;
		const int py = lumpData.height / 2 * 1500;

		int xoff = lumpData.unitXMid * 2;
		int yoff = lumpData.unitZMid * 2;
		xoff -= px + 750;
		yoff -= py + 750;

		const int numRoads = levMapDriver1->GetNumRoads();
		for (int i = 0; i < numRoads; i++)
		{
			DRIVER1_ROAD* road = levMapDriver1->GetRoad(i);
			DRIVER1_ROADBOUNDS* roadBounds = levMapDriver1->GetRoadBounds(i);

			const int length = road->length * 1500;

			int sn, cs;
			sn = isin(roadBounds->dir * 1024);
			cs = icos(roadBounds->dir * 1024);

			const int numLanes = road->NumLanes * 2;
			for (int j = 0; j < numLanes; j++)
			{
				VECTOR_NOPAD offset{
					(length * sn) / ONE,
					0, 
					(length * cs) / ONE
				};
				VECTOR_NOPAD lane_offset{ 
					(750 * cs) / ONE / 2 + (j * 750 * cs) / ONE,
					0, 
					(750 * -sn) / ONE / 2 + (j * 750 * -sn) / ONE
				};
				VECTOR_NOPAD road_pos{
					road->x * 1500 - xoff,
					0,
					(lumpData.height - road->z) * 1500 - yoff
				};

				VECTOR_NOPAD positionA{ 
					road_pos.vx + lane_offset.vx, 
					0,
					road_pos.vz + lane_offset.vz 
				};
				VECTOR_NOPAD positionB{ 
					road_pos.vx + offset.vx + lane_offset.vx, 
					0,
					road_pos.vz - offset.vz + lane_offset.vz 
				};

				ColorRGBA color(0.0f, 1.0f, 0.0f, 1.0f);

				if (route.roadIndex == i)
					color.x = 1.0f;

				if (ROAD_LANE_DIR(road, j))
					color.z = 1.0f;

				if ((road->flags & ROAD_FLAG_PARKING) && (j == 0 || j == numLanes-1))
				{
					color.x = 1.0f;
					color.y = 1.0f;
				}

				DebugOverlay_Line(FromFixedVector(positionA), FromFixedVector(positionB), color);
			}
		}
	}
}