/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef R_VOLUMETRICS_H
#define R_VOLUMETRICS_H

#include "r_glsl.h"
#include "r_vbo.h"
#include "r_fbocache.h"

class CCVar;
struct cl_dlight_t;

static constexpr Uint32 MAX_VOLUMETRIC_LIGHTS = 4;

enum vol_pass_t
{
	VOL_PASS_RAYMARCH = 0,
	VOL_PASS_BLUR
};

struct vol_light_attribs_t
{
	vol_light_attribs_t():
		u_posRadius(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_colorIntensity(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_spotDirCone(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_lightMatrix(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_hasShadow(CGLSLShader::PROPERTY_UNAVAILABLE)
	{}

	Int32 u_posRadius;
	Int32 u_colorIntensity;
	Int32 u_spotDirCone;
	Int32 u_lightMatrix;
	Int32 u_hasShadow;
};

struct vol_shader_attribs_t
{
	vol_shader_attribs_t():
		a_position(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_texcoord(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_modelview(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_scene_projection_inverse(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_inv_view(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_viewPos(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_maxRayDist(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_depthMap(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_volTexture(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_numLights(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_samples(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_hasSunLight(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_sunColorIntensity(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_sunDir(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_csmMatrix(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_csmShadowMap(CGLSLShader::PROPERTY_UNAVAILABLE),
		d_pass(CGLSLShader::PROPERTY_UNAVAILABLE)
	{
		for (Uint32 i = 0; i < MAX_VOLUMETRIC_LIGHTS; ++i)
		{
			u_spotProjTex[i] = CGLSLShader::PROPERTY_UNAVAILABLE;
			u_spotShadowMap[i] = CGLSLShader::PROPERTY_UNAVAILABLE;
			u_cubeShadowMap[i] = CGLSLShader::PROPERTY_UNAVAILABLE;
		}
	}

	Int32 a_position;
	Int32 a_texcoord;

	Int32 u_modelview;
	Int32 u_projection;
	Int32 u_scene_projection_inverse;
	Int32 u_inv_view;
	Int32 u_viewPos;
	Int32 u_maxRayDist;

	Int32 u_depthMap;
	Int32 u_volTexture;
	Int32 u_numLights;
	Int32 u_samples;

	Int32 u_hasSunLight;
	Int32 u_sunColorIntensity;
	Int32 u_sunDir;
	Int32 u_csmMatrix;
	Int32 u_csmShadowMap;

	Int32 u_spotProjTex[MAX_VOLUMETRIC_LIGHTS];
	Int32 u_spotShadowMap[MAX_VOLUMETRIC_LIGHTS];
	Int32 u_cubeShadowMap[MAX_VOLUMETRIC_LIGHTS];

	vol_light_attribs_t lights[MAX_VOLUMETRIC_LIGHTS];
	Int32 d_pass;
};

/*
====================
CVolumetricsManager

====================
*/
class CVolumetricsManager
{
public:
	CVolumetricsManager( void );
	~CVolumetricsManager( void );

public:
	// Initializes CVars
	bool Init( void );
	// Performs complete shutdown
	void Shutdown( void );

	// Compiles shaders, binds uniforms and sets up quad VBO
	bool InitGL( void );
	// Clears GL objects
	void ClearGL( void );

	// Initializes per-game session resources
	bool InitGame( void );
	// Clears game resources on map change
	void ClearGame( void );

	// Evaluates volumetric scattering and additively blends over scene FBO
	bool DrawVolumetrics( void );

private:
	// Collects closest active volumetric dynamic lights
	Uint32 CollectVolumetricLights( cl_dlight_t** pOutLights, Uint32 maxLights );

private:
	CGLSLShader* m_pShader;
	CVBO* m_pVBO;
	vol_shader_attribs_t m_attribs;

	CCVar* m_pCvarVolumetrics;
	CCVar* m_pCvarVolumetricSteps;
	CCVar* m_pCvarVolumetricDownsample;
};

extern CVolumetricsManager gVolumetrics;

#endif // R_VOLUMETRICS_H