#include "obj_loader.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/cmdlib.h"
#include "util/tokenizer.h"
#include <math/Vector.h>

#include "util/util.h"

#include <nstd/Array.hpp>

bool isNotWhiteSpace(const char ch)
{
	return (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n');
}

float readFloat(Tokenizer &tok)
{
	char *str = tok.next( isNotWhiteSpace );
	//Msg("readFloat: str=%s\n", str);

	return (float)atof(str);
}

int readInt(Tokenizer &tok)
{
	char *str = tok.next( isNotWhiteSpace );
	//Msg("readInt: str=%s\n", str);

	return atoi(str);
}

int strchcount( char *str, char ch )
{
	int count = 0;
	while (*str++)
	{
		if (*str == ch)
			count++;
	}

	return count;
}

bool isNotNewLine(const char ch)
{
	return (ch != '\r' && ch != '\n');
}

bool isNumericAlpha(const char ch)
{
	return (ch == 'e' || ch == '-' || ch == '+' || ch == '.') || isNumeric(ch);
}

struct obj_material_t
{
	char name[128];
	char texture[1024];
};

//--------------------------------------------------------------------------
// Loads MTL file
//--------------------------------------------------------------------------
bool LoadMTL(const char* filename, Array<obj_material_t> &material_list)
{
	Tokenizer tok;

	if (!tok.setFile(filename))
	{
		MsgError("Couldn't open MTL file '%s'", filename);
		return false;
	}

	obj_material_t* current = nullptr;

	char *str;

	while ((str = tok.next()) != nullptr)
	{
		if(str[0] == '#')
		{
			tok.goToNextLine();
		}
		else if(!stricmp(str, "newmtl"))
		{
			obj_material_t mat;
			strcpy(mat.name, tok.next(isNotNewLine));
			strcpy(mat.texture, mat.name);

			int new_idx = material_list.size();
			material_list.append(mat);

			current = &material_list[new_idx];
		}
		else if(!stricmp(str, "map_Kd"))
		{
			if(current)
				strcpy(current->texture, tok.next(isNotNewLine));
		}
		else
			tok.goToNextLine();
	}

	Msg("Num materials: %d\n", material_list.size());

	return true;
}

char* GetMTLTexture(char* pszMaterial, Array<obj_material_t> &material_list)
{
	//Msg("search for: %s\n", pszMaterial);
	for(usize i = 0; i < material_list.size(); i++)
	{
		//Msg("search id: %d\n", i);
		if(!strcmp(material_list[i].name, pszMaterial))
			return material_list[i].texture;
	}

	return pszMaterial;
}

//--------------------------------------------------------------------------
// Loads OBJ file
//--------------------------------------------------------------------------
bool LoadOBJ(smdmodel_t* model, const char* filename)
{
	Array<obj_material_t> material_list;

	Tokenizer tok;

	int bufferSize = 0;
	FILE* fp = fopen(filename, "rb");

	if(!fp)
	{
		MsgError("Couldn't open OBJ file '%s'\n", filename);
		return false;
	}

	// read file into buffer
	fseek(fp, 0, SEEK_END);
	bufferSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	char* pBuffer = (char*)malloc(bufferSize);

	fread(pBuffer, 1, bufferSize, fp);
	fclose(fp);

	tok.setString(pBuffer);

	// done with it
	free(pBuffer);

	char *str;

	int nVerts = 0;
	int nTexCoords = 0;
	int nNormals = 0;
	int nFaces = 0;

	// read counters for fast alloc
	while ((str = tok.next()) != nullptr)
	{
		if(!stricmp(str, "v"))
		{
			nVerts++;
		}
		else if(!stricmp(str, "vt"))
		{
			nTexCoords++;
		}
		else if(!stricmp(str, "vn"))
		{
			nNormals++;
		}
		else if(!stricmp(str, "f"))
		{
			nFaces++;
		}

		tok.goToNextLine();
	}

	Msg("%d verts, %d normals, %d texcoords, %d faces total in OBJ\n", nVerts, nNormals, nTexCoords, nFaces);

	// reset reader
	tok.reset();

	bool bUseMTL = false;

	// For now, don't use MTL

	//EqString mtl_file_name(filename);
	//mtl_file_name = mtl_file_name.Path_Strip_Ext() + ".mtl";
	//if(LoadMTL((char*)mtl_file_name.GetData(), material_list))
	///	bUseMTL = true;

	strcpy(model->name, "temp");

	bool bLoaded = false;

	char material_name[1024];
	strcpy(material_name, "error");

	Array<Vector3D>& vertices = model->verts;
	Array<Vector2D>& texcoords = model->texcoords;
	Array<Vector3D>& normals = model->normals;

	vertices.reserve(nVerts);
	texcoords.reserve(nTexCoords);
	normals.reserve(nNormals);

	smdgroup_t* curgroup = nullptr;
	//int*		group_remap = new int[nVerts];

	bool gl_to_eq = true;
	bool blend_to_eq = false;

	bool reverseNormals = false;

	int numComments = 0;
	bool smoothEnabled = 0;

	while (str = tok.next())
	{
		if(str[0] == '#')
		{
			numComments++;

			if (numComments <= 3) // check for blender comment to activate hack
			{
				char* check_tok = tok.next();
				if (check_tok && !stricmp(check_tok, "blender"))
				{
					gl_to_eq = true;
					blend_to_eq = true;
					//reverseNormals = true;
				}
				else
					str = check_tok;	// check, maybe there is a actual props
			}
		}

		if(str[0] == 'v')
		{
			//char stored_str[3] = {str[0], str[1], 0};

			if(str[1] == 't')
			{
				// parse vector3
				Vector2D t;

				t.x = readFloat(tok);
				t.y = readFloat(tok);

				if(gl_to_eq)
					t.y = 1.0f - t.y;

				texcoords.append(t);
			}
			else if(str[1] == 'n')
			{
				// parse vector3
				Vector3D n;

				n.x = readFloat(tok);
				n.y = readFloat(tok);
				n.z = readFloat(tok);

				//if(blend_to_eq)
				//	n = Vector3D(n.z, n.y, n.x);

				normals.append(n);
			}
			else
			{
				// parse vector3
				Vector3D v;

				v.x = readFloat( tok );
				v.y = readFloat( tok );
				v.z = readFloat( tok );

				//if(blend_to_eq)
				//	v = Vector3D(v.z, v.y, v.x);

				vertices.append(v);
			}
		}
		else if(!stricmp(str, "f"))
		{
			//Msg("face!\n");

			// search for new group
			for (usize i = 0; i < model->groups.size(); i++)
			{
				if (!stricmp(model->groups[i]->name, material_name))
				{
					curgroup = model->groups[i];
					break;
				}
			}

			// no luck, make a new group
			if(!curgroup)
			{
				curgroup = new smdgroup_t;
				strcpy(curgroup->name, material_name);

				model->groups.append(curgroup);

				if(bUseMTL)
					strcpy(curgroup->texture, GetMTLTexture(material_name, material_list));
				else
					strcpy(curgroup->texture, material_name);
			}

			smdpoly_t poly;
			memset(&poly, 0, sizeof(poly));

			poly.smooth = smoothEnabled;
			
			int* vindices = poly.vindices;
			int* tindices = poly.tindices;
			int* nindices = poly.nindices;

			int slashcount = 0;
			bool doubleslash = false;

			bool has_v = false;
			bool has_vt = false;
			bool has_vn = false;

			char string[512];
			string[0] = '\0';

			char* ofs = tok.nextLine() + 1;

			// trim white space in the beginning
			while (*ofs == ' ')
				ofs++;
			
			strcpy(string, ofs);

			char* str_idxs[32] = {nullptr};
			xstrsplitws(string, str_idxs);

			int i = 0;
			for(i = 0; i < 4; i++, poly.vcount++)
			{
				char* pstr = str_idxs[i];

				if(!pstr)
					break;

				int len = strlen( pstr );

				if(len == 0)
					break;

				// detect on first iteration what format of vertex we have
				if(i == 0)
				{
					// face format handling
					slashcount = strchcount(pstr, '/');
					doubleslash = (strstr(pstr,"//") != nullptr);
				}

				// v//vn
				if (doubleslash && (slashcount == 2))
				{
					has_v = has_vn = 1;
					sscanf( pstr,"%d//%d",&vindices[ i ],&nindices[ i ] );
					vindices[ i ] -= 1;
					nindices[ i ] -= 1;
				}
				// v/vt/vn
				else if (!doubleslash && (slashcount == 2))
				{
					has_v = has_vt = has_vn = 1;
					sscanf( pstr,"%d/%d/%d",&vindices[ i ],&tindices[ i ],&nindices[ i ] );

					vindices[ i ] -= 1;
					tindices[ i ] -= 1;
					nindices[ i ] -= 1;
				}
				// v/vt
				else if (!doubleslash && (slashcount == 1))
				{
					has_v = has_vt = 1;
					sscanf( pstr,"%d/%d",&vindices[ i ],&tindices[ i ] );

					vindices[ i ] -= 1;
					tindices[ i ] -= 1;
				}
				// only vertex
				else
				{
					has_v = true;
					vindices[i] = atoi(pstr)-1;
				}
			}

			// add new polygon
			curgroup->polygons.append(poly);

			// skip 'goToNextLine' as we already called 'nextLine'
			continue;
		}
		else if(str[0] == 'g')
		{
			curgroup = nullptr;
		}
		else if(str[0] == 's')
		{
			char* test = tok.next(isNotNewLine);
			smoothEnabled = stricmp(test, "off") != 0;
		}
		else if(!stricmp(str, "usemtl"))
		{
			curgroup = nullptr;
			strcpy(material_name, tok.next(isNotNewLine));
		}

		tok.goToNextLine();
	}

	if(normals.size() == 0)
	{
		MsgWarning("WARNING: No normals found in %s. Did you forget to export it?\n", filename);
	}

	if(model->groups.size() > 0)
	{
		bLoaded = true;
	}

	return bLoaded;
}

void FreeOBJ(smdmodel_t* model)
{
	for (usize i = 0; i < model->groups.size(); i++)
	{
		delete model->groups[i];
	}
}