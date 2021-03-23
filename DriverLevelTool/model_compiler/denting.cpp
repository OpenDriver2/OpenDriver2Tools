#include "compiler.h"
#include "obj_loader.h"
#include "core/VirtualStream.h"
#include "math/psx_math_types.h"
#include <nstd/String.hpp>
#include <nstd/File.hpp>

// TODO: sync with DR2LIMITS.H
#define NUM_DAMAGE_ZONES		6

#define MAX_FILE_DAMAGE_ZONE_VERTS	50
#define MAX_FILE_DAMAGE_ZONE_POLYS	70
#define MAX_FILE_DAMAGE_LEVELS		256

struct Denting_t
{
	ubyte dentZoneVertices[NUM_DAMAGE_ZONES][MAX_FILE_DAMAGE_ZONE_VERTS];
	ubyte dentZonePolys[NUM_DAMAGE_ZONES][MAX_FILE_DAMAGE_ZONE_POLYS];
	ubyte dentDamageLevels[MAX_FILE_DAMAGE_LEVELS];

	int numDentPolys[NUM_DAMAGE_ZONES];
	int numDentVerts[NUM_DAMAGE_ZONES];
	int polygonCount;
};

//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Converts ambigous to side zone types to proper ones using vertex position
//--------------------------------------------------------------------------
int GetSideDamageZoneByVerts(smdmodel_t& model, const smdpoly_t& poly, int internalZoneType)
{
	float poly_side = 0;
	
	for (int i = 0; i < poly.vcount; i++)
	{
		Vector3D& vert = model.verts[poly.vindices[i]];

		// TODO: input actual DMODEL, this is an overkill
		SVECTOR temp;
		ConvertVertexToDriver(&temp, &vert);
		
		if(temp.x > 0) // left... WTF.. why it is at opposite side
		{
			poly_side -= 1.0f;
		}
		else // right
		{
			poly_side += 1.0f;
		}
	}

	poly_side /= poly.vcount;
	
	if(poly_side > 0.0f)
	{
		if (internalZoneType == ZONE_FRONT)
			return ZONE_FRONT_RIGHT;
		else if (internalZoneType == ZONE_REAR)
			return ZONE_REAR_RIGHT;
		else if (internalZoneType == ZONE_SIDE)
			return ZONE_SIDE_RIGHT;
	}
	else
	{
		if (internalZoneType == ZONE_FRONT)
			return ZONE_FRONT_LEFT;
		else if (internalZoneType == ZONE_REAR)
			return ZONE_REAR_LEFT;
		else if (internalZoneType == ZONE_SIDE)
			return ZONE_SIDE_LEFT;
	}

	// no need in conversions
	return internalZoneType;
}

//--------------------------------------------------------------------------
// Check denting limts
// Returns:
//		1 = if polygon damage levels exceeded
//		2 = if zone polygon count exceeded
//		3 = if zone vertices count exceeded
//--------------------------------------------------------------------------
int CheckDentingLimits(const Denting_t& denting)
{
	if (denting.polygonCount > MAX_FILE_DAMAGE_LEVELS)
	{
		return 1;
	}
	
	for(int i = 0; i < NUM_DAMAGE_ZONES; i++)
	{
		if(denting.numDentPolys[i] > MAX_FILE_DAMAGE_ZONE_POLYS)
		{
			return 1;
		}

		if (denting.numDentVerts[i] > MAX_FILE_DAMAGE_ZONE_VERTS)
		{
			return 2;
		}
	}

	return -1;
}

//--------------------------------------------------------------------------
// Adding vertices to zones with checking to be unique
//--------------------------------------------------------------------------
void AddDentingVerts(Denting_t& outDenting, int zone, const smdpoly_t& poly)
{
	for (int i = 0; i < poly.vcount; i++)
	{
		bool duplicateFound = false;
		for(int j = 0; j < outDenting.numDentVerts[zone]; j++)
		{
			if (outDenting.dentZoneVertices[zone][j] == poly.vindices[i])
			{
				duplicateFound = true;
				break;
			}
		}
		
		if (duplicateFound)
			continue;
	
		outDenting.dentZoneVertices[zone][outDenting.numDentVerts[zone]++] = poly.vindices[i];
	}
}

//--------------------------------------------------------------------------
// Writes denting to stream for single group
//--------------------------------------------------------------------------
bool ProcessDentingForGroup(Denting_t& outDenting, smdmodel_t& model, smdgroup_t* group)
{
	// select polygons and add vertices to denting
	int numPolys = group->polygons.size();
	for (int i = 0; i < numPolys; i++, outDenting.polygonCount++)
	{
		smdpoly_t& poly = group->polygons[i];

		if(poly.flags & POLY_EXTRA_DENTING)
		{
			int zone = poly.extraData & 0xf;
			int level = poly.extraData >> 4 & 0xf;

			// damage level is stored for each model polygon
			outDenting.dentDamageLevels[outDenting.polygonCount] = level;

			int realZone = GetSideDamageZoneByVerts(model, poly, zone);

			if (realZone >= NUM_DAMAGE_ZONES)
			{
				MsgError("Invalid damage zone index %d\n", realZone);
				return false;
			}
			
			if(zone == ZONE_FRONT)
			{
				// polygons has to be added to their zones
				if(poly.flags & POLY_DAMAGE_SPLIT)
				{
					outDenting.dentZonePolys[realZone][outDenting.numDentPolys[realZone]++] = outDenting.polygonCount;	// zone polygons are for each zone
				}
				else
				{
					outDenting.dentZonePolys[ZONE_FRONT_LEFT][outDenting.numDentPolys[ZONE_FRONT_LEFT]++] = outDenting.polygonCount;
					outDenting.dentZonePolys[ZONE_FRONT_RIGHT][outDenting.numDentPolys[ZONE_FRONT_RIGHT]++] = outDenting.polygonCount;
				}

				AddDentingVerts(outDenting, ZONE_FRONT_LEFT, poly);
				AddDentingVerts(outDenting, ZONE_FRONT_RIGHT, poly);
			}
			else if (zone == ZONE_REAR)
			{
				// polygons has to be added to their zones
				if (poly.flags & POLY_DAMAGE_SPLIT)
				{
					outDenting.dentZonePolys[realZone][outDenting.numDentPolys[realZone]++] = outDenting.polygonCount;	// zone polygons are for each zone
				}
				else
				{
					outDenting.dentZonePolys[ZONE_REAR_LEFT][outDenting.numDentPolys[ZONE_REAR_LEFT]++] = outDenting.polygonCount;
					outDenting.dentZonePolys[ZONE_REAR_RIGHT][outDenting.numDentPolys[ZONE_REAR_RIGHT]++] = outDenting.polygonCount;
				}
				
				AddDentingVerts(outDenting, ZONE_REAR_LEFT, poly);
				AddDentingVerts(outDenting, ZONE_REAR_RIGHT, poly);
			}
			else
			{
				outDenting.dentZonePolys[realZone][outDenting.numDentPolys[realZone]++] = outDenting.polygonCount;	// zone polygons are for each zone
				AddDentingVerts(outDenting, realZone, poly);
			}

			
		}

		int checkResult = CheckDentingLimits(outDenting);
		
		if(checkResult >= 0)
		{
			if(checkResult == 0)
				MsgWarning("polygon damage levels exceeded (got %d, max: %d)\n", outDenting.polygonCount, MAX_FILE_DAMAGE_LEVELS);
			else if (checkResult == 1)
			{
				for (int i = 0; i < 6; i++)
				{
					if(outDenting.numDentPolys[i] > MAX_FILE_DAMAGE_ZONE_POLYS)
						MsgWarning("zone polygon count exceeded (got %d in zone %d, max: %d)\n", outDenting.numDentPolys[i], i, MAX_FILE_DAMAGE_ZONE_POLYS);
				}
			}
			else if (checkResult == 2)
			{
				for (int i = 0; i < 6; i++)
				{
					if (outDenting.numDentPolys[i] > MAX_FILE_DAMAGE_ZONE_VERTS)
						MsgWarning("zone vertices count exceeded (got %d in zone %d, max: %d)\n", outDenting.numDentVerts[i], i, MAX_FILE_DAMAGE_ZONE_VERTS);
				}
			}
		
			return false;
		}
	}

	return true;
}

void GenerateDenting(smdmodel_t& model, const char* outputName)
{
	// replace extension with .den
	String den_filename(outputName, strlen(outputName));
	den_filename = den_filename.replace("_clean", String());
	den_filename = File::dirname(den_filename) + "/" + File::basename(den_filename, File::extension(den_filename)) + ".den";

	MsgInfo("Generating denting '%s'\n", (char*)den_filename);

	Denting_t newDenting;
	memset(&newDenting, 0, sizeof(newDenting));
	memset(newDenting.dentZoneVertices, 0xFF, sizeof(newDenting.dentZoneVertices));
	memset(newDenting.dentZonePolys, 0xFF, sizeof(newDenting.dentZonePolys));

	for (usize i = 0; i < model.groups.size(); i++)
	{
		if (!ProcessDentingForGroup(newDenting, model, model.groups[i]))
		{
			// abort if any arrors
			MsgError("No denting is generated.\n");
			return;
		}
	}

	// store denting
	MsgWarning("Generated denting:\n");
	Msg("Polys:\n");
	Msg("\tFL %d\t\tFR %d\n", newDenting.numDentPolys[ZONE_FRONT_LEFT], newDenting.numDentPolys[ZONE_FRONT_RIGHT]);
	Msg("\tSL %d\t\tSR %d\n", newDenting.numDentPolys[ZONE_SIDE_LEFT], newDenting.numDentPolys[ZONE_SIDE_RIGHT]);
	Msg("\tRL %d\t\tRR %d\n", newDenting.numDentPolys[ZONE_REAR_LEFT], newDenting.numDentPolys[ZONE_REAR_RIGHT]);

	Msg("Verts:\n");
	Msg("\tFL %d\t\tFR %d\n", newDenting.numDentVerts[ZONE_FRONT_LEFT], newDenting.numDentVerts[ZONE_FRONT_RIGHT]);
	Msg("\tSL %d\t\tSR %d\n", newDenting.numDentVerts[ZONE_SIDE_LEFT], newDenting.numDentVerts[ZONE_SIDE_RIGHT]);
	Msg("\tRL %d\t\tRR %d\n", newDenting.numDentVerts[ZONE_REAR_LEFT], newDenting.numDentVerts[ZONE_REAR_RIGHT]);

	FILE* fp = fopen(den_filename, "wb");

	if (fp)
	{
		fwrite(newDenting.dentZoneVertices, 1, NUM_DAMAGE_ZONES * MAX_FILE_DAMAGE_ZONE_VERTS, fp);
		fwrite(newDenting.dentZonePolys, 1, NUM_DAMAGE_ZONES * MAX_FILE_DAMAGE_ZONE_POLYS, fp);
		fwrite(newDenting.dentDamageLevels, 1, MAX_FILE_DAMAGE_LEVELS, fp);

		fclose(fp);
	}
}
