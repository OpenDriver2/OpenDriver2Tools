#include "core_base_header.h"
#include "DebugInterface.h"
#include "VirtualStream.h"
#include "cmdlib.h"

#include "driver_level.h"

ConVar g_export_carmodels("carmodels", "0");
ConVar g_export_models("models", "0");
ConVar g_export_world("world", "0");
ConVar g_export_textures("textures", "0");

#include "ImageLoader.h"

//---------------------------------------------------------------------------------------------------------------------------------

EqString			g_levname_moddir;
EqString			g_levname_texdir;

int					g_levSize = 0;

const float			texelSize = 1.0f/256.0f;
const float			halfTexelSize = texelSize*0.5f;

//-------------------------------------------------------------
// writes Wavefront OBJ into stream
//-------------------------------------------------------------
void WriteMODELToObjStream(	IVirtualStream* pStream, MODEL* model, int model_index, 
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
		pStream->Print("#vert count %d\r\n", model->numverts);

	
	pStream->Print("g lev_model_%d\r\n", model_index);
	pStream->Print("o lev_model_%d\r\n", model_index);


	MODEL* vertex_ref = model;

	if(model->vertex_ref > 0) // car models have vertex_ref=0
	{
		pStream->Print("#vertex data ref model: %d (count = %d)\r\n", model->vertex_ref, model->numverts);

		vertex_ref = FindModelByIndex(model->vertex_ref, regModels);

		if(!vertex_ref)
		{
			Msg("vertex ref not found %d\n", model->vertex_ref);
			return;
		}
	}

	for(int j = 0; j < vertex_ref->numverts; j++)
	{
		Vector3D vertexTransformed = Vector3D(vertex_ref->pVertex(j)->x*-0.00015f, vertex_ref->pVertex(j)->y*-0.00015f, vertex_ref->pVertex(j)->z*0.00015f);

		vertexTransformed = (translation * Vector4D(vertexTransformed, 1.0f)).xyz();

		pStream->Print("v %g %g %g\r\n",	vertexTransformed.x,
											vertexTransformed.y,
											vertexTransformed.z);
	}

	int face_ofs = 0;

	if(debugInfo)
	{
		pStream->Print("#face ofs %d\r\n", model->faceOffset);
		pStream->Print("#face count %d\r\n", model->numfaces);
	}

	pStream->Print("usemtl none\r\n", model->numverts);

	int numVertCoords = 0;
	int numVerts = 0;

	if(first_t)
		numVertCoords = *first_t;

	if(first_v)
		numVerts = *first_v;

	for(int j = 0; j < model->numfaces; j++)
	{
		char* facedata = model->pFaceAt(face_ofs);

		dface_t dec_face;
		int face_size = decode_face(facedata, &dec_face);

		if((dec_face.flags & FACE_TEXTURED) || (dec_face.flags & FACE_TEXTURED2))
		{
			pStream->Print("usemtl page_%d\r\n", dec_face.page);
		}

		if(debugInfo)
			pStream->Print("#ft=%d ofs=%d size=%d\r\n", dec_face.flags,  model->faceOffset+face_ofs, face_size);

		if(dec_face.flags & FACE_IS_QUAD)
		{
			// flip
			int vertex_index[4] = {
				dec_face.vindex[3],
				dec_face.vindex[2],
				dec_face.vindex[1],
				dec_face.vindex[0]
			};

			if( dec_face.vindex[0] < 0 || dec_face.vindex[0] > vertex_ref->numverts ||
				dec_face.vindex[1] < 0 || dec_face.vindex[1] > vertex_ref->numverts ||
				dec_face.vindex[2] < 0 || dec_face.vindex[2] > vertex_ref->numverts ||
				dec_face.vindex[3] < 0 || dec_face.vindex[3] > vertex_ref->numverts)
			{
				MsgError("quad %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, dec_face.flags, model->faceOffset+face_ofs);
				face_ofs += face_size;
				continue;
			}

			if((dec_face.flags & FACE_TEXTURED) || (dec_face.flags & FACE_TEXTURED2))
			{
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[0][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[0][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[1][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[1][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[2][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[2][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[3][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[3][1])) / 256.0f+halfTexelSize);

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
				dec_face.vindex[2],
				dec_face.vindex[1],
				dec_face.vindex[0]
			};

			if( dec_face.vindex[0] < 0 || dec_face.vindex[0] > vertex_ref->numverts ||
				dec_face.vindex[1] < 0 || dec_face.vindex[1] > vertex_ref->numverts ||
				dec_face.vindex[2] < 0 || dec_face.vindex[2] > vertex_ref->numverts)
			{
				MsgError("triangle %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, dec_face.flags, model->faceOffset+face_ofs);
				face_ofs += face_size;
				continue;
			}

			if((dec_face.flags & FACE_TEXTURED) || (dec_face.flags & FACE_TEXTURED2))
			{
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[0][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[0][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[1][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[1][1])) / 256.0f+halfTexelSize);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[2][0]) / 256.0f+halfTexelSize, (255.0f-float(dec_face.texcoord[2][1])) / 256.0f+halfTexelSize);

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
		*first_v = numVerts+vertex_ref->numverts;
}

//-------------------------------------------------------------
// exports model to single file
//-------------------------------------------------------------
void ExportDMODELToOBJ(MODEL* model, const char* model_name, int model_index, bool isCarModel = false)
{
	IFile* mdlFile = GetFileSystem()->Open(varargs("%s.obj", model_name), "wb", SP_ROOT);

	if(mdlFile)
	{
		Msg("----------\nModel %s (%d)\n", model_name, model_index);

		mdlFile->Print("mtllib MODELPAGES.mtl\r\n");

		WriteMODELToObjStream( mdlFile, model, model_index, true );

		// success
		GetFileSystem()->Close(mdlFile);
	}
}

//-------------------------------------------------------------
// exports car model. Car models are typical MODEL structures
//-------------------------------------------------------------
void ExportCarModel(MODEL* model, int size, int index, const char* name_suffix)
{
	EqString model_name(varargs("%s/CARMODEL_%d_%s", g_levname_moddir.c_str(), index, name_suffix));

	// export model
	ExportDMODELToOBJ(model, model_name.c_str(), index, true);
			
	// save original dmodel2
	IFile* dFile = GetFileSystem()->Open(varargs("%s.dmodel",  model_name.c_str()), "wb", SP_ROOT);
	if(dFile)
	{
		dFile->Write(model, size, 1);
		GetFileSystem()->Close(dFile);
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

	CImage img;
	uint* color_data = (uint*)img.Create(FORMAT_RGBA8, 256,256,1,1);

	memset(color_data, 0, img.GetSliceSize());

	int clut = max(page->numPalettes, g_texPages[nPage].numDetails);

	for(int i = 0; i < g_texPages[nPage].numDetails; i++, clut++)
	{
		int ox = g_texPages[nPage].details[i].x;
		int oy = g_texPages[nPage].details[i].y;
		int w = g_texPages[nPage].details[i].w;
		int h = g_texPages[nPage].details[i].h;

		char* name = g_textureNamesData + g_texPages[nPage].details[i].texNameOffset;
		
		/*
		Msg("Texture detail %d (%s) [%d %d %d %d]\n", i,name,
														g_texPages[nPage].details[i].x,
														g_texPages[nPage].details[i].y,
														g_texPages[nPage].details[i].w,
														g_texPages[nPage].details[i].h);
		*/
		for(int y = oy; y < oy+h; y++)
		{
			for(int x = ox; x < ox+w; x++)
			{
				ubyte clindex = page->data[y*256 + x];

				TVec4D<ubyte> color = bgr5a1_ToRGBA8( page->clut[i].colors[clindex] );

				color_data[y*256 + x] = *(uint*)(&color);
			}
		}
	}

	Msg("Writing texture %s/PAGE_%d.tga\n", g_levname_texdir.c_str(), nPage);

	img.SaveTGA(varargs("%s/PAGE_%d.tga", g_levname_texdir.c_str(), nPage));
}

void ExportRegions()
{
	int counter = 0;
	
	int dim_x = g_mapInfo.width / g_mapInfo.visTableWidth;
	int dim_y = g_mapInfo.height / g_mapInfo.visTableWidth;

	Msg("World size:\n [%dx%d] vis cells\n [%dx%d] regions\n", g_mapInfo.width, g_mapInfo.height, dim_x, dim_y);

	// print region map to console
	for(int i = 0; i < g_numRegionOffsets; i++, counter++)
	{
		// Debug information: Print the map to the console.
		Msg(g_regionOffsets[i] == REGION_EMPTY ? "." : "O");
		if(counter == dim_x)
		{
			Msg("\n");
			counter = 0;
		}
	}

	Msg("\n");
	 
	int numCellObjectsRead = 0;

	IFile* objFile = NULL;
	IFile* levModelFile = NULL;

	objFile = GetFileSystem()->Open(varargs("%s_CELLPOS_MAP.obj", g_levname.c_str()), "wb", SP_ROOT);
	levModelFile = GetFileSystem()->Open(varargs("%s_LEVELMODEL.obj", g_levname.c_str()), "wb", SP_ROOT);

	levModelFile->Print("mtllib %s_LEVELMODEL.mtl\r\n", g_levname.c_str());

	int lobj_first_v = 0;
	int lobj_first_t = 0;

	int nSModels = 0;

	// 2d region map
	// easy seeking
	for(int y = 0; y < dim_y; y++)
	{
		for(int x = 0; x < dim_x; x++)
		{
			int sPosIdx = y*dim_x + x;

			if(g_regionOffsets[sPosIdx] == REGION_EMPTY)
				continue;

			// determine region position
			int region_pos_x = x * 65536;
			int region_pos_z = y * 65536;

			// region at offset
			REGIONINFO* regInfo = (REGIONINFO*)((ubyte*)g_regionInfos + g_regionOffsets[sPosIdx]);

			Msg("---------\nregion %d %d (offset: %d)\n", x, y, g_regionOffsets[sPosIdx]);
			Msg("offset: %d\n",regInfo->offset);
			Msg("unk1: %d\n",regInfo->unk1);
			Msg("unk2: %d\n",regInfo->unk2);
			Msg("unk3: %d\n",regInfo->unk3);
			Msg("unk4: %d\n",regInfo->unk4);
			Msg("unk5: %d\n",regInfo->unk5);

			nSModels += regInfo->unk4;

			// Msg("cells: %d\n",regInfo->cellsSize);
			Msg("nodes: %d\n",regInfo->contentsNodesSize);
			Msg("tables: %d\n",regInfo->contentsTableSize);
			Msg("models: %d\n",regInfo->modelsSize);

			nSModels += regInfo->modelsSize;

			Msg("regDataIndex: %d\n",regInfo->dataIndex);

			int visDataOffset = g_levInfo.spooldata_offset + 2048 * regInfo->offset;
			int cellsOffset = g_levInfo.spooldata_offset + 2048 * (regInfo->offset + regInfo->contentsNodesSize + regInfo->contentsTableSize);
			int heightDataOffset = g_levInfo.spooldata_offset + 2048 * (regInfo->offset + regInfo->contentsNodesSize + regInfo->contentsTableSize + regInfo->modelsSize);
			
			// to read
			int numVisBytes;
			int numHeightFieldPoints;

			g_levStream->Seek(visDataOffset, VS_SEEK_SET);
			g_levStream->Read(&numVisBytes, 1, sizeof(int));
			// looks like visdata offsets

			// TODO: Read and decode vis bytes, possible model count found here or computed by cell visibility

			g_levStream->Seek(heightDataOffset, VS_SEEK_SET);
			g_levStream->Read(&numHeightFieldPoints, 1, sizeof(int));
			// also depends on visibility

			Msg("numVisBytes = %d\n", numVisBytes);
			Msg("numHeightFieldPoints = %d\n", numHeightFieldPoints);

			Msg("visdata ofs: %d\n", visDataOffset);
			Msg("cell objects ofs: %d\n", cellsOffset);
			Msg("heightdata ofs: %d\n", heightDataOffset);

			RegionModels_t* regModels = NULL;

			// if region data is available, load models and textures
			if(regInfo->dataIndex != REGDATA_EMPTY)
			{
				LoadRegionData(g_levStream, &g_regionModels[regInfo->dataIndex], &g_regionDataDescs[regInfo->dataIndex], &g_regionPages[regInfo->dataIndex]);
				regModels = &g_regionModels[regInfo->dataIndex];
			}

			// seek to cells
			g_levStream->Seek(cellsOffset, VS_SEEK_SET);

			int cell_count = 0;

			objFile->Print("#--------\r\n# region %d %d (ofs=%d)\r\n", x, y, cellsOffset);
			objFile->Print("g cell_%d\r\n", sPosIdx);

			PACKED_CELL_OBJECT object;
			do
			{
				g_levStream->Read(&object, 1, sizeof(PACKED_CELL_OBJECT));
				
				// bad hack, need to parse visibility data for it
				if(	(0x4544 == object.pos_x && 0x2153 == object.addidx_pos_y) ||
					(0x4809 == object.pos_x && 0x4809 == object.addidx_pos_y) ||
					(0xA608 == object.pos_x && 0xA608 == object.addidx_pos_y) ||cell_count >= 8192)
					break;
				
				// unpack cell
				CELL_OBJECT cell;
				UnpackCellObject(object, cell);

				if(objFile)
				{
					objFile->Print("# m %d r %d\r\n", cell.modelindex, cell.rotation);
					objFile->Print("v %g %g %g\r\n", (region_pos_x+cell.x)*-0.00015f, cell.y*-0.00015f, (region_pos_z+cell.z)*0.00015f);
				}

				if(cell.modelindex >= g_levelModels.numElem())
				{
					cell_count++;
					continue;
				}

				MODEL* model = FindModelByIndex(cell.modelindex, regModels);

				if(!model)
					break;

				// transform objects and save
				Matrix4x4 transform = translate(Vector3D((region_pos_x+cell.x)*-0.00015f, cell.y*-0.00015f, (region_pos_z+cell.z)*0.00015f));
				transform = transform * rotateY(cell.rotation / 64.0f*PI_F*2.0f) * scale(1.0f,1.0f,1.0f);

				WriteMODELToObjStream(levModelFile, model, cell.modelindex, false, transform, &lobj_first_v, &lobj_first_t, regModels);
				
				cell_count++;
			}while(true);

			MsgInfo("cell_count = %d\n", cell_count);

			if(objFile)
			{
				objFile->Print("# total region cells %d\r\n", cell_count);
			}

			numCellObjectsRead += cell_count;
		}
	}

	GetFileSystem()->Close(objFile);
	GetFileSystem()->Close(levModelFile);

	int numCellsObjectsFile = g_mapInfo.numAllObjects - g_mapInfo.numStraddlers;

	if(numCellObjectsRead != numCellsObjectsFile)
		MsgError("numAllObjects mismath: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);

	Msg("nSModels = %d\n", nSModels);
}

//-------------------------------------------------------------
// Exports level data
//-------------------------------------------------------------

void ExportLevelData()
{
	Msg("-------------\nExporting level data\n-------------\n");

	// export models
	if(g_export_models.GetBool())
	{
		Msg("exporting models\n");

		for(int i = 0; i < g_levelModels.numElem(); i++)
		{
			if(!g_levelModels[i].model)
				continue;

			EqString modelFileName = varargs("%s/ZMOD_%d", g_levname_moddir.c_str(), i);

			if(g_model_names[i].GetLength())
				modelFileName = varargs("%s/%d_%s", g_levname_moddir.c_str(), i, g_model_names[i]);

			// export model
			ExportDMODELToOBJ(g_levelModels[i].model, modelFileName.c_str(), i);
			
			// save original dmodel2
			IFile* dFile = GetFileSystem()->Open(varargs("%s.dmodel", modelFileName.c_str()), "wb", SP_ROOT);
			if(dFile)
			{
				dFile->Write(g_levelModels[i].model, g_levelModels[i].size, 1);
				GetFileSystem()->Close(dFile);
			}
		}

		// create material file
		IFile* pMtlFile = GetFileSystem()->Open(varargs("%s/MODELPAGES.mtl", g_levname_moddir.c_str()), "wb", SP_ROOT);

		if(pMtlFile)
		{
			for(int i = 0; i < g_numTexPages; i++)
			{
				pMtlFile->Print("newmtl page_%d\r\n", i);
				pMtlFile->Print("map_Kd ../../%s/PAGE_%d.tga\r\n", g_levname_texdir.c_str(), i);
			}

			GetFileSystem()->Close(pMtlFile);
		}
	}

	// export car models
	if(g_export_carmodels.GetBool())
	{
		for(int i = 0; i < MAX_CAR_MODELS; i++)
		{
			ExportCarModel(g_carModels[i].cleanmodel, g_carModels[i].cleanSize, i, "clean");
			ExportCarModel(g_carModels[i].lowmodel, g_carModels[i].lowSize, i, "low");
			// TODO: export damaged model
		}
	}

	// export world regions
	if(g_export_world.GetBool())
	{
		Msg("exporting cell points and world model\n");
		ExportRegions();
	}

	if(g_export_textures.GetBool())
	{
		// preload region data if needed
		if(!g_export_world.GetBool())
		{
			Msg("preloading region datas (%d)\n", g_numRegionDatas);

			for(int i = 0; i < g_numRegionDatas; i++)
			{
				LoadRegionData( g_levStream, NULL, &g_regionDataDescs[i], &g_regionPages[i] );
			}
		}

		Msg("exporting texture data\n");
		for(int i = 0; i < g_numTexPages; i++)
		{
			ExportTexturePage(i);
		}

		// create material file
		IFile* pMtlFile = GetFileSystem()->Open(varargs("%s_LEVELMODEL.mtl", g_levname.c_str()), "wb", SP_ROOT);

		if(pMtlFile)
		{
			for(int i = 0; i < g_numTexPages; i++)
			{
				pMtlFile->Print("newmtl page_%d\r\n", i);
				pMtlFile->Print("map_Kd %s_textures/PAGE_%d.tga\r\n", g_levname.c_str(), i);
			}

			GetFileSystem()->Close(pMtlFile);
		}

	}

	Msg("Export done\n");

	// create material file
	IFile* pLevFile = GetFileSystem()->Open(varargs("%s.lev", g_levname.c_str()), "wb");

	if(pLevFile)
	{
		g_levStream->Seek(g_levSize, VS_SEEK_SET);

		((CMemoryStream*)g_levStream)->WriteToFileStream(pLevFile);
		GetFileSystem()->Close(pLevFile);
	}
}

//-------------------------------------------------------------------------------------------------------------------------

void Usage()
{
	MsgWarning("USAGE:\n	driver_level +lev <input atl file> <output image file>\n");
}

DECLARE_CMD(lev, "Loads level, extracts model", 0)
{
	if(args && args->numElem())
	{
		EqString levName = CMD_ARGV(0);

		g_levname_moddir = levName.Path_Strip_Ext() + "_models";
		g_levname_texdir = levName.Path_Strip_Ext() + "_textures";

		GetFileSystem()->MakeDir(g_levname_moddir.c_str(), SP_ROOT);
		GetFileSystem()->MakeDir(g_levname_texdir.c_str(), SP_ROOT);

		g_levSize = GetFileSystem()->GetFileSize(levName.c_str());

		LoadLevelFile( levName.c_str() );
		ExportLevelData();

		FreeLevelData();
	}
	else
		Usage();
}

int main(int argc, char* argv[])
{
	GetCore()->Init("driver_level", argc, argv);

	Install_SpewFunction();

	if(!GetFileSystem()->Init(false))
		return -1;

	Msg("driver_level - Driver 2 level loader\n");

	if(GetCmdLine()->GetArgumentCount() == 0)
		Usage();

	GetCmdLine()->ExecuteCommandLine( true, true );

	GetCore()->Shutdown();

	return 0;
}

