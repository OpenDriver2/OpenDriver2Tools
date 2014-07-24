
// Some containers

#ifndef TEXTURES_H
#define TEXTURES_H

#include "level.h"

// forward
class IVirtualStream;

//---------------------------------------------------------------------------------------------------------------------------------

struct TexPage_t
{
	TEXTUREDETAIL*	details;
	int				numDetails;
};

struct texdata_t
{
	ubyte*		data;
	int			size;

	TEXCLUT*	clut;
	int			numPalettes;
};

//---------------------------------------------------------------------------------------------------------------------------------

// unpacks compressed texture
int		UnpackTexture(ubyte* pSrc, ubyte* pDst);

// loads texture (you must specify offset in virtual stream before)
int		LoadTexturePageData(IVirtualStream* pFile, texdata_t* out, int pageIndex);

// loads global textures (pre-loading stage)
void	LoadGlobalTextures(IVirtualStream* pFile);

// loads texture atlas information (details per page)
void	LoadTextureInfoLump(IVirtualStream* pFile);

#endif // TEXTURES_H