
// Some containers

#ifndef TEXTURES_H
#define TEXTURES_H

#include "level.h"
#include "math/vector.h"

// forward
class IVirtualStream;

//---------------------------------------------------------------------------------------------------------------------------------

struct XYPAIR {
	int x; // size=0, offset=0
	int y; // size=0, offset=4
};

struct TexPage_t
{
	TEXINF*			details;
	int				numDetails;
};

struct texdata_t
{
	ubyte*		data;
	int			rsize;

	TEXCLUT*	clut;
	int			numPalettes;
};

struct extclutdata_t
{
	TEXCLUT clut;
	int texnum[32];
	int texcnt;
	int palette;
	int tpage;

};

//---------------------------------------------------------------------------------------------------------------------------------

TVec4D<ubyte>	bgr5a1_ToRGBA8(ushort color);

//---------------------------------------------------------------------------------------------------------------------------------

// unpacks compressed texture
int				UnpackTexture(ubyte* pSrc, ubyte* pDst);

// loads texture (you must specify offset in virtual stream before)
void			LoadTPageAndCluts(IVirtualStream* pFile, texdata_t* out, int pageIndex, bool isCompressed);

// loads global textures (pre-loading stage)
void			LoadPermanentTPages(IVirtualStream* pFile);

// loads texture atlas information (details per page)
void			LoadTextureInfoLump(IVirtualStream* pFile);

// searches for texture detail
TEXINF*			FindTextureDetail(const char* name);

#endif // TEXTURES_H
