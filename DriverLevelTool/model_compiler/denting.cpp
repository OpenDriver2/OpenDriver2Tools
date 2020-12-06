#include <string>

#include "compiler.h"
#include "obj_loader.h"
#include "core/VirtualStream.h"

enum DamageZoneIds
{
	ZONE_FRONT_LEFT		= 0,
	ZONE_FRONT_RIGHT	= 1,
	ZONE_SIDE_LEFT		= 5,
	ZONE_SIDE_RIGHT		= 2,
	ZONE_REAR_LEFT		= 4,
	ZONE_REAR_RIGHT		= 3,
};

// TODO: sync with DR2LIMITS.H
#define NUM_DAMAGE_ZONES		6

#define MAX_FILE_DAMAGE_ZONE_VERTS	50
#define MAX_FILE_DAMAGE_ZONE_POLYS	70
#define MAX_FILE_DAMAGE_LEVELS		255

struct Denting_t
{
	ubyte dentZoneVertices[NUM_DAMAGE_ZONES][MAX_FILE_DAMAGE_ZONE_VERTS];
	ubyte dentZonePolys[NUM_DAMAGE_ZONES][MAX_FILE_DAMAGE_ZONE_POLYS];
	ubyte dentDamageZones[MAX_FILE_DAMAGE_LEVELS];

	int numDentPolys[NUM_DAMAGE_ZONES];
	int numDentVerts[NUM_DAMAGE_ZONES];
	int polygonCount;
};

//--------------------------------------------------------------------------
// Check denting limts
// Returns:
//		1 = if polygon damage levels exceeded
//		2 = if zone polygon count exceeded
//		3 = if zone vertices count exceeded
//--------------------------------------------------------------------------
int CheckDentingLimits(const Denting_t& denting)
{
	if (denting.polygonCount >= MAX_FILE_DAMAGE_LEVELS)
	{
		return 1;
	}
	
	for(int i = 0; i < NUM_DAMAGE_ZONES; i++)
	{
		if(denting.numDentPolys[i] >= MAX_FILE_DAMAGE_ZONE_POLYS)
		{
			return 1;
		}

		if (denting.numDentVerts[i] >= MAX_FILE_DAMAGE_ZONE_VERTS)
		{
			return 2;
		}
	}

	return -1;
}

//--------------------------------------------------------------------------
// Writes denting to stream for single group
//--------------------------------------------------------------------------
bool ProcessDentingForGroup(Denting_t& outDenting, smdmodel_t& model, smdgroup_t* group)
{
	// select polygons and add vertices to denting
	int numPolys = group->polygons.numElem();
	for (int i = 0; i < numPolys; i++, outDenting.polygonCount++)
	{
		smdpoly_t& poly = group->polygons[i];

		if(poly.flags & POLY_EXTRA_DENTING)
		{
			int zone = poly.extraData & 0xf;
			int level = poly.extraData >> 4 & 0xf;

			if(zone >= NUM_DAMAGE_ZONES)
			{
				MsgError("Invalid damage zone index %d\n", zone);
				return false;
			}

			outDenting.dentDamageZones[outDenting.polygonCount] = level;					// damage level is stored for each model polygon
			outDenting.dentZonePolys[zone][outDenting.numDentPolys[zone]] = outDenting.polygonCount;	// zone polygons are for each zone

			// ... as well as vertices
			for (int i = 0; i < poly.vcount; i++)
				outDenting.dentZoneVertices[zone][outDenting.numDentVerts[zone]++] = poly.vindices[i];
		}

		int checkResult = CheckDentingLimits(outDenting);
		
		if(checkResult >= 0)
		{
			if(checkResult == 0)

				MsgWarning("polygon damage levels exceeded (max: %d)\n", MAX_FILE_DAMAGE_LEVELS);
			else if (checkResult == 1)
				MsgWarning("zone polygon count exceeded (max: %d)\n", MAX_FILE_DAMAGE_ZONE_POLYS);
			else if (checkResult == 2)
				MsgWarning("zone vertices count exceeded (max: %d)\n", MAX_FILE_DAMAGE_ZONE_VERTS);
		
			return false;
		}
	}

	return true;
}

void GenerateDenting(smdmodel_t& model, const char* outputName)
{
	// replace extension with .den
	std::string den_filename = outputName;

	size_t str_idx = den_filename.find("_clean");
	if(str_idx == -1)
		str_idx = den_filename.find_last_of(".");
	
	den_filename = den_filename.substr(0, str_idx) + ".den";

	MsgInfo("Generating denting '%s'\n", den_filename.c_str());

	Denting_t newDenting;
	memset(&newDenting, 0, sizeof(newDenting));
	memset(newDenting.dentZoneVertices, 0xFF, sizeof(newDenting.dentZoneVertices));
	memset(newDenting.dentZonePolys, 0xFF, sizeof(newDenting.dentZonePolys));

	for (int i = 0; i < model.groups.numElem(); i++)
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

	FILE* fp = fopen(den_filename.c_str(), "wb");

	if (fp)
	{
		fwrite(newDenting.dentZoneVertices, 1, NUM_DAMAGE_ZONES * MAX_FILE_DAMAGE_ZONE_VERTS, fp);
		fwrite(newDenting.dentZonePolys, 1, NUM_DAMAGE_ZONES * MAX_FILE_DAMAGE_ZONE_POLYS, fp);
		fwrite(newDenting.dentDamageZones, 1, MAX_FILE_DAMAGE_LEVELS, fp);

		fclose(fp);
	}
}
