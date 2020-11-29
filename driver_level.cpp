#include "driver_level.h"
#include "math/Matrix.h"
#include "core/cmdlib.h"
#include "core/VirtualStream.h"
#include <stdarg.h>
#include <direct.h>

#define MODEL_SCALING			(0.00015f)

// string util
char* varargs(const char* fmt,...)
{
	va_list		argptr;

	static int index = 0;
	static char	string[4][4096];

	char* buf = string[index];
	index = (index + 1) & 3;

	memset(buf, 0, 4096);

	va_start (argptr,fmt);
	vsnprintf(buf, 4096, fmt,argptr);
	va_end (argptr);

	return buf;
}

bool g_extract_dmodels = false;
bool g_extract_tpages = false;
bool g_export_carmodels = false;
bool g_export_models = false;
bool g_export_textures = false;
bool g_export_world = false;

extern int g_region_format;
extern int g_format;

//#include "ImageLoader.h"

//---------------------------------------------------------------------------------------------------------------------------------

std::string			g_levname_moddir;
std::string			g_levname_texdir;

int					g_levSize = 0;

const float			texelSize = 1.0f/256.0f;
const float			halfTexelSize = texelSize*0.5f;

//-------------------------------------------------------------
// writes Wavefront OBJ into stream
//-------------------------------------------------------------
void WriteMODELToObjStream(	IVirtualStream* pStream, MODEL* model, int model_index, const char* name_prefix,
							bool debugInfo = true,
							const Matrix4x4& translation = identity4(),
							int* first_v = NULL,
							int* first_t = NULL,
							RegionModels_t* regModels = NULL)
{
	if(!model)
	{
		MsgError("no model %d!!!\n", model_index);
		return;
	}

	// export OBJ with points
	if(debugInfo)
		pStream->Print("#vert count %d\r\n", model->num_vertices);

	
	pStream->Print("g %s_%d\r\n", name_prefix, model_index);
	pStream->Print("o %s_%d\r\n", name_prefix, model_index);


	MODEL* vertex_ref = model;

	if(model->instance_number > 0) // car models have vertex_ref=0
	{
		pStream->Print("#vertex data ref model: %d (count = %d)\r\n", model->instance_number, model->num_vertices);

		vertex_ref = FindModelByIndex(model->instance_number, regModels);

		if(!vertex_ref)
		{
			Msg("vertex ref not found %d\n", model->instance_number);
			return;
		}
	}

	for(int j = 0; j < vertex_ref->num_vertices; j++)
	{
		Vector3D vertexTransformed = Vector3D(vertex_ref->pVertex(j)->x*-MODEL_SCALING, vertex_ref->pVertex(j)->y*-MODEL_SCALING, vertex_ref->pVertex(j)->z*MODEL_SCALING);

		vertexTransformed = (translation * Vector4D(vertexTransformed, 1.0f)).xyz();

		pStream->Print("v %g %g %g\r\n",	vertexTransformed.x,
											vertexTransformed.y,
											vertexTransformed.z);
	}

	int face_ofs = 0;

	if(debugInfo)
	{
		pStream->Print("#poly ofs %d\r\n", model->poly_block);
		pStream->Print("#poly count %d\r\n", model->num_polys);
	}

	pStream->Print("usemtl none\r\n", model->num_vertices);

	int numVertCoords = 0;
	int numVerts = 0;

	if(first_t)
		numVertCoords = *first_t;

	if(first_v)
		numVerts = *first_v;

	bool bSmooth = false;

	for(int j = 0; j < model->num_polys; j++)
	{
		char* facedata = model->pPolyAt(face_ofs);

		dpoly_t dec_face;
		int face_size = decode_poly(facedata, &dec_face);

		//if((dec_face.flags & FACE_SMOOTH) > 0 != bSmooth)
		//{
		//	bSmooth = (dec_face.flags & FACE_SMOOTH) > 0;
		//	pStream->Print("s %s\r\n", bSmooth ? "1" : "off");
		//}

		if(dec_face.flags & FACE_TEXTURED)
		{
			pStream->Print("usemtl page_%d\r\n", dec_face.page);
		}

		if(debugInfo)
			pStream->Print("#ft=%d ofs=%d size=%d\r\n", dec_face.flags,  model->poly_block+face_ofs, face_size);

		if(dec_face.flags & FACE_IS_QUAD)
		{
			// flip
			int vertex_index[4] = {
				dec_face.vindices[3],
				dec_face.vindices[2],
				dec_face.vindices[1],
				dec_face.vindices[0]
			};

			if( dec_face.vindices[0] < 0 || dec_face.vindices[0] > vertex_ref->num_vertices ||
				dec_face.vindices[1] < 0 || dec_face.vindices[1] > vertex_ref->num_vertices ||
				dec_face.vindices[2] < 0 || dec_face.vindices[2] > vertex_ref->num_vertices ||
				dec_face.vindices[3] < 0 || dec_face.vindices[3] > vertex_ref->num_vertices)
			{
				MsgError("quad %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, *facedata & 31, model->poly_block+face_ofs);
				face_ofs += face_size;
				continue;
			}

			if(dec_face.flags & FACE_TEXTURED)
			{
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[0][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[0][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[1][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[1][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[2][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[2][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[3][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[3][1])) / 256.0f+halfTexelSize);

				pStream->Print("f %d/%d %d/%d %d/%d %d/%d\r\n", 
						vertex_index[0]+1+numVerts, numVertCoords+4,
						vertex_index[1]+1+numVerts, numVertCoords+3,
						vertex_index[2]+1+numVerts, numVertCoords+2,
						vertex_index[3]+1+numVerts, numVertCoords+1);

				numVertCoords += 4;
			}
			else
			{
				pStream->Print("f %d %d %d %d\r\n",
						vertex_index[0]+1+numVerts,
						vertex_index[1]+1+numVerts,
						vertex_index[2]+1+numVerts,
						vertex_index[3]+1+numVerts);
			}
		}
		else
		{
			// flip
			int vertex_index[3] = {
				dec_face.vindices[2],
				dec_face.vindices[1],
				dec_face.vindices[0]
			};

			if( dec_face.vindices[0] < 0 || dec_face.vindices[0] > vertex_ref->num_vertices ||
				dec_face.vindices[1] < 0 || dec_face.vindices[1] > vertex_ref->num_vertices ||
				dec_face.vindices[2] < 0 || dec_face.vindices[2] > vertex_ref->num_vertices)
			{
				MsgError("triangle %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, *facedata & 31, model->poly_block+face_ofs);
				face_ofs += face_size;
				continue;
			}

			if(dec_face.flags & FACE_TEXTURED)
			{
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[0][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[0][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[1][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[1][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.uv[2][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.uv[2][1])) / 256.0f+halfTexelSize);

				pStream->Print("f %d/%d %d/%d %d/%d\r\n", 
						vertex_index[0]+1+numVerts, numVertCoords+3,
						vertex_index[1]+1+numVerts, numVertCoords+2,
						vertex_index[2]+1+numVerts, numVertCoords+1);

				numVertCoords += 3;
			}
			else
			{
				pStream->Print("f %d %d %d\r\n", 
						vertex_index[0]+1+numVerts,
						vertex_index[1]+1+numVerts,
						vertex_index[2]+1+numVerts);
			}
						
		}

		face_ofs += face_size;
	}

	if(first_t)
		*first_t = numVertCoords;

	if(first_v)
		*first_v = numVerts+vertex_ref->num_vertices;

	PrintUnknownPolys();
}

//-------------------------------------------------------------
// Saves raw MODEL to file
//-------------------------------------------------------------
void ExtractDMODEL(MODEL* model, const char* model_name, int modelSize)
{
	FILE* mdlFile = fopen(varargs("%s.dmodel", model_name), "wb");

	if(mdlFile)
	{
		CFileStream fstr(mdlFile);

		fwrite(model, 1, modelSize, mdlFile);

		Msg("----------\nSaving model %s\n", model_name);

		// success
		fclose(mdlFile);
	}
}

//-------------------------------------------------------------
// exports model to single file
//-------------------------------------------------------------
void ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, int modelSize)
{
	if (!model)
		return;
	
	if(g_extract_dmodels)
	{
		ExtractDMODEL(model, model_name, modelSize);
		return;
	}
	
	FILE* mdlFile = fopen(varargs("%s.obj", model_name), "wb");

	if(mdlFile)
	{
		CFileStream fstr(mdlFile);

		Msg("----------\nModel %s (%d)\n", model_name, model_index);

		fstr.Print("mtllib MODELPAGES.mtl\r\n");

		WriteMODELToObjStream( &fstr, model, model_index, model_name, true );

		// success
		fclose(mdlFile);
	}
}

//-------------------------------------------------------------
// exports car model. Car models are typical MODEL structures
//-------------------------------------------------------------
void ExportCarModel(MODEL* model, int size, int index, const char* name_suffix)
{
	std::string model_name(varargs("%s/CARMODEL_%d_%s", g_levname_moddir.c_str(), index, name_suffix));

	// export model
	ExportDMODELToOBJ(model, model_name.c_str(), index, size);
}

void SaveTGA(const char* filename, ubyte* data, int w, int h, int c)
{
	// Define targa header.
	#pragma pack(1)
	typedef struct
		{
		int8	identsize;              // Size of ID field that follows header (0)
		int8	colorMapType;           // 0 = None, 1 = paletted
		int8	imageType;              // 0 = none, 1 = indexed, 2 = rgb, 3 = grey, +8=rle
		unsigned short	colorMapStart;          // First colour map entry
		unsigned short	colorMapLength;         // Number of colors
		unsigned char 	colorMapBits;   // bits per palette entry
		unsigned short	xstart;                 // image x origin
		unsigned short	ystart;                 // image y origin
		unsigned short	width;                  // width in pixels
		unsigned short	height;                 // height in pixels
		int8	bits;                   // bits per pixel (8 16, 24, 32)
		int8	descriptor;             // image descriptor
		} TGAHEADER;
	#pragma pack(8)

	TGAHEADER tgaHeader;

	// Initialize the Targa header
    tgaHeader.identsize = 0;
    tgaHeader.colorMapType = 0;
    tgaHeader.imageType = 2;
    tgaHeader.colorMapStart = 0;
    tgaHeader.colorMapLength = 0;
    tgaHeader.colorMapBits = 0;
    tgaHeader.xstart = 0;
    tgaHeader.ystart = 0;
    tgaHeader.width = w;
    tgaHeader.height = h;
    tgaHeader.bits = c*8;
    tgaHeader.descriptor = 0;

	int imageSize = w*h*c;

	FILE* pFile = fopen(filename, "wb");
	if(!pFile)
		return;

    // Write the header
    fwrite(&tgaHeader, sizeof(TGAHEADER), 1, pFile);
    
    // Write the image data
    fwrite(data, imageSize, 1, pFile);
              
    fclose(pFile);
}

void ConvertIndexedTextureToRGBA(int nPage, uint* dest_color_data, int detail, TEXCLUT* clut)
{
	texdata_t* page = &g_pageDatas[nPage];

	if (!(detail < g_texPages[nPage].numDetails))
	{
		MsgError("Cannot apply palette to non-existent detail! Programmer error?\n");
		return;
	}
	
	int ox = g_texPages[nPage].details[detail].x;
	int oy = g_texPages[nPage].details[detail].y;
	int w = g_texPages[nPage].details[detail].width;
	int h = g_texPages[nPage].details[detail].height;

	if (w == 0)
		w = 256;

	if (h == 0)
		h = 256;
	
	char* textureName = g_textureNamesData + g_texPages[nPage].details[detail].nameoffset;
	//MsgWarning("Applying detail %d '%s' (xywh: %d %d %d %d)\n", detail, textureName, ox, oy, w, h);

	int tp_wx = ox + w;
	int tp_hy = oy + h;
	
	for(int y = oy; y < tp_hy; y++)
	{
		for(int x = ox; x < tp_wx; x++)
		{
			ubyte clindex = page->data[y*256 + x];

			TVec4D<ubyte> color = bgr5a1_ToRGBA8( clut->colors[clindex] );

			// flip texture by Y because of TGA
			int ypos = (TEXPAGE_SIZE_Y - y-1)*TEXPAGE_SIZE_Y;

			dest_color_data[ypos + x] = *(uint*)(&color);
		}
	}
}

void ExportTexturePage(int nPage)
{
	//
	// This is where texture is converted from paletted BGR5A1 to BGRA8
	// Also saving to LEVEL_texture/PAGE_*
	//

	texdata_t* page = &g_pageDatas[nPage];

	if(!page->data)
		return;		// NO DATA

	if(g_extract_tpages)
		return;

#define TEX_CHANNELS 4

	int imgSize = TEXPAGE_SIZE * TEX_CHANNELS;

	uint* color_data = (uint*)malloc(imgSize);

	memset(color_data, 0, imgSize);

	int numPal = min(page->numPalettes, g_texPages[nPage].numDetails);

	// Dump whole TPAGE indexes
	for(int y = 0; y < 256; y++)
	{
		for(int x = 0; x < 256; x++)
		{
			ubyte clindex = page->data[y*256 + x];
			int ypos = (TEXPAGE_SIZE_Y - y-1)*TEXPAGE_SIZE_Y;
			
			color_data[ypos + x] = clindex * 32;
		}
	}

	for(int i = 0; i < numPal; i++)
	{
		ConvertIndexedTextureToRGBA(nPage, color_data, i, &page->clut[i]);
	}

	Msg("Writing texture %s/PAGE_%d.tga\n", g_levname_texdir.c_str(), nPage);
	SaveTGA(varargs("%s/PAGE_%d.tga", g_levname_texdir.c_str(), nPage), (ubyte*)color_data, TEXPAGE_SIZE_Y,TEXPAGE_SIZE_Y,TEX_CHANNELS);

	int numPalettes = 0;
	for(int pal = 0; pal < 16; pal++)
	{
		bool anyMatched = false;
		for(int i = 0; i < g_numExtraPalettes; i++)
		{
			if (g_extraPalettes[i].tpage != nPage)
				continue;

			if (g_extraPalettes[i].palette != pal)
				continue;

			for(int j = 0; j < g_extraPalettes[i].texcnt; j++)
			{
				ConvertIndexedTextureToRGBA(nPage, color_data, g_extraPalettes[i].texnum[j], &g_extraPalettes[i].clut);
			}
			
			anyMatched = true;
		}

		if(anyMatched)
		{
			Msg("Writing texture %s/PAGE_%d_%d.tga\n", g_levname_texdir.c_str(), nPage, numPalettes);
			SaveTGA(varargs("%s/PAGE_%d_%d.tga", g_levname_texdir.c_str(), nPage, numPalettes), (ubyte*)color_data, TEXPAGE_SIZE_Y,TEXPAGE_SIZE_Y,TEX_CHANNELS);
			numPalettes++;
		}
	}

	
	free(color_data);
}

void DumpOverlayMap()
{
#if 0
	CVECTOR* rgba = new CVECTOR[overlaidmaps[GameLevel].width * overlaidmaps[GameLevel].height];

	ushort* clut = (ushort*)(MapBitMaps + 512);

	int x, y, xx, yy;
	int wide, tall;

	wide = overlaidmaps[GameLevel].width / 32;
	tall = overlaidmaps[GameLevel].height / 32;

	for(x = 0; x < wide; x++)
	{
		for(y = 0; y < tall; y++)
		{
			int idx = x + y * wide;
			int temp = x << 5;

			if (idx > -1 && idx < overlaidmaps[GameLevel].toptile &&
				temp > -1 && (temp < overlaidmaps[GameLevel].width))
			{
				UnpackRNC(MapBitMaps + *((ushort*)MapBitMaps + idx), MapBuffer);

				// place segment
				for(yy = 0; yy < 32; yy++)
				{
					for(xx = 0; xx < 32; xx++)
					{
						int px, py;

						u_char colorIndex = (u_char)MapBuffer[yy * 16 + xx / 2];

						if(0 != (xx & 1))
							colorIndex >>= 4;

						colorIndex &= 0xf;

						CVECTOR color = bgr5a1_ToRGBA8(clut[colorIndex]);

						px = x * 32 + xx;
						py = y * 32 + yy;

						rgba[px + py * overlaidmaps[GameLevel].width] = color;
					}
				}
			}
		}
	}

	SaveTGA("MAP.TGA", (uchar*)rgba,overlaidmaps[GameLevel].width, overlaidmaps[GameLevel].height, 4 );
	delete [] rgba;
#endif
}

int UnpackCellPointers(int region, int targetRegion, int cell_slots_add, ushort* dest_ptrs, char* src_data)
{
	ushort cell;
	ushort* short_ptr;
	ushort* source_packed_data;
	int loop;
	uint bitpos;
	uint pcode;
	int numCellPointers;
	
	int packtype;

	packtype = *(int *)(src_data + 4);
	source_packed_data = (ushort *)(src_data + 8);

	numCellPointers = 0;

	if (packtype == 0)
	{
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
			*short_ptr++ = 0xffff;
	}
	else if (packtype == 1)
	{
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
		{
			cell = *source_packed_data++;

			if (cell != 0xffff)
				cell += cell_slots_add;

			*short_ptr++ = cell;
			numCellPointers++;
		}
	}
	else if (packtype == 2)
	{
		bitpos = 0x8000;
		pcode = (uint)*source_packed_data;
		source_packed_data++;
		short_ptr = dest_ptrs + targetRegion * 1024;

		for (loop = 0; loop < 1024; loop++)
		{
			if (pcode & bitpos)
			{
				cell = *source_packed_data++;
				cell += cell_slots_add;
			}
			else
				cell = 0xffff;

			bitpos >>= 1;
			*short_ptr++ = cell;
			numCellPointers++;

			if (bitpos == 0)
			{
				bitpos = 0x8000;
				pcode = *source_packed_data++;
			}
		}
	}
	else 
	{
		MsgError("BAD PACKED CELL POINTER DATA, region = %d\n", region);
	}

	return numCellPointers;
}

#define SPOOL_CD_BLOCK_SIZE 2048

char g_packed_cell_pointers[8192];
ushort g_cell_ptrs[8192];

CELL_DATA g_cells[16384];
CELL_DATA_D1 g_cells_d1[16384];

// Driver 2
PACKED_CELL_OBJECT g_packed_cell_objects[16384];

// Driver 1
CELL_OBJECT g_cell_objects[16384];

PACKED_CELL_OBJECT* GetFirstPackedCop(CELL_DATA** cell, ushort cell_ptr, int barrel_region)
{
	*cell = &g_cells[cell_ptr];

	if ((*cell)->num & 0x8000)
		return NULL;

	return &g_packed_cell_objects[((*cell)->num & 0x3fff) - g_cell_objects_add[barrel_region]];
}

PACKED_CELL_OBJECT* GetNextPackedCop(CELL_DATA** cell, int barrel_region)
{
	ushort num;
	PACKED_CELL_OBJECT* ppco;

	do {
		if ((*cell)->num & 0x8000)
			return NULL;

		(*cell) = (*cell) + 1;
		num = (*cell)->num;

		if (num & 0x4000)
			return NULL;

		ppco = &g_packed_cell_objects[((*cell)->num & 0x3fff) - g_cell_objects_add[barrel_region]];
	} while (ppco->value == 0xffff && (ppco->pos.vy & 1) != 0);

	return ppco;
}

void UnpackCellObject(PACKED_CELL_OBJECT* pco, CELL_OBJECT& co, VECTOR_NOPAD nearCell)
{
	//bool isStraddler = pco - cell_objects < g_numStraddlers;
	
	co.pos.vy = (pco->pos.vy << 0x10) >> 0x11;

	//if(isStraddler)
	{
		co.pos.vx = nearCell.vx + ((nearCell.vx - pco->pos.vx << 0x10) >> 0x10);
		co.pos.vz = nearCell.vz + ((nearCell.vz - pco->pos.vz << 0x10) >> 0x10);
	}
	//else
	{
		co.pos.vx = nearCell.vx + pco->pos.vx;// ((pco.pos.vx - nearCell.vx << 0x10) >> 0x10);
		co.pos.vz = nearCell.vz + pco->pos.vz;// ((pco.pos.vz - nearCell.vz << 0x10) >> 0x10);
	}

	// unpack
	co.yang = (pco->value & 0x3f);
	co.type = (pco->value >> 6) + ((pco->pos.vy & 0x1) ? 1024 : 0);
}

void ExportRegions()
{
	int counter = 0;

	int dim_x = g_mapInfo.cells_across / g_mapInfo.region_size;
	int dim_y = g_mapInfo.cells_down / g_mapInfo.region_size;

	Msg("World size:\n [%dx%d] cells\n [%dx%d] regions\n", g_mapInfo.cells_across, g_mapInfo.cells_down, dim_x, dim_y);

	// print region map to console
	for(int i = 0; i < g_numSpoolInfoOffsets; i++, counter++)
	{
		// Debug information: Print the map to the console.
		Msg(g_spoolInfoOffsets[i] == REGION_EMPTY ? "." : "O");
		if(counter == dim_x)
		{
			Msg("\n");
			counter = 0;
		}
	}

	Msg("\n");
	 
	int numCellObjectsRead = 0;

	FILE* cellsFile = fopen(varargs("%s_CELLPOS_MAP.obj", g_levname.c_str()), "wb");
	FILE* levelFile = fopen(varargs("%s_LEVELMODEL.obj", g_levname.c_str()), "wb");

	CFileStream cellsFileStream(cellsFile);
	CFileStream levelFileStream(levelFile);

	levelFileStream.Print("mtllib %s_LEVELMODEL.mtl\r\n", g_levname.c_str());

	int lobj_first_v = 0;
	int lobj_first_t = 0;

	// copy straddlers to cell objects
	if(g_format == 2)
	{
		memcpy(g_packed_cell_objects, g_straddlers, sizeof(PACKED_CELL_OBJECT)*g_numStraddlers);
	}
	else
	{
		memcpy(g_cell_objects, g_straddlers, sizeof(CELL_OBJECT)*g_numStraddlers);
	}
	

	// dump 2d region map
	// easy seeking
	for(int y = 0; y < dim_y; y++)
	{
		for(int x = 0; x < dim_x; x++)
		{
			int sPosIdx = y*dim_x + x;

			if(g_spoolInfoOffsets[sPosIdx] == REGION_EMPTY)
				continue;

			int barrel_region = (x & 1) + (y & 1) * 2;

			// determine region position
			VECTOR_NOPAD regionPos;
			regionPos.vx = (x - dim_x / 2) * g_mapInfo.region_size * g_mapInfo.cell_size;
			regionPos.vz = (y - dim_y / 2) * g_mapInfo.region_size * g_mapInfo.cell_size;
			regionPos.vy = 0;

			// region at offset
			Spool* spool = (Spool*)((ubyte*)g_regionSpool + g_spoolInfoOffsets[sPosIdx]);

			Msg("---------\nSpool %d %d (offset: %d)\n", x, y, g_spoolInfoOffsets[sPosIdx]);
			Msg(" - offset: %d\n",spool->offset);

			for(int i = 0; i < spool->num_connected_areas; i++)
				Msg(" - connected area %d: %d\n", i, spool->connected_areas[i]);

			Msg(" - pvs_size: %d\n",spool->pvs_size);
			Msg(" - cell_data_size: %d %d %d\n",spool->cell_data_size[0], spool->cell_data_size[1], spool->cell_data_size[2]);

			Msg(" - super_region: %d\n",spool->super_region);

			// LoadRegionData - calculate offsets
			Msg(" - cell pointers size: %d\n", spool->cell_data_size[1]);
			Msg(" - cell data size: %d\n", spool->cell_data_size[0]);
			Msg(" - cell objects size: %d\n", spool->cell_data_size[2]);
			Msg(" - PVS data size: %d\n", spool->roadm_size);

			// prepare
			int cellPointerCount = 0;

			memset(g_packed_cell_pointers, 0, sizeof(g_packed_cell_pointers));
			
			memset(g_cells, 0, sizeof(g_cells));
			
			memset(g_cells_d1, 0, sizeof(g_cells_d1));

			if(g_format == 2)	// Driver 2
			{
				//
				// Driver 2 use PACKED_CELL_OBJECTS - 8 bytes. not wasting, but tricky
				//

				memset(g_packed_cell_objects + g_numStraddlers, 0, sizeof(g_packed_cell_objects) - g_numStraddlers * sizeof(PACKED_CELL_OBJECT));
				
				int cellPointersOffset;
				int cellDataOffset;
				int cellObjectsOffset;
				int pvsDataOffset;
				
				if(g_region_format == 3) // retail
				{
					cellPointersOffset = spool->offset; // SKIP PVS buffers, no need rn...
					cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
					cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
					pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
				}
				else if(g_region_format == 2) // 1.6 alpha
				{
					cellPointersOffset = spool->offset + spool->roadm_size;
					cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
					cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
					pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2];
				}
				
				// read packed cell pointers
				g_levStream->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// unpack cell pointers so we can use them
				cellPointerCount = UnpackCellPointers(sPosIdx, 0, 0, g_cell_ptrs, g_packed_cell_pointers);

				// read cell data
				g_levStream->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cells, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// read cell objects
				g_levStream->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_objects + g_numStraddlers, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				
			}
			else if(g_format == 1) // Driver 1
			{
				//
				// Driver 1 use CELL_OBJECTS directly - 16 bytes, wasteful in RAM
				//

				memset(g_cell_objects + g_numStraddlers, 0, sizeof(g_cell_objects) - g_numStraddlers * sizeof(CELL_OBJECT));
				
				int cellPointersOffset = spool->offset + spool->roadm_size + spool->roadh_size; // SKIP road map
				int cellDataOffset = cellPointersOffset + spool->cell_data_size[1];
				int cellObjectsOffset = cellDataOffset + spool->cell_data_size[0];
				int pvsDataOffset = cellObjectsOffset + spool->cell_data_size[2]; // FIXME: is it even there in Driver 1?

				 // read packed cell pointers
				g_levStream->Seek(g_levInfo.spooldata_offset + cellPointersOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_packed_cell_pointers, spool->cell_data_size[1] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// unpack cell pointers so we can use them
				cellPointerCount = UnpackCellPointers(sPosIdx, 0, 0, g_cell_ptrs, g_packed_cell_pointers);

				// read cell data
				g_levStream->Seek(g_levInfo.spooldata_offset + cellDataOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cells_d1, spool->cell_data_size[0] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				// read cell objects
				g_levStream->Seek(g_levInfo.spooldata_offset + cellObjectsOffset * SPOOL_CD_BLOCK_SIZE, VS_SEEK_SET);
				g_levStream->Read(g_cell_objects + g_numStraddlers, spool->cell_data_size[2] * SPOOL_CD_BLOCK_SIZE, sizeof(char));

				
			}

			// read cell objects after we spool additional area data
			RegionModels_t* regModels = NULL;
			
			// if region data is available, load models and textures
			if(spool->super_region != SUPERREGION_NONE)
			{
				regModels = &g_regionModels[spool->super_region];

				bool isNew = (regModels->modelRefs.size() == 0);

				LoadRegionData(g_levStream, regModels, &g_areaData[spool->super_region], &g_regionPages[spool->super_region]);

				if( g_export_models && isNew )
				{
					// export region models
					for(int i = 0; i < regModels->modelRefs.size(); i++)
					{
						int mod_indxex = regModels->modelRefs[i].index;

						std::string modelFileName = varargs("%s/ZREG%d_MOD_%d", g_levname_moddir.c_str(), spool->super_region, mod_indxex);

						if(g_model_names[mod_indxex].length())
							modelFileName = varargs("%s/ZREG%d_%d_%s", g_levname_moddir.c_str(), spool->super_region, i, g_model_names[mod_indxex].c_str());

						// export model
						ExportDMODELToOBJ(regModels->modelRefs[i].model, modelFileName.c_str(), regModels->modelRefs[i].index, regModels->modelRefs[i].size);
					}
				}
			}
			


			int numRegionObjects = 0;

			if(g_format == 2)	// Driver 2
			{
				CELL_DATA* celld;
				PACKED_CELL_OBJECT* pcop;

				// go through all cell pointers
				for(int i = 0; i < cellPointerCount; i++)
				{
					ushort cell_ptr = g_cell_ptrs[i];
					if (cell_ptr == 0xffff)
						continue;

					// iterate just like game would
					if (pcop = GetFirstPackedCop(&celld, cell_ptr, barrel_region))
					{
						do
						{
							// unpack cell object
							CELL_OBJECT co;
							UnpackCellObject(pcop, co, regionPos);

							{
								Vector3D absCellPosition(co.pos.vx*-MODEL_SCALING, co.pos.vy*-MODEL_SCALING, co.pos.vz*MODEL_SCALING);
								float cellRotationRad = co.yang / 64.0f*PI_F*2.0f;

								if(cellsFile)
								{
									cellsFileStream.Print("# m %d r %d\r\n", co.type, co.yang);
									cellsFileStream.Print("v %g %g %g\r\n", absCellPosition.x,absCellPosition.y,absCellPosition.z);
								}

								MODEL* model = FindModelByIndex(co.type, regModels);

								if(model)
								{
									// transform objects and save
									Matrix4x4 transform = translate(absCellPosition);
									transform = transform * rotateY4(cellRotationRad) * scale4(1.0f,1.0f,1.0f);

									const char* modelNamePrefix = varargs("reg%d", sPosIdx);

									WriteMODELToObjStream(&levelFileStream, model, co.type, modelNamePrefix, false, transform, &lobj_first_v, &lobj_first_t, regModels);
								}

								numRegionObjects++;
							}
						} while (pcop = GetNextPackedCop(&celld, barrel_region));
					}

				}
			}
			else // Driver 1
			{
				CELL_DATA_D1* celld;

				// go through all cell pointers
				for(int i = 0; i < cellPointerCount; i++)
				{
					ushort cell_ptr = g_cell_ptrs[i];

					// iterate just like game would
					while (cell_ptr != 0xffff)
					{
						celld = &g_cells_d1[cell_ptr];

						int number = (celld->num & 0x3fff);// -g_cell_objects_add[barrel_region];
						
						{
							// barrel_region doesn't work here
							int val = 0;
							for(int j = 0; j < 4; j++)
							{
								if(number >= g_cell_objects_add[j])
									val = g_cell_objects_add[j];
							}

							number -= val;
						}
						
						CELL_OBJECT& co = g_cell_objects[number];
						
						{
							Vector3D absCellPosition(co.pos.vx*-MODEL_SCALING, co.pos.vy*-MODEL_SCALING, co.pos.vz*MODEL_SCALING);
							float cellRotationRad = co.yang / 64.0f*PI_F*2.0f;

							if(cellsFile)
							{
								cellsFileStream.Print("# m %d r %d\r\n", co.type, co.yang);
								cellsFileStream.Print("v %g %g %g\r\n", absCellPosition.x,absCellPosition.y,absCellPosition.z);
							}

							MODEL* model = FindModelByIndex(co.type, regModels);

							if(model)
							{
								// transform objects and save
								Matrix4x4 transform = translate(absCellPosition);
								transform = transform * rotateY4(cellRotationRad) * scale4(1.0f,1.0f,1.0f);

								const char* modelNamePrefix = varargs("reg%d", sPosIdx);

								WriteMODELToObjStream(&levelFileStream, model, co.type, modelNamePrefix, false, transform, &lobj_first_v, &lobj_first_t, regModels);
							}

							numRegionObjects++;
						}

						
						
						cell_ptr = celld->next_ptr;

						if (cell_ptr != 0xffff)
						{
							// barrel_region doesn't work here
							int val = 0;
							for(int j = 0; j < 4; j++)
							{
								if(cell_ptr >= g_cell_slots_add[j])
									val = g_cell_slots_add[j];
							}
							
							cell_ptr -= val;
						}
					}
				}
			}

			if(cellsFile)
			{
				cellsFileStream.Print("# total region cells %d\r\n", numRegionObjects);
			}

			numCellObjectsRead += numRegionObjects;
		}
	}

	fclose(cellsFile);
	fclose(levelFile);

	int numCellsObjectsFile = g_mapInfo.num_cell_objects;

	if(numCellObjectsRead != numCellsObjectsFile)
		MsgError("numAllObjects mismatch: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);
}

//-------------------------------------------------------------
// Exports level data
//-------------------------------------------------------------

void ExportLevelData()
{
	Msg("-------------\nExporting level data\n-------------\n");

	// export models
	if(g_export_models)
	{
		Msg("exporting models\n");

		for(int i = 0; i < 1536; i++)
		{
			if(!g_levelModels[i].model)
				continue;

			std::string modelFileName(varargs("%s/ZMOD_%d", g_levname_moddir.c_str(), i));

			if(g_model_names[i].size())
				modelFileName = varargs("%s/%d_%s", g_levname_moddir.c_str(), i, g_model_names[i].c_str());

			// export model
			ExportDMODELToOBJ(g_levelModels[i].model, modelFileName.c_str(), i, g_levelModels[i].size);
			
			// save original dmodel2
			FILE* dFile = fopen(varargs("%s.dmodel", modelFileName.c_str()), "wb");
			if(dFile)
			{
				fwrite(g_levelModels[i].model, g_levelModels[i].size, 1, dFile);
				fclose(dFile);
			}
		}

		// create material file
		FILE* pMtlFile = fopen(varargs("%s/MODELPAGES.mtl", g_levname_moddir.c_str()), "wb");

		if(pMtlFile)
		{
			for(int i = 0; i < g_numTexPages; i++)
			{
				fprintf(pMtlFile, "newmtl page_%d\r\n", i);
				fprintf(pMtlFile, "map_Kd ../../%s/PAGE_%d.tga\r\n", g_levname_texdir.c_str(), i);
			}

			fclose(pMtlFile);
		}
	}

	// export car models
	if(g_export_carmodels)
	{
		for(int i = 0; i < MAX_CAR_MODELS; i++)
		{
			ExportCarModel(g_carModels[i].cleanmodel, g_carModels[i].cleanSize, i, "clean");
			ExportCarModel(g_carModels[i].dammodel, g_carModels[i].cleanSize, i, "damaged");
			ExportCarModel(g_carModels[i].lowmodel, g_carModels[i].lowSize, i, "low");

			// TODO: export damaged model
		}
	}

	// export world region
	if(g_export_world)
	{
		Msg("exporting cell points and world model\n");
		ExportRegions();
	}

	if(g_export_textures)
	{
		// preload region data if needed
		if(!g_export_world)
		{
			Msg("preloading region datas (%d)\n", g_numAreas);

			for(int i = 0; i < g_numAreas; i++)
			{
				LoadRegionData( g_levStream, NULL, &g_areaData[i], &g_regionPages[i] );
			}
		}

		Msg("exporting texture data\n");
		for(int i = 0; i < g_numTexPages; i++)
		{
			ExportTexturePage(i);
		}

		// create material file
		FILE* pMtlFile = fopen(varargs("%s_LEVELMODEL.mtl", g_levname.c_str()), "wb");

		std::string justLevFilename = g_levname.substr(g_levname.find_last_of("/\\") + 1);

		size_t lastindex = justLevFilename.find_last_of("."); 
		justLevFilename = justLevFilename.substr(0, lastindex); 

		if(pMtlFile)
		{
			for(int i = 0; i < g_numTexPages; i++)
			{
				
				fprintf(pMtlFile, "newmtl page_%d\r\n", i);
				fprintf(pMtlFile, "map_Kd %s_textures/PAGE_%d.tga\r\n", justLevFilename.c_str(), i);
			}

			fclose(pMtlFile);
		}

	}

	Msg("Export done\n");
}

//-------------------------------------------------------------------------------------------------------------------------

void Usage()
{
	MsgWarning("USAGE:\n	driver_level +lev <input atl file> <output image file>\n");
	MsgWarning("	-format <n> : level file format. 1 - driver1 level, 2 - driver2 level\n");
	MsgWarning("	-regformat <n> : level region format. 1 - driver1 (unsupported), 2 - driver2 demo, 3 - driver2 release\n");
	MsgWarning("	-textures <1/0> : export textures\n");
	MsgWarning("	-models <1/0> : export models\n");
	MsgWarning("	-carmodels <1/0> : export car models\n");
	MsgWarning("	-world <1/0> : export whole world\n");
}


void ProcessLevFile(const char* filename)
{
	FILE* levTest = fopen(filename, "rb");

	if(!levTest)
	{
		MsgError("LEV file '%s' does not exists!\n", filename);
		return;
	}

	fseek(levTest, 0, SEEK_END);
	g_levSize = ftell(levTest);
	fclose(levTest);

	g_levname = filename;

	size_t lastindex = g_levname.find_last_of("."); 
	g_levname = g_levname.substr(0, lastindex); 

	g_levname_moddir = g_levname + "_models";
	g_levname_texdir = g_levname + "_textures";

	_mkdir(g_levname_moddir.c_str());
	_mkdir(g_levname_texdir.c_str());

	LoadLevelFile( filename );
	ExportLevelData();

	FreeLevelData();
}

void ConvertDModelFileToOBJ(const char* filename, const char* outputFilename)
{
	MODEL* model;
	int modelSize;

	FILE* fp;
	fp = fopen(filename, "rb");
	if(fp)
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

	if(model->instance_number > 0)
	{
		MsgError("This model points to instance '%d' and cannot be exported!\n", model->instance_number);
		return;
	}

	ExportDMODELToOBJ(model, outputFilename, 0, modelSize);
}

int main(int argc, char* argv[])
{
	Install_ConsoleSpewFunction();

	Msg("---------------\ndriver_level - Driver 2 level loader\n---------------\n\n");

	if(argc == 0)
	{
		Usage();
		return 0;
	}

	std::string levName;

	for(int i = 0; i < argc; i++)
	{
		if(!stricmp(argv[i], "-format"))
		{
			g_format = atoi(argv[i+1]);

			if(g_format == 1)
			{
				g_region_format = 1;
			}
			else
			{
				if(g_region_format == 1)
					g_region_format = 3;
			}
			i++;
		}
		else if(!stricmp(argv[i], "-regformat"))
		{
			g_region_format = atoi(argv[i+1]);
			i++;
		}
		else if(!stricmp(argv[i], "-textures"))
		{
			g_export_textures = atoi(argv[i+1]) > 0;
			i++;
		}
		else if(!stricmp(argv[i], "-world"))
		{
			g_export_world = atoi(argv[i+1]) > 0;
			i++;
		}
		else if(!stricmp(argv[i], "-models"))
		{
			g_export_models = atoi(argv[i+1]) > 0;
			i++;
		}
		else if(!stricmp(argv[i], "-carmodels"))
		{
			g_export_carmodels = atoi(argv[i+1]) > 0;
			i++;
		}
		else if(!stricmp(argv[i], "-extractmodels"))
		{
			g_extract_dmodels = true;
		}
		else if(!stricmp(argv[i], "-extractpages"))
		{
			g_extract_tpages = true;
		}
		else if(!stricmp(argv[i], "-lev"))
		{
			levName = argv[i+1];
			i++;
		}
		else if(!stricmp(argv[i], "-dmodel2obj"))
		{
			ConvertDModelFileToOBJ(argv[i+1], argv[i+2]);
			return 0;
		}
	}

	if(levName.length() == 0)
	{
		MsgError("Empty filename!\nDid you forgot -lev key?\n");
		Usage();
		return 0;
	}

	ProcessLevFile( levName.c_str() );

	return 0;
}

