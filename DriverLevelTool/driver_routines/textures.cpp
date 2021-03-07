
#include "driver_level.h"

#include "core/cmdlib.h"
#include "core/IVirtualStream.h"
#include "math/Vector.h"

//-------------------------------------------------------------------------------

class CDriverLevelTextures
{
public:
	CDriverLevelTextures();
	~CDriverLevelTextures();
	
	
protected:
	char*			m_textureNamesData {nullptr};

	TEXPAGE_POS*	m_texPagePos {nullptr};

	int				m_numTexPages {0};
	int				m_numTexDetails {0};

	int				m_numPermanentPages {0};
	int				m_numSpecPages {0};

	XYPAIR			m_permsList[16];
	XYPAIR			m_specList[16];

	CTexturePage*		m_texPages { nullptr};
	
	ExtClutData_t*	m_extraPalettes { nullptr};
	int				m_numExtraPalettes { 0};
};

char*						g_textureNamesData = nullptr;

TEXPAGE_POS*				g_texPagePos = nullptr;

int							g_numTexPages = 0;
int							g_numTexDetails = 0;

int							g_numPermanentPages = 0;
int							g_numSpecPages = 0;

XYPAIR						g_permsList[16];
XYPAIR						g_specList[16];
CTexturePage*					g_texPages = nullptr;
ExtClutData_t*				g_extraPalettes = nullptr;
int							g_numExtraPalettes = 0;

// 16 bit color to BGRA
// originalTransparencyKey makes it pink
TVec4D<ubyte> bgr5a1_ToRGBA8(ushort color, bool originalTransparencyKey /*= true*/)
{
	ubyte b = (color & 0x1F) * 8;
	ubyte g = ((color >> 5) & 0x1F) * 8;
	ubyte r = ((color >> 9) & 0x1F) * 8;
	
	ubyte a = (color >> 15);
	
	// restore source transparency key
	if(originalTransparencyKey && color == 0)
	{
		return TVec4D<ubyte>(255, 0, 255, 0);
	}

	return TVec4D<ubyte>(r,g,b,a);
}

// 16 bit color to RGBA
// originalTransparencyKey makes it pink
TVec4D<ubyte> rgb5a1_ToRGBA8(ushort color, bool originalTransparencyKey /*= true*/)
{
	ubyte r = (color & 0x1F) * 8;
	ubyte g = ((color >> 5) & 0x1F) * 8;
	ubyte b = ((color >> 10) & 0x1F) * 8;

	ubyte a = (color >> 15);

	// restore source transparency key
	if (originalTransparencyKey && color == 0)
	{
		return TVec4D<ubyte>(255, 0, 255, 0);
	}

	return TVec4D<ubyte>(r, g, b, a);
}

//-------------------------------------------------------------------------------

// unpacks texture, returns new source pointer
// there is something like RLE used
char* unpackTexture(char* src, char* dest)
{
	// start from the end
	char* ptr = dest + TEXPAGE_4BIT_SIZE - 1;

	do {
		char pix = *src++;

		if ((pix & 0x80) != 0)
		{
			char p = *src++;

			do (*ptr-- = p);
			while (pix++ <= 0);
		}
		else
		{
			do (*ptr-- = *src++);
			while (pix-- != 0);
		}
	} while (ptr >= dest);

	return src;
}

//-------------------------------------------------------------------------------

#define MAX_PAGE_CLUTS 63

struct texturedata_t
{
	int			numPalettes;
	TEXCLUT		palettes[63];
	ubyte		compTable[28];		// padding for 512-B alignment if uncompressed
	ubyte		texels[128 * 256];	// 256×256 four-bit indices
};

CTexturePage::CTexturePage()
{
}

CTexturePage::~CTexturePage()
{
	delete[] m_bitmap.data;
	delete[] m_bitmap.clut;
	delete[] m_details;
}

//-------------------------------------------------------------
// Conversion of indexed palettized texture to 32bit RGBA
//-------------------------------------------------------------
void CTexturePage::ConvertIndexedTextureToRGBA(uint* dest_color_data, int detail, TEXCLUT* clut, bool outputBGR, bool originalTransparencyKey)
{
	TexBitmap_t& bitmap = m_bitmap;

	if (!(detail < m_numDetails))
	{
		MsgError("Cannot apply palette to non-existent detail! Programmer error?\n");
		return;
	}

	if (clut == nullptr)
		clut = &bitmap.clut[detail];

	int ox = m_details[detail].x;
	int oy = m_details[detail].y;
	int w = m_details[detail].width;
	int h = m_details[detail].height;

	if (w == 0)
		w = 256;

	if (h == 0)
		h = 256;

	//char* textureName = g_textureNamesData + m_details[detail].nameoffset;
	//MsgWarning("Applying detail %d '%s' (xywh: %d %d %d %d)\n", detail, textureName, ox, oy, w, h);

	int tp_wx = ox + w;
	int tp_hy = oy + h;

	for (int y = oy; y < tp_hy; y++)
	{
		for (int x = ox; x < tp_wx; x++)
		{
			ubyte clindex = bitmap.data[y * 128 + x / 2];

			if (0 != (x & 1))
				clindex >>= 4;

			clindex &= 0xF;

			TVec4D<ubyte> color = outputBGR ?
				bgr5a1_ToRGBA8(clut->colors[clindex], originalTransparencyKey) :
				rgb5a1_ToRGBA8(clut->colors[clindex], originalTransparencyKey);

			// flip texture by Y because of TGA
			int ypos = (TEXPAGE_SIZE_Y - y - 1) * TEXPAGE_SIZE_Y;

			dest_color_data[ypos + x] = *(uint*)(&color);
		}
	}
}

void CTexturePage::InitFromFile(int id, IVirtualStream* pFile)
{
	m_id = id;

	pFile->Read(&m_numDetails, 1, sizeof(int));

	// don't load empty tpage details
	if (m_numDetails)
	{
		// read texture detail info
		m_details = new TEXINF[m_numDetails];
		pFile->Read(m_details, m_numDetails, sizeof(TEXINF));
	}
	else
		m_details = nullptr;
}


void CTexturePage::LoadCompressedTexture(IVirtualStream* pFile)
{
	pFile->Read( &m_bitmap.numPalettes, 1, sizeof(int) );

	// allocate palettes
	m_bitmap.clut = new TEXCLUT[m_bitmap.numPalettes];

	for(int i = 0; i < m_bitmap.numPalettes; i++)
	{
		// read 16 palettes
		pFile->Read(&m_bitmap.clut[i].colors, 16, sizeof(ushort));
	}

	int imageStart = pFile->Tell();

	// read compression data
	ubyte* compressedData = new ubyte[TEXPAGE_4BIT_SIZE];
	pFile->Read(compressedData, 1, TEXPAGE_4BIT_SIZE);

	char* unpackEnd = unpackTexture((char*)compressedData, (char*)m_bitmap.data);
	
	// unpack
	m_bitmap.rsize = (char*)unpackEnd - (char*)compressedData;

	// seek to the right position
	// this is necessary because it's aligned to CD block size
	pFile->Seek(imageStart + m_bitmap.rsize, VS_SEEK_SET);
}

//-------------------------------------------------------------------------------
// Loads Texture page itself with it's color lookup tables
//-------------------------------------------------------------------------------
bool CTexturePage::LoadTPageAndCluts(IVirtualStream* pFile, bool isCompressed)
{
	int rStart = pFile->Tell();

	if(m_bitmap.data)
	{
		// skip already loaded data
		pFile->Seek(m_bitmap.rsize, VS_SEEK_CUR);
		return true;
	}

	m_bitmap.data = new ubyte[TEXPAGE_4BIT_SIZE];

	if( isCompressed )
	{
		LoadCompressedTexture(pFile);
	}
	else
	{
		// non-compressed textures loads different way, with a fixed size
		texturedata_t* texData = new texturedata_t;
		pFile->Read( texData, 1, sizeof(texturedata_t) );

		// palettes are after them
		m_bitmap.numPalettes = texData->numPalettes;

		m_bitmap.clut = new TEXCLUT[m_bitmap.numPalettes];
		memcpy(m_bitmap.clut, texData->palettes, sizeof(TEXCLUT)* m_bitmap.numPalettes);

		memcpy(m_bitmap.data, texData->texels, TEXPAGE_4BIT_SIZE);

		// not need anymore
		delete texData;
	}

	int readSize = pFile->Tell()-rStart;

	m_bitmap.rsize = readSize;

	Msg("PAGE %d (%s) datasize=%d\n", m_id, isCompressed ? "compr" : "spooled", readSize);

	return true;
}

//-------------------------------------------------------------------------------
// searches for detail in this TPAGE
//-------------------------------------------------------------------------------
TEXINF* CTexturePage::FindTextureDetail(const char* name) const
{
	for (int i = 0; i < m_numDetails; i++)
	{
		char* pTexName = g_textureNamesData + m_details[i].nameoffset;

		if (!strcmp(pTexName, name)) // FIXME: hashing and case insensitive?
			return &m_details[i];
	}

	return nullptr;
}

const TexBitmap_t& CTexturePage::GetBitmap() const
{
	return m_bitmap;
}

TEXINF* CTexturePage::GetTextureDetail(int num) const
{
	return &m_details[num];
}

int CTexturePage::GetDetailCount() const
{
	return m_numDetails;
}

//-------------------------------------------------------------------------------

//
// loads global textures (pre-loading stage)
//
void LoadPermanentTPages(IVirtualStream* pFile)
{
	pFile->Seek( g_levInfo.texdata_offset, VS_SEEK_SET );
	
	//-----------------------------------

	Msg("Loading permanent texture pages (%d)\n", g_numPermanentPages);

	// simulate sectors
	// convert current file offset to sectors
	long sector = pFile->Tell() / 2048; 
	int nsectors = 0;

	for (int i = 0; i < g_numPermanentPages; i++)
		nsectors += (g_permsList[i].y + 2047) / 2048;
	
	// load permanent pages
	for(int i = 0; i < g_numPermanentPages; i++)
	{
		long curOfs = pFile->Tell(); 
		int tpage = g_permsList[i].x;

		// permanents are also compressed
		g_texPages[tpage].LoadTPageAndCluts(pFile, true);

		pFile->Seek(curOfs + ((g_permsList[i].y + 2047) & -2048), VS_SEEK_SET);
	}

	// simulate sectors
	sector += nsectors;
	pFile->Seek(sector * 2048, VS_SEEK_SET);

	// Driver 2 - special cars only
	// Driver 1 - only player cars
	Msg("Loading special/car texture pages (%d)\n", g_numSpecPages);

	// load compressed spec pages
	// those are non-spooled ones
	for (int i = 0; i < g_numSpecPages; i++)
	{
		TexBitmap_t* newTexData = new TexBitmap_t;

		long curOfs = pFile->Tell();
		int tpage = g_specList[i].x;

		// permanents are compressed
		g_texPages[tpage].LoadTPageAndCluts(pFile, true);

		pFile->Seek(curOfs + ((g_specList[i].y + 2047) & -2048), VS_SEEK_SET);
	}
}

//-------------------------------------------------------------
// parses texture info lumps. Quite simple
//-------------------------------------------------------------
void LoadTextureInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int numPages;
	pFile->Read(&numPages, 1, sizeof(int));

	int numTextures;
	pFile->Read(&numTextures, 1, sizeof(int));

	Msg("TPage count: %d\n", numPages);
	Msg("Texture amount: %d\n", numTextures);

	// read array of texutre page info
	g_texPagePos = new TEXPAGE_POS[numPages+1];
	g_texPages = new CTexturePage[numPages];

	pFile->Read(g_texPagePos, numPages+1, sizeof(TEXPAGE_POS));

	// read page details
	g_numTexPages = numPages;

	for(int i = 0; i < numPages; i++) 
	{
		CTexturePage& tp = g_texPages[i];
		tp.InitFromFile(i, pFile);
	}

	pFile->Read(&g_numPermanentPages, 1, sizeof(int));
	Msg("Permanent TPages = %d\n", g_numPermanentPages);
	pFile->Read(g_permsList, 16, sizeof(XYPAIR));

	// Driver 2 - special cars only
	// Driver 1 - only player cars
	pFile->Read(&g_numSpecPages, 1, sizeof(int));
	pFile->Read(g_specList, 16, sizeof(XYPAIR));

	Msg("Special/Car TPages = %d\n", g_numSpecPages);

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//----------------------------------------------------------------------------------------------------

TEXINF* FindTextureDetail(const char* name)
{
	for(int i = 0; i < g_numTexPages; i++)
	{
		TEXINF* found = g_texPages[i].FindTextureDetail(name);

		if (found)
			return found;
	}

	return nullptr;
}
