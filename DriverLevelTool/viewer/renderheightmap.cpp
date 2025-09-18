#include <nstd/Map.hpp>
#include <utility>

#include "debug_overlay.h"
#include "driver_routines/regions_d2.h"
#include "math/isin.h"
#include "math/Vector.h"
#include "math/Plane.h"
#include "core/cmdlib.h"
#include "util/util.h"

extern CBaseLevelMap* g_levMap;

struct HeightmapDebugData
{
	Array<Vector3D>					planeVerts;
	Array<std::pair<sdNode, int>>	nodeStack;
	VECTOR_NOPAD	cellPos;
	int				planeIdx{ 0 };
};

extern int SdHeightOnPlane(const VECTOR_NOPAD& position, const sdPlane* plane, const DRIVER2_CURVE* curves);

static void MakeVertexPlane(Array<Vector3D>& verts, sdPlane& plane, const Vector3D& origin, float size)
{
	verts.clear();
	verts.reserve(4);

	Vector3D points[] = {
		-vec3_right * size + vec3_forward * size,
		vec3_right* size + vec3_forward * size,
		vec3_right* size - vec3_forward * size,
		-vec3_right * size - vec3_forward * size
	};

	for(int i = 0; i < 4; ++i)
	{
		Vector3D tmp = (points[i] + Vector3D(origin.x, 0.0f, origin.z)) * ONE_F;
		VECTOR_NOPAD vec{ tmp.x, 0, tmp.z };

		// use fixed point because floats sucks (getting lots of incorrect heights)
		points[i].y = SdHeightOnPlane(vec, &plane, ((CDriver2LevelMap*)g_levMap)->GetCurve(0x4000)) / ONE_F;
		points[i].x += origin.x;
		points[i].z += origin.z;
	}

	verts.append(points, 4);
}

// clips vertices
static void ChopVertsByPlane(Array<Vector3D>& verts, const Plane& plane, ClassifyPlane_e side)
{
	Array<bool> negative;
	Array<Vector3D> newVerts;

	float t;
	Vector3D v, v1, v2;

	// classify vertices
	int negativeCount = 0;
	for (int i = 0; i < verts.size(); i++)
	{
		negative.append(plane.ClassifyPoint(verts[i]) != side);
		negativeCount += negative[i];
	}

	if (negativeCount == verts.size())
	{
		verts.clear();
		return; // completely culled, we have no polygon
	}

	if (negativeCount == 0)
		return;			// not clipped

	for (int i = 0; i < verts.size(); i++)
	{
		// prev is the index of the previous vertex
		int	prev = (i != 0) ? i - 1 : verts.size() - 1;

		if (negative[i])
		{
			if (!negative[prev])
			{
				v1 = verts[prev];
				v2 = verts[i];

				// Current vertex is on negative side of plane,
				// but previous vertex is on positive side.
				Vector3D edge = v1 - v2;
				t = plane.Distance(v1) / dot(plane.normal, edge);

				newVerts.append(lerp(v1, v2, t));
			}
		}
		else
		{
			if (negative[prev])
			{
				v1 = verts[i];
				v2 = verts[prev];

				// Current vertex is on positive side of plane,
				// but previous vertex is on negative side.
				Vector3D edge = v1 - v2;

				t = plane.Distance(v1) / dot(plane.normal, edge);

				newVerts.append(lerp(v1, v2, t));
			}

			// include current vertex
			newVerts.append(verts[i]);
		}
	}

	// swap with new verts
	newVerts.swap(verts);
}

// recursively walks heightmap nodes
static void DebugDriver2SdCell_R(int level, sdNode* parent, sdNode* node, sdPlane* planeData, int depth, int side, void* userData)
{
	HeightmapDebugData& dbgData = *reinterpret_cast<HeightmapDebugData*>(userData);

	if (*(short*)node == 0x7fff)
		return;	// end marker

	if (!node->node)
	{
		// encountered a plane (already processed at top level (below)
		return;
	}

	// creates surface and clip with planes
	auto MakeSurface = [planeData, depth, node, &dbgData](int planeIdx, int side) {
		if (planeIdx == 0x7fff)
			return;	// end marker

		dbgData.planeVerts.clear();

		{
			Vector3D cpos(dbgData.cellPos.vx + 512, dbgData.cellPos.vy, dbgData.cellPos.vz + 512);
			cpos /= ONE_F;

			sdPlane& pl = planeData[planeIdx];

			Plane plane;
			plane.normal = normalize(Vector3D(pl.a, pl.b, pl.c) / 16384.0f);
			plane.offset = pl.d / ONE_F;
			MakeVertexPlane(dbgData.planeVerts, pl, cpos, 512 / ONE_F);
		}

		// clip surface by planes
		{
			Vector3D cpos(dbgData.cellPos.vx, dbgData.cellPos.vy, dbgData.cellPos.vz);
			cpos /= ONE_F;

			for (const std::pair<sdNode, int>& prevNode : dbgData.nodeStack)
			{
				const Vector2D dir = Vector2D(icos(prevNode.first.angle), isin(prevNode.first.angle)) / ONE_F;
				const Vector3D tangent(-dir.y, 0.0f, dir.x);

				Plane clipPlane;
				clipPlane.normal = tangent;
				clipPlane.offset = -dot(cpos + clipPlane.normal * float(prevNode.first.dist / ONE_F), clipPlane.normal);
				ChopVertsByPlane(dbgData.planeVerts, clipPlane, prevNode.second ? CP_FRONT : CP_BACK);
			}
		}

		static ColorRGBA s_colors[] = {
			{ 1.0f, 1.0f, 0.0f, 0.5f },
			{ 1.0f, 0.0f, 1.0f, 0.5f },
			{ 0.0f, 1.0f, 0.0f, 0.5f },
			{ 0.0f, 1.0f, 1.0f, 0.5f },
			{ 0.2f, 0.2f, 1.0f, 0.5f }
		};

		ColorRGBA color = s_colors[dbgData.planeIdx++ % _countof(s_colors)];
		const int cnt = dbgData.planeVerts.size();
		for (int i = 0; i < cnt - 2; ++i)
			DebugOverlay_Poly(dbgData.planeVerts[0], dbgData.planeVerts[i + 1], dbgData.planeVerts[i + 2], color);
	};

	{
		dbgData.nodeStack.resize(depth);
		dbgData.nodeStack.append(std::make_pair(*node, 0));

		sdNode* left = node + 1;
		sdNode* right = node + node->offset;

		if (!left->node)
			MakeSurface(*(short*)left, 0);
		else
			DebugDriver2SdCell_R(level, node, left, planeData, depth + 1, 0, userData);

		dbgData.nodeStack.resize(depth);
		dbgData.nodeStack.append(std::make_pair(*node, 1));

		if (!right->node)
			MakeSurface(*(short*)right, 1);
		else
			DebugDriver2SdCell_R(level, node, right, planeData, depth + 1, 1, userData);
	}
}

//-------------------------------------------------------------
// Displays heightmap cell BSP
//-------------------------------------------------------------
void DebugDrawDriver2HeightmapCell(const VECTOR_NOPAD& cellPos)
{
	// cell bounds
	const int cellMinX = (((cellPos.vx - 512) >> 10) << 10) + 512;
	const int cellMinZ = (((cellPos.vz - 512) >> 10) << 10) + 512;

	VECTOR_NOPAD cellLookupPos;
	cellLookupPos.vx = cellPos.vx - 512;	// FIXME: is that a quarter of a cell?
	cellLookupPos.vy = cellPos.vy;
	cellLookupPos.vz = cellPos.vz - 512;

	XZPAIR cell;
	g_levMap->WorldPositionToCellXZ(cell, cellLookupPos);
	CDriver2LevelRegion* region = (CDriver2LevelRegion*)g_levMap->GetRegion(cell);

	static HeightmapDebugData dbgData{};
	dbgData.planeIdx = 0;
	dbgData.nodeStack.clear();
	dbgData.cellPos.vx = cellMinX;
	dbgData.cellPos.vy = cellPos.vy;
	dbgData.cellPos.vz = cellMinZ;

	region->IterateHeightmapAtCell(cellLookupPos, DebugDriver2SdCell_R, &dbgData);
}