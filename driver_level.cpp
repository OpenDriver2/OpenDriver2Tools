#include "core_base_header.h"
#include "DebugInterface.h"
#include "VirtualStream.h"
#include "cmdlib.h"

#include "driver_level.h"

ConVar g_export_carmodels("carmodels", "0");
ConVar g_export_models("models", "0");
ConVar g_export_world("world", "0");

ConVar g_format("format", "2");

//---------------------------------------------------------------------------------------------------------------------------------

LEVELINFO			g_levInfo;
MAPINFO				g_mapInfo;

TEXPAGEINFO*		g_texPageInfos = NULL;

int					g_numTexPages = 0;
int					g_numTexDetails = 0;

texdata_t*			g_pageDatas = NULL;
TexPage_t*			g_texPages = NULL;

DkList<ModelRef_t>	g_models;

DkList<EqString>	g_model_names;
DkList<EqString>	g_texture_names;
EqString			g_levname_moddir;
EqString			g_levname_texdir;

char*				g_textureNamesData = NULL;

//-------------------------------------------------------------
// load model names
//-------------------------------------------------------------
void LoadModelNamesLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	char* modelnames = new char[size];

	pFile->Read(modelnames, size, 1);

	int len = strlen(modelnames);
	int sz = 0;

	do
	{
		char* str = modelnames+sz;

		len = strlen(str);

		g_model_names.append(str);

		//if(len)
		//	Msg("model name: %s\n", str);

		sz += len + 1; 
	}while(sz < size);

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// load texture names, same as model names
//-------------------------------------------------------------
void LoadTextureNamesLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	g_textureNamesData = new char[size];

	pFile->Read(g_textureNamesData, size, 1);

	int len = strlen(g_textureNamesData);
	int sz = 0;

	do
	{
		char* str = g_textureNamesData+sz;

		len = strlen(str);

		g_texture_names.append(str);

		//if(len)
		//	Msg("texture name: %s\n", str);

		sz += len + 1; 
	}while(sz < size);

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// writes Wavefront OBJ into stream
//-------------------------------------------------------------
void WriteMODELToObjStream(	IVirtualStream* pStream, MODEL* model, int model_index, 
							bool debugInfo = true,
							const Matrix4x4& translation = identity4(),
							int* first_v = NULL,
							int* first_t = NULL)
{
	if(!model)
	{
		MsgError("no model %d!!!\n", model_index);
		return;
	}

	// export OBJ with points
	if(debugInfo)
		pStream->Print("#vert count %d\r\n", model->numverts);

	

	pStream->Print("o lev_model_%d\r\n", model_index);


	MODEL* vertex_ref = model;

	if(model->vertex_ref > 0) // car models have vertex_ref=0
	{
		pStream->Print("#vertex data ref model: %d (count = %d)\r\n", model->vertex_ref, model->numverts);

		vertex_ref = g_models[model->vertex_ref].model;

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
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[0][0]) / 255.0f, (255.0f-float(dec_face.texcoord[0][1])) / 255.0f);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[1][0]) / 255.0f, (255.0f-float(dec_face.texcoord[1][1])) / 255.0f);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[2][0]) / 255.0f, (255.0f-float(dec_face.texcoord[2][1])) / 255.0f);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[3][0]) / 255.0f, (255.0f-float(dec_face.texcoord[3][1])) / 255.0f);

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
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[0][0]) / 255, (255.0f-float(dec_face.texcoord[0][1])) / 255);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[1][0]) / 255, (255.0f-float(dec_face.texcoord[1][1])) / 255);
				pStream->Print("vt %g %g\r\n", float(dec_face.texcoord[2][0]) / 255, (255.0f-float(dec_face.texcoord[2][1])) / 255);

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

		WriteMODELToObjStream(mdlFile, model, model_index, true);

		// success
		GetFileSystem()->Close(mdlFile);
	}
}

//-------------------------------------------------------------
// exports car model. Car models are typical MODEL structures
//-------------------------------------------------------------
void ExportCarModel(IVirtualStream* pFile, int offset, int index, const char* name_suffix)
{
	if(offset == -1)
		return;

	pFile->Seek(offset, VS_SEEK_CUR);

	int modelSize;
	pFile->Read(&modelSize, sizeof(int), 1);

	Msg("model %d %s size: %d\n", index, name_suffix, modelSize);

	char* data = (char*)malloc(modelSize);

	// read car model data
	pFile->Read(data, modelSize, 1);

	EqString model_name(varargs("%s/CARMODEL_%d_%s", g_levname_moddir.c_str(), index, name_suffix));

	// export model
	ExportDMODELToOBJ((MODEL*)data, model_name.c_str(), index, true);
			
	// save original dmodel2
	IFile* dFile = GetFileSystem()->Open(varargs("%s.dmodel",  model_name.c_str()), "wb", SP_ROOT);
	if(dFile)
	{
		dFile->Write(data, modelSize, 1);
		GetFileSystem()->Close(dFile);
	}
				
	free(data);
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void LoadCarModelsLump(IVirtualStream* pFile, int ofs, int size)
{

	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	Msg("	all car models count: %d\n", modelCount);

	// read entries
	carmodelentry_t model_entries[MAX_CAR_MODELS];
	pFile->Read(&model_entries, sizeof(carmodelentry_t), MAX_CAR_MODELS);

	// position
	int r_ofs = pFile->Tell();

	int pad; // really padding?
	pFile->Read(&pad, sizeof(int), 1);

	if(g_export_carmodels.GetBool())
	{
		// export car models
		for(int i = 0; i < MAX_CAR_MODELS; i++)
		{
			// read models offsets
			Msg("----\n%d clean model offset: %d\n", i, model_entries[i].cleanOffset);
			Msg("%d damaged model offset: %d\n", i, model_entries[i].damagedOffset);
			Msg("%d lowpoly model offset: %d\n", i, model_entries[i].lowOffset);

			pFile->Seek(r_ofs, VS_SEEK_SET);
			ExportCarModel(pFile, model_entries[i].cleanOffset, i, "clean");

			// TODO: only export faces
			//pFile->Seek(r_ofs, VS_SEEK_SET);
			//ExportCarModel(pFile, model_entries[i].damagedOffset, i, "dam");

			pFile->Seek(r_ofs, VS_SEEK_SET);
			ExportCarModel(pFile, model_entries[i].lowOffset, i, "low");
		}
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void LoadModelsLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	MsgInfo("	model count: %d\n", modelCount);

	for(int i = 0; i < modelCount; i++)
	{
		int modelSize;

		pFile->Read(&modelSize, sizeof(int), 1);

		if(modelSize > 0)
		{
			char* data = (char*)malloc(modelSize);

			pFile->Read(data, modelSize, 1);

			ModelRef_t ref;
			ref.index = i;
			ref.model = (MODEL*)data;
			ref.size = modelSize;
			ref.swap = false;

			g_models.append(ref);
		}
		else
		{
			ModelRef_t ref;
			ref.index = i;
			ref.model = NULL;
			ref.size = modelSize;
			ref.swap = true;

			g_models.append(ref);
		}
	}

	if(g_export_models.GetBool())
	{
		for(int i = 0; i < g_models.numElem(); i++)
		{
			if(!g_models[i].model)
				continue;

			EqString modelFileName = varargs("%s/ZMOD_%d", g_levname_moddir.c_str(), i);

			if(g_model_names[i].GetLength())
				modelFileName = varargs("%s/%d_%s", g_levname_moddir.c_str(), i, g_model_names[i]);

			// export model
			ExportDMODELToOBJ(g_models[i].model, modelFileName.c_str(), i);
			
			// save original dmodel2
			IFile* dFile = GetFileSystem()->Open(varargs("%s.dmodel", modelFileName.c_str()), "wb", SP_ROOT);
			if(dFile)
			{
				dFile->Write(g_models[i].model, g_models[i].size, 1);
				GetFileSystem()->Close(dFile);
			}
		}
	}
	 
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// parses LUMP_MAP and it's straddler objects
//-------------------------------------------------------------
void LoadMapLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	pFile->Read(&g_mapInfo, 1, sizeof(MAPINFO));

	Msg("Level dimensions[%d %d], tile size: %d\n", g_mapInfo.width, g_mapInfo.height, g_mapInfo.tileSize);
	Msg("numRegions: %d\n", g_mapInfo.numRegions);
	Msg("visTableWidth: %d\n", g_mapInfo.visTableWidth);

	Msg("numAllObjects : %d\n", g_mapInfo.numAllObjects);
	Msg("NumStraddlers: %d\n", g_mapInfo.numStraddlers);
	
	for(int i = 0; i < g_mapInfo.numStraddlers; i++)
	{
		PACKED_CELL_OBJECT object;
		
		pFile->Read(&object, 1, sizeof(PACKED_CELL_OBJECT));

		CELL_OBJECT cell;
		UnpackCellObject(object, cell);

		// HOW to load them into regions?
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// parses LUMP_SPOOLINFO, and also loads region data
//-------------------------------------------------------------
void LoadSpoolInfoLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int unk;
	pFile->Read(&unk, 1, sizeof(int));

	Msg("unk = %d\n", unk);

	int numSomething; // i think it's always 32
	pFile->Read(&numSomething, 1, sizeof(int));

	Msg("numSomething = %d\n", numSomething);
	pFile->Seek(numSomething, VS_SEEK_CUR);
	
	int numBundles;
	pFile->Read(&numBundles, 1, sizeof(int));

	Msg("numBundles = %d\n", numBundles);
	RegionModels_t* regionModels = new RegionModels_t[numBundles];

	regiondata_t* loc_geom = new regiondata_t[numBundles];
	regionpages_t* loc_pages = new regionpages_t[numBundles];

	// read local geometry and texture pages
	pFile->Read(loc_geom, numBundles, sizeof(regiondata_t));
	pFile->Read(loc_pages, numBundles, sizeof(regionpages_t));

	// something decompled, 48 bytes
	struct V17Data
	{
		int a[4];
		int b[4];
		int c[4];
	};

	V17Data unknStruct;

	pFile->Read(&unknStruct, 1, sizeof(unknStruct));

	Msg("unk a = [%d %d %d %d]\n", unknStruct.a[0], unknStruct.a[1], unknStruct.a[2], unknStruct.a[3]);
	Msg("unk b = [%d %d %d %d]\n", unknStruct.b[0], unknStruct.b[1], unknStruct.b[2], unknStruct.b[3]);
	Msg("unk c = [%d %d %d %d]\n", unknStruct.c[0]+2047 & -2048, unknStruct.c[1]+2047 & -2048, unknStruct.c[2]+2047 & -2048, unknStruct.c[3]+2047 & -2048);

	int numRegionOffsets;
	pFile->Read(&numRegionOffsets, 1, sizeof(int));
	Msg("numRegionOffsets: %d\n", numRegionOffsets);

	ushort* regionOffsets = new ushort[numRegionOffsets];
	pFile->Read(regionOffsets, numRegionOffsets, sizeof(short));

	int regionsInfoSize;
	pFile->Read(&regionsInfoSize, 1, sizeof(int));
	int numRegionInfos = regionsInfoSize/sizeof(REGIONINFO);

	Msg("Region info count %d (size=%d bytes)\n", numRegionInfos, regionsInfoSize);

	REGIONINFO* regionInfos = (REGIONINFO*)malloc(regionsInfoSize);
	pFile->Read(regionInfos, 1, regionsInfoSize);

	int counter = 0;
	
	int dim_x = g_mapInfo.width / g_mapInfo.visTableWidth;
	int dim_y = g_mapInfo.height / g_mapInfo.visTableWidth;

	Msg("World size:\n [%dx%d] vis cells\n [%dx%d] regions\n", g_mapInfo.width, g_mapInfo.height, dim_x, dim_y);

	// print region map to console
	for(int i = 0; i < numRegionOffsets; i++, counter++)
	{
		// Debug information: Print the map to the console.
		Msg(regionOffsets[i] == REGION_EMPTY ? "." : "O");
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

	if(g_export_world.GetBool())
	{
		objFile = GetFileSystem()->Open("POS_REGIONOBJECTS_MAP.obj", "wb", SP_ROOT);
		levModelFile = GetFileSystem()->Open("LEVELMODEL.obj", "wb", SP_ROOT);

		levModelFile->Print("mtllib LEVELMODEL.mtl\r\n");
	}

	int lobj_first_v = 0;
	int lobj_first_t = 0;

	// 2d region map
	// easy seeking
	for(int y = 0; y < dim_y; y++)
	{
		for(int x = 0; x < dim_x; x++)
		{
			int sPosIdx = y*dim_x + x;

			if(regionOffsets[sPosIdx] == REGION_EMPTY)
				continue;

			// determine region position
			int region_pos_x = x * 65536;
			int region_pos_z = y * 65536;

			// region at offset
			REGIONINFO* regInfo = (REGIONINFO*)((ubyte*)regionInfos + regionOffsets[sPosIdx]);

			Msg("---------\nregion %d %d (offset: %d)\n", x, y, regionOffsets[sPosIdx]);
			Msg("offset: %d\n",regInfo->offset);
			Msg("unk1: %d\n",regInfo->unk1);
			Msg("unk2: %d\n",regInfo->unk2);

			// Msg("cells: %d\n",regInfo->cellsSize);
			Msg("nodes: %d\n",regInfo->contentsNodesSize);
			Msg("tables: %d\n",regInfo->contentsTableSize);
			Msg("models: %d\n",regInfo->modelsSize);

			Msg("regDataIndex: %d\n",regInfo->dataIndex);

			int visDataOffset = g_levInfo.spooldata_offset + 2048 * regInfo->offset;
			int cellsOffset = g_levInfo.spooldata_offset + 2048 * (regInfo->offset + regInfo->contentsNodesSize + regInfo->contentsTableSize);
			int modelsOffset = g_levInfo.spooldata_offset + 2048 * (regInfo->offset + regInfo->contentsNodesSize + regInfo->contentsTableSize + regInfo->modelsSize);
			
			// to read
			int numVisBytes;
			int numHeightFieldPoints;

			pFile->Seek(visDataOffset, VS_SEEK_SET);
			pFile->Read(&numVisBytes, 1, sizeof(int));
			
			// TODO: Read and decode vis bytes, possible model count found here or computed by cell visibility

			pFile->Seek(modelsOffset, VS_SEEK_SET);
			pFile->Read(&numHeightFieldPoints, 1, sizeof(int));

			Msg("numVisBytes = %d\n", numVisBytes);
			Msg("numHeightFieldPoints = %d\n", numHeightFieldPoints);

			Msg("visdata ofs: %d\n", visDataOffset);
			Msg("cell objects ofs: %d\n", cellsOffset);
			Msg("models ofs: %d\n", modelsOffset);

			// if region data is available, load models and textures
			if(regInfo->dataIndex != REGDATA_EMPTY)
			{
				LoadRegionData(pFile, &regionModels[regInfo->dataIndex], &loc_geom[regInfo->dataIndex], &loc_pages[regInfo->dataIndex]);
			}

			// seek to cells
			pFile->Seek(cellsOffset, VS_SEEK_SET);

			int cell_count = 0;

			if(objFile)
			{
				objFile->Print("#--------\r\n# region %d %d (ofs=%d)\r\n", x, y, cellsOffset);
				objFile->Print("g cell_%d\r\n", sPosIdx);
			}

			PACKED_CELL_OBJECT object;
			do
			{
				pFile->Read(&object, 1, sizeof(PACKED_CELL_OBJECT));
				
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

				if(levModelFile)
				{
					if(cell.modelindex >= g_models.numElem())
						break;

					MODEL* model = g_models[cell.modelindex].model;

					if(!model)
						break;

					// transform objects and save
					Matrix4x4 transform = translate(Vector3D((region_pos_x+cell.x)*-0.00015f, cell.y*-0.00015f, (region_pos_z+cell.z)*0.00015f));
					transform = transform * rotateY(cell.rotation / 64.0f*PI_F*2.0f) * scale(1.0f,1.0f,1.0f);

					WriteMODELToObjStream(levModelFile, model, cell.modelindex, false, transform, &lobj_first_v, &lobj_first_t);
				}
				
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

	if(levModelFile)
	{
		GetFileSystem()->Close(objFile);
		GetFileSystem()->Close(levModelFile);
	}

	int numCellsObjectsFile = g_mapInfo.numAllObjects - g_mapInfo.numStraddlers;

	if(numCellObjectsRead != numCellsObjectsFile)
		MsgError("numAllObjects mismath: in file: %d, read %d\n", numCellsObjectsFile, numCellObjectsRead);
	
	// free all
	for(int i = 0; i < numBundles; i++)
	{
		for(int j = 0; j < regionModels[i].modelRefs.numElem(); j++)
		{
			int idx = regionModels[i].modelRefs[j].index;

			if(g_models[idx].swap)
				free(regionModels[i].modelRefs[j].model);
		}
	}

	delete [] regionModels;
	
	delete [] loc_geom;
	delete [] loc_pages;

	free(regionInfos);
	delete [] regionOffsets;

	// seek back
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//---------------------------------------------------------------------------------------------------------------------------------

void ReadLevelChunk(IVirtualStream* pFile)
{
	int lump_count = 255; // Driver 2 difference: you not need to read lump count

	if(g_format.GetInt() == 1)
		pFile->Read(&lump_count, sizeof(int), 1);	// uncomment to support driver 1 TODO: command line switch

	LUMP lump;

	Msg("FILE OFS: %d bytes\n", pFile->Tell());

	for(int i = 0; i < lump_count; i++)
	{
		// read lump info
		pFile->Read(&lump, sizeof(LUMP), 1);

		// stop marker
		if(lump.type == 255)
			break;

		switch(lump.type)
		{
			case LUMP_MODELS:
				MsgWarning("LUMP_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadModelsLump(pFile);
				break;
			case LUMP_MAP:
				MsgWarning("LUMP_MAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadMapLump(pFile);
				break;
			case LUMP_TEXTURENAMES:
				MsgWarning("LUMP_TEXTURENAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadTextureNamesLump(pFile, lump.size);
				break;
			case LUMP_MODELNAMES:
				MsgWarning("LUMP_MODELNAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadModelNamesLump(pFile, lump.size);
				break;
			case LUMP_SUBDIVISION:
				MsgWarning("LUMP_SUBDIVISION ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_LOWDETAILTABLE:
				MsgWarning("LUMP_LOWDETAILTABLE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_MOTIONCAPTURE:
				MsgWarning("LUMP_MOTIONCAPTURE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_OVERLAYMAP:
				MsgWarning("LUMP_OVERLAYMAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_PALLET:
				MsgWarning("LUMP_PALLET ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_SPOOLINFO:
				MsgWarning("LUMP_SPOOLINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadSpoolInfoLump(pFile);
				break;
			case LUMP_STRAIGHTS2:
				MsgWarning("LUMP_STRAIGHTS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CURVES2:
				MsgWarning("LUMP_CURVES2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2:
				MsgWarning("LUMP_JUNCTIONS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CHAIR:
				MsgWarning("LUMP_CHAIR ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CAR_MODELS:
				MsgWarning("LUMP_CAR_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadCarModelsLump(pFile, pFile->Tell(), lump.size);
				break;
			case LUMP_TEXTUREINFO:
				MsgWarning("LUMP_TEXTUREINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadTextureInfoLump(pFile);
				break;
			default:
				MsgInfo("LUMP type: %d (0x%X) ofs=%d size=%d\n", lump.type, lump.type, pFile->Tell(), lump.size);
		}

		// skip lump
		pFile->Seek(lump.size, VS_SEEK_CUR);

		// position alignment
        if((pFile->Tell()%4) != 0)
			pFile->Seek(4-(pFile->Tell()%4),VS_SEEK_CUR);
	}
}

//---------------------------------------------------------------------------------------------------------------------------------

void LoadLevelFile(const char* filename)
{
	// try load driver2 lev file
	IFile* pReadFile = GetFileSystem()->Open(filename, "r+b");

	if(!pReadFile)
		return;

	CMemoryStream levStream;
	levStream.Open(NULL, VS_OPEN_WRITE | VS_OPEN_READ, 0);

	if(levStream.ReadFromFileStream(pReadFile))
		GetFileSystem()->Close(pReadFile);

	// seek to begin
	levStream.Seek(0, VS_SEEK_SET);

	Msg("-----------\nloading lev file '%s'\n", filename);

	EqString fileName = filename;

	g_levname_moddir = fileName.Path_Strip_Ext() + "_models";

	g_levname_texdir = fileName.Path_Strip_Ext() + "_textures";

	GetFileSystem()->MakeDir(g_levname_moddir.c_str(), SP_ROOT);
	GetFileSystem()->MakeDir(g_levname_texdir.c_str(), SP_ROOT);

	//-------------------------------------------------------------------

	LUMP curLump;

	levStream.Read(&curLump, sizeof(curLump), 1);

	if(curLump.type != LUMP_LUMPDESC)
	{
		MsgError("Not a LEV file!\n");
		return;
	}

	// read chunk offsets
	levStream.Read(&g_levInfo, sizeof(dlevinfo_t), 1);

	Msg("levdesc_offset = %d\n", g_levInfo.levdesc_offset);
	Msg("levdesc_size = %d\n", g_levInfo.levdesc_size);

	Msg("texdata_offset = %d\n", g_levInfo.texdata_offset);
	Msg("texdata_size = %d\n", g_levInfo.texdata_size);

	Msg("levdata_offset = %d\n", g_levInfo.levdata_offset);
	Msg("levdata_size = %d\n", g_levInfo.levdata_size);

	Msg("spooldata_offset = %d\n", g_levInfo.spooldata_offset);
	Msg("spooldata_size = %d\n", g_levInfo.spooldata_size);

	// read cells
	
	//-----------------------------------------------------
	// seek to section 1 - lump data 1
	levStream.Seek( g_levInfo.levdesc_offset, VS_SEEK_SET );

	// read lump
	levStream.Read( &curLump, sizeof(curLump), 1 );

	if(curLump.type != LUMP_LEVELDESC)
	{
		MsgError("Not a LUMP_LEVELDESC!\n");
	}

	Msg("entering LUMP_LEVELDESC size = %d\n--------------\n", curLump.size);

	// read sublumps
	ReadLevelChunk( &levStream );

	//-----------------------------------------------------
	// seek to section 2 - read global textures
	levStream.Seek( g_levInfo.texdata_offset, VS_SEEK_SET );

	LoadGlobalTextures(&levStream);

	//-----------------------------------------------------
	// seek to section 3 - lump data 2
	levStream.Seek( g_levInfo.levdata_offset, VS_SEEK_SET );

	// read lump
	levStream.Read(&curLump, sizeof(curLump), 1);

	if(curLump.type != LUMP_LEVELDATA)
	{
		MsgError("Not a lump 0x24!\n");
	}

	Msg("entering LUMP_LEVELDATA size = %d\n--------------\n", curLump.size);

	// read sublumps
	ReadLevelChunk( &levStream );

	// create material file
	if(g_export_world.GetBool())
	{
		IFile* pMtlFile = GetFileSystem()->Open("LEVELMODEL.mtl", "wb", SP_ROOT);

		if(pMtlFile)
		{
			for(int i = 0; i < g_numTexPages; i++)
			{
				pMtlFile->Print("newmtl page_%d\r\n", i);
				pMtlFile->Print("map_Kd %s/PAGE_%d.tga\r\n", g_levname_texdir.c_str(), i);
			}

			GetFileSystem()->Close(pMtlFile);
		}

		DumpFaceTypes();
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
		EqString outname = CMD_ARGV(1);

		LoadLevelFile( CMD_ARGV(0).GetData() );
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

