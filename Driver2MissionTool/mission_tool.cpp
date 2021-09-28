#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nstd/File.hpp>
#include "core/cmdlib.h"

void CreateMission(const char* filename)
{

}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
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
		
	}

	return 0;
}