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
#include "r_ssao.h"
#include "r_main.h"
#include "r_vbo.h"
#include "r_glsl.h"
#include "r_rttcache.h"
#include "r_fbocache.h"
#include "r_basic_vertex.h"
#include "texturemanager.h"
#include "system.h"
#include "console.h"
#include "cvar.h"
#include "file.h"
#include "window.h"

// Class definition
CSSAOManager gSSAO;

//=============================================
//
//=============================================
CSSAOManager::CSSAOManager(void) :
	m_pShader(nullptr),
	m_pVBO(nullptr),
	m_pNoiseTexture(nullptr),
	m_pCvarSSAO(nullptr),
	m_pCvarSSAORadius(nullptr),
	m_pCvarSSAOIntensity(nullptr)
{
}

//=============================================
//
//=============================================
CSSAOManager::~CSSAOManager(void)
{
	Shutdown();
}

//=============================================
//
//=============================================
bool CSSAOManager::Init(void)
{
	m_pCvarSSAO = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_ssao", "1", "Toggle Screen Space Ambient Occlusion.");
	m_pCvarSSAORadius = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_ssao_radius", "8.0", "SSAO sample radius in world units.");
	m_pCvarSSAOIntensity = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_ssao_intensity", "2.0", "SSAO darkness intensity.");

	GenerateKernel();
	return true;
}

//=============================================
//
//=============================================
void CSSAOManager::Shutdown(void)
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CSSAOManager::InitGame(void)
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

	CreateNoiseTexture();
	return true;
}

//=============================================
//
//=============================================
void CSSAOManager::ClearGame(void)
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

	m_pNoiseTexture = nullptr;
}

//=============================================
//
//=============================================
void CSSAOManager::GenerateKernel(void)
{
	for (Uint32 i = 0; i < SSAO_KERNEL_SIZE; ++i)
	{
		Vector sample(Common::RandomFloat(-1.0f, 1.0f), Common::RandomFloat(-1.0f, 1.0f), Common::RandomFloat(0.1f, 1.0f));
		sample.Normalize();

		Float scale = static_cast<Float>(i) / static_cast<Float>(SSAO_KERNEL_SIZE);
		Float scaleLerp = 0.1f + (scale * scale) * (1.0f - 0.1f);
		sample = sample * scaleLerp;

		m_kernel[i] = sample;
	}
}

//=============================================
//
//=============================================
void CSSAOManager::CreateNoiseTexture(void)
{
	if (m_pNoiseTexture)
		return;

	const Uint32 totalTexels = SSAO_NOISE_DIMENSION * SSAO_NOISE_DIMENSION;
	vec4_t noiseData[totalTexels];
	for (Uint32 i = 0; i < totalTexels; ++i)
	{
		noiseData[i][0] = Common::RandomFloat(-1.0f, 1.0f);
		noiseData[i][1] = Common::RandomFloat(-1.0f, 1.0f);
		noiseData[i][2] = 0.0f;
		noiseData[i][3] = 1.0f;
	}

	CTextureManager* pTextureManager = CTextureManager::GetInstance();
	m_pNoiseTexture = pTextureManager->GenTextureIndex(RS_GAME_LEVEL);

	glBindTexture(GL_TEXTURE_2D, m_pNoiseTexture->gl_index);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SSAO_NOISE_DIMENSION, SSAO_NOISE_DIMENSION, 0, GL_RGBA, GL_FLOAT, noiseData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
}

//=============================================
//
//=============================================
bool CSSAOManager::InitGL(void)
{
	if (!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;
		m_pShader = new CGLSLShader(FL_GetInterface(), "ssao.bss", shaderFlags, VID_ShaderCompileCallback);
		if (m_pShader->HasError())
		{
			Sys_ErrorPopup("%s - Failed to compile shader: %s.", __FUNCTION__, m_pShader->GetError());
			return false;
		}

		m_attribs.a_position = m_pShader->InitAttribute("in_position", 4, GL_FLOAT, sizeof(basic_vertex_t), OFFSET(basic_vertex_t, origin));
		m_attribs.a_texcoord = m_pShader->InitAttribute("in_texcoord", 2, GL_FLOAT, sizeof(basic_vertex_t), OFFSET(basic_vertex_t, texcoords));

		m_attribs.u_modelview = m_pShader->InitUniform("modelview", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_projection = m_pShader->InitUniform("projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_scene_projection = m_pShader->InitUniform("u_scene_projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_scene_projection_inverse = m_pShader->InitUniform("u_scene_projection_inverse", CGLSLShader::UNIFORM_MATRIX4);

		m_attribs.u_depthMap = m_pShader->InitUniform("u_depthMap", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_noise = m_pShader->InitUniform("u_noise", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_ssaoTexture = m_pShader->InitUniform("u_ssaoTexture", CGLSLShader::UNIFORM_SAMPLER2D);

		m_attribs.u_noiseScale = m_pShader->InitUniform("u_noiseScale", CGLSLShader::UNIFORM_FLOAT2);
		m_attribs.u_sampleRad = m_pShader->InitUniform("u_sampleRad", CGLSLShader::UNIFORM_FLOAT1);
		m_attribs.u_intensity = m_pShader->InitUniform("u_intensity", CGLSLShader::UNIFORM_FLOAT1);
		m_attribs.u_kernel = m_pShader->InitUniform("u_kernel[0]", CGLSLShader::UNIFORM_FLOAT3, SSAO_KERNEL_SIZE);

		m_attribs.d_pass = m_pShader->GetDeterminatorIndex("pass");

		if (!R_CheckShaderVertexAttribute(m_attribs.a_position, "in_position", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_texcoord, "in_texcoord", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelview, "modelview", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_projection, "projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_scene_projection, "u_scene_projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_scene_projection_inverse, "u_scene_projection_inverse", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_depthMap, "u_depthMap", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_noise, "u_noise", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_ssaoTexture, "u_ssaoTexture", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_noiseScale, "u_noiseScale", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_sampleRad, "u_sampleRad", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_intensity, "u_intensity", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_kernel, "u_kernel[0]", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderDeterminator(m_attribs.d_pass, "pass", m_pShader, Sys_ErrorPopup))
			return false;
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

	CreateNoiseTexture();
	return true;
}

//=============================================
//
//=============================================
void CSSAOManager::ClearGL(void)
{
	if (m_pShader)
	{
		delete m_pShader;
		m_pShader = nullptr;
	}

	if (m_pVBO)
		m_pVBO->ClearGL();

	if (m_pNoiseTexture)
		m_pNoiseTexture = nullptr;
}

//=============================================
//
//=============================================
bool CSSAOManager::DrawSSAO(void)
{
	if (m_pCvarSSAO->GetValue() < 1.0f)
		return true;

	if (!m_pShader)
		return false;

	CFBOCache::cache_fbo_t* pDepthFBO = gFBOCache.Alloc(rns.screenwidth, rns.screenheight, true);
	CFBOCache::cache_fbo_t* pRawSSAO_FBO = gFBOCache.Alloc(rns.screenwidth, rns.screenheight, false);
	CFBOCache::cache_fbo_t* pBlurSSAO_FBO = gFBOCache.Alloc(rns.screenwidth, rns.screenheight, false);
	if (!pDepthFBO || !pRawSSAO_FBO || !pBlurSSAO_FBO)
	{
		if (pDepthFBO)
			gFBOCache.Free(pDepthFBO);
		if (pRawSSAO_FBO)
			gFBOCache.Free(pRawSSAO_FBO);
		if (pBlurSSAO_FBO)
			gFBOCache.Free(pBlurSSAO_FBO);
		return false;
	}

	GLuint srcFBO = (rns.pboundfbo) ? rns.pboundfbo->fboid : ((rns.fboused && rns.usehdr && rns.mainfbo.fboid) ? rns.mainfbo.fboid : 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pDepthFBO->fbo.fboid);
	glBlitFramebuffer(0, 0, rns.screenwidth, rns.screenheight, 0, 0, rns.screenwidth, rns.screenheight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	Float sceneProj[16];
	memcpy(sceneProj, rns.view.projection.GetMatrix(), sizeof(sceneProj));

	Float sceneProjInv[16];
	memcpy(sceneProjInv, rns.view.projection.GetInverse(), sizeof(sceneProjInv));

	if (!m_pShader->EnableShader())
	{
		gFBOCache.Free(pDepthFBO);
		gFBOCache.Free(pRawSSAO_FBO);
		gFBOCache.Free(pBlurSSAO_FBO);
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
	m_pShader->SetUniformMatrix4fv(m_attribs.u_scene_projection, sceneProj);
	m_pShader->SetUniformMatrix4fv(m_attribs.u_scene_projection_inverse, sceneProjInv);

	R_BindFBO(&pRawSSAO_FBO->fbo);
	glViewport(0, 0, rns.screenwidth, rns.screenheight);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_pShader->SetDeterminator(m_attribs.d_pass, SSAO_PASS_RAW);

	m_pShader->SetUniform2f(m_attribs.u_noiseScale, static_cast<Float>(rns.screenwidth) / static_cast<Float>(SSAO_NOISE_DIMENSION), static_cast<Float>(rns.screenheight) / static_cast<Float>(SSAO_NOISE_DIMENSION));
	m_pShader->SetUniform1f(m_attribs.u_sampleRad, m_pCvarSSAORadius->GetValue());
	m_pShader->SetUniform1f(m_attribs.u_intensity, m_pCvarSSAOIntensity->GetValue());
	m_pShader->SetUniform3fv(m_attribs.u_kernel, reinterpret_cast<const Float*>(m_kernel), SSAO_KERNEL_SIZE);

	m_pShader->SetUniform1i(m_attribs.u_depthMap, 0);
	R_Bind2DTexture(GL_TEXTURE0, pDepthFBO->fbo.pdepth->gl_index);

	m_pShader->SetUniform1i(m_attribs.u_noise, 1);
	R_Bind2DTexture(GL_TEXTURE1, m_pNoiseTexture->gl_index);

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	R_BindFBO(&pBlurSSAO_FBO->fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	m_pShader->SetDeterminator(m_attribs.d_pass, SSAO_PASS_BLUR);
	m_pShader->SetUniform1i(m_attribs.u_ssaoTexture, 0);
	R_Bind2DTexture(GL_TEXTURE0, pRawSSAO_FBO->fbo.ptexture1->gl_index);

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	if (rns.fboused && rns.usehdr)
		R_BindFBO(&rns.mainfbo);
	else
		R_BindFBO(nullptr);

	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);

	m_pShader->SetDeterminator(m_attribs.d_pass, SSAO_PASS_APPLY);
	m_pShader->SetUniform1i(m_attribs.u_ssaoTexture, 0);
	R_Bind2DTexture(GL_TEXTURE0, pBlurSSAO_FBO->fbo.ptexture1->gl_index);

	R_ValidateShader(m_pShader);
	m_pShader->DrawArrays(GL_TRIANGLES, 0, 6);

	rns.view.modelview.PopMatrix();
	rns.view.projection.PopMatrix();

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	m_pShader->DisableShader();

	gFBOCache.Free(pDepthFBO);
	gFBOCache.Free(pRawSSAO_FBO);
	gFBOCache.Free(pBlurSSAO_FBO);

	R_ClearBinds();
	return true;
}