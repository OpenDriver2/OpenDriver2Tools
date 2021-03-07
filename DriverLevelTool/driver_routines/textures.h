
// Some containers

#ifndef TEXTURES_H
#define TEXTURES_H

#include "level.h"
#include "math/Vector.h"
#include "math/psx_math_types.h"

// forward
class IVirtualStream;

//---------------------------------------------------------------------------------------------------------------------------------

struct TexBitmap_t
{
	ubyte*			data { nullptr };	// 4 bit texture data
	int				rsize{ 0 };

	TEXCLUT*		clut {nullptr};
	int				numPalettes{ 0 };
};

//---------------------------------------------------------------------------------------------------------------------------------
class CTexturePage
{
public:
	CTexturePage();
	virtual ~CTexturePage();

	// loading texture page properties from file
	void					InitFromFile(int id, IVirtualStream* pFile);
	
	// loading texture page from lump
	bool					LoadTPageAndCluts(IVirtualStream* pFile, bool isCompressed);

	// converting 4bit texture page to 32 bit full color RGBA/BGRA
	void					ConvertIndexedTextureToRGBA(uint* dest_color_data, 
												int detail, TEXCLUT* clut = nullptr,
												bool outputBGR = false, bool originalTransparencyKey = true);

	// searches for detail in this TPAGE
	TEXINF*					FindTextureDetail(const char* name) const;
	TEXINF*					GetTextureDetail(int num) const;
	int						GetDetailCount() const;

	// returns the 4bit map data
	const TexBitmap_t&		GetBitmap() const;	
protected:

	void					LoadCompressedTexture(IVirtualStream* pFile);
	
	int						m_id{ -1 };

	TEXINF*					m_details{nullptr};
	int						m_numDetails{ 0 };

	TexBitmap_t				m_bitmap;
};

struct ExtClutData_t
{
	TEXCLUT clut;
	int texnum[32];
	int texcnt;
	int palette;
	int tpage;
};

//---------------------------------------------------------------------------------------------------------------------------------

extern CTexturePage*				g_texPages;
extern ExtClutData_t*			g_extraPalettes;
extern int						g_numExtraPalettes;

extern char*					g_textureNamesData;

extern TEXPAGE_POS*				g_texPagePos;

extern int						g_numTexPages;
extern int						g_numTexDetail;

extern char*					g_overlayMapData;

//---------------------------------------------------------------------------------------------------------------------------------

TVec4D<ubyte>	bgr5a1_ToRGBA8(ushort color, bool originalTransparencyKey = true);

//---------------------------------------------------------------------------------------------------------------------------------

int				GetCarPalIndex(int tpage);

// loads global textures (pre-loading stage)
void			LoadPermanentTPages(IVirtualStream* pFile);

// loads texture atlas information (details per page)
void			LoadTextureInfoLump(IVirtualStream* pFile);

// searches for texture detail
TEXINF*			FindTextureDetail(const char* name);

#endif // TEXTURES_H
