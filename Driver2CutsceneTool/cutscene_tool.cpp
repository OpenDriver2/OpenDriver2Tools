#include <math.h>

#include <malloc.h>
#include "core/VirtualStream.h"
#include "core/cmdlib.h"
#include "math/dktypes.h"
#include "util/image.h"
#include "util/util.h"
#include <string>
#include <DriverLevelTool/driver_routines/d2_types.h>

#define MAX_FILE_CUTSCENES			15
#define REPLAY_BUFFER_MINSIZE		2280

// TODO: Cutscene.h

struct CUTSCENE_INFO
{
	ushort offset;
	ushort size;
};

struct CUTSCENE_HEADER
{
	int maxsize;
	CUTSCENE_INFO data[MAX_FILE_CUTSCENES];
};

//----------------------------------------------------

// TODO: replay.h
struct ACTIVE_CHEATS
{
	ubyte cheat1 : 1;
	ubyte cheat2 : 1;
	ubyte cheat3 : 1;
	ubyte cheat4 : 1;
	ubyte cheat5 : 1;
	ubyte cheat6 : 1;
	ubyte cheat7 : 1;
	ubyte cheat8 : 1;
	ubyte cheat9 : 1;
	ubyte cheat10 : 1;
	ubyte cheat11 : 1;
	ubyte cheat12 : 1;
	ubyte cheat13 : 1;
	ubyte cheat14 : 1;
	ubyte cheat15 : 1;
	ubyte cheat16 : 1;
	ubyte reserved1;
	ubyte reserved2;
};

struct SAVED_PLAYER_POS
{
	ushort type;
	short direction;
	int vx;
	int vy;
	int vz;
	uint felony;
	ushort totaldamage;
	short damage[6];
};

struct SAVED_CAR_POS
{
	char active;
	ubyte model;
	ubyte palette;
	ushort totaldamage;
	ushort damage[6];
	short direction;
	int vx;
	int vy;
	int vz;
};

struct MISSION_DATA
{
	SAVED_PLAYER_POS PlayerPos;
	SAVED_CAR_POS CarPos[6];
};

struct REPLAY_SAVE_HEADER
{
	uint magic;
	ubyte GameLevel;
	ubyte GameType;
	ubyte reserved1;
	ubyte NumReplayStreams;
	ubyte NumPlayers;
	ubyte RandomChase;
	ubyte CutsceneEvent;
	ubyte gCopDifficultyLevel;
	MISSION_DATA SavedData;
	ACTIVE_CHEATS ActiveCheats;
	int wantedCar[2];
	int MissionNumber;
	int HaveStoredData;
	int reserved2[6];
};

struct PADRECORD
{
	ubyte pad;
	ubyte analogue;
	ubyte run;
};

struct STREAM_SOURCE
{
	ubyte type;
	ubyte model;
	ubyte palette;
	char controlType;
	ushort flags;
	ushort rotation;
	VECTOR_NOPAD position;
	int totaldamage;
	int damage[6];
};

struct REPLAY_STREAM_HEADER
{
	STREAM_SOURCE SourceType;
	int Size;
	int Length;
};

//----------------------------------------------------

void UnpackCutsceneFile(const char* filename)
{
	// replace extension with .den
	std::string cut_name = filename;

	size_t str_idx = cut_name.find_last_of(".");
	cut_name = cut_name.substr(0, str_idx);

	char* buffer;
	char folderPath[512];

	CUTSCENE_HEADER header;
	FILE* fp = fopen(filename, "rb");

	if (!fp)
	{
		MsgError("Cannot open file '%s'!\n", filename);
	}

	MsgInfo("Loaded '%s'\n", filename);

	fread(&header, sizeof(header), 1, fp);

	Msg("Max replay buffer size: %d\n", header.maxsize);

	// make the folder
	sprintf(folderPath, "%s_files", cut_name.c_str());
	mkdirRecursive(folderPath, true);

	buffer = (char*)malloc(0x200000);

	sizeof(CUTSCENE_HEADER);

	if (header.data[2].offset != 0xFFFF)
	{
		MsgAccept("This is chase mission\n");
	}

	for (int i = 0; i < MAX_FILE_CUTSCENES; i++)
	{
		// save only if it's exists
		if (header.data[i].offset == 0xFFFF)
		{
			if (i == 0)
				MsgWarning("No intro cutscene\n");
			else if (i == 1)
				MsgWarning("No outro cutscene\n");

			continue;
		}

		if (i == 0)
			MsgAccept("Got intro cutscene\n");
		else if (i == 1)
			MsgAccept("Got outro cutscene\n");

		// read replay data
		fseek(fp, header.data[i].offset * 4, SEEK_SET);
		fread(buffer, 1, header.data[i].size, fp);

		REPLAY_SAVE_HEADER* hdr = (REPLAY_SAVE_HEADER*)buffer;
		REPLAY_STREAM_HEADER* sheader = (REPLAY_STREAM_HEADER*)(hdr + 1);
			
		Msg("\tReplay %d\n", i);
		for (int j = 0; j < hdr->NumReplayStreams; j++)
		{
			// add padded
			int size = (sheader->Size + sizeof(PADRECORD)) & -4;
			Msg("\t  stream %d length: %d\n", j, size);
			sheader = (REPLAY_STREAM_HEADER*)((ubyte*)(sheader + 1) + size);
		}

		// save separate file
		sprintf(folderPath, "%s_files/%s_%d.D2RP", cut_name.c_str(), cut_name.c_str(), i);

		FILE* wp = fopen(folderPath, "wb");
		if (wp)
		{
			MsgWarning("\tSaving '%s', at %d, %d bytes\n", folderPath, header.data[i].offset, header.data[i].size);
			fwrite(buffer, 1, header.data[i].size, wp);
			fclose(wp);
		}
		else
			MsgError("Unable to save '%s'\n", folderPath);
	}

	free(buffer);
}

void PackCutsceneFile(const char* foldername)
{
	int offset;
	char* buffer;
	char folderPath[512];
	CUTSCENE_HEADER header;
	header.maxsize = REPLAY_BUFFER_MINSIZE;

	char* replays[MAX_FILE_CUTSCENES];
	int repsizes[MAX_FILE_CUTSCENES];
	memset(replays, 0, sizeof(replays));

	int numCutsceneReplays = 0;
	int numChaseReplays = 0;

	for (int i = 0; i < MAX_FILE_CUTSCENES; i++)
	{
		sprintf(folderPath, "%s_files/%s_%d.D2RP", foldername, foldername, i);
		FILE* fp = fopen(folderPath, "rb");
		if (fp)
		{
			if (i == 0)
			{
				MsgAccept("Got intro cutscene\n");
				numCutsceneReplays++;
			}
			else if (i == 1)
			{
				MsgAccept("Got outro cutscene\n");
				numCutsceneReplays++;
			}
			else
				numChaseReplays++;

			int size;
			fseek(fp, 0, SEEK_END);
			size = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			replays[i] = (char*)malloc(size);
			repsizes[i] = size;

			MsgWarning("\tLoaded '%s', %d bytes\n", folderPath, size);
			fread(replays[i], 1, size, fp);
			fclose(fp);

			REPLAY_SAVE_HEADER* hdr = (REPLAY_SAVE_HEADER*)replays[i];
			REPLAY_STREAM_HEADER* sheader = (REPLAY_STREAM_HEADER*)(hdr + 1);

			Msg("\tReplay %d\n", i);
			for (int j = 0; j < hdr->NumReplayStreams; j++)
			{
				// add padded
				int size = (sheader->Size + sizeof(PADRECORD)) & -4;
				Msg("\t  stream %d length: %d\n", j, size);
				sheader = (REPLAY_STREAM_HEADER*)((ubyte*)(sheader + 1) + size);

				if (size > header.maxsize)
				{
					header.maxsize = size * 2;
				}
			}
		}
		else
		{
			if (i == 0)
				MsgWarning("No intro cutscene\n");
			else if (i == 1)
				MsgWarning("No outro cutscene\n");
		}
	}

	if (numChaseReplays)
	{
		if (numChaseReplays < MAX_FILE_CUTSCENES - 2)
		{
			MsgWarning("Got %d chase replays, randomly fill it to %d\n", numChaseReplays, MAX_FILE_CUTSCENES);
		}
		else
			MsgAccept("Got %d chase replays\n", numChaseReplays);
	}
	else
	{
		if (!numCutsceneReplays)
		{
			MsgError("No files been added! Cannot continue\n");
			return;
		}

		MsgWarning("No chase replays\n");
	}

	sprintf(folderPath, "%s_N.R", foldername);
	FILE* wp = fopen(folderPath, "wb");

	if (!wp)
	{
		for (int i = 0; i < MAX_FILE_CUTSCENES; i++)
			free(replays[i]);

		MsgError("Unable to save '%s'\n", folderPath);
		return;
	}

	// make buffer so we can freely manipulate cutscenes
	buffer = (char*)malloc(0x200000);
	memset(buffer, 0, 0x200000);
	char* bufptr = buffer;
	offset = 2048;

	for (int i = 0; i < MAX_FILE_CUTSCENES; i++)
	{
		int replayId = i;

		// initialize
		header.data[i].offset = 0xFFFF;
		header.data[i].size = 0;

		if (!replays[i])
		{
			if (i == 0 || i == 1)
				continue;

			if (numChaseReplays > 0)
			{
				do
				{
					replayId = 2 + (rand() % (MAX_FILE_CUTSCENES - 2));
				} while (replayId == i || !replays[replayId]);
				MsgInfo("Picked random replay %d for chase %d\n", replayId, i);
			}
		}

		// write to file
		memcpy(bufptr, replays[replayId], repsizes[replayId]);

		header.data[i].offset = offset / 4;	// because it use shorts we have an multiplier
		header.data[i].size = repsizes[replayId];

		int sizeStep = repsizes[replayId] / 2048;
		if (sizeStep < 1)
			sizeStep = 1;

		bufptr += sizeStep * 2048;
		offset += sizeStep * 2048;
	}

	MsgInfo("Saved %d cutscenes and %d chase replays\n", numCutsceneReplays, numChaseReplays);

	// write resulting header and buffer
	fwrite(&header, sizeof(header), 1, wp);
	fseek(wp, 512 * 4, SEEK_SET);
	fwrite(buffer, bufptr-buffer, 1, wp);
	fclose(wp);

	// free all
	free(buffer);

	for (int i = 0; i < MAX_FILE_CUTSCENES; i++)
		free(replays[i]);
}

//-----------------------------------------------------

void PrintCommandLineArguments()
{
	MsgInfo("Example usage:\n");
	MsgInfo("\tDriverCutsceneTool -unpack CUT2.R\n");
	MsgInfo("\tDriverCutsceneTool -pack CUT2\n");
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	Install_ConsoleSpewFunction();
#endif

	Msg("---------------\nDriverCutsceneTool - Driver 2 cutscene packer\n---------------\n\n");

	if (argc <= 1)
	{
		PrintCommandLineArguments();
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		if (!stricmp(argv[i], "-pack"))
		{
			if (i + 1 <= argc)
				PackCutsceneFile(argv[i + 1]);
			else
				MsgWarning("-pack must have an argument!");
		}
		else if (!stricmp(argv[i], "-unpack"))
		{
			if (i + 1 <= argc)
				UnpackCutsceneFile(argv[i + 1]);
			else
				MsgWarning("-unpack must have an argument!");
		}
	}

	return 0;
}