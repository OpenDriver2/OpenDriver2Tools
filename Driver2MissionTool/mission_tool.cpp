#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nstd/File.hpp>
#include "core/cmdlib.h"
#include "script.hpp"
#include "mission.hpp"

void CreateMission(const char* filename)
{

}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriver2MissionTool -make <mission_script_name.d2ms>\n");
	MsgInfo("\tDriver2MissionTool -decompile MISSIONS.BLK [mission_number]\n");
	MsgInfo("\tDriver2MissionTool -compile <mission_script_name.d2ms>\n");
}

int main(const int argc, char** argv)
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
		if (!stricmp(argv[i], "-make"))
		{
			if (i + 1 <= argc)
				CreateMission(argv[i + 1]);
			else
				MsgWarning("-make must have a filename argument!");

		}
	}

	return 0;
}