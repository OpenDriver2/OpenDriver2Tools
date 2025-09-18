#include "gl_renderer.h"

#define DBG_VERTEX_SHADER \
	"	attribute vec4 a_position_tu;\n"\
	"	attribute vec4 a_normal_tv;\n"\
	"	attribute vec4 a_color;\n"\
	"	uniform mat4 u_View;\n"\
	"	uniform mat4 u_Projection;\n"\
	"	uniform mat4 u_World;\n"\
	"	uniform mat4 u_WorldViewProj;\n"\
	"	void main() {\n"\
	"		v_color = a_color;\n"\
	"		gl_Position = u_WorldViewProj * vec4(a_position_tu.xyz, 1.0);\n"\
	"	}\n"

#define DBG_FRAGMENT_SHADER \
	"	void main() {\n"\
	"		fragColor = v_color;\n"\
	"	}\n"

const char* dbg_shader =
"varying vec2 v_texcoord;\n"
"varying vec3 v_normal;\n"
"varying vec4 v_color;\n"
"#ifdef VERTEX\n"
DBG_VERTEX_SHADER
"#else\n"
DBG_FRAGMENT_SHADER
"#endif\n";

#define MAX_LINE_BUFFER_SIZE		32768
#define MAX_POLY_BUFFER_SIZE		65535

GrVertex g_lineBuffer[MAX_LINE_BUFFER_SIZE];
int g_numLineVerts = 0;

GrVertex g_polyBuffer[MAX_POLY_BUFFER_SIZE];
int g_numPolyVerts = 0;

GrVAO* g_linesVAO = nullptr;
GrVAO* g_polysVAO = nullptr;

ShaderID g_dbgShader = -1;
Matrix4x4 g_dbgTransform = identity3();

void DebugOverlay_Init()
{
	g_linesVAO = GR_CreateVAO(MAX_LINE_BUFFER_SIZE, nullptr, 1);
	g_polysVAO = GR_CreateVAO(MAX_POLY_BUFFER_SIZE, nullptr, 1);

	g_dbgShader = GR_CompileShader(dbg_shader);
}

void DebugOverlay_Destroy()
{
	GR_DestroyVAO(g_linesVAO);
	g_linesVAO = nullptr;

	GR_DestroyVAO(g_polysVAO);
	g_polysVAO = nullptr;
}

void DebugOverlay_SetTransform(const Matrix4x4& transform)
{
	g_dbgTransform = transform;
}

void DebugOverlay_Line(const Vector3D& posA, const Vector3D& posB, const ColorRGBA& color)
{
	if (g_numLineVerts + 2 >= MAX_LINE_BUFFER_SIZE)
		return;

	Vector3D rPosA = (g_dbgTransform*Vector4D(posA, 1.0f)).xyz();
	Vector3D rPosB = (g_dbgTransform*Vector4D(posB, 1.0f)).xyz();
		
	g_lineBuffer[g_numLineVerts++] = GrVertex{ rPosA.x, rPosA.y,rPosA.z, 0,0,0, 0,0, color.x, color.y, color.z, color.w };
	g_lineBuffer[g_numLineVerts++] = GrVertex{ rPosB.x, rPosB.y,rPosB.z, 0,0,0, 0,0, color.x, color.y, color.z, color.w };
}

void DebugOverlay_Poly(const Vector3D& p0, const Vector3D& p1, const Vector3D& p2, const ColorRGBA& color)
{
	if (g_numPolyVerts + 3 >= MAX_POLY_BUFFER_SIZE)
		return;

	Vector3D rPos0 = (g_dbgTransform * Vector4D(p0, 1.0f)).xyz();
	Vector3D rPos1 = (g_dbgTransform * Vector4D(p1, 1.0f)).xyz();
	Vector3D rPos2 = (g_dbgTransform * Vector4D(p2, 1.0f)).xyz();

	g_polyBuffer[g_numPolyVerts++] = GrVertex{ rPos0.x, rPos0.y, rPos0.z, 0,0,0, 0,0, color.x, color.y, color.z, color.w };
	g_polyBuffer[g_numPolyVerts++] = GrVertex{ rPos1.x, rPos1.y, rPos1.z, 0,0,0, 0,0, color.x, color.y, color.z, color.w };
	g_polyBuffer[g_numPolyVerts++] = GrVertex{ rPos2.x, rPos2.y, rPos2.z, 0,0,0, 0,0, color.x, color.y, color.z, color.w };
}

void DebugOverlay_Box(const Vector3D& mins, const Vector3D& maxs, const ColorRGBA& color)
{
	DebugOverlay_Line(Vector3D(mins.x, maxs.y, mins.z),
		Vector3D(mins.x, maxs.y, maxs.z), color);

	DebugOverlay_Line(Vector3D(maxs.x, maxs.y, maxs.z),
		Vector3D(maxs.x, maxs.y, mins.z), color);

	DebugOverlay_Line(Vector3D(maxs.x, mins.y, mins.z),
		Vector3D(maxs.x, mins.y, maxs.z), color);

	DebugOverlay_Line(Vector3D(mins.x, mins.y, maxs.z),
		Vector3D(mins.x, mins.y, mins.z), color);

	DebugOverlay_Line(Vector3D(mins.x, mins.y, maxs.z),
		Vector3D(mins.x, maxs.y, maxs.z), color);

	DebugOverlay_Line(Vector3D(maxs.x, mins.y, maxs.z),
		Vector3D(maxs.x, maxs.y, maxs.z), color);

	DebugOverlay_Line(Vector3D(mins.x, mins.y, mins.z),
		Vector3D(mins.x, maxs.y, mins.z), color);

	DebugOverlay_Line(Vector3D(maxs.x, mins.y, mins.z),
		Vector3D(maxs.x, maxs.y, mins.z), color);

	DebugOverlay_Line(Vector3D(mins.x, maxs.y, mins.z),
		Vector3D(maxs.x, maxs.y, mins.z), color);

	DebugOverlay_Line(Vector3D(mins.x, maxs.y, maxs.z),
		Vector3D(maxs.x, maxs.y, maxs.z), color);

	DebugOverlay_Line(Vector3D(mins.x, mins.y, mins.z),
		Vector3D(maxs.x, mins.y, mins.z), color);

	DebugOverlay_Line(Vector3D(mins.x, mins.y, maxs.z),
		Vector3D(maxs.x, mins.y, maxs.z), color);
}

void DebugOverlay_Draw()
{
	if (g_numLineVerts == 0 && g_numPolyVerts == 0)
		return;

	GR_UpdateVAO(g_linesVAO, g_numLineVerts, g_lineBuffer);
	GR_UpdateVAO(g_polysVAO, g_numPolyVerts, g_polyBuffer);
	
	GR_SetShader(g_dbgShader);

	GR_SetMatrix(MATRIX_WORLD, identity4());
	GR_UpdateMatrixUniforms();
	
	GR_SetCullMode(CULL_NONE);
	GR_SetBlendMode(BM_SEMITRANS_ALPHA);
	GR_SetDepth(0);

	GR_SetVAO(g_linesVAO);
	GR_DrawNonIndexed(PRIM_LINES, 0, g_numLineVerts);

	GR_SetVAO(g_polysVAO);
	GR_DrawNonIndexed(PRIM_TRIANGLES, 0, g_numPolyVerts);

	GR_SetPolygonOffset(0.0f);

	g_numLineVerts = 0;
	g_numPolyVerts = 0;
	g_dbgTransform = identity3();
}