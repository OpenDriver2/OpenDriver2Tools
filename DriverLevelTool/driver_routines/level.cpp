#include "driver_level.h"

#include "regions_d1.h"
#include "regions_d2.h"

#include "core/VirtualStream.h"

#include <nstd/File.hpp>

ELevelFormat g_format = LEV_FORMAT_AUTODETECT;

//--------------------------------------------------------------------------------------------------------------------------

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
				MsgInfo("Detected 'Driver 2 DEMO' 1.6 alpha LEV file\n");
				g_format = LEV_FORMAT_DRIVER2_ALPHA16; // as it is an old junction format - it's clearly a alpha 1.6 level
				break;
			}
			case LUMP_JUNCTIONS2_NEW:
			{
				MsgInfo("Detected 'Driver 2' final LEV file\n");
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
				MsgInfo("Detected 'Driver 1' LEV file\n");
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
				DevMsg(SPEW_WARNING, "LUMP_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadLevelModelsLump(pFile);
				break;
			case LUMP_MAP:
				DevMsg(SPEW_WARNING, "LUMP_MAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levMap->LoadMapLump(pFile);
				break;
			case LUMP_TEXTURENAMES:
				DevMsg(SPEW_WARNING, "LUMP_TEXTURENAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.LoadTextureNamesLump(pFile, lump.size);
				break;
			case LUMP_MODELNAMES:
				DevMsg(SPEW_WARNING, "LUMP_MODELNAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadModelNamesLump(pFile, lump.size);
				break;
			case LUMP_SUBDIVISION:
				DevMsg(SPEW_WARNING, "LUMP_SUBDIVISION ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_LOWDETAILTABLE:
				g_levModels.LoadLowDetailTableLump(pFile, lump.size);
				DevMsg(SPEW_WARNING, "LUMP_LOWDETAILTABLE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_MOTIONCAPTURE:
				DevMsg(SPEW_WARNING, "LUMP_MOTIONCAPTURE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_OVERLAYMAP:
				DevMsg(SPEW_WARNING, "LUMP_OVERLAYMAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadOverlayMapLump(pFile, lump.size);
				break;
			case LUMP_PALLET:
				DevMsg(SPEW_WARNING, "LUMP_PALLET ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.ProcessPalletLump(pFile);
				break;
			case LUMP_SPOOLINFO:
				DevMsg(SPEW_WARNING, "LUMP_SPOOLINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levMap->LoadSpoolInfoLump(pFile);
				break;
			case LUMP_STRAIGHTS2:
				DevMsg(SPEW_WARNING, "LUMP_STRAIGHTS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CURVES2:
				DevMsg(SPEW_WARNING, "LUMP_CURVES2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2:
				DevMsg(SPEW_WARNING, "LUMP_JUNCTIONS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2_NEW:
				DevMsg(SPEW_WARNING, "LUMP_JUNCTIONS2_NEW ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CHAIR:
				DevMsg(SPEW_WARNING, "LUMP_CHAIR ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CAR_MODELS:
				DevMsg(SPEW_WARNING, "LUMP_CAR_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levModels.LoadCarModelsLump(pFile, lump.size);
				break;
			case LUMP_TEXTUREINFO:
				DevMsg(SPEW_WARNING, "LUMP_TEXTUREINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				g_levTextures.LoadTextureInfoLump(pFile);
				break;
			default:
				DevMsg(SPEW_WARNING, "LUMP type: %d (0x%X) ofs=%d size=%d\n", lump.type, lump.type, pFile->Tell(), lump.size);
		}

		// skip lump
		pFile->Seek(lump.size, VS_SEEK_CUR);

		// position alignment
		if ((pFile->Tell() % 4) != 0)
			pFile->Seek(4 - (pFile->Tell() % 4), VS_SEEK_CUR);
	}
}

bool LoadLevelFile(const char* filename)
{
	// try load driver2 lev file
	FILE* pReadFile = fopen(filename, "rb");

	if (!pReadFile)
	{
		MsgError("Failed to open LEV file!\n");
		return false;
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
		return false;
	}

	// seek to begin
	g_levStream->Seek(0, VS_SEEK_SET);

	MsgWarning("-----------\nLoading LEV file '%s'\n", filename);

	String fileName = File::basename(String::fromCString(filename, strlen(filename)));

	//-------------------------------------------------------------------

	LUMP curLump;

	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_LUMPDESC)
	{
		MsgError("Not a LEV file!\n");
		return false;
	}

	// read chunk offsets
	g_levStream->Read(&g_levInfo, sizeof(OUT_CITYLUMP_INFO), 1);

	DevMsg(SPEW_NORM, "data1_offset = %d\n", g_levInfo.loadtime_offset);
	DevMsg(SPEW_NORM, "data1_size = %d\n", g_levInfo.loadtime_size);

	DevMsg(SPEW_NORM, "tpage_offset = %d\n", g_levInfo.tpage_offset);
	DevMsg(SPEW_NORM, "tpage_size = %d\n", g_levInfo.tpage_size);

	DevMsg(SPEW_NORM, "data2_offset = %d\n", g_levInfo.inmem_offset);
	DevMsg(SPEW_NORM, "data2_size = %d\n", g_levInfo.inmem_size);

	DevMsg(SPEW_NORM, "spooled_offset = %d\n", g_levInfo.spooled_offset);
	DevMsg(SPEW_NORM, "spooled_size = %d\n", g_levInfo.spooled_size);

	// read cells

	//-----------------------------------------------------
	// seek to section 1 - lump data 1
	g_levStream->Seek(g_levInfo.loadtime_offset, VS_SEEK_SET);

	// read lump
	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_LOADTIME_DATA)
	{
		MsgError("Not a LUMP_LEVELDESC!\n");
	}

	DevMsg(SPEW_INFO, "entering LUMP_LEVELDESC size = %d\n--------------\n", curLump.size);

	// read sublumps
	ProcessLumps(g_levStream);

	//-----------------------------------------------------
	// read global textures

	g_levTextures.LoadPermanentTPages(g_levStream);

	//-----------------------------------------------------
	// seek to section 3 - lump data 2
	g_levStream->Seek(g_levInfo.inmem_offset, VS_SEEK_SET);

	// read lump
	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if (curLump.type != LUMP_INMEMORY_DATA)
	{
		MsgError("Not a lump 0x24!\n");
	}

	DevMsg(SPEW_INFO, "entering LUMP_LEVELDATA size = %d\n--------------\n", curLump.size);

	// read sublumps
	ProcessLumps(g_levStream);

	return true;
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

	delete g_levMap;

	delete[] g_overlayMapData;
}