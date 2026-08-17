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
#include "includes.h"
#include "cache_model.h"
#include "modelcache.h"
#include "cl_entity.h"
#include "cl_main.h"
#include "cl_utils.h"
#include "r_volumetrics.h"
#include "r_dlights.h"
#include "r_main.h"
#include "r_vbo.h"
#include "r_glsl.h"
#include "r_fbocache.h"
#include "r_basic_vertex.h"
#include "texturemanager.h"
#include "system.h"
#include "console.h"
#include "cvar.h"
#include "file.h"
#include "window.h"
#include "r_lightstyles.h"

CVolumetricsManager gVolumetrics;

//=============================================
//
//=============================================
CVolumetricsManager::CVolumetricsManager( void ):
	m_pShader(nullptr),
	m_pVBO(nullptr),
	m_pCvarVolumetrics(nullptr),
	m_pCvarVolumetricSteps(nullptr),
	m_pCvarVolumetricDownsample(nullptr)
{
}

//=============================================
//
//=============================================
CVolumetricsManager::~CVolumetricsManager( void )
{
	Shutdown();
}

//=============================================
//
//=============================================
bool CVolumetricsManager::Init( void )
{
	m_pCvarVolumetrics = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_volumetrics", "1", "Toggle volumetric dynamic lighting.");
	m_pCvarVolumetricSteps = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "r_volumetric_steps", "16", "Raymarching step sample count for volumetric lights.");
	m_pCvarVolumetricDownsample = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_volumetric_downsample", "2", "Volumetric rendering downsample factor (1 = full res, 2 = half res, 4 = quarter res).");
	return true;
}

//=============================================
//
//=============================================
void CVolumetricsManager::Shutdown( void )
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CVolumetricsManager::InitGame( void )
{
	if (!m_pVBO && m_pShader)
	{
		basic_vertex_t pverts[6];
		pverts[0].origin[0] = 0; pverts[0].origin[1] = 1; pverts[0].origin[2] = -1; pverts[0].origin[3] = 1;
		pverts[0].texcoords[0] = 0; pverts[0].texcoords[1] = 0;

		pverts[1].origin[0] = 0; pverts[1].origin[1] = 0; pverts[1].origin[2] = -1; pverts[1].origin[3] = 1;
		pverts[1].texcoords[0] = 0; pverts[1].texcoords[1] = 1;

		pverts[2].origin[0] = 1; pverts[2].origin[1] = 0; pverts[2].origin[2] = -1; pverts[2].origin[3] = 1;
		pverts[2].texcoords[0] = 1; pverts[2].texcoords[1] = 1;

		pverts[3].origin[0] = 0; pverts[3].origin[1] = 1; pverts[3].origin[2] = -1; pverts[3].origin[3] = 1;
		pverts[3].texcoords[0] = 0; pverts[3].texcoords[1] = 0;

		pverts[4].origin[0] = 1; pverts[4].origin[1] = 0; pverts[4].origin[2] = -1; pverts[4].origin[3] = 1;
		pverts[4].texcoords[0] = 1; pverts[4].texcoords[1] = 1;

		pverts[5].origin[0] = 1; pverts[5].origin[1] = 1; pverts[5].origin[2] = -1; pverts[5].origin[3] = 1;
		pverts[5].texcoords[0] = 1; pverts[5].texcoords[1] = 0;

		m_pVBO = new CVBO(pverts, sizeof(basic_vertex_t) * 6, nullptr, 0);
		m_pShader->SetVBO(m_pVBO);
	}
	return true;
}

//=============================================
//
//=============================================
void CVolumetricsManager::ClearGame( void )
{
	if (m_pShader)
	{
		m_pShader->SetVBO(nullptr);
		m_pShader->ResetShader();
	}

	if (m_pVBO)
	{
		delete m_pVBO;
		m_pVBO = nullptr;
	}
}

//=============================================
//
//=============================================
bool CVolumetricsManager::InitGL( void )
{
	if (!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;
		m_pShader = new CGLSLShader(FL_GetInterface(), "volumetrics.bss", shaderFlags, VID_ShaderCompileCallback);
		if (m_pShader->HasError())
		{
			Sys_ErrorPopup("%s - Failed to compile shader: %s.", __FUNCTION__, m_pShader->GetError());
			return false;
		}

		m_attribs.a_position = m_pShader->InitAttribute("in_position", 4, GL_FLOAT, sizeof(basic_vertex_t), OFFSET(basic_vertex_t, origin));
		m_attribs.a_texcoord = m_pShader->InitAttribute("in_texcoord", 2, GL_FLOAT, sizeof(basic_vertex_t), OFFSET(basic_vertex_t, texcoords));

		m_attribs.u_modelview = m_pShader->InitUniform("modelview", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_projection = m_pShader->InitUniform("projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_scene_projection_inverse = m_pShader->InitUniform("u_scene_projection_inverse", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_inv_view = m_pShader->InitUniform("u_inv_view", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_viewPos = m_pShader->InitUniform("u_viewPos", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_maxRayDist = m_pShader->InitUniform("u_maxRayDist", CGLSLShader::UNIFORM_FLOAT1);

		m_attribs.u_depthMap = m_pShader->InitUniform("u_depthMap", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_volTexture = m_pShader->InitUniform("u_volTexture", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_numLights = m_pShader->InitUniform("u_numLights", CGLSLShader::UNIFORM_INT1);
		m_attribs.u_samples = m_pShader->InitUniform("u_samples", CGLSLShader::UNIFORM_INT1);

		m_attribs.d_pass = m_pShader->GetDeterminatorIndex("pass");

		if (!R_CheckShaderVertexAttribute(m_attribs.a_position, "in_position", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_texcoord, "in_texcoord", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelview, "modelview", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_projection, "projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_scene_projection_inverse, "u_scene_projection_inverse", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_inv_view, "u_inv_view", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_viewPos, "u_viewPos", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_maxRayDist, "u_maxRayDist", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_depthMap, "u_depthMap", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_volTexture, "u_volTexture", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_numLights, "u_numLights", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_samples, "u_samples", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderDeterminator(m_attribs.d_pass, "pass", m_pShader, Sys_ErrorPopup))
			return false;

		for (Uint32 i = 0; i < MAX_VOLUMETRIC_LIGHTS; ++i)
		{
			CString prefix;
			prefix << "u_lights[" << static_cast<Int32>(i) << "]";

			CString name = prefix + ".posRadius";
			m_attribs.lights[i].u_posRadius = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_FLOAT4);

			name = prefix + ".colorIntensity";
			m_attribs.lights[i].u_colorIntensity = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_FLOAT4);

			name = prefix + ".spotDirCone";
			m_attribs.lights[i].u_spotDirCone = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_FLOAT4);

			name = prefix + ".lightMatrix";
			m_attribs.lights[i].u_lightMatrix = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_MATRIX4);

			name = prefix + ".hasShadow";
			m_attribs.lights[i].u_hasShadow = m_pShader->InitUniform(name.c_str(), CGLSLShader::UNIFORM_INT1);

			CString texName;
			texName << "u_spotProjTex[" << static_cast<Int32>(i) << "]";
			m_attribs.u_spotProjTex[i] = m_pShader->InitUniform(texName.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);

			texName.clear();
			texName << "u_spotShadowMap[" << static_cast<Int32>(i) << "]";
			m_attribs.u_spotShadowMap[i] = m_pShader->InitUniform(texName.c_str(), CGLSLShader::UNIFORM_SAMPLER2D);

			texName.clear();
			texName << "u_cubeShadowMap[" << static_cast<Int32>(i) << "]";
			m_attribs.u_cubeShadowMap[i] = m_pShader->InitUniform(texName.c_str(), CGLSLShader::UNIFORM_SAMPLERCUBE);

			if (!R_CheckShaderUniform(m_attribs.lights[i].u_posRadius, (prefix + ".posRadius").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_colorIntensity, (prefix + ".colorIntensity").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_spotDirCone, (prefix + ".spotDirCone").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_lightMatrix, (prefix + ".lightMatrix").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.lights[i].u_hasShadow, (prefix + ".hasShadow").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.u_spotProjTex[i], ("u_spotProjTex[" + std::to_string(i) + "]").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.u_spotShadowMap[i], ("u_spotShadowMap[" + std::to_string(i) + "]").c_str(), m_pShader, Sys_ErrorPopup)
				|| !R_CheckShaderUniform(m_attribs.u_cubeShadowMap[i], ("u_cubeShadowMap[" + std::to_string(i) + "]").c_str(), m_pShader, Sys_ErrorPopup))
				return false;
		}
	}

	if (!m_pVBO)
	{
		basic_vertex_t pverts[6];
		pverts[0].origin[0] = 0; pverts[0].origin[1] = 1; pverts[0].origin[2] = -1; pverts[0].origin[3] = 1;
		pverts[0].texcoords[0] = 0; pverts[0].texcoords[1] = 0;

		pverts[1].origin[0] = 0; pverts[1].origin[1] = 0; pverts[1].origin[2] = -1; pverts[1].origin[3] = 1;
		pverts[1].texcoords[0] = 0; pverts[1].texcoords[1] = 1;

		pverts[2].origin[0] = 1; pverts[2].origin[1] = 0; pverts[2].origin[2] = -1; pverts[2].origin[3] = 1;
		pverts[2].texcoords[0] = 1; pverts[2].texcoords[1] = 1;

		pverts[3].origin[0] = 0; pverts[3].origin[1] = 1; pverts[3].origin[2] = -1; pverts[3].origin[3] = 1;
		pverts[3].texcoords[0] = 0; pverts[3].texcoords[1] = 0;

		pverts[4].origin[0] = 1; pverts[4].origin[1] = 0; pverts[4].origin[2] = -1; pverts[4].origin[3] = 1;
		pverts[4].texcoords[0] = 1; pverts[4].texcoords[1] = 1;

		pverts[5].origin[0] = 1; pverts[5].origin[1] = 1; pverts[5].origin[2] = -1; pverts[5].origin[3] = 1;
		pverts[5].texcoords[0] = 1; pverts[5].texcoords[1] = 0;

		m_pVBO = new CVBO(pverts, sizeof(basic_vertex_t) * 6, nullptr, 0);
		m_pShader->SetVBO(m_pVBO);
	}
	else
	{
		m_pVBO->RebindGL();
		m_pShader->SetVBO(m_pVBO);
	}

	return true;
}

//=============================================
//
//=============================================
void CVolumetricsManager::ClearGL( void )
{
	if (m_pShader)
	{
		delete m_pShader;
		m_pShader = nullptr;
	}

	if (m_pVBO)
		m_pVBO->ClearGL();
}

//=============================================
//
//=============================================
Uint32 CVolumetricsManager::CollectVolumetricLights( cl_dlight_t** pOutLights, Uint32 maxLights )
{
	CLinkedList<cl_dlight_t*>& dlightList = gDynamicLights.GetLightList();
	if (dlightList.empty())
		return 0;

	struct candidate_t
	{
		cl_dlight_t* light;
		Float distSq;
	};

	CArray<candidate_t> candidates;
	dlightList.begin();
	while (!dlightList.end())
	{
		cl_dlight_t* dl = dlightList.get();
		if (dl->key > 0 && dl->radius > 0.0f)
		{
			cl_entity_t* pentity = CL_GetEntityByIndex(dl->key);
			if (pentity && pentity->curstate.iuser4 > 0)
			{
				Float distSq = (dl->origin - rns.view.v_origin).Length();
				candidate_t cand;
				cand.light = dl;
				cand.distSq = distSq;
				candidates.push_back(cand);
			}
		}
		dlightList.next();
	}

	if (candidates.empty())
		return 0;

	// Sort by distance to camera
	for (Uint32 i = 0; i < candidates.size(); ++i)
	{
		for (Uint32 j = i + 1; j < candidates.size(); ++j)
		{
			if (candidates[j].distSq < candidates[i].distSq)
			{
				candidate_t tmp = candidates[i];
				candidates[i] = candidates[j];
				candidates[j] = tmp;
			}
		}
	}

	Uint32 count = candidates.size() > maxLights ? maxLights : candidates.size();
	for (Uint32 i = 0; i < count; ++i)
		pOutLights[i] = candidates[i].light;

	return count;
}

//=============================================
//
//=============================================
bool CVolumetricsManager::DrawVolumetrics( void )
{
	if (m_pCvarVolumetrics->GetValue() < 1.0f)
		return true;

	cl_dlight_t* activeLights[MAX_VOLUMETRIC_LIGHTS] = { nullptr };
	Uint32 numVolLights = CollectVolumetricLights(activeLights, MAX_VOLUMETRIC_LIGHTS);
	if (numVolLights == 0)
		return true;

	if (!m_pShader)
		return false;

	Uint32 downsample = m_pCvarVolumetricDownsample ? static_cast<Uint32>(m_pCvarVolumetricDownsample->GetValue()) : 1;
	if (downsample < 1)
		downsample = 1;

	Uint32 volWidth = rns.screenwidth / downsample;
	Uint32 volHeight = rns.screenheight / downsample;
	if (volWidth < 1) 
		volWidth = 1;
	if (volHeight < 1)
		volHeight = 1;

	CFBOCache::cache_fbo_t* pDepthFBO = gFBOCache.Alloc(rns.screenwidth, rns.screenheight, true);
	CFBOCache::cache_fbo_t* pRawVol_FBO = gFBOCache.Alloc(volWidth, volHeight, false);
	CFBOCache::cache_fbo_t* pBlurVol_FBO = gFBOCache.Alloc(volWidth, volHeight, false);
	if (!pDepthFBO || !pRawVol_FBO || !pBlurVol_FBO)
	{
		if (pDepthFBO) 
			gFBOCache.Free(pDepthFBO);
		if (pRawVol_FBO) 
			gFBOCache.Free(pRawVol_FBO);
		if (pBlurVol_FBO) 
			gFBOCache.Free(pBlurVol_FBO);
		return false;
	}

	GLuint srcFBO = (rns.pboundfbo) ? rns.pboundfbo->fboid : rns.mainfbo.fboid;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pDepthFBO->fbo.fboid);
	glBlitFramebuffer(0, 0, rns.screenwidth, rns.screenheight, 0, 0, rns.screenwidth, rns.screenheight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	Float sceneProjInv[16];
	memcpy(sceneProjInv, rns.view.projection.GetInverse(), sizeof(sceneProjInv));

	Float invView[16];
	memcpy(invView, rns.view.modelview.GetInverse(), sizeof(invView));

	if (!m_pShader->EnableShader())
	{
		gFBOCache.Free(pDepthFBO);
		gFBOCache.Free(pRawVol_FBO);
		gFBOCache.Free(pBlurVol_FBO);
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_position);
	m_pShader->EnableAttribute(m_attribs.a_texcoord);

	rns.view.projection.PushMatrix();
	rns.view.modelview.PushMatrix();

	rns.view.projection.LoadIdentity();
	rns.view.modelview.LoadIdentity();
	rns.view.modelview.Ortho(GL_ZERO, GL_ONE, GL_ONE, GL_ZERO, 0.1f, 100.0f);

	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_scene_projection_inverse, sceneProjInv);
	m_pShader->SetUniformMatrix4fv(m_attribs.u_inv_view, invView);
	m_pShader->SetUniform3f(m_attribs.u_viewPos, rns.view.v_origin.x, rns.view.v_origin.y, rns.view.v_origin.z);
	m_pShader->SetUniform1f(m_attribs.u_maxRayDist, rns.view.zfar > 0 ? rns.view.zfar : 4096.0f);

	R_BindFBO(&pRawVol_FBO->fbo);
	glViewport(0, 0, volWidth, volHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_pShader->SetDeterminator(m_attribs.d_pass, VOL_PASS_RAYMARCH);
	m_pShader->SetUniform1i(m_attribs.u_numLights, numVolLights);
	m_pShader->SetUniform1i(m_attribs.u_samples, static_cast<Int32>(m_pCvarVolumetricSteps->GetValue()));

	m_pShader->ResetSamplerIndex();
	Int32 depthUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_depthMap);
	R_Bind2DTexture(GL_TEXTURE0 + depthUnit, pDepthFBO->fbo.pdepth->gl_index);

	for (Uint32 i = 0; i < numVolLights; ++i)
	{
		cl_dlight_t* dl = activeLights[i];
		cl_entity_t* pentity = (dl->key > 0) ? CL_GetEntityByIndex(dl->key) : nullptr;
		Float volIntensity = pentity ? static_cast<Float>(pentity->curstate.iuser4) : 0.0f;

		Vector color;
		Math::VectorCopy(dl->color, color);
		gLightStyles.ApplyLightStyle(dl, color);

		m_pShader->SetUniform4f(m_attribs.lights[i].u_posRadius, dl->origin.x, dl->origin.y, dl->origin.z, dl->radius);
		m_pShader->SetUniform4f(m_attribs.lights[i].u_colorIntensity, color.x, color.y, color.z, volIntensity * 0.005f);

		bool isSpot = (dl->cone_size > 0.0f);
		bool hasShadow = DL_CanShadow(dl);
		m_pShader->SetUniform1i(m_attribs.lights[i].u_hasShadow, hasShadow ? 1 : 0);

		if (isSpot)
		{
			Vector vforward;
			Vector angles = dl->angles;
			Common::FixVector(angles);
			Math::AngleVectors(angles, &vforward, nullptr, nullptr);
			m_pShader->SetUniform4f(m_attribs.lights[i].u_spotDirCone, vforward.x, vforward.y, vforward.z, dl->cone_size);

			CMatrix matrix;
			matrix.LoadIdentity();
			matrix.Translate(0.5, 0.5, 0.5);
			matrix.Scale(0.5, 0.5, 1.0);
			Float flsize = tan((M_PI / 360) * dl->cone_size);
			matrix.SetFrustum(-flsize, flsize, -flsize, flsize, 1, dl->radius);

			Vector vtarget;
			Math::VectorMA(dl->origin, dl->radius, vforward, vtarget);
			matrix.LookAt(dl->origin[0], dl->origin[1], dl->origin[2], vtarget[0], vtarget[1], vtarget[2], 0, 0, Common::IsPitchReversed(angles[PITCH]) ? -1 : 1);

			m_pShader->SetUniformMatrix4fv(m_attribs.lights[i].u_lightMatrix, matrix.Transpose());

			Int32 projUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_spotProjTex[i]);
			Int32 textureIndex = dl->textureindex;
			if (textureIndex >= rns.objects.projective_textures.size())
				textureIndex = 0;
			R_Bind2DTexture(GL_TEXTURE0 + projUnit, rns.objects.projective_textures[textureIndex]->palloc->gl_index);

			Int32 shadowUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_spotShadowMap[i]);
			if (hasShadow && dl->getProjShadowMap() && dl->getProjShadowMap()->pfbo)
				R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, dl->getProjShadowMap()->pfbo->ptexture1->gl_index);
			else
				R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, 0);

			Int32 cubeUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_cubeShadowMap[i]);
			R_BindCubemapTexture(GL_TEXTURE0 + cubeUnit, 0);
		}
		else
		{
			m_pShader->SetUniform4f(m_attribs.lights[i].u_spotDirCone, 0.0f, 0.0f, 0.0f, 0.0f);

			CMatrix matrix;
			matrix.LoadIdentity();
			matrix.Rotate(-90, 1, 0, 0);
			matrix.Rotate(90, 0, 0, 1);
			matrix.Translate(-dl->origin[0], -dl->origin[1], -dl->origin[2]);
			m_pShader->SetUniformMatrix4fv(m_attribs.lights[i].u_lightMatrix, matrix.GetMatrix(), true);

			Int32 projUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_spotProjTex[i]);
			R_Bind2DTexture(GL_TEXTURE0 + projUnit, 0);

			Int32 shadowUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_spotShadowMap[i]);
			R_Bind2DTexture(GL_TEXTURE0 + shadowUnit, 0);

			Int32 cubeUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_cubeShadowMap[i]);
			if (hasShadow && dl->getCubeShadowMap() && dl->getCubeShadowMap()->pfbo)
			{
				R_BindCubemapTexture(GL_TEXTURE0 + cubeUnit, dl->getCubeShadowMap()->pfbo->ptexture1->gl_index);
			}
			else
			{
				R_BindCubemapTexture(GL_TEXTURE0 + cubeUnit, 0);
			}
		}
	}

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	R_BindFBO(&pBlurVol_FBO->fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	m_pShader->SetDeterminator(m_attribs.d_pass, VOL_PASS_BLUR);
	m_pShader->ResetSamplerIndex();
	Int32 volUnit = m_pShader->AutoSetSamplerUniform(m_attribs.u_volTexture);
	R_Bind2DTexture(GL_TEXTURE0 + volUnit, pRawVol_FBO->fbo.ptexture1->gl_index);

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	R_BindFBO(&rns.mainfbo);
	glViewport(0, 0, rns.screenwidth, rns.screenheight);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	R_Bind2DTexture(GL_TEXTURE0 + volUnit, pBlurVol_FBO->fbo.ptexture1->gl_index);

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	rns.view.modelview.PopMatrix();
	rns.view.projection.PopMatrix();

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	m_pShader->DisableShader();

	gFBOCache.Free(pDepthFBO);
	gFBOCache.Free(pRawVol_FBO);
	gFBOCache.Free(pBlurVol_FBO);

	R_ClearBinds();
	return true;
}