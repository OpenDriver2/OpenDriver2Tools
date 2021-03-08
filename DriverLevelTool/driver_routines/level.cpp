#include "driver_level.h"

#include "regions_d1.h"
#include "regions_d2.h"

#include "core/VirtualStream.h"
#include "util/DkList.h"

ELevelFormat g_format = LEV_FORMAT_AUTODETECT;

//--------------------------------------------------------------------------------------------------------------------------

std::string					g_levname;

IVirtualStream*				g_levStream = nullptr;
char*						g_overlayMapData = nullptr;

CDriverLevelTextures		g_levTextures;
CDriverLevelModels			g_levModels;
CBaseLevelMap*				g_levMap = nullptr;

//---------------------------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------
// Loads overhead map lump
//-------------------------------------------------------------
void LoadOverlayMapLump(IVirtualStream* pFile, int lumpSize)
{
	int l_ofs = pFile->Tell();

	g_overlayMapData = new char[lumpSize];
	pFile->Read(g_overlayMapData, 1, lumpSize);

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// Auto-detects level format
//-------------------------------------------------------------
void DetectLevelFormat(IVirtualStream* pFile)
{
	long curPos = pFile->Tell();
	int lump_count = 255;

	LUMP lump;

	for (int i = 0; i < lump_count; i++)
	{
		// read lump info
		pFile->Read(&lump, sizeof(LUMP), 1);

		// stop marker
		if (lump.type == 255)
			break;

		switch (lump.type)
		{
			case LUMP_MODELS:
			case LUMP_MAP:
			case LUMP_TEXTURENAMES:
			case LUMP_MODELNAMES:
			case LUMP_LOWDETAILTABLE:
			case LUMP_MOTIONCAPTURE:
			case LUMP_OVERLAYMAP:
			case LUMP_PALLET:
			case LUMP_SPOOLINFO:
			case LUMP_CURVES2:
			case LUMP_CHAIR:
			case LUMP_CAR_MODELS:
			case LUMP_TEXTUREINFO:
			case LUMP_STRAIGHTS2:
				break;
			case LUMP_JUNCTIONS2:
			{
				MsgInfo("Assuming it's a 'Driver 2 DEMO' 1.6 alpha LEV file\n");
				g_format = LEV_FORMAT_DRIVER2_ALPHA16; // as it is an old junction format - it's clearly a alpha 1.6 level
				break;
			}
			case LUMP_JUNCTIONS2_NEW:
			{
				MsgInfo("Assuming it's a 'Driver 2' final LEV file\n");
				g_format = LEV_FORMAT_DRIVER2_RETAIL; // most recent LEV file
				break;
			}
			case LUMP_ROADMAP:
			case LUMP_ROADS:
			case LUMP_JUNCTIONS:
			case LUMP_ROADSURF:
			case LUMP_ROADBOUNDS:
			case LUMP_JUNCBOUNDS:
			case LUMP_SUBDIVISION:
			default: // maybe Lump 11?
			{
				MsgInfo("Assuming it's a 'Driver 1' LEV file\n");
				g_format = LEV_FORMAT_DRIVER1;
				break;
			}
		}
		
		// dun
		if (g_format != LEV_FORMAT_AUTODETECT)
			break;

		// skip lump
		pFile->Seek(lump.size, VS_SEEK_CUR);

		// position alignment
		if ((pFile->Tell() % 4) != 0)
			pFile->Seek(4 - (pFile->Tell() % 4), VS_SEEK_CUR);
	}

	pFile->Seek(curPos, VS_SEEK_SET);
}

//-------------------------------------------------------------
// Iterates LEV file lumps and loading data from them
//-------------------------------------------------------------
void ProcessLumps(IVirtualStream* pFile)
{
	// perform auto-detection if format is not specified
	if(g_format == LEV_FORMAT_AUTODETECT)
		DetectLevelFormat(pFile);

	if(!g_levMap)
	{
		// failed to detect Driver 1 level file - try Driver 2 loader
		if (g_format >= LEV_FORMAT_DRIVER2_ALPHA16 || g_format == LEV_FORMAT_AUTODETECT)
			g_levMap = new CDriver2LevelMap();
		else
			g_levMap = new CDriver1LevelMap();
	}

	int lump_count = 255; // Driver 2 difference: you not need to read lump count

	// Driver 1 has lump count
	if (g_format == LEV_FORMAT_DRIVER1)
		pFile->Read(&lump_count, sizeof(int), 1);

	LUMP lump;
	for (int i = 0; i < lump_count; i++)
	{
		// read lump info
		pFile->Read(&lump, sizeof(LUMP), 1);

		// stop marker
		if (lump.type == 255)
			break;

		switch (lump.type)
		{
			case LUMP_MODELS:
				MsgWarning("LUMP_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadLevelModelsLump(pFile);
				break;
			case LUMP_MAP:
				MsgWarning("LUMP_MAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levMap->LoadMapLump(pFile);
				break;
			case LUMP_TEXTURENAMES:
				MsgWarning("LUMP_TEXTURENAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.LoadTextureNamesLump(pFile, lump.size);
				break;
			case LUMP_MODELNAMES:
				MsgWarning("LUMP_MODELNAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadModelNamesLump(pFile, lump.size);
				break;
			case LUMP_SUBDIVISION:
				MsgWarning("LUMP_SUBDIVISION ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_LOWDETAILTABLE:
				MsgWarning("LUMP_LOWDETAILTABLE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_MOTIONCAPTURE:
				MsgWarning("LUMP_MOTIONCAPTURE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_OVERLAYMAP:
				MsgWarning("LUMP_OVERLAYMAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadOverlayMapLump(pFile, lump.size);
				break;
			case LUMP_PALLET:
				MsgWarning("LUMP_PALLET ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.ProcessPalletLump(pFile);
				break;
			case LUMP_SPOOLINFO:
				MsgWarning("LUMP_SPOOLINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levMap->LoadSpoolInfoLump(pFile);
				break;
			case LUMP_STRAIGHTS2:
				MsgWarning("LUMP_STRAIGHTS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CURVES2:
				MsgWarning("LUMP_CURVES2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2:
				MsgWarning("LUMP_JUNCTIONS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2_NEW:
				MsgWarning("LUMP_JUNCTIONS2_NEW ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CHAIR:
				MsgWarning("LUMP_CHAIR ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CAR_MODELS:
				MsgWarning("LUMP_CAR_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadCarModelsLump(pFile, lump.size);
				break;
			case LUMP_TEXTUREINFO:
				MsgWarning("LUMP_TEXTUREINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.LoadTextureInfoLump(pFile);
				break;
			default:
				MsgInfo("LUMP type: %d (0x%X) ofs=%d size=%d\n", lump.type, lump.type, pFile->Tell(), lump.size);
		}

		// skip lump
		pFile->Seek(lump.size, VS_SEEK_CUR);

		// position alignment
		if ((pFile->Tell() % 4) != 0)
			pFile->Seek(4 - (pFile->Tell() % 4), VS_SEEK_CUR);
	}
}

void LoadLevelFile(const char* filename)
{
	// try load driver2 lev file
	FILE* pReadFile = fopen(filename, "rb");

	if (!pReadFile)
	{
		MsgError("Failed to open LEV file!\n");
		return;
	}

	CMemoryStream* stream = new CMemoryStream();
	stream->Open(nullptr, VS_OPEN_WRITE | VS_OPEN_READ, 0);

	// read file into stream
	if (stream->ReadFromFileStream(pReadFile))
	{
		fclose(pReadFile);
		g_levStream = stream;
	}
	else
	{
		delete stream;
		return;
	}

	// seek to begin
	g_levStream->Seek(0, VS_SEEK_SET);

	MsgWarning("-----------\nloading lev file '%s'\n", filename);

	std::string fileName = filename;

	size_t lastindex = fileName.find_last_of(".");
	fileName = fileName.substr(0, lastindex);

	//-------------------------------------------------------------------

	LUMP curLump;

	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_LUMPDESC)
	{
		MsgError("Not a LEV file!\n");
		return;
	}

	// read chunk offsets
	g_levStream->Read(&g_levInfo, sizeof(dlevinfo_t), 1);

	Msg("levdesc_offset = %d\n", g_levInfo.levdesc_offset);
	Msg("levdesc_size = %d\n", g_levInfo.levdesc_size);

	Msg("texdata_offset = %d\n", g_levInfo.texdata_offset);
	Msg("texdata_size = %d\n", g_levInfo.texdata_size);

	Msg("levdata_offset = %d\n", g_levInfo.levdata_offset);
	Msg("levdata_size = %d\n", g_levInfo.levdata_size);

	Msg("spooldata_offset = %d\n", g_levInfo.spooldata_offset);
	Msg("spooldata_size = %d\n", g_levInfo.spooldata_size);

	// read cells

	//-----------------------------------------------------
	// seek to section 1 - lump data 1
	g_levStream->Seek(g_levInfo.levdesc_offset, VS_SEEK_SET);

	// read lump
	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_LEVELDESC)
	{
		MsgError("Not a LUMP_LEVELDESC!\n");
	}

	MsgInfo("entering LUMP_LEVELDESC size = %d\n--------------\n", curLump.size);

	// read sublumps
	ProcessLumps(g_levStream);

	//-----------------------------------------------------
	// read global textures

	g_levTextures.LoadPermanentTPages(g_levStream);

	//-----------------------------------------------------
	// seek to section 3 - lump data 2
	g_levStream->Seek(g_levInfo.levdata_offset, VS_SEEK_SET);

	// read lump
	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_LEVELDATA)
	{
		MsgError("Not a lump 0x24!\n");
	}

	MsgInfo("entering LUMP_LEVELDATA size = %d\n--------------\n", curLump.size);

	// read sublumps
	ProcessLumps(g_levStream);
}

void FreeLevelData()
{
	MsgWarning("FreeLevelData() ...\n");

	if (g_levStream)
		delete g_levStream;
	g_levStream = nullptr;

	g_levMap->FreeAll();
	g_levTextures.FreeAll();
	g_levModels.FreeAll();

	delete[] g_overlayMapData;
}