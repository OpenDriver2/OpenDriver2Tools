#include "driver_level.h"

#include <string.h>

#include "core/cmdlib.h"
#include "core/VirtualStream.h"

#include "model_compiler/compiler.h"
#include "util/util.h"
#include "viewer/viewer.h"

#include "driver_routines/regions_d1.h"
#include "driver_routines/regions_d2.h"

#include <nstd/String.hpp>
#include <nstd/Directory.hpp>
#include <nstd/File.hpp>

bool g_export_carmodels = false;
bool g_export_models = false;
bool g_extract_dmodels = false;

bool g_export_world = false;

bool g_export_textures = false;
bool g_explode_tpages = false;

bool g_export_overmap = false;

int g_overlaymap_width = 1;

//---------------------------------------------------------------------------------------------------------------------------------

OUT_CITYLUMP_INFO		g_levInfo;
CDriverLevelTextures	g_levTextures;
CDriverLevelModels		g_levModels;
CBaseLevelMap*			g_levMap = nullptr;

String					g_levname;
String					g_levname_moddir;
String					g_levname_texdir;

const float				texelSize = 1.0f / 256.0f;
const float				halfTexelSize = texelSize * 0.5f;


//-------------------------------------------------------------
// Exports level data
//-------------------------------------------------------------
void ExportLevelData()
{
	Msg("-------------\nExporting level data\n-------------\n");

	if (g_export_models || g_export_carmodels)
	{
		Directory::create(g_levname_moddir);
		SaveModelPagesMTL();
	}
	
	if (g_export_models)
	{
		Directory::create(g_levname_moddir);
		ExportAllModels();
	}

	if (g_export_carmodels)
	{
		Directory::create(g_levname_moddir);
		ExportAllCarModels();
	}
	
	if (g_export_world)
	{
		ExportRegions();
	}

	if (g_export_textures)
	{
		Directory::create(g_levname_texdir);
		ExportAllTextures();
	}

	if (g_export_overmap)
	{
		Directory::create(g_levname_texdir);
		ExportOverlayMap();
	}

	Msg("Export done\n");
}

//-------------------------------------------------------------------------------------------------------------------------

void ExportLevelFile()
{
	FILE* levTest = fopen(g_levname, "rb");

	if (!levTest)
	{
		MsgError("LEV file '%s' does not exists!\n", (char*)g_levname);
		return;
	}

	CFileStream stream(levTest);
	ELevelFormat levFormat = CDriverLevelLoader::DetectLevelFormat(&stream);

	fclose(levTest);

	CDriverLevelLoader levLoader;

	// create map accordingly
	if (levFormat >= LEV_FORMAT_DRIVER2_ALPHA16 || levFormat == LEV_FORMAT_AUTODETECT)
		g_levMap = new CDriver2LevelMap();
	else
		g_levMap = new CDriver1LevelMap();

	levLoader.Initialize(g_levInfo, &g_levTextures, &g_levModels, g_levMap);

	if (levLoader.LoadFromFile(g_levname))
	{
		ExportLevelData();
	}

	MsgWarning("Freeing level data ...\n");

	g_levMap->FreeAll();
	g_levTextures.FreeAll();
	g_levModels.FreeAll();

	delete g_levMap;
}

// 
void ConvertDModelFileToOBJ(const char* filename, const char* outputFilename)
{
	MODEL* model;
	int modelSize;

	FILE* fp;
	fp = fopen(filename, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		modelSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		model = (MODEL*)malloc(modelSize + 16);
		fread(model, 1, modelSize, fp);
		fclose(fp);
	}
	else
	{
		MsgError("Failed to open '%s'!\n", filename);
		return;
	}

	if (model->instance_number > 0)
	{
		MsgError("This model points to instance '%d' and cannot be exported!\n", model->instance_number);
		return;
	}

	ExportDMODELToOBJ(model, outputFilename, 0, modelSize);
}

//----------------------------------------------------------------------------------------

void PrintCommandLineArguments()
{
	const char* argumentsMessage =
		"Example: DriverLevelTool <command> [arguments]\n\n\n"
		"  -lev <filename.LEV> \t: Specify level file you want to input\n\n"
		"  -format <n> \t: Specify level format. 1 = Driver 1, 2 = Driver 2 Demo (alpha 1.6), 3 = Driver 2 Retail\n\n"
		"  -textures <1/0> \t: Export textures (TGA)\n\n"
		"  -models <1/0> \t: Export models (OBJ\n\n"
		"  -carmodels <1/0> \t: Export car models (OBJ)\n\n"
		"  -world <1/0> \t\t: Export whole world regions (OBJ)\n\n"
		"  -extractmodels \t: Extracts DMODEL files instead of exporting to OBJ\n\n"
		"  -overmap <width> \t: Extract overlay map with specified width\n\n"
		"  -explodetpages \t: Extracts textures as separate TIM files instead of whole texture page exporting as TGA\n\n"
		"  -dmodel2obj <filename.DMODEL> <output.OBJ> \t: converts DMODEL to OBJ file\n\n";
		"  -compiledmodel <filename.OBJ> <output.DMODEL> \t: compiles OBJ to DMODEL file\n\n";
		"  -denting \t: enables car denting file generation for next -compilemodel key\n\n";
	MsgInfo(argumentsMessage);
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
	Install_ConsoleSpewFunction();
#endif

	Msg("---------------\nDriverLevelTool - PSX Driver level utilities\n---------------\n\n");

	if (argc == 0)
	{
		PrintCommandLineArguments();
		return 0;
	}

	bool generate_denting = false;
	int main_routine = 2;

	for (int i = 1; i < argc; i++)
	{
		if (!stricmp(argv[i], "-textures"))
		{
			g_export_textures = atoi(argv[i + 1]) > 0;
			main_routine = 1;
			i++;
		}
		else if (!stricmp(argv[i], "-world"))
		{
			g_export_world = atoi(argv[i + 1]) > 0;
			main_routine = 1;
			i++;
		}
		else if (!stricmp(argv[i], "-models"))
		{
			g_export_models = atoi(argv[i + 1]) > 0;
			main_routine = 1;
			i++;
		}
		else if (!stricmp(argv[i], "-carmodels"))
		{
			g_export_carmodels = atoi(argv[i + 1]) > 0;
			main_routine = 1;
			i++;
		}
		else if (!stricmp(argv[i], "-extractmodels"))
		{
			g_extract_dmodels = true;
		}
		else if (!stricmp(argv[i], "-explodetpages"))
		{
			g_explode_tpages = true;
		}
		else if (!stricmp(argv[i], "-overmap"))
		{
			g_export_overmap = true;
			g_overlaymap_width = atoi(argv[i + 1]);
			main_routine = 1;
			i++;
		}
		else if (!stricmp(argv[i], "-lev"))
		{
			g_levname = String::fromCString(argv[i + 1], strlen(argv[i + 1]));
			i++;
		}
		else if (!stricmp(argv[i], "-dmodel2obj"))
		{
			ConvertDModelFileToOBJ(argv[i + 1], argv[i + 2]);
			main_routine = 0;
			i += 2;
		}
		else if (!stricmp(argv[i], "-denting"))
		{
			generate_denting = true;
		}
		else if (!stricmp(argv[i], "-compiledmodel"))
		{
			CompileOBJModelToDMODEL(argv[i + 1], argv[i + 2], generate_denting);
			main_routine = 0;
			generate_denting = false; // disable denting compiler after it's job done
			i += 2;
		}
		else
		{
			MsgWarning("Unknown command line parameter '%s'\n", argv[i]);
			PrintCommandLineArguments();
			return 0;
		}
	}

	if (g_levname.length() == 0)
	{
		PrintCommandLineArguments();
		return 0;
	}

	// prepare anyway
	String lev_no_ext = File::dirname(g_levname) + "/" + File::basename(g_levname, File::extension(g_levname));
	g_levname_moddir = lev_no_ext + "_models";
	g_levname_texdir = lev_no_ext + "_textures";

	if (main_routine == 1)
	{
		ExportLevelFile();
	}
	else if (main_routine == 2)
	{
		ViewerMain();
	}

	return 0;
}