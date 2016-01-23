#include "core_base_header.h"
#include "DebugInterface.h"
#include "VirtualStream.h"
#include "cmdlib.h"

#include "driver_level.h"

ConVar g_export_carmodels("carmodels", "0");
ConVar g_export_models("models", "0");
ConVar g_export_world("world", "0");
ConVar g_export_textures("textures", "0");

extern ConVar g_region_format;

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
		pStream->Print("#vert count %d\r\n", model->num_vertices);

	
	pStream->Print("g lev_model_%d\r\n", model_index);
	pStream->Print("o lev_model_%d\r\n", model_index);


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
		Vector3D vertexTransformed = Vector3D(vertex_ref->pVertex(j)->x*-0.00015f, vertex_ref->pVertex(j)->y*-0.00015f, vertex_ref->pVertex(j)->z*0.00015f);

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

		dface_t dec_face;
		int face_size = decode_poly(facedata, &dec_face);

		//if((dec_face.flags & FACE_SMOOTH) > 0 != bSmooth)
		//{
		//	bSmooth = (dec_face.flags & FACE_SMOOTH) > 0;
		//	pStream->Print("s %s\r\n", bSmooth ? "1" : "off");
		//}

		if((dec_face.flags & FACE_TEXTURED) || (dec_face.flags & FACE_TEXTURED2))
		{
			pStream->Print("usemtl page_%d\r\n", dec_face.page);
		}

		if(debugInfo)
			pStream->Print("#ft=%d ofs=%d size=%d\r\n", dec_face.flags,  model->poly_block+face_ofs, face_size);

		if(dec_face.flags & FACE_IS_QUAD)
		{
			// flip
			int vertex_index[4] = {
				dec_face.vindex[3],
				dec_face.vindex[2],
				dec_face.vindex[1],
				dec_face.vindex[0]
			};

			if( dec_face.vindex[0] < 0 || dec_face.vindex[0] > vertex_ref->num_vertices ||
				dec_face.vindex[1] < 0 || dec_face.vindex[1] > vertex_ref->num_vertices ||
				dec_face.vindex[2] < 0 || dec_face.vindex[2] > vertex_ref->num_vertices ||
				dec_face.vindex[3] < 0 || dec_face.vindex[3] > vertex_ref->num_vertices)
			{
				MsgError("quad %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, dec_face.flags, model->poly_block+face_ofs);
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

			if( dec_face.vindex[0] < 0 || dec_face.vindex[0] > vertex_ref->num_vertices ||
				dec_face.vindex[1] < 0 || dec_face.vindex[1] > vertex_ref->num_vertices ||
				dec_face.vindex[2] < 0 || dec_face.vindex[2] > vertex_ref->num_vertices)
			{
				MsgError("triangle %d (type=%d ofs=%d) has invalid indices (or format is unknown)\n", j, dec_face.flags, model->poly_block+face_ofs);
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
		*first_v = numVerts+vertex_ref->num_vertices;
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
			Spool* spool = (Spool*)((ubyte*)g_regionSpool + g_regionOffsets[sPosIdx]);

			Msg("---------\nSpool %d %d (offset: %d)\n", x, y, g_regionOffsets[sPosIdx]);
			Msg("offset: %d\n",spool->offset);

			Msg("connected areas offset: %d\n",spool->connected_areas);
			Msg("connected areas count: %d\n",spool->num_connected_areas);
			Msg("pvs block size: %d\n",spool->pvs_size);
			//Msg("padding: %d %d\n",spool->_padding,spool->_padding2);
			Msg("cell data size: %d\n",spool->cell_data_size);
			Msg("super regions: %d\n",spool->super_region);
			Msg("roadm size: %d\n",spool->roadm_size);
			Msg("roadh size: %d\n",spool->roadh_size);

			int cellsOffset = g_levInfo.spooldata_offset + 2048 * (spool->offset + spool->pvs_size + spool->cell_data_size);
			
			if(g_region_format.GetInt() == 2) // driver 2 demo
			{
				cellsOffset = g_levInfo.spooldata_offset + 2048 * (spool->offset + spool->pvs_size + spool->cell_data_size + spool->roadm_size );
			}
			else if(g_region_format.GetInt() == 1) // driver 1
			{
				ASSERT(!"diferrent packed cells used?\n");
			}

			Msg("cell objects ofs: %d\n", cellsOffset);

			RegionModels_t* regModels = NULL;

			// if region data is available, load models and textures
			if(spool->super_region != SUPERREGION_NONE)
			{
				regModels = &g_regionModels[spool->super_region];

				bool isNew = (regModels->modelRefs.numElem() == 0);

				LoadRegionData(g_levStream, regModels, &g_regionDataDescs[spool->super_region], &g_regionPages[spool->super_region]);

				if( g_export_models.GetBool() && isNew )
				{
					// export region models
					for(int i = 0; i < regModels->modelRefs.numElem(); i++)
					{
						int mod_indxex = regModels->modelRefs[i].index;

						EqString modelFileName = varargs("%s/ZREG%d_MOD_%d", g_levname_moddir.c_str(), spool->super_region, mod_indxex);

						if(g_model_names[mod_indxex].GetLength())
							modelFileName = varargs("%s/ZREG%d_%d_%s", g_levname_moddir.c_str(), spool->super_region, i, g_model_names[mod_indxex]);

						// export model
						ExportDMODELToOBJ(regModels->modelRefs[i].model, modelFileName.c_str(), regModels->modelRefs[i].index);
					}
				}
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
					Msg("cell model index invalid: %d\n", cell.modelindex);

					cell_count++;
					continue;
				}

				MODEL* model = FindModelByIndex(cell.modelindex, regModels);

				if(!model)
					break;

				// transform objects and save
				Matrix4x4 transform = translate(Vector3D((region_pos_x+cell.x)*-0.00015f, cell.y*-0.00015f, (region_pos_z+cell.z)*0.00015f));
				transform = transform * rotateY4(cell.rotation / 64.0f*PI_F*2.0f) * scale4(1.0f,1.0f,1.0f);

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
		MsgError("numAllObjects mismatch: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);
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

	// export world region
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
				pMtlFile->Print("map_Kd %s_textures/PAGE_%d.tga\r\n", g_levname.Path_Extract_Name().c_str(), i);
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

	Msg("---------------\ndriver_level - Driver 2 level loader\n---------------\n\n");

	if(GetCmdLine()->GetArgumentCount() == 0)
		Usage();

	GetCmdLine()->ExecuteCommandLine( true, true );

	GetCore()->Shutdown();

	return 0;
}

