#include <malloc.h>
#include <SDL_events.h>
#include "gl_renderer.h"
#include "core/cmdlib.h"

#include "driver_routines/d2_types.h"
#include "driver_routines/level.h"
#include "driver_routines/models.h"
#include "driver_routines/textures.h"

int g_quit = 0;

void SDLPollEvent()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
			{
				g_quit = 1;
				break;
			}
			case SDL_WINDOWEVENT:
			{
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_RESIZED:
					GR_UpdateWindowSize(event.window.data1, event.window.data2);

					break;
				case SDL_WINDOWEVENT_CLOSE:
					g_quit = 1;
					break;
				}
				break;
			}
			case SDL_MOUSEMOTION:
			{
				//Emulator_DoDebugMouseMotion(event.motion.x, event.motion.y);
				break;
			}
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				int nKey = event.key.keysym.scancode;

				// lshift/right shift
				if (nKey == SDL_SCANCODE_RSHIFT)
					nKey = SDL_SCANCODE_LSHIFT;
				else if (nKey == SDL_SCANCODE_RCTRL)
					nKey = SDL_SCANCODE_LCTRL;
				else if (nKey == SDL_SCANCODE_RALT)
					nKey = SDL_SCANCODE_LALT;

				//Emulator_DoDebugKeys(nKey, (event.type == SDL_KEYUP) ? false : true);
				break;
			}
		}
	}
}

TextureID g_hwTexturePages[128][32];
extern bool g_originalTransparencyKey;

// Creates hardware texture
void InitHWTexturePage(int nPage)
{
	g_originalTransparencyKey = false;	// off
	
	texdata_t* page = &g_texPageData[nPage];

	if (page->data == nullptr)
		return;
	
	int imgSize = TEXPAGE_SIZE * 4;
	uint* color_data = (uint*)malloc(imgSize);

	memset(color_data, 0, imgSize);

	// Dump whole TPAGE indexes
	for (int y = 0; y < 256; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			ubyte clindex = page->data[y * 128 + (x >> 1)];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			int ypos = (TEXPAGE_SIZE_Y - y - 1) * TEXPAGE_SIZE_Y;

			color_data[ypos + x] = clindex * 32;
		}
	}

	int numDetails = g_texPages[nPage].numDetails;

	// FIXME: load indexes instead?

	for (int i = 0; i < numDetails; i++)
		ConvertIndexedTextureToRGBA(nPage, color_data, i, &page->clut[i], false);

	g_hwTexturePages[nPage][0] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);

	// also load different palettes
	int numPalettes = 0;
	for (int pal = 0; pal < 16; pal++)
	{
		bool anyMatched = false;
		for (int i = 0; i < g_numExtraPalettes; i++)
		{
			if (g_extraPalettes[i].tpage != nPage)
				continue;

			if (g_extraPalettes[i].palette != pal)
				continue;

			for (int j = 0; j < g_extraPalettes[i].texcnt; j++)
				ConvertIndexedTextureToRGBA(nPage, color_data, g_extraPalettes[i].texnum[j], &g_extraPalettes[i].clut, false);

			anyMatched = true;
		}

		if (anyMatched)
		{
			g_hwTexturePages[nPage][numPalettes+1] = GR_CreateRGBATexture(TEXPAGE_SIZE_Y, TEXPAGE_SIZE_Y, (ubyte*)color_data);
			numPalettes++;
		}
	}

	// no longer need in RGBA data
	free(color_data);
}

//-------------------------------------------------------------
// Main level viewer
//-------------------------------------------------------------
int ViewerMain(const char* filename)
{
	if(!GR_Init("OpenDriver2 Level Tool - Viewer", 1280, 720, 0))
	{
		MsgError("Failed to init graphics!\n");
		return -1;
	}

	// Load level file
	LoadLevelFile(filename);

	// Load VAO

	// Load permanent textures
	for(int i = 0; i < g_numTexPages; i++)
	{
		memset(g_hwTexturePages[i], 0, sizeof(g_hwTexturePages[0]));

		if(g_texPagePos[i].flags & TPAGE_PERMANENT)
		{
			InitHWTexturePage(i);
		}
	}

	// Load models
	for(int i = 0; i < MAX_MODELS; i++)
	{
		
	}

	// Load car models
	for (int i = 0; i < MAX_CAR_MODELS; i++)
	{

	}

	do
	{
		SDLPollEvent();
		GR_ClearColor(32, 32, 32);
		GR_ClearDepth(1.0f);
	
		// Control and map

		// Spool map
		
		// Render stuff

		GR_SwapWindow();
	} while (!g_quit);

	// free all
	FreeLevelData();
	
	GR_Shutdown();

	return 0;
}