#include "driver_level.h"
#include "core/cmdlib.h"
#include "core/VirtualStream.h"

#include "model_compiler/compiler.h"
#include "util/util.h"

bool g_export_carmodels = false;
bool g_export_models = false;
bool g_extract_dmodels = false;

bool g_export_world = false;

bool g_export_textures = false;
bool g_explode_tpages = false;

bool g_export_overmap = false;

int g_overlaymap_width = 1;

//---------------------------------------------------------------------------------------------------------------------------------

std::string				g_levname_moddir;
std::string				g_levname_texdir;

extern std::string		g_levname;

int						g_levSize = 0;

const float				texelSize = 1.0f / 256.0f;
const float				halfTexelSize = texelSize * 0.5f;


//-------------------------------------------------------------
// Exports level data
//-------------------------------------------------------------
void ExportLevelData()
{
	Msg("-------------\nExporting level data\n-------------\n");

	if (g_export_models || g_export_carmodels)
		SaveModelPagesMTL();
	
	if (g_export_models)
		ExportAllModels();

	if (g_export_carmodels)
		ExportAllCarModels();
	
	if (g_export_world)
		ExportRegions();

	if (g_export_textures)
		ExportAllTextures();

	if (g_export_overmap)
		ExportOverlayMap();

	Msg("Export done\n");
}

//-------------------------------------------------------------------------------------------------------------------------

void ProcessLevFile(const char* filename)
{
	FILE* levTest = fopen(filename, "rb");

	if (!levTest)
	{
		MsgError("LEV file '%s' does not exists!\n", filename);
		return;
	}

	fseek(levTest, 0, SEEK_END);
	g_levSize = ftell(levTest);
	fclose(levTest);

	std::string lev_no_ext = filename;
	
	size_t lastindex = lev_no_ext.find_last_of(".");
	lev_no_ext = lev_no_ext.substr(0, lastindex);

	g_levname_moddir = lev_no_ext + "_models";
	g_levname_texdir = lev_no_ext + "_textures";

	mkdirRecursive(g_levname_moddir.c_str());
	mkdirRecursive(g_levname_texdir.c_str());

	LoadLevelFile(filename);

	ExportLevelData();

	FreeLevelData();
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
	Install_ConsoleSpewFunction();

	Msg("---------------\nDriverLevelTool - PSX Driver level utilities\n---------------\n\n");

	if (argc == 0)
	{
		PrintCommandLineArguments();
		return 0;
	}

	bool generate_denting = false;
	bool do_main_routine = true;

	for (int i = 1; i < argc; i++)
	{
		if (!stricmp(argv[i], "-format"))
		{
			g_format = (ELevelFormat)atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(argv[i], "-textures"))
		{
			g_export_textures = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-world"))
		{
			g_export_world = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-models"))
		{
			g_export_models = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-carmodels"))
		{
			g_export_carmodels = atoi(argv[i + 1]) > 0;
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
			i++;
		}
		else if (!stricmp(argv[i], "-lev"))
		{
			g_levname = argv[i + 1];
			i++;
		}
		else if (!stricmp(argv[i], "-dmodel2obj"))
		{
			ConvertDModelFileToOBJ(argv[i + 1], argv[i + 2]);
			do_main_routine = false;
			i += 2;
		}
		else if (!stricmp(argv[i], "-denting"))
		{
			generate_denting = true;
		}
		else if (!stricmp(argv[i], "-compiledmodel"))
		{
			CompileOBJModelToDMODEL(argv[i + 1], argv[i + 2], generate_denting);
			do_main_routine = false;
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

	if(do_main_routine)
		ProcessLevFile(g_levname.c_str());

	return 0;
}