/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.

===============================================
*/

// Notes:
// The original Paranoia world renderer was done 
// by BUzer, which this code originates from partially.

#include "includes.h"
#include "brushmodel.h"
#include "cl_entity.h"
#include "cvar.h"
#include "com_math.h"
#include "frustum.h"
#include "console.h"
#include "system.h"
#include "enginestate.h"
#include "modelcache.h"
#include "file.h"
#include "aldformat.h"

#include "r_dlights.h"
#include "r_vbo.h"
#include "r_glsl.h"
#include "texturemanager.h"
#include "r_bsp.h"
#include "r_main.h"
#include "r_common.h"
#include "cl_main.h"
#include "cl_utils.h"
#include "r_dlights.h"
#include "r_decals.h"
#include "r_water.h"
#include "r_particles.h"
#include "r_cubemaps.h"
#include "r_vbm.h"
#include "r_common.h"
#include "r_wadtextures.h"
#include "vid.h"
#include "tga.h"
#include "r_lightstyles.h"

// Default lightmap width
const Uint32 CBSPRenderer::LIGHTMAP_DEFAULT_WIDTH = 128;
// Default lightmap height
const Uint32 CBSPRenderer::LIGHTMAP_DEFAULT_HEIGHT = 128;
// BSP decal cache size
const Uint32 CBSPRenderer::NB_BSP_DECAL_VERTS = 16384;
// Backface epsilon value
const Float CBSPRenderer::BACKFACE_EPSILON = 0.01;
// Max overlapping decals in a place
const Uint32 CBSPRenderer::MAX_DECAL_OVERLAP = 4;
// Decal vertex allocation size
const Uint32 CBSPRenderer::BSP_DECALVERT_ALLOC_SIZE = 8196;
// Temporary decal vertex array allocation size
const Uint32 CBSPRenderer::TEMP_DECAL_VERTEX_ALLOC_SIZE = 64;
// Specialfog distance
const Float CBSPRenderer::SPECIALFOG_DISTANCE = 300;

// Object definition
CBSPRenderer gBSPRenderer;

//=============================================
// @brief
//
//=============================================
CBSPRenderer::CBSPRenderer( void ):
	m_pCurrentEntity(nullptr),
	m_isEntityTransparent(false),
	m_bumpMaps(false),
	m_useLightStyles(false),
	m_pChromeTexture(nullptr),
	m_vertexCacheBase(0),
	m_vertexCacheIndex(0),
	m_vertexCacheSize(0),
	m_pLightStyleValuesArray(nullptr),
	m_pCvarDetailTextures(nullptr),
	m_pCvarDetailScale(nullptr),
	m_pCvarDrawWorld(nullptr),
	m_pCvarNormalBlendAngle(nullptr),
	m_pCvarLegacyTransparents(nullptr),
	m_pShader(nullptr),
	m_pVBO(nullptr),
	m_pDecalVBO(nullptr)
{
	m_tempDecalVertsArray.resize(TEMP_DECAL_VERTEX_ALLOC_SIZE);
	memset(m_lightmapWidths, 0, sizeof(m_lightmapWidths));
	memset(m_lightmapHeights, 0, sizeof(m_lightmapHeights));

	memset(m_lightmapIndexes, 0, sizeof(m_lightmapIndexes));
	memset(m_ambientLightmapIndexes, 0, sizeof(m_ambientLightmapIndexes));
	memset(m_diffuseLightmapIndexes, 0, sizeof(m_diffuseLightmapIndexes));
	memset(m_lightVectorsIndexes, 0, sizeof(m_lightVectorsIndexes));
}

//=============================================
// @brief
//
//=============================================
CBSPRenderer::~CBSPRenderer()
{
	Shutdown();
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::Init( void ) 
{
	// set cvars
	m_pCvarDrawWorld = gConsole.CreateCVar( CVAR_FLOAT, FL_CV_CLIENT, "r_drawworld", "1", "Toggle world rendering." );
	m_pCvarDetailTextures = gConsole.CreateCVar( CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_detail", "1", "Toggle detail textures." );
	m_pCvarDetailScale = gConsole.CreateCVar( CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_detail_scale", "1", "Adjusts detail texture scaling." );
	m_pCvarNormalBlendAngle = gConsole.CreateCVar( CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_smooth_angle", "60", "Controls normal blending for brushes." );
	m_pCvarLegacyTransparents = gConsole.CreateCVar( CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_bsp_legacytransparents", "0", "Controls whether BSP rendering uses legacy(HL1 style unlit) rendering for transparent entities." );

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::Shutdown( void ) 
{
	ClearGL();
	ClearGame();
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::InitGL( void ) 
{
	// Initialize our shader
	if(!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_CHECK_SAMPLER_OVERLAP;

		if(R_IsExtensionSupported("GL_ARB_get_program_binary"))
			shaderFlags |= CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;
		else if(g_pCvarGLSLOnDemand->GetValue() > 0)
			shaderFlags |= CGLSLShader::FL_GLSL_ONDEMAND_LOAD;


		m_pShader = new CGLSLShader(FL_GetInterface(), gGLExtF, shaderFlags, VID_ShaderCompileCallback);
		if(!m_pShader->Compile("bsprenderer.bss"))
		{
			Sys_ErrorPopup("%s - Could not compile shader: %s", __FUNCTION__, m_pShader->GetError());
			return false;
		}		

		// If active load is set, keep loading shaders during game runtime
		if((shaderFlags & CGLSLShader::FL_GLSL_ONDEMAND_LOAD) && g_pCvarGLSLActiveLoad->GetValue() > 0)
			R_AddShaderForLoading(m_pShader);

		// Initialize determinators
		m_attribs.d_shadertype = m_pShader->GetDeterminatorIndex("shadertype");
		m_attribs.d_alphatest = m_pShader->GetDeterminatorIndex("alphatest");
		m_attribs.d_blended = m_pShader->GetDeterminatorIndex("blended");

		if(!R_CheckShaderDeterminator(m_attribs.d_shadertype, "shadertype", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderDeterminator(m_attribs.d_alphatest, "alphatest", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderDeterminator(m_attribs.d_blended, "blended", m_pShader, Sys_ErrorPopup))
			return false;

		m_attribs.u_d_fogtype = m_pShader->InitUniform("d_fogtype", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_bumpmapping = m_pShader->InitUniform("d_bumpmapping", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_mrao = m_pShader->InitUniform("d_mrao", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_cubemaps = m_pShader->InitUniform("d_cubemaps", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_luminance = m_pShader->InitUniform("d_luminance", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_numlights = m_pShader->InitUniform("d_numlights", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_d_lightmap_bicubic = m_pShader->InitUniform("d_lightmap_bicubic", CGLSLShader::UNIFORM_INT1);

		if(!R_CheckShaderUniform(m_attribs.u_d_fogtype, "d_fogtype", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_bumpmapping, "d_bumpmapping", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_mrao, "d_mrao", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_luminance, "d_luminance", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_numlights, "d_numlights", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_lightmap_bicubic, "d_lightmap_bicubic", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_d_cubemaps, "d_cubemaps", m_pShader, Sys_ErrorPopup))
			return false;

		// Initialize attribs
		m_attribs.a_position = m_pShader->InitAttribute("in_position", 4, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, origin));
		m_attribs.a_tangent = m_pShader->InitAttribute("in_tangent", 3, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, tangent));
		m_attribs.a_binormal = m_pShader->InitAttribute("in_binormal", 3, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, binormal));
		m_attribs.a_normal = m_pShader->InitAttribute("in_normal", 3, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, normal));
		m_attribs.a_normal = m_pShader->InitAttribute("in_normal", 3, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, normal));
		
		for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
		{
			CString name;
			name << "in_lmapcoord[" << i << "]";
			m_attribs.a_lmapcoord[i] = m_pShader->InitAttribute(name.c_str(), 2, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, lmapcoord[i]));
		}
		
		m_attribs.a_styles = m_pShader->InitAttribute("in_styles", 4, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, styles));
		m_attribs.a_texcoord = m_pShader->InitAttribute("in_texcoord", 2, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, texcoord));
		m_attribs.a_dtexcoord = m_pShader->InitAttribute("in_dtexcoord", 2, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, dtexcoord));
		m_attribs.a_fogcoord = m_pShader->InitAttribute("in_fogcoord", 1, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, fogcoord));
		m_attribs.a_alpha = m_pShader->InitAttribute("in_alpha", 1, GL_FLOAT, sizeof(bsp_vertex_t), OFFSET(bsp_vertex_t, alpha));

		if(!R_CheckShaderVertexAttribute(m_attribs.a_position, "in_position", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_tangent, "in_tangent", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_binormal, "in_binormal", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_normal, "in_normal", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_texcoord, "in_texcoord", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_dtexcoord, "in_dtexcoord", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_fogcoord, "in_fogcoord", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_alpha, "in_alpha", m_pShader, Sys_ErrorPopup))
			return false;

		for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
		{
			CString name;
			name << "in_lmapcoord[" << i << "]";
			if (!R_CheckShaderVertexAttribute(m_attribs.a_lmapcoord[i], name.c_str(), m_pShader, Sys_ErrorPopup))
			{
				return false;
			}
		}

		// vertex shader uniforms
		m_attribs.u_projection = m_pShader->InitUniform("projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_modelview = m_pShader->InitUniform("modelview", CGLSLShader::UNIFORM_MATRIX4);

		m_attribs.u_normalmatrix = m_pShader->InitUniform("normalmatrix", CGLSLShader::UNIFORM_MATRIX4);

		m_attribs.u_causticsm1 = m_pShader->InitUniform("caustics_m1", CGLSLShader::UNIFORM_NOSYNC);
		m_attribs.u_causticsm2 = m_pShader->InitUniform("caustics_m2", CGLSLShader::UNIFORM_NOSYNC);
		m_attribs.u_interpolant = m_pShader->InitUniform("interpolant", CGLSLShader::UNIFORM_FLOAT1);

		m_attribs.u_vorigin = m_pShader->InitUniform("v_origin", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_vright = m_pShader->InitUniform("v_right", CGLSLShader::UNIFORM_FLOAT3);

		m_attribs.u_uvoffset = m_pShader->InitUniform("uvoffset", CGLSLShader::UNIFORM_FLOAT2);
		
		m_attribs.u_modelmatrix = m_pShader->InitUniform("modelmatrix", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_inv_modelmatrix = m_pShader->InitUniform("inv_modelmatrix", CGLSLShader::UNIFORM_MATRIX4);

		m_attribs.u_decalalpha = m_pShader->InitUniform("decalalpha", CGLSLShader::UNIFORM_NOSYNC);
		m_attribs.u_decalscale = m_pShader->InitUniform("decalscale", CGLSLShader::UNIFORM_NOSYNC);

		m_attribs.u_lightstyle_values = m_pShader->InitUniform("u_lightstyle_values", CGLSLShader::UNIFORM_FLOAT1, 256);
		
		for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
		{
			CString name;
			name << "baselightmap[" << i << "]";
			m_attribs.u_baselightmap[i] = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);
			
			name.clear();
			name << "difflightmap[" << i << "]";
			m_attribs.u_difflightmap[i] = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);
			
			name.clear();
			name << "lightvecstex[" << i << "]";
			m_attribs.u_lightvecstex[i] = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);
		}

		m_attribs.u_maintexture = m_pShader->InitUniform("maintexture", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_maintexture2 = m_pShader->InitUniform("maintexture2", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_detailtex = m_pShader->InitUniform("detailtex", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_chrometex = m_pShader->InitUniform("chrometex", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_normalmap = m_pShader->InitUniform("normalmap", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_normalmap2 = m_pShader->InitUniform("normalmap2", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_luminance = m_pShader->InitUniform("luminance", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_mrao = m_pShader->InitUniform("mraotex", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_mrao2 = m_pShader->InitUniform("mraotex2", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_color = m_pShader->InitUniform("color", CGLSLShader::UNIFORM_FLOAT4);
		m_attribs.u_light_radius = m_pShader->InitUniform("light_radius", CGLSLShader::UNIFORM_FLOAT1);

		m_attribs.u_cubemap = m_pShader->InitUniform("cubemap", CGLSLShader::UNIFORM_SAMPLERCUBE);
		m_attribs.u_cube_min = m_pShader->InitUniform("u_cube_min", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_max = m_pShader->InitUniform("u_cube_max", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_origin = m_pShader->InitUniform("u_cube_origin", CGLSLShader::UNIFORM_FLOAT3);

		m_attribs.u_cubemap_prev = m_pShader->InitUniform("cubemap_prev", CGLSLShader::UNIFORM_SAMPLERCUBE);
		m_attribs.u_cube_prev_min = m_pShader->InitUniform("u_cube_prev_min", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_prev_max = m_pShader->InitUniform("u_cube_prev_max", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_prev_origin = m_pShader->InitUniform("u_cube_prev_origin", CGLSLShader::UNIFORM_FLOAT3);

		m_attribs.u_cube_min = m_pShader->InitUniform("u_cube_min", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_max = m_pShader->InitUniform("u_cube_max", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_cube_origin = m_pShader->InitUniform("u_cube_origin", CGLSLShader::UNIFORM_FLOAT3);

		m_attribs.u_fogcolor = m_pShader->InitUniform("fogcolor", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_fogparams = m_pShader->InitUniform("fogparams", CGLSLShader::UNIFORM_FLOAT2);

		m_attribs.u_causticstex1 = m_pShader->InitUniform("causticstex1", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_causticstex2 = m_pShader->InitUniform("causticstex2", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_causticscolor = m_pShader->InitUniform("causticscolor", CGLSLShader::UNIFORM_FLOAT4);

		if(!R_CheckShaderUniform(m_attribs.u_projection, "projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelview, "modelview", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_normalmatrix, "normalmatrix", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_causticsm1, "caustics_m1", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_causticsm2, "caustics_m2", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_interpolant, "interpolant", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_min, "u_cube_min", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_max, "u_cube_max", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_origin, "u_cube_origin", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_vorigin, "v_origin", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_vright, "v_right", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_uvoffset, "uvoffset", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_decalalpha, "decalalpha", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_decalscale, "decalscale", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_maintexture, "maintexture", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_maintexture2, "maintexture2", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_detailtex, "detailtex", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_chrometex, "chrometex", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_normalmap, "normalmap", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_normalmap2, "normalmap2", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_mrao, "mraotex", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_mrao2, "mraotex2", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_color, "color", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_light_radius, "light_radius", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_fogcolor, "fogcolor", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_fogparams, "fogparams", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_causticstex1, "causticstex1", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_causticstex2, "causticstex2", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_causticscolor, "causticscolor", m_pShader, Sys_ErrorPopup))
			return false;

		for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
		{
			CString name;
			name << "baselightmap[" << i << "]";
			if (!R_CheckShaderUniform(m_attribs.u_baselightmap[i], name.c_str(), m_pShader, Sys_ErrorPopup))
			{
				return false;
			}

			name.clear();
			name << "difflightmap[" << i << "]";
			if (!R_CheckShaderUniform(m_attribs.u_difflightmap[i], name.c_str(), m_pShader, Sys_ErrorPopup))
			{
				return false;
			}

			name.clear();
			name << "lightvecstex[" << i << "]";
			if (!R_CheckShaderUniform(m_attribs.u_lightvecstex[i], name.c_str(), m_pShader, Sys_ErrorPopup))
			{
				return false;
			}
		}

		for(Uint32 i = 0; i < MAX_DLIGHTS; i++)
		{
			CString lightcolor;
			lightcolor << "light_" << i << "_color";

			CString lightorigin;
			lightorigin << "light_" << i << "_origin";

			CString lightradius;
			lightradius << "light_" << i << "_radius";

			CString lightcubemap;
			lightcubemap << "light_" << i << "_cubemap";

			CString lightprojtexture;
			lightprojtexture << "light_" << i << "_projtexture";

			CString lightshadowmap;
			lightshadowmap << "light_" << i << "_shadowmap";

			CString lightmatrix;
			lightmatrix << "light_" << i << "_matrix";

			CString lightconesize;
			lightconesize << "light_" << i << "_cone_size";

			CString lightspotdirection;
			lightspotdirection << "light_" << i << "_spotdirection";

			CString lightdeterminatorshadowmap;
			lightdeterminatorshadowmap << "d_light" << i << "_shadowmap";

			m_attribs.lights[i].u_light_color = m_pShader->InitUniform(lightcolor.c_str(), CGLSLShader::UNIFORM_FLOAT4);
			m_attribs.lights[i].u_light_origin = m_pShader->InitUniform(lightorigin.c_str(), CGLSLShader::UNIFORM_FLOAT3);
			m_attribs.lights[i].u_light_radius = m_pShader->InitUniform(lightradius.c_str(), CGLSLShader::UNIFORM_FLOAT1);
			m_attribs.lights[i].u_light_cubemap = m_pShader->InitUniform(lightcubemap.c_str(), CGLSLShader::UNIFORM_SAMPLERCUBE);
			m_attribs.lights[i].u_light_projtexture = m_pShader->InitUniform(lightprojtexture.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);
			m_attribs.lights[i].u_light_shadowmap = m_pShader->InitUniform(lightshadowmap.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);
			m_attribs.lights[i].u_light_matrix = m_pShader->InitUniform(lightmatrix.c_str(), CGLSLShader::UNIFORM_MATRIX4);
			m_attribs.lights[i].u_light_cone_size = m_pShader->InitUniform(lightconesize.c_str(), CGLSLShader::UNIFORM_FLOAT1);
			m_attribs.lights[i].u_light_spotdirection = m_pShader->InitUniform(lightspotdirection.c_str(), CGLSLShader::UNIFORM_FLOAT3);
			m_attribs.lights[i].u_d_light_shadowmap = m_pShader->InitUniform(lightdeterminatorshadowmap.c_str(), CGLSLShader::UNIFORM_INT1);

			if(!R_CheckShaderUniform(m_attribs.lights[i].u_light_color, lightcolor.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_origin, lightorigin.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_radius, lightradius.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_cubemap, lightcubemap.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_projtexture, lightprojtexture.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_shadowmap, lightshadowmap.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_matrix, lightmatrix.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_cone_size, lightconesize.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_light_spotdirection, lightspotdirection.c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_d_light_shadowmap, lightdeterminatorshadowmap.c_str(), m_pShader, Sys_ErrorPopup))
				return false;
		}

		if(!R_CheckShaderUniform(m_attribs.u_cubemap, "cubemap", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_min, "u_cube_min", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_max, "u_cube_max", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_origin, "u_cube_origin", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cubemap_prev, "cubemap_prev", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_prev_min, "u_cube_prev_min", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_prev_max, "u_cube_prev_max", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_cube_prev_origin, "u_cube_prev_origin", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_inv_modelmatrix, "inv_modelmatrix", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelmatrix, "modelmatrix", m_pShader, Sys_ErrorPopup))
		return false;

		// Disable these by default
		m_pShader->DisableSync(m_attribs.u_modelmatrix);
		m_pShader->DisableSync(m_attribs.u_inv_modelmatrix);
	}

	if(CL_IsGameActive())
	{
		VID_DrawLoadingScreen("Reloading world geometry");

		// Reload textures
		LoadTextures();

		// Init lightmap
		InitLightmaps();

		// Create VBO
		InitVBO();

		// Rebind decal VBO
		m_pDecalVBO->RebindGL();
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::ClearGL( void ) 
{
	if(m_pShader)
	{
		delete m_pShader;
		m_pShader = nullptr;
	}

	if(m_pVBO)
	{
		delete m_pVBO;
		m_pVBO = nullptr;
	}

	if(m_pDecalVBO)
		m_pDecalVBO->ClearGL();

	// Clear these
	memset(m_lightmapIndexes, 0, sizeof(m_lightmapIndexes));
	memset(m_ambientLightmapIndexes, 0, sizeof(m_ambientLightmapIndexes));
	memset(m_diffuseLightmapIndexes, 0, sizeof(m_diffuseLightmapIndexes));
	memset(m_lightVectorsIndexes, 0, sizeof(m_lightVectorsIndexes));
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::LoadTextures( void ) 
{
	VID_DrawLoadingScreen("Loading WAD files");

	// Get the list of WAD files
	CArray<CString> wadFilesList;
	if(!Common::GetWADList(ens.pworld->pentdata, wadFilesList))
	{
		Con_EPrintf("%s - Failed to get WAD list for '%s'.\n", __FUNCTION__, ens.pworld->name.c_str());
		wadFilesList.clear();
	}

	if(!ens.pwadresource)
	{
		// WAD texture managing object
		ens.pwadresource = new CWADTextureResource();
		if(!ens.pwadresource->Init(
			ens.pworld->name.c_str(), 
			wadFilesList, 
			(g_pCvarWadTextureChecks->GetValue() >= 1) ? true : false,
			(g_pCvarBspTextureChecks->GetValue() >= 1) ? true : false))
		{
			Con_Printf("%s - Failed to set up wad textures.\n", __FUNCTION__);
		}
	}

	VID_DrawLoadingScreen("Loading world textures");

	// Load textures
	InitTextures(*ens.pwadresource, wadFilesList);

	CTextureManager* pTextureManager = CTextureManager::GetInstance();
	m_pChromeTexture = pTextureManager->LoadTexture("general/chrome.DDS", RS_GAME_LEVEL);
	if(!m_pChromeTexture)
		m_pChromeTexture = pTextureManager->GetDummyTexture();
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::InitGame( void ) 
{
	// allocate surface array
	m_surfacesArray.resize(ens.pworld->numsurfaces);

	// Load textures
	LoadTextures();

	// Set lightmap texcoords
	SetLightmapCoords();

	// Init the VBO
	InitVBO();

	// Create decal VBO
	InitDecalVBO();

	// Set up lightmap
	InitLightmaps();

	// Set ptr to lightstyles array
	m_pLightStyleValuesArray = gLightStyles.GetLightStyleValuesArray();

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::ClearGame( void ) 
{
	if(m_pShader)
	{
		m_pShader->SetVBO(nullptr);
		m_pShader->ResetShader();
	}

	if(m_pVBO)
	{
		delete m_pVBO;
		m_pVBO = nullptr;
	}

	if(m_pDecalVBO)
	{
		delete m_pDecalVBO;
		m_pDecalVBO = nullptr;
	}

	m_vertexCacheBase = 0;
	m_vertexCacheIndex = 0;
	m_vertexCacheSize = 0;

	memset(m_lightmapIndexes, 0, sizeof(m_lightmapIndexes));
	memset(m_ambientLightmapIndexes, 0, sizeof(m_ambientLightmapIndexes));
	memset(m_diffuseLightmapIndexes, 0, sizeof(m_diffuseLightmapIndexes));
	memset(m_lightVectorsIndexes, 0, sizeof(m_lightVectorsIndexes));

	for(Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
	{
		m_lightmapWidths[i] = 0;
		m_lightmapHeights[i] = 0;
	}

	m_bumpMaps = false;
	m_useLightStyles = false;

	m_pCurrentEntity = nullptr;
	m_isEntityTransparent = false;

	if(!m_surfacesArray.empty())
		m_surfacesArray.clear();

	if(!m_texturesArray.empty())
		m_texturesArray.clear();

	if(!m_staticDecalsArray.empty())
	{
		for(Uint32 i = 0; i < m_staticDecalsArray.size(); i++)
			delete m_staticDecalsArray[i];

		m_staticDecalsArray.clear();
	}

	if(!m_decalsList.empty())
	{
		m_decalsList.begin();
		while(!m_decalsList.end())
		{
			bsp_decal_t* pdecal = m_decalsList.get();
			m_decalsList.remove(m_decalsList.get_link());
			delete pdecal;

			m_decalsList.next();
		}

		m_decalsList.clear();
	}
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::SetLightmapCoords( void ) 
{
	Uint32 paddingAmount = clamp(g_pCvarLightmapPadding->GetValue(), 0, MAX_LIGHTMAP_PADDING);

	// Set default height
	for(Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
	{
		// Set to the default
		m_lightmapWidths[i] = LIGHTMAP_DEFAULT_WIDTH;
		m_lightmapHeights[i] = LIGHTMAP_DEFAULT_HEIGHT;

		Uint32* pallocations = new Uint32[LIGHTMAP_DEFAULT_WIDTH];
		memset(pallocations, 0, sizeof(Uint32)*LIGHTMAP_DEFAULT_WIDTH);

		// Allocate lightmap positions first
		for(Uint32 j = 0; j < ens.pworld->numsurfaces; j++)
		{
			msurface_t* psurface = &ens.pworld->psurfaces[j];
			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			// Determine sizes
			Uint32 xsize = (psurface->extents[0] / psurface->lightmapdivider) + 1;
			Uint32 ysize = (psurface->extents[1] / psurface->lightmapdivider) + 1;

			// Skip empty styles
			if(i > BASE_LIGHTMAP_INDEX && psurface->styles[i] == NULL_LIGHTSTYLE_INDEX)
				continue;

			// Allocate lightmap slot
			Uint32 light_s, light_t;
			R_AllocBlock(xsize, ysize, light_s, light_t, m_lightmapWidths[i], m_lightmapHeights[i], pallocations, paddingAmount);

			bsp_surface_t* pbspsurface = &m_surfacesArray[j];
			psurface->light_s[i] = pbspsurface->light_s[i] = light_s;
			psurface->light_t[i] = pbspsurface->light_t[i] = light_t;
		}

		delete[] pallocations;
	}
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::InitLightmaps( void ) 
{
	CTextureManager* pTextureManager = CTextureManager::GetInstance();
	
	// Get overdarken treshold
	Float overdarken = g_pCvarOverdarkenTreshold->GetValue();
	if(overdarken < 0)
		overdarken = 0;

	Uint32 paddingAmount = clamp(g_pCvarLightmapPadding->GetValue(), 0, MAX_LIGHTMAP_PADDING);

	// Reset this
	m_bumpMaps = false;
	m_useLightStyles = false;

	Uint32 lightmapDataTotal = 0;

	//
	// Initialize the basic lightmap first
	//
	for(Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
	{
		// alloc default lightmap's data
		Uint32 lightmapdatasize = 0;

		Uint32 texturepixelsize = m_lightmapWidths[i]*m_lightmapHeights[i];
		color32_t* plightmap = new color32_t[texturepixelsize];
		for(Uint32 j = 0; j < m_lightmapWidths[i]*m_lightmapHeights[i]; j++)
			plightmap[j] = color32_t(0, 0, 0, 255);

		// Process the surfaces
		for(Uint32 j = 0; j < ens.pworld->numsurfaces; j++)
		{
			const msurface_t* psurface = &ens.pworld->psurfaces[j];
			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			bsp_surface_t* pbspsurface = &m_surfacesArray[j];
		
			// Skip empty styles
			if(i > BASE_LIGHTMAP_INDEX && psurface->styles[i] == NULL_LIGHTSTYLE_INDEX)
				continue;

			bool isfullbright = false;
			if(psurface->infoindex != NO_INFO_INDEX)
			{
				bsp_texture_t* ptexture = pbspsurface->ptexture;
				if(ptexture && ptexture->pmaterial && (ptexture->pmaterial->flags & TX_FL_FULLBRIGHT))
					isfullbright = true;
			}

			// Determine sizes
			Uint32 xsize = (psurface->extents[0] / psurface->lightmapdivider)+1;
			Uint32 ysize = (psurface->extents[1] / psurface->lightmapdivider)+1;
			Uint32 size = xsize*ysize;

			Float overdarkValue = (i == BASE_LIGHTMAP_INDEX) ? overdarken : 0;

			// Build the base lightmap
			color24_t* psrclightdata;
			if(ens.pworld->plightdata[SURF_LIGHTMAP_DEFAULT])
				psrclightdata = reinterpret_cast<color24_t*>(reinterpret_cast<byte*>(ens.pworld->plightdata[SURF_LIGHTMAP_DEFAULT]) + psurface->lightoffset);
			else
				psrclightdata = nullptr;

			R_BuildLightmap(pbspsurface->light_s[i], pbspsurface->light_t[i], psrclightdata, psurface, plightmap, i, m_lightmapWidths[i], overdarkValue, paddingAmount, false, isfullbright);
			lightmapdatasize += size*sizeof(color32_t);
		}

		if(i > BASE_LIGHTMAP_INDEX && lightmapdatasize <= 0)
		{
			delete[] plightmap;
			continue;
		}

		if(g_pCvarDumpLightmaps->GetValue() >= 1)
		{
			CString basename;
			Common::Basename(ens.pworld->name.c_str(), basename);

			CString directoryPath;
			directoryPath << "dumps" << PATH_SLASH_CHAR << "lightmaps" << PATH_SLASH_CHAR << basename << PATH_SLASH_CHAR;
			if(FL_CreateDirectory(directoryPath.c_str()))
			{
				CString filepath;
				filepath << directoryPath << PATH_SLASH_CHAR << "dump_lightmap_default_layer_" << i << ".tga";

				Uint32 compressionPercentage = 0;
				const byte* pwritedata = reinterpret_cast<const byte*>(plightmap);
				if(TGA_Write(pwritedata, 4, m_lightmapWidths[i], m_lightmapHeights[i], filepath.c_str(), FL_GetInterface(), Con_Printf, &compressionPercentage))
					Con_Printf("Exported %s(%d percent compression).\n", filepath.c_str(), compressionPercentage);
			}
			else
			{
				Con_Printf("%s - Failed to create directory '%s'.\n", __FUNCTION__, directoryPath.c_str());
			}
		}

		// Set default lightmap
		if(!m_lightmapIndexes[i])
			m_lightmapIndexes[i] = pTextureManager->GenTextureIndex(RS_GAME_LEVEL)->gl_index;

		Uint32 resultsize;
		R_SetLightmapTexture(m_lightmapIndexes[i], m_lightmapWidths[i], m_lightmapHeights[i], false, plightmap, resultsize);
		Con_Printf("Loaded 1 lightmaps for default layer %d: %.2f mbytes.\n", i, static_cast<Float>(resultsize)/(1024.0f*1024.0f));
		lightmapDataTotal += resultsize;

		if(i > 0)
			m_useLightStyles = true;

		delete[] plightmap;
	}

	//
	// Now process any bump mapped ones
	//
	
	if(g_pCvarBumpMaps->GetValue() >= 1
		&& ens.pworld->plightdata[SURF_LIGHTMAP_AMBIENT]
		&& ens.pworld->plightdata[SURF_LIGHTMAP_DIFFUSE]
		&& ens.pworld->plightdata[SURF_LIGHTMAP_VECTORS])
	{
		for(Uint32 k = 1; k < NB_SURF_LIGHTMAP_LAYERS; k++)
		{
			// Determine specifics
			Float _overdarken = (k == SURF_LIGHTMAP_AMBIENT) ? overdarken : 0;
			bool isvectormap = (k == SURF_LIGHTMAP_VECTORS) ? true : false;

			CString lmapname;
			switch(k)
			{
			case SURF_LIGHTMAP_VECTORS:
				lmapname = "vectors";
				break;
			case SURF_LIGHTMAP_AMBIENT:
				lmapname = "ambient";
				break;
			case SURF_LIGHTMAP_DIFFUSE:
				lmapname = "diffuse";
				break;
			}

			for(Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
			{
				// alloc ambient lightmap's data
				Uint32 lightdatasize = 0;
				Uint32 texturepixelsize = m_lightmapWidths[i]*m_lightmapHeights[i];
				color32_t* plightmapdata = new color32_t[texturepixelsize];
				for(Uint32 j = 0; j < m_lightmapWidths[i]*m_lightmapHeights[i]; j++)
					plightmapdata[j] = color32_t(0, 0, 0, 255);

				// Process the surfaces
				for(Uint32 j = 0; j < ens.pworld->numsurfaces; j++)
				{
					const msurface_t* psurface = &ens.pworld->psurfaces[j];
					if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
						continue;

					// Skip empty styles
					if(i > BASE_LIGHTMAP_INDEX && psurface->styles[i] == NULL_LIGHTSTYLE_INDEX)
						continue;

					bsp_surface_t* pbspsurface = &m_surfacesArray[j];
		
					bool isfullbright = false;
					if(psurface->infoindex != NO_INFO_INDEX)
					{
						bsp_texture_t* ptexture = pbspsurface->ptexture;
						if(ptexture && ptexture->pmaterial && (ptexture->pmaterial->flags & TX_FL_FULLBRIGHT))
							isfullbright = true;
					}

					// Determine sizes
					Uint32 xsize = (psurface->extents[0] / psurface->lightmapdivider)+1;
					Uint32 ysize = (psurface->extents[1] / psurface->lightmapdivider)+1;
					Uint32 size = xsize*ysize;

					color24_t* psrc = reinterpret_cast<color24_t*>(reinterpret_cast<byte*>(ens.pworld->plightdata[k]) + psurface->lightoffset);
					R_BuildLightmap(pbspsurface->light_s[i], pbspsurface->light_t[i], psrc, psurface, plightmapdata, i, m_lightmapWidths[i], _overdarken, paddingAmount, isvectormap);
					lightdatasize += size*sizeof(color32_t);
				}

				if(i > BASE_LIGHTMAP_INDEX && lightdatasize <= 0)
				{
					delete[] plightmapdata;
					continue;
				}

				if(g_pCvarDumpLightmaps->GetValue() >= 1)
				{
					CString basename;
					Common::Basename(ens.pworld->name.c_str(), basename);

					CString directoryPath;
					directoryPath << "dumps" << PATH_SLASH_CHAR << "lightmaps" << PATH_SLASH_CHAR << basename << PATH_SLASH_CHAR;

					if(FL_CreateDirectory(directoryPath.c_str()))
					{
						// Write file
						CString filepath;
						filepath << directoryPath << "dump_" << basename << "_lightmap_" << lmapname << "_layer_" << i << ".tga";

						Uint32 compressionPercentage = 0;
						const byte* pwritedata = reinterpret_cast<const byte*>(plightmapdata);
						if(TGA_Write(pwritedata, 4, m_lightmapWidths[i], m_lightmapHeights[i], filepath.c_str(), FL_GetInterface(), Con_Printf, &compressionPercentage))
							Con_Printf("Exported %s(%d percent compression).\n", filepath.c_str(), compressionPercentage);
					}
					else
					{
						Con_Printf("%s - Failed to create directory '%s'.\n", __FUNCTION__, directoryPath.c_str());
					}
				}

				Uint32* pdestindex = nullptr;
				switch(k)
				{
				case SURF_LIGHTMAP_VECTORS:
					pdestindex = &m_lightVectorsIndexes[i];
					break;
				case SURF_LIGHTMAP_AMBIENT:
					pdestindex = &m_ambientLightmapIndexes[i];
					break;
				case SURF_LIGHTMAP_DIFFUSE:
					pdestindex = &m_diffuseLightmapIndexes[i];
					break;
				}

				// Load the ambient lightmap
				if(!(*pdestindex))
					(*pdestindex) = pTextureManager->GenTextureIndex(RS_GAME_LEVEL)->gl_index;

				Uint32 resultsize;
				R_SetLightmapTexture((*pdestindex), m_lightmapWidths[i], m_lightmapHeights[i], isvectormap, plightmapdata, resultsize);
				Con_Printf("Loaded 1 lightmaps for %s layer %d: %.2f mbytes.\n", lmapname.c_str(), i, static_cast<Float>(resultsize)/(1024.0f*1024.0f));
				lightmapDataTotal += resultsize;

				if(!m_bumpMaps)
					m_bumpMaps = true;
			}
		}
	}

	Con_Printf("Done loading lightmaps: %.2f mbytes loaded total.\n", static_cast<Float>(lightmapDataTotal)/(1024.0f*1024.0f));

	glBindTexture(GL_TEXTURE_2D, 0);
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::InitVBO( void ) 
{
	if(ens.isloading)
		VID_DrawLoadingScreen("Loading world geometry");

	if(m_pShader)
	{
		m_pShader->ResetShader();
		m_pShader->SetVBO(nullptr);
	}

	if(m_pVBO)
	{
		delete m_pVBO;
		m_pVBO = nullptr;
	}

	Uint32 numWorldVertexes = 0;
	Uint32 numVertexes = 0;
	Uint32 curVertexIndex = 0;

	Uint32 numIndexes = 0;
	Uint32 curIndex = 0;

	// Calculate needed sizes
	for(Uint32 i = 0; i < ens.pworld->numsurfaces; i++)
	{
		const msurface_t* psurface = &ens.pworld->psurfaces[i];
		if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
			continue;

		if (psurface->displacement_id != -1)
		{
			mdispinfo_t* pinfo = &ens.pworld->pdispinfo[psurface->displacement_id];
			Uint32 side = (1 << pinfo->power) + 1;
			numWorldVertexes += (side * side);
			numIndexes += (side - 1) * (side - 1) * 6;

			// Dont continue with normal BSP rendering
			continue;
		}

		numWorldVertexes += psurface->numedges;
		numIndexes += 3+(psurface->numedges-3)*3;
	}

	// Set this for full vbo
	numVertexes = numWorldVertexes;

	// create smoothing object
	CNormalSmoothing *psmoothing = nullptr;
	if(m_pCvarNormalBlendAngle->GetValue() > 0)
	{
		Vector totalMins(NULL_MINS);
		Vector totalMaxs(NULL_MAXS);

		// Determine total world mins/maxs including brushmodels
		for(Uint32 i = 0; i < gModelCache.GetNbCachedModels(); i++)
		{
			cache_model_t* pmodel = gModelCache.GetModelByIndex(i+1);
			if(!pmodel || pmodel->type != MOD_BRUSH)
				continue;

			for(int j = 0; j < 3; j++)
			{
				if(pmodel->mins[j] < totalMins[j])
					totalMins[j] = pmodel->mins[j];

				if(pmodel->maxs[j] > totalMaxs[j])
					totalMaxs[j] = pmodel->maxs[j];
			}

			// Pad by 32 for vertexes that are edge cases
			Math::VectorAdd(totalMaxs, Vector(32, 32, 32), totalMaxs);
			Math::VectorSubtract(totalMins, Vector(32, 32, 32), totalMins);
		}

		// Create smoothing object
		psmoothing = new CNormalSmoothing(totalMins, totalMaxs, numWorldVertexes, m_pCvarNormalBlendAngle->GetValue());
	}

	// Create the arrays
	bsp_vertex_t* pvertexes = new bsp_vertex_t[numVertexes];
	Uint32* pindexes = new Uint32[numIndexes];

	// Organize triangles by textures for performance
	for(Uint32 i = 0; i < ens.pworld->numtextures; i++)
	{
		mtexture_t* ptexture = &ens.pworld->ptextures[i];

		// Link with the texture entry
		if(ptexture->infoindex == NO_INFO_INDEX)
		{
			Con_EPrintf("Texture '%s' not linked to info in BSP.\n", ptexture->name.c_str());
			continue;
		}

		// Set up the surfaces
		for(Uint32 j = 0; j < ens.pworld->numsurfaces; j++)
		{
			msurface_t* psurface = &ens.pworld->psurfaces[j];
			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			// Do not add yet if it isn't tied to the current texture
			if(psurface->ptexinfo->ptexture != ptexture)
				continue;

			bsp_surface_t* pbspsurface = &m_surfacesArray[j];

			// link with model_t surface
			pbspsurface->pmsurface = psurface;
			psurface->infoindex = j;

			Uint32 vertexBase = curVertexIndex;
			pbspsurface->start_index = curIndex;

			// Link it to the texture
			pbspsurface->ptexture = &m_texturesArray[ptexture->infoindex];

			// If we are displacement
			if (psurface->displacement_id != -1)
			{
				mdispinfo_t* pdisp = &ens.pworld->pdispinfo[psurface->displacement_id];
				Uint32 side = (1 << pdisp->power) + 1;
				Uint32 v_base = curVertexIndex;
				pbspsurface->start_index = curIndex;

				// Store normal
				Vector faceNormal;
				Math::VectorCopy(psurface->pplane->normal, faceNormal);
				if (psurface->flags & SURF_PLANEBACK)
				{
					Math::VectorScale(faceNormal, -1.0f, faceNormal);
				}

				Vector corners[4];
				for (Uint32 c = 0; c < 4; c++)
				{
					corners[c] = Vector(pdisp->corners[c][0], pdisp->corners[c][1], pdisp->corners[c][2]);
				}

				for (Uint32 y = 0; y < side; y++)
				{
					for (Uint32 x = 0; x < side; x++)
					{
						Float fr_x = static_cast<Float>(x) / (side - 1);
						Float fr_y = static_cast<Float>(y) / (side - 1);

						Vector top, bot, pos;
						Math::VectorAdd(corners[0], (corners[1] - corners[0]) * fr_x, top);
						Math::VectorAdd(corners[3], (corners[2] - corners[3]) * fr_x, bot);
						Math::VectorAdd(top, (bot - top) * fr_y, pos);

						Int32 v_idx = pdisp->vert_start + (y * side + x);
						Vector vertexOrigin;
						Math::VectorMA(pos, ens.pworld->pdispverts[v_idx].distance, ens.pworld->pdispverts[v_idx].vector, vertexOrigin);

						bsp_vertex_t& vertex = pvertexes[curVertexIndex++];
						vertex.origin[0] = vertexOrigin.x;
						vertex.origin[1] = vertexOrigin.y;
						vertex.origin[2] = vertexOrigin.z;
						vertex.origin[3] = 1.0f;
						vertex.normal = faceNormal;
						vertex.alpha = ens.pworld->pdispverts[v_idx].alpha / 255.0f;
						for (Uint32 s = 0; s < MAX_SURFACE_STYLES; s++)
						{
							vertex.styles[s] = static_cast<Float>(psurface->styles[s]);
						}

						if (rns.fog.specialfog)
							vertex.fogcoord = CalcFogCoord(vertex.origin[2]);

						// Store tangents
						mtexinfo_t* ptexinfo = psurface->ptexinfo;
						Math::VectorCopy(ptexinfo->vecs[0], vertex.tangent);
						Math::VectorNormalize(vertex.tangent);
						Math::VectorCopy(ptexinfo->vecs[1], vertex.binormal);
						Math::VectorNormalize(vertex.binormal);

						if (psmoothing)
							psmoothing->ManageVertex(vertexOrigin, vertex.normal, curVertexIndex - 1);

						// Set texcoords
						vertex.texcoord[0] = (Math::DotProduct(pos, ptexinfo->vecs[0]) + ptexinfo->vecs[0][3]) / ptexinfo->ptexture->width;
						vertex.texcoord[1] = (Math::DotProduct(pos, ptexinfo->vecs[1]) + ptexinfo->vecs[1][3]) / ptexinfo->ptexture->height;

						// Set detail texcoords if needed
						if (pbspsurface->ptexture->pmaterial->ptextures[MT_TX_DETAIL])
						{
							vertex.dtexcoord[0] = vertex.texcoord[0] * pbspsurface->ptexture->pmaterial->dt_scalex * m_pCvarDetailScale->GetValue();
							vertex.dtexcoord[1] = vertex.texcoord[1] * pbspsurface->ptexture->pmaterial->dt_scaley * m_pCvarDetailScale->GetValue();
						}

						for (Uint32 l = 0; l < MAX_SURFACE_STYLES; l++)
						{
							// Set lightmap coords
							vertex.lmapcoord[l][0] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
							vertex.lmapcoord[l][0] -= psurface->texturemins[0];
							vertex.lmapcoord[l][0] += pbspsurface->light_s[l] * psurface->lightmapdivider + (psurface->lightmapdivider / 2.0f);
							vertex.lmapcoord[l][0] /= m_lightmapWidths[l] * psurface->lightmapdivider;

							vertex.lmapcoord[l][1] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];
							vertex.lmapcoord[l][1] -= psurface->texturemins[1];
							vertex.lmapcoord[l][1] += pbspsurface->light_t[l] * psurface->lightmapdivider + (psurface->lightmapdivider / 2.0f);
							vertex.lmapcoord[l][1] /= m_lightmapHeights[l] * psurface->lightmapdivider;
						}
					}
				}

				for (Uint32 y = 0; y < side - 1; y++)
				{
					for (Uint32 x = 0; x < side - 1; x++)
					{
						pindexes[curIndex++] = v_base + (y * side + x);
						pindexes[curIndex++] = v_base + ((y + 1) * side + (x + 1));
						pindexes[curIndex++] = v_base + ((y + 1) * side + x);

						pindexes[curIndex++] = v_base + (y * side + x);
						pindexes[curIndex++] = v_base + (y * side + (x + 1));
						pindexes[curIndex++] = v_base + ((y + 1) * side + (x + 1));
					}
				}

				pbspsurface->end_index = curIndex;
				pbspsurface->num_indexes = curIndex - pbspsurface->start_index;

				Vector mins = NULL_MINS;
				Vector maxs = NULL_MAXS;
				for (Uint32 k = 0; k < pbspsurface->num_indexes; k++)
				{
					bsp_vertex_t* pvertex = &pvertexes[pindexes[pbspsurface->start_index + k]];
					for (Int32 l = 0; l < 3; l++)
					{
						if (mins[l] > pvertex->origin[l])
							mins[l] = pvertex->origin[l] - 1;
						if (maxs[l] < pvertex->origin[l])
							maxs[l] = pvertex->origin[l] + 1;
					}
				}

				Math::VectorCopy(mins, pbspsurface->mins);
				Math::VectorCopy(maxs, pbspsurface->maxs);

				// Dont continue with normal BSP rendering
				continue;
			}

			for(Uint32 k = 0; k < psurface->numedges; k++)
			{
				Vector vertexOrigin;
				Int32 edgeIndex = ens.pworld->psurfedges[psurface->firstedge+k];
				if(edgeIndex > 0)
					vertexOrigin = ens.pworld->pvertexes[ens.pworld->pedges[edgeIndex].vertexes[0]].origin;
				else
					vertexOrigin = ens.pworld->pvertexes[ens.pworld->pedges[-edgeIndex].vertexes[1]].origin;

				// allocate the new vertex
				bsp_vertex_t& vertex = pvertexes[curVertexIndex];
				curVertexIndex++;

				// Set origin
				for(Uint32 l = 0; l < 3; l++)
					vertex.origin[l] = vertexOrigin[l];

				vertex.origin[3] = 1.0;
				vertex.alpha = 0.0f;
				for (Uint32 s = 0; s < MAX_SURFACE_STYLES; s++)
				{
					vertex.styles[s] = static_cast<Float>(psurface->styles[s]);
				}

				if(rns.fog.specialfog)
					vertex.fogcoord = CalcFogCoord(vertex.origin[2]);

				// Store tangents
				mtexinfo_t* ptexinfo = psurface->ptexinfo;
				Math::VectorCopy(ptexinfo->vecs[0], vertex.tangent);
				Math::VectorNormalize(vertex.tangent);
				Math::VectorCopy(ptexinfo->vecs[1], vertex.binormal);
				Math::VectorNormalize(vertex.binormal);

				// Store normal
				Math::VectorCopy(psurface->pplane->normal, vertex.normal);
				if(psurface->flags & SURF_PLANEBACK)
				{
					for(Uint32 l = 0; l < 3; l++)
						vertex.normal[l] *= -1;
				}

				if(psmoothing)
					psmoothing->ManageVertex(vertexOrigin, vertex.normal, curVertexIndex-1);

				// Set texcoords
				vertex.texcoord[0] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[0])+ptexinfo->vecs[0][3];
				vertex.texcoord[0] /= static_cast<Float>(psurface->ptexinfo->ptexture->width);

				vertex.texcoord[1] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[1])+ptexinfo->vecs[1][3];
				vertex.texcoord[1] /= static_cast<Float>(psurface->ptexinfo->ptexture->height);

				// Set detail texcoords if needed
				if(pbspsurface->ptexture->pmaterial->ptextures[MT_TX_DETAIL])
				{
					vertex.dtexcoord[0] = vertex.texcoord[0]*pbspsurface->ptexture->pmaterial->dt_scalex*m_pCvarDetailScale->GetValue();
					vertex.dtexcoord[1] = vertex.texcoord[1]*pbspsurface->ptexture->pmaterial->dt_scaley*m_pCvarDetailScale->GetValue();
				}

				for(Uint32 l = 0; l < MAX_SURFACE_STYLES; l++)
				{
					// Set lightmap coords
					vertex.lmapcoord[l][0] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
					vertex.lmapcoord[l][0] -= psurface->texturemins[0];
					vertex.lmapcoord[l][0] += pbspsurface->light_s[l]*psurface->lightmapdivider + (psurface->lightmapdivider / 2.0f);
					vertex.lmapcoord[l][0] /= m_lightmapWidths[l]*psurface->lightmapdivider;

					vertex.lmapcoord[l][1] = Math::DotProduct(vertexOrigin, ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];
					vertex.lmapcoord[l][1] -= psurface->texturemins[1];
					vertex.lmapcoord[l][1] += pbspsurface->light_t[l]*psurface->lightmapdivider + (psurface->lightmapdivider / 2.0f);
					vertex.lmapcoord[l][1] /= m_lightmapHeights[l]*psurface->lightmapdivider;
				}
			}

			// Set indexes
			Uint32 indexes[3] = { 0 };
			for(Uint32 k = 0; k < 3; k++)
			{
				indexes[k] = vertexBase + k;
				pindexes[curIndex] = indexes[k];
				curIndex++;
			}

			// Break the triangle fan into raw triangles
			for(Uint32 k = 0, l = 3; k < (psurface->numedges-3); k++, l++)
			{
				indexes[1] = indexes[2];
				indexes[2] = vertexBase+l;

				pindexes[curIndex++] = indexes[0];
				pindexes[curIndex++] = indexes[1];
				pindexes[curIndex++] = indexes[2];
			}

			// Set end index
			pbspsurface->end_index = curIndex;
			pbspsurface->num_indexes = curIndex - pbspsurface->start_index;

			// Calculate mins/maxs
			Vector mins = NULL_MINS;
			Vector maxs = NULL_MAXS;
			for(Uint32 k = 0; k < pbspsurface->end_index-pbspsurface->start_index; k++)
			{
				bsp_vertex_t *pvertex = &pvertexes[pindexes[pbspsurface->start_index+k]];
				for(Int32 l = 0; l < 3; l++)
				{
					if(mins[l] > pvertex->origin[l])
						mins[l] = pvertex->origin[l]-1;

					if(maxs[l] < pvertex->origin[l])
						maxs[l] = pvertex->origin[l]+1;
				}
			}

			Math::VectorCopy(mins, pbspsurface->mins);
			Math::VectorCopy(maxs, pbspsurface->maxs);
		}
	}

	// Set smoothed normals
	if(psmoothing)
	{
		for(Uint32 i = 0; i < numWorldVertexes; i++)
		{
			bsp_vertex_t* pvertex = &pvertexes[i];

			// If we have a smoothed normal, then apply it over the original
			const Vector* pnormal = psmoothing->GetVertexNormal(i);
			if(pnormal)
				pvertex->normal = *pnormal;

			Vector tangent, binormal;
			for(int j = 0; j < 3; j++)
			{
				tangent[j] = (pvertex->tangent[j] - pvertex->normal[j] * Math::DotProduct(pvertex->normal, pvertex->tangent));
				binormal[j] = (pvertex->binormal[j] - pvertex->normal[j] * Math::DotProduct(pvertex->normal, pvertex->binormal));
			}

			Math::VectorNormalize(tangent);
			Math::VectorCopy(tangent, pvertex->tangent);

			Math::VectorNormalize(binormal);
			Math::VectorCopy(binormal, pvertex->binormal);
		}

		delete psmoothing;
	}

	// Set the VBO
	m_pVBO = new CVBO(gGLExtF, pvertexes, sizeof(bsp_vertex_t)*numVertexes, pindexes, sizeof(Uint32)*numIndexes);

	delete[] pvertexes;
	delete[] pindexes;

	rns.fog.prevspecialfog = rns.fog.specialfog;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::InitDecalVBO( void )
{
	if(m_pDecalVBO)
		delete m_pDecalVBO;

	// Set the decal cache
	m_vertexCacheBase = 0; // We use separate buffers now
	m_vertexCacheIndex = m_vertexCacheBase;
	m_vertexCacheSize = NB_BSP_DECAL_VERTS;

	// Set the VBO
	bsp_vertex_t* pvertexes = new bsp_vertex_t[m_vertexCacheSize];
	m_pDecalVBO = new CVBO(gGLExtF, pvertexes, sizeof(bsp_vertex_t)*m_vertexCacheSize, nullptr, 0, true);
	delete[] pvertexes;
}

//=============================================
// @brief
//
//=============================================
en_material_t* CBSPRenderer::LoadMapTexture( CWADTextureResource& wadTextures, const CArray<CString>& wadFilesList, const Char* pstrtexturename )
{
	// First try loading it under "world" as a normal material
	CString materialPath;
	materialPath << WORLD_TEXTURES_PATH_BASE << pstrtexturename << PMF_FORMAT_EXTENSION;

	CTextureManager* pTextureManager = CTextureManager::GetInstance();

	en_material_t* pmaterial = pTextureManager->LoadMaterialScript(materialPath.c_str(), RS_GAME_LEVEL, false);
	if(!pmaterial)
	{
		CString folderPath = WAD_GetWADFolderPath(ens.pworld->name.c_str(), WORLD_TEXTURES_PATH_BASE);
		materialPath = WAD_GetWADTexturePath(folderPath.c_str(), pstrtexturename);

		pmaterial = pTextureManager->LoadMaterialScript(materialPath.c_str(), RS_GAME_LEVEL, false);
		if(!pmaterial)
		{
			// Look under WAD paths
			for(Uint32 i = 0; i < wadFilesList.size(); i++)
			{
				folderPath = WAD_GetWADFolderPath(wadFilesList[i].c_str(), WORLD_TEXTURES_PATH_BASE);
				materialPath = WAD_GetWADTexturePath(folderPath.c_str(), pstrtexturename);

				pmaterial = pTextureManager->LoadMaterialScript(materialPath.c_str(), RS_GAME_LEVEL, false);
				if(pmaterial)
					break;
			}
		}
	}

	if(!pmaterial)
	{
		// Just get the dummy material
		pmaterial = pTextureManager->GetDummyMaterial();
	}
	else if(!pmaterial->ptextures[MT_TX_DIFFUSE] && !pmaterial->containername.empty())
	{
		// Load the texture from the WAD
		pmaterial->ptextures[MT_TX_DIFFUSE] = wadTextures.GetWADTexture(pmaterial, pmaterial->containername.c_str(), pmaterial->containertexturename.c_str());
	}

	// Make sure this is set
	if(!pmaterial->ptextures[MT_TX_DIFFUSE])
		pmaterial->ptextures[MT_TX_DIFFUSE] = pTextureManager->GetDummyTexture();

	return pmaterial;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::InitTextures( CWADTextureResource& wadTextures, const CArray<CString>& wadFilesList ) 
{
	if(m_texturesArray.empty())
	{
		// allocate array size
		m_texturesArray.resize(ens.pworld->numtextures);
	}

	for(Uint32 i = 0; i < ens.pworld->numtextures; i++)
	{
		mtexture_t* ptexture = &ens.pworld->ptextures[i];
		bsp_texture_t* pbsptexture = &m_texturesArray[i];

		// link it up
		ptexture->infoindex = i;
		pbsptexture->pmodeltexture = ptexture;

		pbsptexture->numsinglebatches = 0;
		
		pbsptexture->index = i;
		pbsptexture->psurfchain = nullptr;

		// Load the texture
		pbsptexture->pmaterial = LoadMapTexture(wadTextures, wadFilesList, ptexture->name.c_str());

		// See how many surfaces are tied to this texture
		Uint32 numsurfaces = 0;
		for(Uint32 j = 0; j < ens.pworld->numsurfaces; j++)
		{
			const msurface_t* psurface = &ens.pworld->psurfaces[j];
			if(psurface->ptexinfo->ptexture->infoindex != static_cast<Int32>(i))
				continue;

			numsurfaces++;
		}

		// Set drawbatch array sizes
		pbsptexture->single_batches.resize(numsurfaces);
	}

	// Get our second texture from the displacement
	for (Uint32 i = 0; i < ens.pworld->numdispinfo; i++)
	{
		const mdispinfo_t& disp = ens.pworld->pdispinfo[i];
		if (disp.texture2[0] == '\0')
			continue;

		if (disp.face_index < 0 || disp.face_index >= static_cast<Int32>(ens.pworld->numsurfaces))
			continue;

		msurface_t* psurface = &ens.pworld->psurfaces[disp.face_index];
		if (!psurface->ptexinfo || !psurface->ptexinfo->ptexture)
			continue;

		bsp_texture_t* pbsptexture = &m_texturesArray[psurface->ptexinfo->ptexture->infoindex];
		en_material_t* pmat1 = pbsptexture->pmaterial;
		en_material_t* pmat2 = LoadMapTexture(wadTextures, wadFilesList, disp.texture2);

		if (pmat1 && pmat2)
		{
			pmat1->ptextures[MT_TX_DIFFUSE2] = pmat2->ptextures[MT_TX_DIFFUSE];
			pmat1->ptextures[MT_TX_NORMALMAP2] = pmat2->ptextures[MT_TX_NORMALMAP];
			pmat1->ptextures[MT_TX_MRAO2] = pmat2->ptextures[MT_TX_MRAO];
		}
	}
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawNormal( void ) 
{
	// Set shader's VBO and bind it
	m_pShader->SetVBO(m_pVBO);

	if(!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_alpha);

	// Set projection
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Draw the world next if we didn't fail
	bool result = DrawWorld();

	// Make sure this is reset
	if(result && !m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED, false))
		result = false;

	m_pShader->DisableShader();

	// Clear any binds
	R_ClearBinds();

	// Error might be thrown by vbm renderer from skybox draw, so check
	// the BSP shader if it has any errors
	if(!result)
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());

	return result;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawTransparent( void ) 
{
	if(m_pCvarDrawWorld->GetValue() < 1)
		return true;

	// Set shader's VBO and bind it
	m_pShader->SetVBO(m_pVBO);
	if(!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_alpha);

	// Set projection
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Check for shader errors
	bool result = true;
	m_isEntityTransparent = true;

	// Draw static entities first
	if(g_pCvarDrawEntities->GetValue() > 0)
	{
		for(Uint32 i = 0; i < rns.objects.numvisents; i++)
		{
			cl_entity_t& entity = *rns.objects.pvisents[i];

			if(!entity.pmodel || entity.pmodel->type != MOD_BRUSH)
				continue;

			if(!R_IsEntityTransparent(entity) || R_IsSpecialRenderEntity(entity))
				continue;

			// Handle skydraw specially
			if (rns.water_skydraw)
			{
				if (entity.curstate.renderfx != RenderFx_SkyEnt
					&& entity.curstate.renderfx != RenderFx_SkyEntScaled)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_SkyEnt
					|| entity.curstate.renderfx == RenderFx_SkyEntScaled)
					continue;
			}

			// Handle portals specially
			if (rns.portalpass)
			{
				if (entity.curstate.renderfx != RenderFx_InPortalEntity
					&& entity.curstate.renderfx != RenderFx_InPortalScaledModel)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_InPortalEntity
					|| entity.curstate.renderfx == RenderFx_InPortalScaledModel)
					continue;
			}

			// Never allow no-depth cull entities to be rendered here
			if (entity.curstate.renderfx == RenderFx_SkyEntNC)
				continue;

			result = DrawBrushModel(entity, false);
			if(!result)
				break;
		}
	}

	m_isEntityTransparent = false;

	// Draw decals last
	if(result)
	{
		m_pShader->SetVBO(m_pDecalVBO);

		if(!m_pShader->EnableShader())
		{
			Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
			return false;
		}

		m_pShader->EnableAttribute(m_attribs.a_position);
		m_pShader->EnableAttribute(m_attribs.a_alpha);

		result = DrawDecals(true);

		// Disable VBO
		m_pShader->DisableShader();
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	// Clear any binds
	R_ClearBinds();

	if(!result)
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());

	return result;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawSkyBox( bool inZElements ) 
{
	// Set shader's VBO and bind it
	m_pShader->SetVBO(m_pVBO);
	if(!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_alpha);

	// Set projection
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	m_pCurrentEntity = CL_GetEntityByIndex(WORLDSPAWN_ENTITY_INDEX);
	m_isEntityTransparent = false;

	if(!inZElements)
	{
		if(!Prepare())
			return false;

		RecursiveWorldNode(ens.pworld->pnodes);

		if(!Draw())
			return false;
	}

	for(Uint32 i = 0; i < rns.objects.numvisents; i++)
	{
		if(!rns.objects.pvisents[i]->pmodel)
			continue;

		if(rns.objects.pvisents[i]->pmodel->type != MOD_BRUSH)
			continue;

		if(!inZElements)
		{
			if(rns.objects.pvisents[i]->curstate.renderfx != RenderFx_SkyEnt
				&& rns.objects.pvisents[i]->curstate.renderfx != RenderFx_SkyEntScaled)
				continue;
		}
		else
		{
			if(rns.objects.pvisents[i]->curstate.renderfx != RenderFx_SkyEntNC)
				continue;
		}

		if(!DrawBrushModel((*rns.objects.pvisents[i]), false))
			return false;
	}

	// Disable so the others can render
	m_pShader->DisableAttribute(m_attribs.a_position);
	m_pShader->DisableAttribute(m_attribs.a_normal);
	for (Uint32 i = 0; i < 4; i++)
	{
		m_pShader->DisableAttribute(m_attribs.a_lmapcoord[i]);
	}
	m_pShader->DisableAttribute(m_attribs.a_styles);
	m_pShader->DisableAttribute(m_attribs.a_texcoord);
	m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
	m_pShader->DisableAttribute(m_attribs.a_fogcoord);
	m_pShader->DisableShader();

	return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawWorld( void ) 
{
	if(m_pCvarDrawWorld->GetValue() < 1)
		return true;

	// Draw world first
	m_pCurrentEntity = CL_GetEntityByIndex(WORLDSPAWN_ENTITY_INDEX);
	m_isEntityTransparent = false;

	if(!Prepare())
		return false;

	RecursiveWorldNode(ens.pworld->pnodes);

	// Draw static entities first
	if(g_pCvarDrawEntities->GetValue() > 0)
	{
		for(Uint32 i = 0; i < rns.objects.numvisents; i++)
		{
			cl_entity_t& entity = *rns.objects.pvisents[i];

			if(!entity.pmodel || entity.pmodel->type != MOD_BRUSH)
				continue;

			// Handle skydraw specially
			if (rns.water_skydraw)
			{
				if (entity.curstate.renderfx != RenderFx_SkyEnt
					&& entity.curstate.renderfx != RenderFx_SkyEntScaled)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_SkyEnt
					|| entity.curstate.renderfx == RenderFx_SkyEntScaled)
					continue;
			}

			// Handle portals specially
			if (rns.portalpass)
			{
				if (entity.curstate.renderfx != RenderFx_InPortalEntity
					&& entity.curstate.renderfx != RenderFx_InPortalScaledModel)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_InPortalEntity
					|| entity.curstate.renderfx == RenderFx_InPortalScaledModel)
					continue;
			}

			// Never allow no-depth cull entities to be rendered here
			if (entity.curstate.renderfx == RenderFx_SkyEntNC)
				continue;

			if(R_IsEntityMoved(entity) 
				|| R_IsEntityTransparent(entity)
				|| R_IsSpecialRenderEntity(entity))
				continue;

			if(!DrawBrushModel(entity, true))
				return false;
		}
	}

	// Reset this for texture anims
	m_pCurrentEntity = CL_GetEntityByIndex(WORLDSPAWN_ENTITY_INDEX);
	m_isEntityTransparent = false;

	if(!Draw())
		return false;

	// Draw moved entities
	if(g_pCvarDrawEntities->GetValue() > 0)
	{
		for(Uint32 i = 0; i < rns.objects.numvisents; i++)
		{
			cl_entity_t& entity = *rns.objects.pvisents[i];

			if(!entity.pmodel || entity.pmodel->type != MOD_BRUSH)
				continue;

			// Handle skydraw specially
			if (rns.water_skydraw)
			{
				if (entity.curstate.renderfx != RenderFx_SkyEnt
					&& entity.curstate.renderfx != RenderFx_SkyEntScaled)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_SkyEnt
					|| entity.curstate.renderfx == RenderFx_SkyEntScaled)
					continue;
			}

			// Handle portals specially
			if (rns.portalpass)
			{
				if (entity.curstate.renderfx != RenderFx_InPortalEntity
					&& entity.curstate.renderfx != RenderFx_InPortalScaledModel)
					continue;
			}
			else
			{
				if (entity.curstate.renderfx == RenderFx_InPortalEntity
					|| entity.curstate.renderfx == RenderFx_InPortalScaledModel)
					continue;
			}

			// Never allow no-depth cull entities to be rendered here
			if (entity.curstate.renderfx == RenderFx_SkyEntNC)
				continue;

			if(!R_IsEntityMoved(entity) 
				|| R_IsEntityTransparent(entity)
				|| R_IsSpecialRenderEntity(entity))
				continue;

			if(!DrawBrushModel(entity, false))
				return false;
		}
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::Prepare( void ) 
{
	if(rns.fog.settings.active)
	{
		m_pShader->SetUniform3f(m_attribs.u_fogcolor, rns.fog.settings.color[0], rns.fog.settings.color[1], rns.fog.settings.color[2]);
		m_pShader->SetUniform2f(m_attribs.u_fogparams, rns.fog.settings.end, 1.0f/(static_cast<Float>(rns.fog.settings.end)- static_cast<Float>(rns.fog.settings.start)));

		if(rns.fog.specialfog)
		{
			m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_fogcoord);
			m_pShader->EnableAttribute(m_attribs.a_fogcoord);
		}
		else
		{
			m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_radial);
		}
	}
	else
	{
		m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_none);
	}

	m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, false);
	m_pShader->SetUniform1i(m_attribs.u_d_mrao, false);
	m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_OFF);

	m_pShader->SetUniform1i(m_attribs.u_d_lightmap_bicubic, g_pCvarBicubicLightmaps->GetValue() > 0 ? 1 : 0);

	// Load in current modelview
	m_pShader->SetUniformMatrix4fv(m_attribs.u_normalmatrix, rns.view.modelview.GetInverse());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, 1.0);

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	// Clear all chains and batches
	for(Uint32 i = 0; i < m_texturesArray.size(); i++)
	{
		bsp_texture_t& texture = m_texturesArray[i];
		texture.numsinglebatches = 0;

		texture.psurfchain = nullptr;
	}

	// Reset everything
	for(Uint32 i = 0; i < MAX_DLIGHTS; i++)
	{
		m_pShader->DisableSync(m_attribs.lights[i].u_light_color);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_origin);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_radius);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_cubemap);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_projtexture);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_shadowmap);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_matrix);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_cone_size);
		m_pShader->DisableSync(m_attribs.lights[i].u_light_spotdirection);
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::RecursiveWorldNode( mnode_t* pnode ) 
{
	Uint32 i;
	Float dot;

	if(pnode->contents == CONTENTS_SOLID)
		return;

	if(rns.view.pviewleaf->contents != CONTENTS_SOLID)
	{
		if(pnode->visframe != rns.visframe)
			return;
	}

	if(rns.view.frustum.CullBBox(pnode->mins, pnode->maxs))
		return;

	if(pnode->contents < 0)
	{
		mleaf_t* pleaf = reinterpret_cast<mleaf_t*>(pnode);
		msurface_t** pmark = pleaf->pfirstmarksurface;

		for(i = 0; i < pleaf->nummarksurfaces; i++, pmark++)
			(*pmark)->visframe = rns.framecount;

		return;
	}

	plane_t* pplane = pnode->pplane;
	switch(pplane->type)
	{
	case PLANE_X:
		dot = rns.view.v_origin[0] - pplane->dist; break;
	case PLANE_Y:
		dot = rns.view.v_origin[1] - pplane->dist; break;
	case PLANE_Z:
		dot = rns.view.v_origin[2] - pplane->dist; break;
	default:
		dot = Math::DotProduct(rns.view.v_origin, pplane->normal) - pplane->dist;
		break;
	}

	Int32 side;
	Int32 sidebit;
	if(dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// Go down the children, front side first
	RecursiveWorldNode(pnode->pchildren[side]);

	// Batch the surfaces
	if(pnode->numsurfaces)
	{
		msurface_t* psurface = ens.pworld->psurfaces + pnode->firstsurface;
		for(i = 0; i < pnode->numsurfaces; i++, psurface++)
		{
			if(psurface->visframe != rns.framecount)
				continue;

			if((psurface->flags & SURF_PLANEBACK) != sidebit)
				continue;

			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			BatchSurface(psurface);
		}
	}

	// Recurse down the back side
	RecursiveWorldNode(pnode->pchildren[!side]);
}

//=============================================
// @brief
//
//=============================================
__forceinline void CBSPRenderer::BatchSurface( msurface_t* psurface )
{
	bsp_surface_t* pbspsurface = nullptr;
	pbspsurface = &m_surfacesArray[psurface->infoindex];
	bsp_texture_t* ptexture = pbspsurface->ptexture;
	if(!ptexture)
		return;

	AddBatch(ptexture->single_batches, ptexture->numsinglebatches, pbspsurface);

	rns.counters.brushpolies++;
}

//=============================================
// @brief
//
//=============================================
__forceinline void CBSPRenderer::AddBatch( CArray<drawbatch_t>& batches, Uint32& numbatches, bsp_surface_t *psurface )
{
	drawbatch_t *pbatch;
	if(numbatches > 0)
	{
		// So based on some testing I did, it doesn't make much
		// of a difference to go through all batches, or to just
		// check the last one, so always only check the last batch
		// for matching indexes
		pbatch = &batches[numbatches-1];

		if(psurface->end_index == pbatch->start_index)
		{
			pbatch->start_index = psurface->start_index;
			return;
		}

		if(psurface->start_index == pbatch->end_index)
		{
			pbatch->end_index = psurface->end_index;
			return;
		}
	}

	// Add a new one
	pbatch = &batches[numbatches];
	numbatches++;

	pbatch->start_index = psurface->start_index;
	pbatch->end_index = psurface->end_index;
	rns.counters.batches++;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::Draw( void ) 
{
	Int32 rendermodeext;
	if(m_pCvarLegacyTransparents->GetValue() >= 1)
		rendermodeext = m_pCurrentEntity->curstate.rendermode;
	else
		rendermodeext = m_pCurrentEntity->curstate.rendermode & RENDERMODE_BITMASK;

	// Flag for whether the view matrix was set
	bool cubematrixSet = false;

	for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
	{
		m_pShader->EnableAttribute(m_attribs.a_lmapcoord[i]);
	}
	
	m_pShader->EnableAttribute(m_attribs.a_styles);
	m_pShader->EnableAttribute(m_attribs.a_texcoord);

	m_pShader->SetUniform2f(m_attribs.u_uvoffset, 0, 0);
	
	m_pShader->SetUniform2f(m_attribs.u_uvoffset, 0, 0);
	if (m_pLightStyleValuesArray && !m_pLightStyleValuesArray->empty())
	{
		m_pShader->SetUniform1fv(m_attribs.u_lightstyle_values, &((*m_pLightStyleValuesArray)[0]), 256);
	}

	// Gather up to 4 dynamic lights
	cl_dlight_t* active_dlights[MAX_DLIGHTS] = { nullptr };
	Uint32 num_active_dlights = 0;
	if(g_pCvarDynamicLights->GetValue() >= 1)
	{
		CLinkedList<cl_dlight_t*>& dlightlist = gDynamicLights.GetLightList();
		dlightlist.begin();
		while(!dlightlist.end() && num_active_dlights < MAX_DLIGHTS)
		{
			cl_dlight_t* dl = dlightlist.get();
			if(DL_IsLightVisible(rns.view.frustum, dl->mins, dl->maxs, dl))
			{
				active_dlights[num_active_dlights] = dl;
				num_active_dlights++;
			}
			dlightlist.next();
		}
	}

	m_pShader->SetUniform1i(m_attribs.u_d_numlights, num_active_dlights);

	// Render normal ones first
	for(Uint32 i = 0; i < m_texturesArray.size(); i++)
	{
		if(!m_texturesArray[i].pmodeltexture)
			continue;

		// Nothing to draw
		if(!m_texturesArray[i].numsinglebatches)
			continue;

		// Get the animated texture
		mtexture_t *pworldtexture = TextureAnimation(m_texturesArray[i].pmodeltexture, m_pCurrentEntity->curstate.frame);
		bsp_texture_t *ptexturehandle = &m_texturesArray[pworldtexture->infoindex];
		en_material_t* pmaterial = ptexturehandle->pmaterial;

		if (pmaterial->ptextures[MT_TX_DIFFUSE2])
			m_pShader->SetDeterminator(m_attribs.d_blended, 1, false);
		else
			m_pShader->SetDeterminator(m_attribs.d_blended, 0, false);

		if(m_pCurrentEntity->curstate.effects & EF_CONVEYOR)
			m_pShader->SetUniform2f(m_attribs.u_uvoffset, -rns.time*m_pCurrentEntity->curstate.scale*0.02, 0);

		GLuint cubemapUnit = 0;
		cubemapinfo_t* pcubemapinfo = nullptr;
		cubemapinfo_t* pprevcubemapinfo = nullptr;

		bool alphaToCoverageEnabled = false;

		// rendermode overrides
		if(m_pCvarLegacyTransparents->GetValue() >= 1 
			&& (rendermodeext == RENDER_TRANSADDITIVE || rendermodeext == RENDER_TRANSTEXTURE 
			|| rendermodeext == RENDER_TRANSALPHA_UNLIT || rendermodeext == RENDER_TRANSCOLOR 
			|| rendermodeext == RENDER_TRANSCOLOR_LIT))
		{
			// Reset sampler to 0
			m_pShader->ResetSamplerIndex();

			m_pShader->DisableAttribute(m_attribs.a_normal);
			m_pShader->DisableAttribute(m_attribs.a_tangent);
			m_pShader->DisableAttribute(m_attribs.a_binormal);

			// Make sure these are disabled
			m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, FALSE);
			m_pShader->SetUniform1i(m_attribs.u_d_mrao, FALSE);
			m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_OFF);
			m_pShader->SetUniform1i(m_attribs.u_d_luminance, FALSE);

			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_chrome, false))
				return false;

			bool result = true;
			switch(rendermodeext)
			{
			case RENDER_TRANSADDITIVE:
			case RENDER_TRANSTEXTURE:
			case RENDER_TRANSALPHA_UNLIT:
				{
					// Only texture
					Uint32 mainsamplerindex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture);
					R_Bind2DTexture(GL_TEXTURE0+mainsamplerindex, pmaterial->ptextures[MT_TX_DIFFUSE]->palloc->gl_index);

					if (pmaterial->ptextures[MT_TX_DIFFUSE2])
					{
						mainsamplerindex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture2);
						R_Bind2DTexture(GL_TEXTURE0 + mainsamplerindex, pmaterial->ptextures[MT_TX_DIFFUSE2]->palloc->gl_index);
					}

					for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
					{
						m_pShader->EnableAttribute(m_attribs.a_lmapcoord[k]);
					}
					m_pShader->EnableAttribute(m_attribs.a_styles);
					m_pShader->EnableAttribute(m_attribs.a_texcoord);

					if (pmaterial->ptextures[MT_TX_DETAIL] && m_pCvarDetailTextures->GetValue() > 0)
					{
						// Base texture AND detail texture
						result = m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_main_detail, false);

						Uint32 detailsamplerindex = m_pShader->AutoSetSamplerUniform(m_attribs.u_detailtex);
						R_Bind2DTexture(GL_TEXTURE0+detailsamplerindex, pmaterial->ptextures[MT_TX_DETAIL]->palloc->gl_index);

						// Enable detail texcoord
						m_pShader->EnableAttribute(m_attribs.a_dtexcoord);
					}
					else
					{
						// Only main texture
						result = m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_texunit1, false);
						m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
					}
				}
				break;
			case RENDER_TRANSCOLOR:
				{
					// Only color
					result = m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_solidcolor, false);

					// Disable both of these
					m_pShader->DisableAttribute(m_attribs.a_texcoord);
					m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
					for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
					{
						m_pShader->DisableAttribute(m_attribs.a_lmapcoord[k]);
					}
					m_pShader->DisableAttribute(m_attribs.a_styles);
				}
				break;
			case RENDER_TRANSCOLOR_LIT:
				{
					// Only lightmap
					for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
					{
						Uint32 lightnapindex = m_pShader->AutoSetSamplerUniform(m_attribs.u_baselightmap[k]);
						R_Bind2DTexture(GL_TEXTURE0 + lightnapindex, m_ambientLightmapIndexes[k] ? m_ambientLightmapIndexes[k] : m_ambientLightmapIndexes[0]);
					}

					// Enable lightmap coord sends
					for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
					{
						m_pShader->EnableAttribute(m_attribs.a_lmapcoord[k]);
					}
					m_pShader->EnableAttribute(m_attribs.a_styles);

					result = m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_texunit0_x4, false);
				}
				break;
			}

			if(!result)
				return false;

			if((pmaterial->flags & TX_FL_ALPHATEST) && (rendermodeext == RENDER_TRANSALPHA_UNLIT || rendermodeext == RENDER_TRANSALPHA))
			{
				if(!rns.msaa || !rns.mainframe)
				{
					if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_LESSTHAN))
						return false;
				}
				else
				{
					if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_COVERAGE))
						return false;

					glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
					gGLExtF.glSampleCoverage(0.5, GL_FALSE);
				}

				alphaToCoverageEnabled = true;
			}
			else
			{
				if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED))
					return false;
			}
		}
		else
		{
			// Find any cubemaps
			if(g_pCvarCubemaps->GetValue() > 0 && pmaterial->flags & TX_FL_CUBEMAPS)
			{
				pcubemapinfo = gCubemaps.GetIdealCubemap();
				if(gCubemaps.GetInterpolant() <= 1.0)
					pprevcubemapinfo = gCubemaps.GetPrevCubemap();
			}

			// Set up binds
			if(!BindTextures(ptexturehandle, pcubemapinfo, pprevcubemapinfo, cubemapUnit, alphaToCoverageEnabled))
				return false;

			// Bind dynamic lights
			for (Uint32 l = 0; l < MAX_DLIGHTS; l++)
			{
				if (l < num_active_dlights)
				{
					cl_dlight_t* pdlight = active_dlights[l];

					m_pShader->EnableSync(m_attribs.lights[l].u_light_color);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_origin);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_radius);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_cubemap);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_projtexture);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_shadowmap);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_matrix);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_cone_size);
					m_pShader->EnableSync(m_attribs.lights[l].u_light_spotdirection);

					Vector vtransorigin;
					Math::MatMultPosition(rns.view.modelview.Transpose(), pdlight->origin, &vtransorigin);

					Vector color;
					Math::VectorCopy(pdlight->color, color);
					gLightStyles.ApplyLightStyle(pdlight, color);

					m_pShader->SetUniform4f(m_attribs.lights[l].u_light_color, color[0], color[1], color[2], 1.0);
					m_pShader->SetUniform3f(m_attribs.lights[l].u_light_origin, vtransorigin[0], vtransorigin[1], vtransorigin[2]);
					m_pShader->SetUniform1f(m_attribs.lights[l].u_light_radius, pdlight->radius);

					Uint32 projUnit = m_pShader->AutoSetSamplerUniform(m_attribs.lights[l].u_light_projtexture);
					Uint32 shadowUnit = m_pShader->AutoSetSamplerUniform(m_attribs.lights[l].u_light_shadowmap);
					Uint32 cubeUnit = m_pShader->AutoSetSamplerUniform(m_attribs.lights[l].u_light_cubemap);

					if (pdlight->cone_size > 0.0f)
					{
						Vector vforward, vtarget;
						Vector angles = pdlight->angles;
						Common::FixVector(angles);
						Math::AngleVectors(angles, &vforward, nullptr, nullptr);
						Math::VectorMA(pdlight->origin, pdlight->radius, vforward, vtarget);

						Int32 textureIndex = pdlight->textureindex;
						if (textureIndex >= rns.objects.projective_textures.size())
						{
							textureIndex = 0;
						}

						R_Bind2DTexture(GL_TEXTURE0 + projUnit, rns.objects.projective_textures[textureIndex]->palloc->gl_index);

						if (DL_CanShadow(pdlight))
						{
							m_pShader->SetUniform1i(m_attribs.lights[l].u_d_light_shadowmap, TRUE);
							R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, pdlight->getProjShadowMap()->pfbo->ptexture1->gl_index);
						}
						else
						{
							m_pShader->SetUniform1i(m_attribs.lights[l].u_d_light_shadowmap, FALSE);
							R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, 0);
						}

						R_BindCubemapTexture(GL_TEXTURE0_ARB + cubeUnit, 0);

						CMatrix matrix;
						matrix.LoadIdentity();
						matrix.Translate(0.5, 0.5, 0.5);
						matrix.Scale(0.5, 0.5, 1.0);
						Float flsize = tan((M_PI / 360) * pdlight->cone_size);
						matrix.SetFrustum(-flsize, flsize, -flsize, flsize, 1, pdlight->radius);
						matrix.LookAt(pdlight->origin[0], pdlight->origin[1], pdlight->origin[2], vtarget[0], vtarget[1], vtarget[2], 0, 0, Common::IsPitchReversed(angles[PITCH]) ? -1 : 1);

						m_pShader->SetUniformMatrix4fv(m_attribs.lights[l].u_light_matrix, matrix.Transpose());
						m_pShader->SetUniform1f(m_attribs.lights[l].u_light_cone_size, pdlight->cone_size);

						Vector transdirection;
						Math::MatMult(rns.view.modelview.Transpose(), vforward, &transdirection);
						m_pShader->SetUniform3f(m_attribs.lights[l].u_light_spotdirection, transdirection[0], transdirection[1], transdirection[2]);
					}
					else
					{
						m_pShader->SetUniform1f(m_attribs.lights[l].u_light_cone_size, 0.0f);
						R_Bind2DTexture(GL_TEXTURE0 + projUnit, 0);
						R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, 0);

						if (DL_CanShadow(pdlight))
						{
							m_pShader->SetUniform1i(m_attribs.lights[l].u_d_light_shadowmap, TRUE);
							R_BindCubemapTexture(GL_TEXTURE0_ARB + cubeUnit, pdlight->getCubeShadowMap()->pfbo->ptexture1->gl_index);
							glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

							CMatrix matrix;
							matrix.LoadIdentity();
							matrix.Rotate(-90, 1, 0, 0);
							matrix.Rotate(90, 0, 0, 1);
							matrix.Translate(-pdlight->origin[0], -pdlight->origin[1], -pdlight->origin[2]);
							m_pShader->SetUniformMatrix4fv(m_attribs.lights[l].u_light_matrix, matrix.GetMatrix(), true);
						}
						else
						{
							m_pShader->SetUniform1i(m_attribs.lights[l].u_d_light_shadowmap, FALSE);
							R_BindCubemapTexture(GL_TEXTURE0_ARB + cubeUnit, 0);
						}
					}
				}
				else
				{
					m_pShader->SetUniform1i(m_attribs.lights[l].u_d_light_shadowmap, FALSE);
					m_pShader->SetUniform1i(m_attribs.lights[l].u_light_projtexture, 0);
					m_pShader->SetUniform1i(m_attribs.lights[l].u_light_shadowmap, 0);
					m_pShader->SetUniform1i(m_attribs.lights[l].u_light_cubemap, 6);
				}
			}

			// Reset cubemap bind
			if(pcubemapinfo && g_pCvarCubemaps->GetValue() > 0)
			{
				// So it gets synced between drawcalls
				if(!cubematrixSet)
				{
					m_pShader->EnableSync(m_attribs.u_modelmatrix);
					m_pShader->EnableSync(m_attribs.u_inv_modelmatrix);
					cubematrixSet = true;

					CMatrix modelMatrix;
					modelMatrix.LoadIdentity();
					modelMatrix.Rotate(90,  1, 0, 0);// put X going down
					modelMatrix.Rotate(-90,  0, 0, 1); // put Z going up
					modelMatrix.Scale(-1.0, 1.0, 1.0);
					modelMatrix.Translate(-rns.view.v_origin[0], -rns.view.v_origin[1], -rns.view.v_origin[2]);

					// We need to multiply normals with the inverse
					m_pShader->SetUniformMatrix4fv(m_attribs.u_modelmatrix, modelMatrix.GetMatrix());
					m_pShader->SetUniformMatrix4fv(m_attribs.u_inv_modelmatrix, modelMatrix.GetInverse());
				}

				// Parallax correction
				if (pcubemapinfo->use_parallax)
				{
					Vector cam = rns.view.v_origin;
					Vector cubemin = pcubemapinfo->box_mins - cam;
					Vector cubemax = pcubemapinfo->box_maxs - cam;
					Vector cubeorigin = pcubemapinfo->origin - cam;

					m_pShader->SetUniform3f(m_attribs.u_cube_min, cubemin.x, cubemin.y, cubemin.z);
					m_pShader->SetUniform3f(m_attribs.u_cube_max, cubemax.x, cubemax.y, cubemax.z);
					m_pShader->SetUniform3f(m_attribs.u_cube_origin, cubeorigin.x, cubeorigin.y, cubeorigin.z);
				}
				else
				{
					m_pShader->SetUniform3f(m_attribs.u_cube_origin, 0, 0, 0);
					m_pShader->SetUniform3f(m_attribs.u_cube_min, 0, 0, 0);
					m_pShader->SetUniform3f(m_attribs.u_cube_max, 0, 0, 0);
				}

				if(pprevcubemapinfo && pprevcubemapinfo->use_parallax)
				{
					Vector cam = rns.view.v_origin;
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_min, pprevcubemapinfo->box_mins.x - cam.x, pprevcubemapinfo->box_mins.y - cam.y, pprevcubemapinfo->box_mins.z - cam.z);
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_max, pprevcubemapinfo->box_maxs.x - cam.x, pprevcubemapinfo->box_maxs.y - cam.y, pprevcubemapinfo->box_maxs.z - cam.z);
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_origin, pprevcubemapinfo->origin.x - cam.x, pprevcubemapinfo->origin.y - cam.y, pprevcubemapinfo->origin.z - cam.z);
				}
				else
				{
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_origin, 0, 0, 0);
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_min, 0, 0, 0);
					m_pShader->SetUniform3f(m_attribs.u_cube_prev_max, 0, 0, 0);
				}

				glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
			}
			else if(cubematrixSet)
			{
				m_pShader->DisableSync(m_attribs.u_modelmatrix);
				m_pShader->DisableSync(m_attribs.u_inv_modelmatrix);
				cubematrixSet = false;
			}
		}

		R_ValidateShader(m_pShader);

		ptexturehandle = &m_texturesArray[i];
		drawbatch_t *pbatch = &ptexturehandle->single_batches[0];
		for(Uint32 j = 0; j < ptexturehandle->numsinglebatches; j++, pbatch++)
			m_pShader->DrawElements(GL_TRIANGLES, pbatch->end_index-pbatch->start_index, GL_UNSIGNED_INT, BUFFER_OFFSET(pbatch->start_index));

		if(m_pCurrentEntity->curstate.effects & EF_CONVEYOR)
			m_pShader->SetUniform2f(m_attribs.u_uvoffset, 0, 0);

		if(alphaToCoverageEnabled)
		{
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			gGLExtF.glSampleCoverage(1.0, GL_FALSE);
		}

		// Reset cubemap bind
		if(pcubemapinfo && g_pCvarCubemaps->GetValue() > 0)
		{
			R_BindCubemapTexture(GL_TEXTURE0_ARB + cubemapUnit, 0);

			if(pprevcubemapinfo)
				R_BindCubemapTexture(GL_TEXTURE0_ARB + cubemapUnit + 1, 0);

			glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		}

		if(g_pCvarWireFrame->GetValue() >= 1 && !m_isEntityTransparent)
		{
			for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
			{
				m_pShader->DisableAttribute(m_attribs.a_lmapcoord[k]);
			}
			m_pShader->DisableAttribute(m_attribs.a_styles);
			m_pShader->DisableAttribute(m_attribs.a_texcoord);
			m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
			m_pShader->DisableAttribute(m_attribs.a_normal);
			m_pShader->DisableAttribute(m_attribs.a_tangent);
			m_pShader->DisableAttribute(m_attribs.a_binormal);

			if(g_pCvarWireFrame->GetValue() >= 2)
				glDisable(GL_DEPTH_TEST);

			glLineWidth(1);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, 1.0);
			m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_none);
			m_pShader->SetUniform1i(m_attribs.u_d_mrao, FALSE);
			m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, FALSE);
			m_pShader->SetUniform1i(m_attribs.u_d_luminance, FALSE);
			m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_OFF);

			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_solidcolor, false)
				|| !m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED))
				return false;

			R_ValidateShader(m_pShader);

			pbatch = &ptexturehandle->single_batches[0];
			for(Uint32 j = 0; j < ptexturehandle->numsinglebatches; j++, pbatch++)
				m_pShader->DrawElements(GL_TRIANGLES, pbatch->end_index-pbatch->start_index, GL_UNSIGNED_INT, BUFFER_OFFSET(pbatch->start_index));

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			if(g_pCvarWireFrame->GetValue() >= 2)
				glEnable(GL_DEPTH_TEST);

			for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
			{
				m_pShader->EnableAttribute(m_attribs.a_lmapcoord[k]);
			}
			m_pShader->EnableAttribute(m_attribs.a_styles);
			m_pShader->EnableAttribute(m_attribs.a_texcoord);

			if(rns.fog.settings.active)
			{
				if(rns.fog.specialfog)
					m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_fogcoord);
				else
					m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_radial);
			}
		}
	}

	// disable sends on detail texcoord
	m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
	m_pShader->DisableAttribute(m_attribs.a_normal);
	m_pShader->DisableAttribute(m_attribs.a_tangent);
	m_pShader->DisableAttribute(m_attribs.a_binormal);

	m_pShader->SetUniform1i(m_attribs.u_d_mrao, FALSE);
	m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, FALSE);

	// Make sure this gets disabled
	m_pShader->DisableSync(m_attribs.u_modelmatrix);
	m_pShader->DisableSync(m_attribs.u_inv_modelmatrix);

	for (Uint32 i = 0; i < MAX_SURFACE_STYLES; i++)
	{
		m_pShader->DisableAttribute(m_attribs.a_lmapcoord[i]);
	}
	m_pShader->DisableAttribute(m_attribs.a_styles);
	m_pShader->DisableAttribute(m_attribs.a_texcoord);
	m_pShader->DisableAttribute(m_attribs.a_dtexcoord);

	// Make sure to restore these
	if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED, false))
		return false;

	m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, FALSE);
	m_pShader->SetUniform1i(m_attribs.u_d_luminance, FALSE);

	return true;
}

//=============================================
// @brief
//
//=============================================
mtexture_t *CBSPRenderer::TextureAnimation( mtexture_t *pbase, Uint32 frame )
{
	mtexture_t* ptexture = pbase;
	if(frame)
	{
		if(ptexture->palt_anims)
			ptexture = ptexture->palt_anims;
	}
	
	if((ptexture->name[0] != '+') || (!ptexture->anim_total))
		return ptexture;

	Int32 count = 0;
	Int32 relative = static_cast<Uint32>(rns.time*10) % ptexture->anim_total;
	while (ptexture->anim_min > relative || ptexture->anim_max <= relative)
	{
		ptexture = ptexture->panim_next;
		if (!ptexture)
			Con_Printf("TextureAnimation: broken cycle");
		
		count++;
		if (count > 100)
			Con_Printf("TextureAnimation: infinite cycle");
	}

	return ptexture;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::BindTextures( bsp_texture_t* phandle, cubemapinfo_t* pcubemapinfo, cubemapinfo_t* pprevcubemap, GLuint& cubemapUnit, bool& alphaToCoverageEnabled )
{
	Uint32 textureIndex = 0;
	en_material_t* pmaterial = phandle->pmaterial;
	bool bChrome = (m_isEntityTransparent && pmaterial->flags & TX_FL_CHROME);

	bool enableNormal = false;
	bool enableTangent = false;
	bool enableBinormal = false;
	bool mraoTexBound = false;
	bool normalTexBound = false;

	// Reset this to 0
	m_pShader->ResetSamplerIndex();

	en_texture_t* pnormalmap = pmaterial->ptextures[MT_TX_NORMALMAP];
	en_texture_t* pnormalmap2 = pmaterial->ptextures[MT_TX_NORMALMAP2];
	if(m_bumpMaps && pnormalmap && g_pCvarBumpMaps->GetValue() > 0)
	{
		m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, TRUE);

		for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_baselightmap[k]);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, m_ambientLightmapIndexes[k] ? m_ambientLightmapIndexes[k] : m_ambientLightmapIndexes[0]);
			
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_difflightmap[k]);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, m_diffuseLightmapIndexes[k] ? m_diffuseLightmapIndexes[k] : m_diffuseLightmapIndexes[0]);
			
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_lightvecstex[k]);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, m_lightVectorsIndexes[k] ? m_lightVectorsIndexes[k] : m_lightVectorsIndexes[0]);
		}

		textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_normalmap);
		R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pnormalmap->palloc->gl_index);

		if (pmaterial->ptextures[MT_TX_NORMALMAP2])
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_normalmap2);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pnormalmap2->palloc->gl_index);
		}

		normalTexBound = true;
		
		en_texture_t* pmrao = pmaterial->ptextures[MT_TX_MRAO];
		en_texture_t* pmrao2 = pmaterial->ptextures[MT_TX_MRAO2];
		if(pmrao && g_pCvarMrao->GetValue() > 0)
		{
			m_pShader->SetUniform1i(m_attribs.u_d_mrao, TRUE);
			enableNormal = enableBinormal = enableTangent = true;

			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_mrao);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmrao->palloc->gl_index);
			mraoTexBound = true;
		}
		else
		{
			m_pShader->SetUniform1i(m_attribs.u_d_mrao, FALSE);
		}

		if (pmaterial->ptextures[MT_TX_MRAO2])
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_mrao2);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmrao2->palloc->gl_index);
		}
	}
	else
	{
		m_pShader->SetUniform1i(m_attribs.u_d_bumpmapping, FALSE);
		m_pShader->SetUniform1i(m_attribs.u_d_mrao, FALSE);
		
		for (Uint32 k = 0; k < MAX_SURFACE_STYLES; k++)
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_baselightmap[k]);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, m_lightmapIndexes[k] ? m_lightmapIndexes[k] : m_lightmapIndexes[0]);
		}
	}

	// Bind the main texture
	textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture);
	R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_DIFFUSE]->palloc->gl_index);

	if (pmaterial->ptextures[MT_TX_DIFFUSE2])
	{
		textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture2);
		R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_DIFFUSE2]->palloc->gl_index);
	}

	// Bind chrome if present
	if(bChrome)
	{
		Vector vorigin, view_right;
		Math::VectorSubtract(rns.view.v_origin, m_pCurrentEntity->curstate.origin, vorigin);
		Math::VectorCopy(rns.view.v_right, view_right);

		if(m_pCurrentEntity->curstate.angles[0] || m_pCurrentEntity->curstate.angles[1] || m_pCurrentEntity->curstate.angles[2])
		{
			Math::RotateToEntitySpace(m_pCurrentEntity->curstate.angles, vorigin);
			Math::RotateToEntitySpace(m_pCurrentEntity->curstate.angles, view_right);
		}

		m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
		if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_chrome, false))
			return false;

		m_pShader->SetUniform3f(m_attribs.u_vorigin, vorigin[0], vorigin[1], vorigin[2]);
		m_pShader->SetUniform3f(m_attribs.u_vright, view_right[0], view_right[1], view_right[2]);

		textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_chrometex);
		R_Bind2DTexture(GL_TEXTURE0 + textureIndex, m_pChromeTexture->palloc->gl_index);

		enableNormal = true;
	}
	else
	{
		if(pmaterial->flags & TX_FL_ALPHATEST)
		{
			if(!rns.msaa || !rns.mainframe)
			{
				if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_LESSTHAN, false))
					return false;
			}
			else
			{
				if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_COVERAGE, false))
					return false;

				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
				gGLExtF.glSampleCoverage(0.5, GL_FALSE);
				alphaToCoverageEnabled = true;
			}
		}
		else
		{
			if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED, false))
				return false;
		}

		en_texture_t* pdetailtexture = pmaterial->ptextures[MT_TX_DETAIL];
		if( pdetailtexture && m_pCvarDetailTextures->GetValue() > 0)
		{
			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_detailtex, false))
				return false;

			m_pShader->EnableAttribute(m_attribs.a_dtexcoord);

			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_detailtex);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pdetailtexture->palloc->gl_index);
		}
		else
		{
			m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_nodetail, false))
				return false;
		}
	}

	// Manage cubemaps
	if(pmaterial->ptextures[MT_TX_MRAO] && pcubemapinfo && g_pCvarCubemaps->GetValue() > 0)
	{
		if(pmaterial->ptextures[MT_TX_NORMALMAP])
		{
			if(!normalTexBound)
			{
				textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_normalmap);
				R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_NORMALMAP]->palloc->gl_index);
			}

			enableBinormal = enableTangent = true;
		}

		if (pmaterial->ptextures[MT_TX_NORMALMAP2])
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_normalmap2);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_NORMALMAP2]->palloc->gl_index);
		}

		if(!mraoTexBound)
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_mrao);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_MRAO]->palloc->gl_index);
		}

		if (pmaterial->ptextures[MT_TX_MRAO2])
		{
			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_mrao2);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_MRAO2]->palloc->gl_index);
		}

		// Remember the texture unit
		cubemapUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_cubemap);
		R_BindCubemapTexture(GL_TEXTURE0_ARB + cubemapUnit, pcubemapinfo->palloc->gl_index);
		enableNormal = true;

		if(pprevcubemap)
		{
			m_pShader->SetUniform1f(m_attribs.u_interpolant, gCubemaps.GetInterpolant());
			m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_INTERP);

			Uint32 prevUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_cubemap_prev);
			R_BindCubemapTexture(GL_TEXTURE0_ARB + prevUnit, pprevcubemap->palloc->gl_index);
		}
		else
		{
			m_pShader->SetUniform1f(m_attribs.u_interpolant, 0.0);
			m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_ON);
		}
	}
	else
	{
		m_pShader->SetUniform1i(m_attribs.u_d_cubemaps, CUBEMAPS_OFF);
	}

	if(pmaterial->ptextures[MT_TX_LUMINANCE])
	{
		en_texture_t* pluminancetexture = pmaterial->ptextures[MT_TX_LUMINANCE];

		m_pShader->SetUniform1i(m_attribs.u_d_luminance, TRUE);
		textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_luminance);
		R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pluminancetexture->palloc->gl_index);
	}
	else
	{
		m_pShader->SetUniform1i(m_attribs.u_d_luminance, FALSE);
	}

	if(enableNormal)
		m_pShader->EnableAttribute(m_attribs.a_normal);
	else
		m_pShader->DisableAttribute(m_attribs.a_normal);

	if(enableTangent)
		m_pShader->EnableAttribute(m_attribs.a_tangent);
	else
		m_pShader->DisableAttribute(m_attribs.a_tangent);

	if(enableBinormal)
		m_pShader->EnableAttribute(m_attribs.a_binormal);
	else
		m_pShader->DisableAttribute(m_attribs.a_binormal);

	Uint32 texUnit1 = m_pShader->AutoSetSamplerUniform(m_attribs.u_causticstex1);
	Uint32 texUnit2 = m_pShader->AutoSetSamplerUniform(m_attribs.u_causticstex2);

	if (rns.inwater && g_pCvarCaustics->GetValue() >= 1)
	{
		const water_settings_t* psettings = gWaterShader.GetActiveSettings();
		if (psettings && !psettings->cheaprefraction && psettings->causticscale > 0 && psettings->causticstrength > 0 && !rns.objects.caustics_textures.empty())
		{
			GLfloat splane[4] = { static_cast<Float>(0.005) * psettings->causticscale, static_cast<Float>(0.0025) * psettings->causticscale, 0.0f, 0.0f };
			GLfloat tplane[4] = { 0.0f, static_cast<Float>(0.005) * psettings->causticscale, static_cast<Float>(0.0025) * psettings->causticscale, 0.0f };

			Float causticsTime = rns.time * 10.0f * psettings->causticstimescale;
			Int32 causticsCurFrame = static_cast<Int32>(causticsTime) % rns.objects.caustics_textures.size();
			Int32 causticsNextFrame = (causticsCurFrame + 1) % rns.objects.caustics_textures.size();
			Float causticsInterp = causticsTime - static_cast<Int32>(causticsTime);

			R_Bind2DTexture(GL_TEXTURE0 + texUnit1, rns.objects.caustics_textures[causticsCurFrame]->palloc->gl_index);
			R_Bind2DTexture(GL_TEXTURE0 + texUnit2, rns.objects.caustics_textures[causticsNextFrame]->palloc->gl_index);

			m_pShader->SetUniform4f(m_attribs.u_causticsm1, splane[0], splane[1], splane[2], splane[3]);
			m_pShader->SetUniform4f(m_attribs.u_causticsm2, tplane[0], tplane[1], tplane[2], tplane[3]);
			m_pShader->SetUniform1f(m_attribs.u_interpolant, causticsInterp);

			m_pShader->SetUniform4f(m_attribs.u_causticscolor, 
				psettings->fogparams.color[0] * psettings->causticstrength,
				psettings->fogparams.color[1] * psettings->causticstrength,
				psettings->fogparams.color[2] * psettings->causticstrength,
				1.0f);
		}
		else
		{
			m_pShader->SetUniform4f(m_attribs.u_causticscolor, 0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	else
	{
		m_pShader->SetUniform4f(m_attribs.u_causticscolor, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	if(!m_pShader->VerifyDeterminators())
		return false;
	else
		return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawBrushModel( cl_entity_t& entity, bool isstatic )
{
	Int32 rendermode = entity.curstate.rendermode & RENDERMODE_BITMASK;
	Int32 rendermodeext = entity.curstate.rendermode;

	if(rendermodeext != RENDER_NORMAL
		&& entity.curstate.renderamt == 0)
		return true;

	if(entity.curstate.rendertype == RT_WATERSHADER 
		|| entity.curstate.rendertype == RT_MIRROR 
		|| entity.curstate.rendertype == RT_MONITORENTITY
		|| entity.curstate.rendertype == RT_PORTALSURFACE)
		return true;

	m_pCurrentEntity = &entity;

	const brushmodel_t* pmodel = entity.pmodel->getBrushmodel();

	Vector vorigin_local;
	Vector mins, maxs;

	Math::VectorCopy(rns.view.v_origin, vorigin_local);

	// Determine the mins/maxs
	if(R_IsEntityRotated(*m_pCurrentEntity))
	{
		for(Uint32 i = 0; i < 3; i++)
		{
			mins[i] = m_pCurrentEntity->curstate.origin[i] - pmodel->radius;
			maxs[i] = m_pCurrentEntity->curstate.origin[i] + pmodel->radius;
		}
	}
	else
	{
		Math::VectorAdd(m_pCurrentEntity->curstate.origin, pmodel->mins, mins);
		Math::VectorAdd(m_pCurrentEntity->curstate.origin, pmodel->maxs, maxs);
	}

	// Also cull skybox entities now with frustum culling - the exception for 
	// sky ents was an ancient remnant from the Paranoia-type skybox rendering, 
	// and was never removed after that got replaced
	if(rns.view.frustum.CullBBox(mins, maxs))
		return true;

	// Transform to local space the origin
	Math::VectorSubtract(vorigin_local, m_pCurrentEntity->curstate.origin, vorigin_local);
	if(R_IsEntityRotated(*m_pCurrentEntity))
		Math::RotateToEntitySpace(m_pCurrentEntity->curstate.angles, vorigin_local);

	if(!isstatic)
	{
		// Apply the transformation to the
		bool ismoved = R_IsEntityMoved(*m_pCurrentEntity);
		if(ismoved)
		{
			rns.view.modelview.PushMatrix();
			R_RotateForEntity(rns.view.modelview, *m_pCurrentEntity);
		}

		// Prepare and load the matrix
		if(!Prepare())
			return false;

		if(ismoved)
			rns.view.modelview.PopMatrix();
	}

	Int32 highlightEntity = g_pCvarHighlightEntity->GetValue();
	if(highlightEntity != 0 && highlightEntity != m_pCurrentEntity->entindex)
		return true;

	// Apply transparency if any
	if(rendermode == RENDER_TRANSADDITIVE)
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		Float flalpha = R_RenderFxBlend(m_pCurrentEntity)/255.0f;
		m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, flalpha);
		m_pShader->SetUniform3f(m_attribs.u_fogcolor, 0, 0, 0);
	}
	else if(rendermode == RENDER_TRANSTEXTURE)
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		Float flalpha = R_RenderFxBlend(m_pCurrentEntity)/255.0f;
		m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, flalpha);
	}
	else if(rendermode == RENDER_TRANSCOLOR)
	{
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		Float flalpha = R_RenderFxBlend(m_pCurrentEntity)/255.0f;
		Float flr = m_pCurrentEntity->curstate.rendercolor.x/255.0f;
		Float flg = m_pCurrentEntity->curstate.rendercolor.y/255.0f;
		Float flb = m_pCurrentEntity->curstate.rendercolor.z/255.0f;
		m_pShader->SetUniform4f(m_attribs.u_color, flr, flg, flb, flalpha);
	}

	// Batch the surfaces
	msurface_t* psurface = ens.pworld->psurfaces + pmodel->firstmodelsurface;
	for(Uint32 i = 0; i < pmodel->nummodelsurfaces; i++, psurface++)
	{
		plane_t* pplane = psurface->pplane;
		Float dp = Math::DotProduct(vorigin_local, pplane->normal) - pplane->dist;

		if(((psurface->flags & SURF_PLANEBACK) && (dp < -BACKFACE_EPSILON))
			|| (!(psurface->flags & SURF_PLANEBACK) && (dp > BACKFACE_EPSILON)))
		{
			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			BatchSurface(psurface);
		}
	}

	// Draw everything now if it's not a static pass
	if(!isstatic)
	{
		if(!Draw())
			return false;
	}

	// Disable blending
	if(rendermode == RENDER_TRANSADDITIVE
		|| rendermode == RENDER_TRANSTEXTURE
		|| rendermode == RENDER_TRANSCOLOR)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// Set this for decals
	m_pCurrentEntity->visframe = rns.framecount;

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::PrepareVSM( void )
{
	// Load current modelview
	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	// clear all chains
	for(Uint32 i = 0; i < m_texturesArray.size(); i++)
	{
		bsp_texture_t& texture = m_texturesArray[i];

		texture.numsinglebatches = 0;
		texture.psurfchain = nullptr;
	}
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawVSMFaces( void )
{
	// Render normal ones first
	for(Uint32 i = 0; i < m_texturesArray.size(); i++)
	{
		if(!m_texturesArray[i].pmodeltexture)
			continue;

		// Nothing to draw
		if(!m_texturesArray[i].numsinglebatches)
			continue;

		// Get the animated texture
		mtexture_t *pworldtexture = TextureAnimation(m_texturesArray[i].pmodeltexture, m_pCurrentEntity->curstate.frame);
		bsp_texture_t *ptexturehandle = &m_texturesArray[pworldtexture->infoindex];
		en_material_t* pmaterial = ptexturehandle->pmaterial;

		if (pmaterial->ptextures[MT_TX_DIFFUSE2])
			m_pShader->SetDeterminator(m_attribs.d_blended, 1, false);
		else
			m_pShader->SetDeterminator(m_attribs.d_blended, 0, false);

		m_pShader->ResetSamplerIndex();

		if(pmaterial->flags & TX_FL_ALPHATEST)
		{
			Int32 textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_DIFFUSE]->palloc->gl_index);

			if (pmaterial->ptextures[MT_TX_DIFFUSE2])
			{
				textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture2);
				R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pmaterial->ptextures[MT_TX_DIFFUSE2]->palloc->gl_index);
			}

			m_pShader->EnableAttribute(m_attribs.a_texcoord);
			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_vsm_alpha))
				return false;

			R_ValidateShader(m_pShader);

			ptexturehandle = &m_texturesArray[i];
			drawbatch_t *pbatch = &ptexturehandle->single_batches[0];
			for(Uint32 j = 0; j < ptexturehandle->numsinglebatches; j++, pbatch++)
				m_pShader->DrawElements(GL_TRIANGLES, pbatch->end_index-pbatch->start_index, GL_UNSIGNED_INT, BUFFER_OFFSET(pbatch->start_index));

			// Restore and disable
			m_pShader->DisableAttribute(m_attribs.a_texcoord);
		}
		else
		{
			if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_vsm_store))
				return false;

			R_ValidateShader(m_pShader);

			ptexturehandle = &m_texturesArray[i];
			drawbatch_t *pbatch = &ptexturehandle->single_batches[0];
			for(Uint32 j = 0; j < ptexturehandle->numsinglebatches; j++, pbatch++)
				m_pShader->DrawElements(GL_TRIANGLES, pbatch->end_index-pbatch->start_index, GL_UNSIGNED_INT, BUFFER_OFFSET(pbatch->start_index));
		}
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawVSM( cl_dlight_t *dl, cl_entity_t** pvisents, Uint32 numentities, bool drawworld )
{
	// Set shader's VBO and bind it
	m_pShader->SetVBO(m_pVBO);

	if(!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_alpha);
	m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_none);

	// Set static uniforms
	m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, 1.0);
	m_pShader->SetUniform1f(m_attribs.u_light_radius, dl->radius);
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);

	// Set initial entity to world
	m_pCurrentEntity = CL_GetEntityByIndex(WORLDSPAWN_ENTITY_INDEX);
	m_isEntityTransparent = false;

	PrepareVSM();

	if(drawworld)
		RecursiveWorldNode(ens.pworld->pnodes);

	// Check for errors
	bool result = true;

	// render non-moved ents here
	if(g_pCvarDrawEntities->GetValue() >= 1)
	{
		// Draw all static entities
		for (Uint32 i = 0; i < numentities; i++)
		{
			cl_entity_t *pEntity = pvisents[i];

			if(pEntity->pmodel->type != MOD_BRUSH)
				continue;

			if(R_IsEntityMoved(*pEntity))
				continue;

			if(R_IsEntityTransparent(*pEntity))
				continue;

			if(pEntity->curstate.renderfx == RenderFx_SkyEnt ||
				pEntity->curstate.renderfx == RenderFx_SkyEntScaled ||
				pEntity->curstate.renderfx == RenderFx_SkyEntNC ||
				pEntity->curstate.renderfx == RenderFx_NoShadow ||
				pEntity->curstate.rendertype == RT_WATERSHADER ||
				pEntity->curstate.rendertype == RT_MIRROR ||
				pEntity->curstate.rendertype == RT_MONITORENTITY ||
				pEntity->curstate.rendertype == RT_PORTALSURFACE)
				continue;

			result = DrawBrushModel(*pEntity, true);
			if(!result)
				break;
		}
	}

	// Reset entity to world
	m_pCurrentEntity = CL_GetEntityByIndex(WORLDSPAWN_ENTITY_INDEX);
	m_isEntityTransparent = false;

	// Render all statics to vsm
	if(result)
		result = DrawVSMFaces();

	if(g_pCvarDrawEntities->GetValue() >= 1 && result)
	{
		// Now render moved entities seperately each
		for (Uint32 i = 0; i < numentities; i++)
		{
			cl_entity_t *pEntity = pvisents[i];

			if(pEntity->pmodel->type != MOD_BRUSH)
				continue;

			if(dl->isStatic() && !(pEntity->curstate.effects & EF_STATICENTITY))
				continue;

			if(!R_IsEntityMoved(*pEntity))
				continue;

			if(R_IsEntityTransparent(*pEntity))
				continue;

			if(pEntity->curstate.renderfx == RenderFx_SkyEnt)
				continue;

			if (pEntity->curstate.renderfx == RenderFx_SkyEntScaled)
				continue;

			if(pEntity->curstate.renderfx == RenderFx_SkyEntNC)
				continue;

			if(pEntity->curstate.rendertype == RT_WATERSHADER)
				continue;

			if(pEntity->curstate.rendertype == RT_MIRROR)
				continue;

			if(pEntity->curstate.rendertype == RT_MONITORENTITY)
				continue;

			if (pEntity->curstate.rendertype == RT_PORTALSURFACE)
				continue;

			result = BatchBrushModelForVSM(*pEntity, false);
			if(!result)
				break;
		}
	}

	m_pShader->DisableShader();

	if(!result)
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	return result;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::BatchBrushModelForVSM( cl_entity_t& entity, bool isstatic )
{
	if(entity.curstate.rendermode != RENDER_NORMAL 
		&& entity.curstate.renderamt == 0)
		return true;

	if(entity.curstate.rendertype == RT_WATERSHADER 
		|| entity.curstate.rendertype == RT_MIRROR 
		|| entity.curstate.rendertype == RT_MONITORENTITY
		|| entity.curstate.rendertype == RT_PORTALSURFACE)
		return true;

	m_pCurrentEntity = &entity;
	m_isEntityTransparent = false;

	const brushmodel_t* pmodel = entity.pmodel->getBrushmodel();

	Vector vorigin_local;
	Vector mins, maxs;

	Math::VectorCopy(rns.view.v_origin, vorigin_local);

	// Determine the mins/maxs
	if(R_IsEntityRotated(*m_pCurrentEntity))
	{
		for(Uint32 i = 0; i < 3; i++)
		{
			mins[i] = m_pCurrentEntity->curstate.origin[i] - pmodel->radius;
			maxs[i] = m_pCurrentEntity->curstate.origin[i] + pmodel->radius;
		}
	}
	else
	{
		Math::VectorAdd(m_pCurrentEntity->curstate.origin, pmodel->mins, mins);
		Math::VectorAdd(m_pCurrentEntity->curstate.origin, pmodel->maxs, maxs);
	}

	// Do not culling on skybox entities
	if(m_pCurrentEntity->curstate.renderfx != RenderFx_SkyEnt &&
		m_pCurrentEntity->curstate.renderfx != RenderFx_SkyEntScaled && 
		rns.view.frustum.CullBBox(mins, maxs))
		return true;

	// Transform to local space the origin
	Math::VectorSubtract(vorigin_local, m_pCurrentEntity->curstate.origin, vorigin_local);
	if(R_IsEntityRotated(*m_pCurrentEntity))
		Math::RotateToEntitySpace(m_pCurrentEntity->curstate.angles, vorigin_local);

	if(!isstatic)
	{
		// Apply the transformation to the
		bool ismoved = R_IsEntityMoved(*m_pCurrentEntity);
		if(ismoved)
		{
			rns.view.modelview.PushMatrix();
			R_RotateForEntity(rns.view.modelview, *m_pCurrentEntity);
		}

		// Prepare and load the matrix
		PrepareVSM();
		
		if(ismoved)
			rns.view.modelview.PopMatrix();
	}

	// Batch the surfaces
	msurface_t* psurface = ens.pworld->psurfaces + pmodel->firstmodelsurface;
	for(Uint32 i = 0; i < pmodel->nummodelsurfaces; i++, psurface++)
	{
		plane_t* pplane = psurface->pplane;
		Float dp = Math::DotProduct(vorigin_local, pplane->normal) - pplane->dist;

		if(((psurface->flags & SURF_PLANEBACK) && (dp < -BACKFACE_EPSILON))
			|| (!(psurface->flags & SURF_PLANEBACK) && (dp > BACKFACE_EPSILON)))
		{
			if(psurface->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
				continue;

			BatchSurface(psurface);
		}
	}

	if(!isstatic)
	{
		if(!DrawVSMFaces())
			return false;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::CreateDecal( const Vector& origin, const Vector& normal, decalgroupentry_t* pentry, byte flags, Float life, Float fadetime, Float growthtime )
{
	// Make sure the texture is loaded
	if(!pentry->ptexture)
	{
		gDecals.PrecacheTexture(pentry->name.c_str());
		if(!pentry->ptexture)
		{
			Con_Printf("%s - Could not load texture for entry '%s'.\n", __FUNCTION__, pentry->name.c_str());
			return;
		}
	}

	// Set mins/maxs
	Vector mins, maxs;
	Vector decalmins, decalmaxs;
	Float radius = (pentry->xsize > pentry->ysize) ? pentry->xsize : pentry->ysize;
	for(Uint32 i = 0; i < 3; i++)
	{
		decalmins[i] = origin[i]-radius;
		decalmaxs[i] = origin[i]+radius;
	}

	// Allocate decal
	bsp_decal_t *pdecal = nullptr;
	if (flags & FL_DECAL_PERSISTENT)
	{
		pdecal = new bsp_decal_t;
		m_staticDecalsArray.push_back(pdecal);
	}
	else
	{
		if(!(flags & FL_DECAL_ALLOWOVERLAP))
		{
			Uint32 counter = 0;
			m_decalsList.begin();
			while(!m_decalsList.end())
			{
				bsp_decal_t* pdecalcheck = m_decalsList.get();
				if(pdecalcheck->ptexinfo->pgroup != pentry->pgroup)
				{
					m_decalsList.next();
					continue;
				}

				const decalgroupentry_t *pdecalentry = pdecalcheck->ptexinfo;
				Float fldecalrad = (pdecalentry->xsize > pdecalentry->ysize) ? pdecalentry->xsize : pdecalentry->ysize;
				for(Uint32 i = 0; i < 3; i++)
				{
					mins[i] = pdecalcheck->origin[i]-fldecalrad;
					maxs[i] = pdecalcheck->origin[i]+fldecalrad;
				}

				if(!Math::CheckMinsMaxs(mins, maxs, decalmins, decalmaxs))
				{
					counter++;

					if(counter == MAX_DECAL_OVERLAP)
					{
						DeleteDecal(pdecalcheck);
						counter--;
					}
				}

				m_decalsList.next();
			}
		}

		pdecal = new bsp_decal_t();
		m_decalsList.add(pdecal);
	}

	if(!pdecal)
		return;

	pdecal->ptexinfo = pentry;
	pdecal->spawntime = CL_GetClientTime();
	pdecal->growthtime = growthtime;

	if(!(flags & FL_DECAL_PERSISTENT) && life > 0)
	{
		pdecal->life = life;
		pdecal->fadetime = fadetime;
	}

	Math::VectorCopy(origin, pdecal->origin);
	Math::VectorCopy(normal, pdecal->normal);

	m_pCurrentEntity = nullptr;
	RecursivePasteDecal(ens.pworld->pnodes, pdecal, flags, decalmins, decalmaxs);

	Vector localorigin, localnormal;
	for(Uint32 i = 1; i < MAX_RENDER_ENTITIES; i++)
	{
		cl_entity_t *pentity = CL_GetEntityByIndex(i);

		if ( !pentity )
			break;

		if ( !pentity->pmodel )
			continue;
		
		if ( pentity->pmodel->type != MOD_BRUSH )
			continue;

		if( pentity->curstate.rendermode != RENDER_NORMAL && !pentity->curstate.renderamt )
			continue;

		if(R_IsEntityMoved((*pentity)))
		{
			Math::VectorSubtract(origin, pentity->curstate.origin, localorigin);
			if(pentity->curstate.angles[0] || pentity->curstate.angles[1] || pentity->curstate.angles[2])
			{
				Math::RotateToEntitySpace(pentity->curstate.angles, localorigin);
				Math::RotateToEntitySpace(pentity->curstate.angles, localnormal);
			}
			else
			{
				// Just copy the normal
				Math::VectorCopy(normal, localnormal);
			}
		}
		else
		{
			Math::VectorCopy(normal, localnormal);
			Math::VectorCopy(origin, localorigin);
		}

		for(Uint32 j = 0; j < 3; j++)
		{
			mins[j] = localorigin[j]-radius;
			maxs[j] = localorigin[j]+radius;
		}

		if (Math::CheckMinsMaxs(pentity->pmodel->mins, pentity->pmodel->maxs, mins, maxs))
			continue;

		m_pCurrentEntity = pentity;
		const brushmodel_t* pbrushmodel = pentity->pmodel->getBrushmodel();
		msurface_t *psurfaces = ens.pworld->psurfaces + pbrushmodel->firstmodelsurface;

		// Process non-alphatested first
		for(Uint32 k = 0; k < pbrushmodel->nummodelsurfaces; k++)
		{
			msurface_t* psurf = &psurfaces[k];

			bsp_texture_t* ptexture = &m_texturesArray[psurf->ptexinfo->ptexture->infoindex];
			if(ptexture->pmaterial->flags & TX_FL_NODECAL)
				continue;

			if(ptexture->pmaterial->flags & TX_FL_ALPHATEST)
				continue;

			Float dot;
			plane_t *pplane = psurf->pplane;
			dot = Math::DotProduct(localorigin, pplane->normal) - pplane->dist;

			if(dot < 0)
				dot *= -1;

			if(dot < radius)
			{
				if(!(flags & FL_DECAL_NORMAL_PERMISSIVE))
				{
					Vector planenormal = pplane->normal;
					if(psurf->flags & SURF_PLANEBACK)
						Math::VectorScale(planenormal, -1, planenormal);

					if( Math::DotProduct(planenormal, localnormal) < 0.01 )
						continue;
				}

				DecalSurface(psurf, pdecal, localnormal, localorigin, false);
			}
		}

		// Process alpha tested per texture
		for(Uint32 k = 0; k < m_texturesArray.size(); k++)
		{
			bsp_texture_t* ptexture = &m_texturesArray[k];
			if(ptexture->pmaterial->flags & TX_FL_NODECAL)
				continue;

			if(!(ptexture->pmaterial->flags & TX_FL_ALPHATEST))
				continue;

			for(Uint32 l = 0; l < pbrushmodel->nummodelsurfaces; l++)
			{
				msurface_t* psurf = &psurfaces[l];
				if(psurf->ptexinfo->ptexture->infoindex != static_cast<Int32>(k))
					continue;

				Float dot;
				plane_t *pplane = psurf->pplane;
				dot = Math::DotProduct(localorigin, pplane->normal) - pplane->dist;

				if(dot < 0)
					dot *= -1;

				if(dot < radius)
				{
					if(!(flags & FL_DECAL_NORMAL_PERMISSIVE))
					{
						Vector planenormal = pplane->normal;
						if(psurf->flags & SURF_PLANEBACK)
							Math::VectorScale(planenormal, -1, planenormal);

						if( Math::DotProduct(planenormal, localnormal) < 0.01 )
							continue;
					}

					DecalSurface(psurf, pdecal, localnormal, localorigin, true);
				}
			}
		}
	}

	if(pdecal->polygroups.empty())
	{
		if(!(flags & FL_DECAL_PERSISTENT))
		{
			DeleteDecal(pdecal);
		}
		else
		{
			// Remove the static decal
			m_staticDecalsArray.resize(m_staticDecalsArray.size()-1);
		}

		// oh god I missed this
		return;
	}

	// Restore mins/maxs before finding touched leaves
	for(Uint32 i = 0; i < 3; i++)
	{
		mins[i] = origin[i]-radius;
		maxs[i] = origin[i]+radius;
	}

	Mod_FindTouchedLeafs(ens.pworld, pdecal->leafnums, pdecal->numleafs, mins, maxs, ens.pworld->pnodes);
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::DeleteDecal( bsp_decal_t *pdecal )
{
	// Remove it from the list
	m_decalsList.remove(pdecal);
	RemoveDecalFromVBO(pdecal);
	delete pdecal;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::RemoveDecalFromVBO( bsp_decal_t *pdelete )
{
	if(pdelete->polygroups.empty())
		return;

	decalpolygroup_t* pfirstgroup = pdelete->polygroups[0];
	decalpolygroup_t* plastgroup = pdelete->polygroups[pdelete->polygroups.size()-1];

	Uint32 vertexstart = pfirstgroup->start_vertex;
	Uint32 vertexend = plastgroup->start_vertex + plastgroup->num_vertexes;
	Uint32 nbremoved = vertexend - vertexstart;

	assert(vertexend <= m_vertexCacheIndex);
	Uint32 nbshift = m_vertexCacheIndex - vertexend;
	m_vertexCacheIndex -= nbremoved;

	// See if there's anything to shift
	if(nbshift > 0)
	{
		const bsp_vertex_t* pvertexdata = static_cast<const bsp_vertex_t*>(m_pDecalVBO->GetVBOData());
		const bsp_vertex_t* psrcdata = pvertexdata + vertexend;

		Uint32 offsetsize = sizeof(bsp_vertex_t)*vertexstart;
		Uint32 sizemove = sizeof(bsp_vertex_t)*nbshift;

		m_pDecalVBO->VBOSubBufferData(offsetsize, psrcdata, sizemove);

		// Now shift existing decals
		m_decalsList.push_iterator();
		m_decalsList.begin();
		while(!m_decalsList.end())
		{
			bsp_decal_t* pdecal = m_decalsList.get();
			for(Uint32 i = 0; i < pdecal->polygroups.size(); i++)
			{
				decalpolygroup_t* pgroup = pdecal->polygroups[i];
				if(pgroup->start_vertex >= vertexstart)
					pgroup->start_vertex -= nbremoved;
			}

			m_decalsList.next();
		}
		m_decalsList.pop_iterator();

		// Shift static ones too
		for(Uint32 i = 0; i < m_staticDecalsArray.size(); i++)
		{
			bsp_decal_t* pdecal = m_staticDecalsArray[i];
			for(Uint32 j = 0; j < pdecal->polygroups.size(); j++)
			{
				decalpolygroup_t* pgroup = pdecal->polygroups[j];
				if(pgroup->start_vertex >= vertexstart)
					pgroup->start_vertex -= nbremoved;
			}
		}
	}
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::RecursivePasteDecal( mnode_t *node, bsp_decal_t *pdecal, byte flags, const Vector& mins, const Vector& maxs )
{
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->contents < 0)
		return;

	if(Math::CheckMinsMaxs(node->mins, node->maxs, mins, maxs))
		return;

	Int32 side;
	Float dot;
	plane_t *plane = node->pplane;

	switch (plane->type)
	{
		case PLANE_X:
			dot = pdecal->origin[0] - plane->dist;	break;
		case PLANE_Y:
			dot = pdecal->origin[1] - plane->dist;	break;
		case PLANE_Z:
			dot = pdecal->origin[2] - plane->dist;	break;
		default:
			dot = Math::DotProduct(pdecal->origin, plane->normal) - plane->dist; 
			break;
	}

	if (dot >= 0) 
		side = 0;
	else 
		side = 1;

	// recurse down the children, front side first
	RecursivePasteDecal(node->pchildren[side], pdecal, flags, mins, maxs);

	// draw stuff
	if (node->numsurfaces)
	{
		if (dot < 0 -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		Int32 xsize = pdecal->ptexinfo->xsize;
		Int32 ysize = pdecal->ptexinfo->ysize;

		Float radius = (xsize > ysize) ? xsize : ysize;

		msurface_t *surf = ens.pworld->psurfaces + node->firstsurface;
		for (Uint32 i = 0; i < node->numsurfaces; i++, surf++)
		{	
			plane_t *pplane = surf->pplane;
			Float dp = Math::DotProduct(pdecal->origin, pplane->normal) - pplane->dist;

			if(dp < 0)
				dp *= -1;

			if(dp < radius)
			{
				if(!(flags & FL_DECAL_NORMAL_PERMISSIVE))
				{
					Vector planenormal = pplane->normal;
					if(surf->flags & SURF_PLANEBACK)
						Math::VectorScale(planenormal, -1, planenormal);

					if(Math::DotProduct(planenormal, pdecal->normal) < 0.01)
						continue;
				}

				DecalSurface(surf, pdecal, pdecal->normal, pdecal->origin, false);
			}
		}
	}

	RecursivePasteDecal (node->pchildren[!side], pdecal, flags, mins, maxs);
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::DecalSurface( const msurface_t *surf, bsp_decal_t *pdecal, const Vector& normal, const Vector& origin, bool transparent )
{
	Vector right, up, tmp;

	Int32 numverts = 0;
	static Vector dverts1[64];
	static Vector dverts2[64];

	// Disregard water and sky
	if(surf->flags & SURF_DRAWTURB || surf->flags & SURF_DRAWSKY)
		return;

	// Extract vertexes
	CArray<CArray<Vector>> polygonsToClip;

	// If we are displacement
	if (surf->displacement_id != -1)
	{
		const mdispinfo_t& info = ens.pworld->pdispinfo[surf->displacement_id];
		Uint32 side = (1 << info.power) + 1;

		Vector corners[4];
		for (Uint32 c = 0; c < 4; c++)
			corners[c] = Vector(info.corners[c][0], info.corners[c][1], info.corners[c][2]);

		CArray<Vector> dispVerts(side * side);
		for (Uint32 y = 0; y < side; y++)
		{
			for (Uint32 x = 0; x < side; x++)
			{
				Float fr_x = static_cast<Float>(x) / (side - 1);
				Float fr_y = static_cast<Float>(y) / (side - 1);

				Vector top, bot, pos;
				Math::VectorAdd(corners[0], (corners[1] - corners[0]) * fr_x, top);
				Math::VectorAdd(corners[3], (corners[2] - corners[3]) * fr_x, bot);
				Math::VectorAdd(top, (bot - top) * fr_y, pos);

				Int32 v_idx = info.vert_start + (y * side + x);
				Math::VectorMA(pos, ens.pworld->pdispverts[v_idx].distance, ens.pworld->pdispverts[v_idx].vector, dispVerts[y * side + x]);
			}
		}

		for (Uint32 y = 0; y < side - 1; y++)
		{
			for (Uint32 x = 0; x < side - 1; x++)
			{
				CArray<Vector> tri1(3);
				tri1[0] = dispVerts[y * side + x];
				tri1[1] = dispVerts[(y + 1) * side + (x + 1)];
				tri1[2] = dispVerts[(y + 1) * side + x];
				polygonsToClip.push_back(tri1);

				CArray<Vector> tri2(3);
				tri2[0] = dispVerts[y * side + x];
				tri2[1] = dispVerts[y * side + (x + 1)];
				tri2[2] = dispVerts[(y + 1) * side + (x + 1)];
				polygonsToClip.push_back(tri2);
			}
		}
	}
	else
	{
		CArray<Vector> facePoly(surf->numedges);
		for(Uint32 i = 0; i < surf->numedges; i++)
		{
			Int32 e_index = ens.pworld->psurfedges[surf->firstedge+i];
			if(e_index > 0)
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, facePoly[i]);
			else
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, facePoly[i]);
		}
		polygonsToClip.push_back(facePoly);
	}

	Math::GetUpRight(normal, up, right);

	Int32 xsize = pdecal->ptexinfo->xsize;
	Int32 ysize = pdecal->ptexinfo->ysize;

	Float texc_orig_x = Math::DotProduct(origin, right);
	Float texc_orig_y = Math::DotProduct(origin, up);

	for (Uint32 polyIdx = 0; polyIdx < polygonsToClip.size(); polyIdx++)
	{
		const CArray<Vector>& srcPoly = polygonsToClip[polyIdx];
		for (Uint32 i = 0; i < srcPoly.size(); i++)
			dverts1[i] = srcPoly[i];

		Int32 nv;
		Vector planepoint;
		Math::VectorMA(origin, -xsize, right, planepoint);
		nv = Decal_ClipPolygon(dverts1, srcPoly.size(), right, planepoint, dverts2);
		if (nv < 3)
			continue;

		Math::VectorMA(origin, xsize, right, planepoint);
		Math::VectorScale(right, -1, tmp);
		nv = Decal_ClipPolygon(dverts2, nv, tmp, planepoint, dverts1);
		if (nv < 3)
			continue;

		Math::VectorMA(origin, -ysize, up, planepoint);
		nv = Decal_ClipPolygon(dverts1, nv, up, planepoint, dverts2);
		if (nv < 3)
			continue;

		Math::VectorMA(origin, ysize, up, planepoint);
		Math::VectorScale(up, -1, tmp);
		nv = Decal_ClipPolygon(dverts2, nv, tmp, planepoint, dverts1);
		if (nv < 3)
			continue;

		// see if we have enough space
		numverts = 3 + (nv - 3) * 3;
		Int32 ioffset = GetDecalOffset(numverts);

		// See if we need to allocate
		decalpolygroup_t* pgroup = nullptr;
		if (!pdecal->polygroups.empty())
			pgroup = pdecal->polygroups[pdecal->polygroups.size() - 1];

		if (!pgroup || ioffset < pgroup->start_vertex
			|| pgroup->pentity != m_pCurrentEntity
			|| pgroup->alphatest != transparent
			|| pgroup->alphatest && pgroup->ptexture != surf->ptexinfo->ptexture)
		{
			pgroup = new decalpolygroup_t;
			pdecal->polygroups.push_back(pgroup);
			pgroup->pentity = m_pCurrentEntity;
			pgroup->start_vertex = ioffset;
			pgroup->alphatest = transparent;
			pgroup->localmins = NULL_MINS;
			pgroup->localmaxs = NULL_MAXS;
			pgroup->localorigin = pdecal->origin;

			if (m_pCurrentEntity && R_IsEntityMoved(*m_pCurrentEntity))
			{
				Math::VectorSubtract(pgroup->localorigin, m_pCurrentEntity->curstate.origin, pgroup->localorigin);
				if (m_pCurrentEntity)
					Math::RotateToEntitySpace(m_pCurrentEntity->curstate.angles, pgroup->localorigin);
			}

			if (pgroup->alphatest)
				pgroup->ptexture = surf->ptexinfo->ptexture;
		}

		if (static_cast<Int32>(m_tempDecalVertsArray.size()) < numverts)
		{
			const Uint32 currentsize = m_tempDecalVertsArray.size();
			m_tempDecalVertsArray.resize(currentsize + numverts);
		}

		// triangulate
		Int32 curvert = 0;
		Vector* pvert = dverts1;
		bsp_vertex_t pconvverts[3];
		for (Int32 j = 0; j < 3; j++, pvert++)
		{
			Float texc_x = (Math::DotProduct(*pvert, right) - texc_orig_x) / xsize;
			Float texc_y = (Math::DotProduct(*pvert, up) - texc_orig_y) / ysize;

			pconvverts[j].texcoord[0] = ((texc_x + 1) / 2);
			pconvverts[j].texcoord[1] = ((texc_y + 1) / 2);

			pconvverts[j].origin[0] = (*pvert)[0];
			pconvverts[j].origin[1] = (*pvert)[1];
			pconvverts[j].origin[2] = (*pvert)[2];
			pconvverts[j].origin[3] = 1.0;

			for (Uint32 k = 0; k < 3; k++)
			{
				if (pconvverts[j].origin[k] < pgroup->localmins[k])
					pgroup->localmins[k] = pconvverts[j].origin[k];

				if (pconvverts[j].origin[k] > pgroup->localmaxs[k])
					pgroup->localmaxs[k] = pconvverts[j].origin[k];
			}

			if (transparent)
			{
				pconvverts[j].dtexcoord[0] = Math::DotProduct(&pconvverts[j].origin[0], surf->ptexinfo->vecs[0]) + surf->ptexinfo->vecs[0][3];
				pconvverts[j].dtexcoord[0] /= static_cast<Float>(surf->ptexinfo->ptexture->width);

				pconvverts[j].dtexcoord[1] = Math::DotProduct(&pconvverts[j].origin[0], surf->ptexinfo->vecs[1]) + surf->ptexinfo->vecs[1][3];
				pconvverts[j].dtexcoord[1] /= static_cast<Float>(surf->ptexinfo->ptexture->height);
			}
		}

		memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[0], sizeof(bsp_vertex_t)); curvert++;
		memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[1], sizeof(bsp_vertex_t)); curvert++;
		memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[2], sizeof(bsp_vertex_t)); curvert++;

		for (Int32 j = 0; j < (nv - 3); j++, pvert++)
		{
			memcpy(&pconvverts[1], &pconvverts[2], sizeof(bsp_vertex_t));

			Float texc_x = (Math::DotProduct(*pvert, right) - texc_orig_x) / xsize;
			Float texc_y = (Math::DotProduct(*pvert, up) - texc_orig_y) / ysize;
			pconvverts[2].texcoord[0] = ((texc_x + 1) / 2);
			pconvverts[2].texcoord[1] = ((texc_y + 1) / 2);

			pconvverts[2].origin[0] = (*pvert)[0];
			pconvverts[2].origin[1] = (*pvert)[1];
			pconvverts[2].origin[2] = (*pvert)[2];
			pconvverts[2].origin[3] = 1.0;

			for (Uint32 k = 0; k < 3; k++)
			{
				if (pconvverts[2].origin[k] < pgroup->localmins[k])
					pgroup->localmins[k] = pconvverts[2].origin[k];

				if (pconvverts[2].origin[k] > pgroup->localmaxs[k])
					pgroup->localmaxs[k] = pconvverts[2].origin[k];
			}

			if (transparent)
			{
				pconvverts[2].dtexcoord[0] = Math::DotProduct(&pconvverts[2].origin[0], surf->ptexinfo->vecs[0]) + surf->ptexinfo->vecs[0][3];
				pconvverts[2].dtexcoord[0] /= static_cast<Float>(surf->ptexinfo->ptexture->width);

				pconvverts[2].dtexcoord[1] = Math::DotProduct(&pconvverts[2].origin[0], surf->ptexinfo->vecs[1]) + surf->ptexinfo->vecs[1][3];
				pconvverts[2].dtexcoord[1] /= static_cast<Float>(surf->ptexinfo->ptexture->height);
			}

			memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[0], sizeof(bsp_vertex_t)); curvert++;
			memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[1], sizeof(bsp_vertex_t)); curvert++;
			memcpy(&m_tempDecalVertsArray[curvert], &pconvverts[2], sizeof(bsp_vertex_t)); curvert++;
		}

		for (Uint32 j = 0; j < 3; j++)
		{
			Float size = pgroup->localmaxs[j] - pgroup->localmins[j];
			if (size > pgroup->radius)
				pgroup->radius = size;
		}

		m_pDecalVBO->VBOSubBufferData(ioffset * sizeof(bsp_vertex_t), &m_tempDecalVertsArray[0], numverts * sizeof(bsp_vertex_t));
		pgroup->num_vertexes += numverts;
	}
}

//=============================================
// @brief
//
//=============================================
Uint32 CBSPRenderer::GetDecalOffset( Uint32 numverts )
{
	Uint32 cacheMaxOffset = (m_vertexCacheIndex-m_vertexCacheBase)+numverts;
	if(cacheMaxOffset > m_vertexCacheSize)
	{
		Uint32 missingSize = cacheMaxOffset - m_vertexCacheSize;
		Uint32 allocSize = missingSize / BSP_DECALVERT_ALLOC_SIZE;
		if(missingSize % BSP_DECALVERT_ALLOC_SIZE != 0)
			allocSize += BSP_DECALVERT_ALLOC_SIZE;

		// Expand VBO
		bsp_vertex_t *pvertexes = new bsp_vertex_t[allocSize];
		m_pDecalVBO->Append(pvertexes, sizeof(bsp_vertex_t)*allocSize, nullptr, 0);
		m_vertexCacheSize += allocSize;
		delete[] pvertexes;
	}

	Int32 offset = m_vertexCacheIndex;
	m_vertexCacheIndex += numverts;

	return offset;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawDecal( bsp_decal_t *pdecal, bool transparents, decal_rendermode_t& rendermode )
{
	for(Uint32 i = 0; i < pdecal->polygroups.size(); i++)
	{
		decalpolygroup_t *pgroup = pdecal->polygroups[i];

		if(transparents && !pgroup->pentity)
			continue;

		Vector mins, maxs;
		if(pgroup->pentity)
		{
			if(R_IsEntityRotated((*pgroup->pentity)))
			{
				Vector origin = pgroup->localorigin;
				Math::RotateFromEntitySpace(pgroup->pentity->curstate.angles, origin);

				for(Uint32 j = 0; j < 3; j++)
				{
					mins[j] = pgroup->pentity->curstate.origin[j] + (origin[j] - pgroup->radius);
					maxs[j] = pgroup->pentity->curstate.origin[j] + (origin[j] + pgroup->radius);
				}
			}
			else
			{
				Math::VectorAdd(pgroup->localmins, pgroup->pentity->curstate.origin, mins);
				Math::VectorAdd(pgroup->localmaxs, pgroup->pentity->curstate.origin, maxs);
			}
		}
		else
		{
			mins = pgroup->localmins;
			maxs = pgroup->localmaxs;
		}

		if(rns.view.frustum.CullBBox(mins, maxs))
			continue;

		// Determine rendering method required
		decal_rendermode_t requestedRendermode;
		if(pgroup->alphatest && pgroup->ptexture)
			requestedRendermode = DECAL_RENDERMODE_ALPHATEST;
		else
			requestedRendermode = DECAL_RENDERMODE_NORMAL;

		// Switch only if needed
		if(requestedRendermode != rendermode)
		{
			switch(requestedRendermode)
			{
			case DECAL_RENDERMODE_ALPHATEST:
				{
					m_pShader->EnableAttribute(m_attribs.a_dtexcoord);
					if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_decal_holes))
						return false;
				}
				break;
			case DECAL_RENDERMODE_NORMAL:
				{
					m_pShader->DisableAttribute(m_attribs.a_dtexcoord);
					if(!m_pShader->SetDeterminator(m_attribs.d_shadertype, shader_decal))
						return false;
				}
				break;
			}

			// Optimize state switches
			rendermode = requestedRendermode;
		}

		R_ValidateShader(m_pShader);

		if(pgroup->pentity)
		{
			bool isTransparent = R_IsEntityTransparent((*pgroup->pentity), false);
			if(!transparents && isTransparent
				|| transparents && !isTransparent
				|| pgroup->pentity->visframe != rns.framecount)
				continue;

			if(R_IsEntityMoved(*pgroup->pentity))
			{
				rns.view.modelview.PushMatrix();
				R_RotateForEntity(rns.view.modelview, *pgroup->pentity);
			
				// load modelview and pop
				m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
				rns.view.modelview.PopMatrix();
			}
		}

		Float decalalpha;
		if(pdecal->life > 0 && pdecal->fadetime)
		{
			// Apply fade
			Double fadebegin = pdecal->spawntime + (pdecal->life - pdecal->fadetime);
			if(fadebegin > rns.time)
			{
				// No fade yet
				decalalpha = 1.0;
			}
			else
			{
				decalalpha = 1.0 - ((rns.time - fadebegin) / pdecal->fadetime);
				decalalpha = clamp(decalalpha, 0.0, 1.0);
			}
		}
		else
		{
			// No fade
			decalalpha = 1.0;
		}

		Float decalscale;
		if(pdecal->growthtime > 0 && rns.time < (pdecal->spawntime + pdecal->growthtime))
		{
			decalscale = (rns.time - pdecal->spawntime) / pdecal->growthtime;
			decalscale = clamp(decalscale, 0.0, 1.0);
			decalscale = (1.0 - decalscale) * 10 + decalscale;
		}
		else
		{
			// No growth
			decalscale = 1.0;
		}

		m_pShader->SetUniform1f(m_attribs.u_decalalpha, decalalpha);
		m_pShader->SetUniform1f(m_attribs.u_decalscale, decalscale);
		m_pShader->ResetSamplerIndex();

		Int32 textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_maintexture);
		R_Bind2DTexture(GL_TEXTURE0 + textureIndex, pdecal->ptexinfo->ptexture->palloc->gl_index);

		if (pgroup->alphatest && pgroup->ptexture && pgroup->ptexture->infoindex != NO_INFO_INDEX)
		{
			bsp_texture_t* ptexturehandle = &m_texturesArray[pgroup->ptexture->infoindex];
			en_texture_t* ptexture = ptexturehandle->pmaterial->ptextures[MT_TX_DIFFUSE];

			textureIndex = m_pShader->AutoSetSamplerUniform(m_attribs.u_detailtex);
			R_Bind2DTexture(GL_TEXTURE0 + textureIndex, ptexture->palloc->gl_index);
		}

		m_pShader->DrawArrays(GL_TRIANGLES, pgroup->start_vertex, pgroup->num_vertexes);

		// Reload the original if needed
		if(pgroup->pentity && R_IsEntityMoved(*pgroup->pentity))	
			m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawDecals( bool transparents )
{
	if(m_staticDecalsArray.empty() && m_decalsList.empty())
		return true;

	if(!m_pShader->SetDeterminator(m_attribs.d_blended, 0, false))
		return false;

	if(rns.fog.settings.active)
	{
		m_pShader->SetUniform3f(m_attribs.u_fogcolor, 0.5, 0.5, 0.5);
		m_pShader->SetUniform2f(m_attribs.u_fogparams, rns.fog.settings.end, 1.0f/(static_cast<Float>(rns.fog.settings.end)-static_cast<Float>(rns.fog.settings.start)));

		if(rns.fog.specialfog)
		{
			m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_fogcoord);
		}
		else
		{
			m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_radial);
		}
	}
	else
	{
		m_pShader->SetUniform1i(m_attribs.u_d_fogtype, fog_none);
	}

	Int32 alphatestMode = (rns.msaa && rns.mainframe) ? ALPHATEST_COVERAGE : ALPHATEST_LESSTHAN;
	if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, alphatestMode, false))
		return false;

	if(alphatestMode == ALPHATEST_COVERAGE)
	{
		glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		gGLExtF.glSampleCoverage(0.5, GL_FALSE);
	}

	m_pShader->EnableAttribute(m_attribs.a_texcoord);

	// reload modelview
	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	m_pShader->SetUniform4f(m_attribs.u_color, 1.0, 1.0, 1.0, 1.0);

	if(transparents)
		glDisable(GL_CULL_FACE);
	
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glPolygonOffset(-1, -1);
	glEnable(GL_POLYGON_OFFSET_FILL);

	decal_rendermode_t renderMode = DECAL_RENDERMODE_NONE;

	for(Uint32 i = 0; i < m_staticDecalsArray.size(); i++)
	{
		bsp_decal_t* pdecal = m_staticDecalsArray[i];
		if(!Common::CheckVisibility(pdecal->leafnums, pdecal->numleafs, rns.pvisbuffer))
			continue;

		if(!DrawDecal(pdecal, transparents, renderMode))
			return false;
	}

	m_decalsList.begin();
	while(!m_decalsList.end())
	{
		bsp_decal_t* pdecal = m_decalsList.get();
		if(Common::CheckVisibility(pdecal->leafnums, pdecal->numleafs, rns.pvisbuffer))
		{
			if(!DrawDecal(pdecal, transparents, renderMode))
				return false;
		}
		m_decalsList.next();
	}

	glDepthMask(GL_TRUE);

	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);

	if(transparents)
		glEnable(GL_CULL_FACE);

	if(!m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED, false))
		return false;

	if(alphatestMode == ALPHATEST_COVERAGE)
	{
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		gGLExtF.glSampleCoverage(1.0, GL_FALSE);
	}

	m_pShader->DisableAttribute(m_attribs.a_texcoord);
	m_pShader->DisableAttribute(m_attribs.a_dtexcoord);

	return true;
}

//=============================================
// @brief
//
//=============================================
bool CBSPRenderer::DrawNormalDecals( void )
{
	// Set shader's VBO
	m_pShader->SetVBO(m_pDecalVBO);

	if(!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Rendering error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_alpha);

	// Set the projection matrix
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Draw decals last
	bool result = DrawDecals(false);

	// Make sure to restore this
	if(result)
		result = m_pShader->SetDeterminator(m_attribs.d_alphatest, ALPHATEST_DISABLED, false);

	m_pShader->DisableShader();

	return result;
}

//=============================================
// @brief
//
//=============================================
Float CBSPRenderer::CalcFogCoord( Float z )
{
	Float fogcoord;
	Float start = 800; // set for detour07f
	Float end;

	if(z < 0) end = abs((ens.pworld->mins.z+SPECIALFOG_DISTANCE));
	else end = abs((ens.pworld->maxs.z-SPECIALFOG_DISTANCE));

	fogcoord = (end-abs(z))/(end-start);
	if(fogcoord > 1) fogcoord = 1;
	if(fogcoord < 0) fogcoord = 0;

	return (1.0-fogcoord)*rns.fog.settings.end;
}

//=============================================
// @brief
//
//=============================================
void CBSPRenderer::Think( void )
{
	if(!m_decalsList.empty())
	{
		m_decalsList.begin();
		while(!m_decalsList.end())
		{
			bsp_decal_t* pdecal = m_decalsList.get();
			if(pdecal->life > 0)
			{
				Double deathtime = pdecal->spawntime + pdecal->life;
				if(deathtime <= cls.cl_time)
				{
					// Delete this decal
					m_decalsList.remove(m_decalsList.get_link());
					m_decalsList.next();

					RemoveDecalFromVBO(pdecal);
					delete pdecal;
				}
			}

			m_decalsList.next();
		}
	}
}