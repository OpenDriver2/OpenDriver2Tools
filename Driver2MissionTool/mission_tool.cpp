#include <math.h>

#include <malloc.h>
#include "core/VirtualStream.h"
#include "core/cmdlib.h"
#include "math/dktypes.h"
#include "util/image.h"
#include "util/util.h"
#include <string>
#include <DriverLevelTool/driver_routines/d2_types.h>

//----------------------------------------------------

void CompileMissionBlk(const char* filename)
{
}

void DecompileMissionBlk(const char* filename, int missionNumber)
{
	if (missionNumber <= 0) // decompile all
	{

	}
}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriver2MissionTool -decompile MISSIONS.BLK [mission_number]\n");
	MsgInfo("\tDriver2MissionTool -compile <mission_script_name.d2ms>\n");
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	Install_ConsoleSpewFunction();
#endif

	Msg("---------------\nDriverMissionTool - Driver 2 mission decompiler\n---------------\n\n");

	if (argc <= 1)
	{
		PrintCommandLineArguments();
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		if (!stricmp(argv[i], "-compile"))
		{
			if (i + 1 <= argc)
				CompileMissionBlk(argv[i + 1]);
			else
				MsgWarning("-compile must have an argument!");
		}
		else if (!stricmp(argv[i], "-decompile"))
		{
			if (i + 1 <= argc)
				DecompileMissionBlk(argv[i + 1], atoi(argv[i + 2]));
			else
				MsgWarning("-decompile must have at least one argument!");
		}
	}

	return 0;
}