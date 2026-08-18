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
#include "textures_shared.h"
#include "r_cas.h"
#include "r_main.h"
#include "console.h"
#include "cvar.h"
#include "system.h"
#include "file.h"
#include "r_common.h"
#include "r_rttcache.h"
#include "r_fbocache.h"

#define A_CPU 1
#include "ffx_a.h"
#include "ffx_cas.h"

CCAS gCAS;

//=============================================
//
//=============================================
CCAS::CCAS( void ):
	m_pShader(nullptr),
	m_pCvarCAS(nullptr),
	m_pCvarSharpness(nullptr),
	m_pInputRTT(nullptr)
{
}

//=============================================
//
//=============================================
CCAS::~CCAS( void )
{
	Shutdown();
}

//=============================================
//
//=============================================
bool CCAS::Init( void )
{
	m_pCvarCAS = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_cas", "0", "Toggle AMD FidelityFX Contrast Adaptive Sharpening (CAS).");
	m_pCvarSharpness = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT | FL_CV_SAVE), "r_cas_sharpness", "0.5", "AMD CAS sharpening intensity (0.0 to 1.0).");
	return true;
}

//=============================================
//
//=============================================
void CCAS::Shutdown( void )
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CCAS::InitGL( void )
{
	if(!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;
		m_pShader = new CGLSLShader(FL_GetInterface(), "cas.bss", shaderFlags, VID_ShaderCompileCallback);
		if(m_pShader->HasError())
		{
			Sys_ErrorPopup("%s - Failed to compile CAS shader: %s.", __FUNCTION__, m_pShader->GetError());
			return false;
		}

		m_attribs.u_input = m_pShader->InitUniform("u_input", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_const0 = m_pShader->InitUniform("u_const0", CGLSLShader::UNIFORM_FLOAT4);
		m_attribs.u_const1 = m_pShader->InitUniform("u_const1", CGLSLShader::UNIFORM_FLOAT4);

		if(!R_CheckShaderUniform(m_attribs.u_input, "u_input", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_const0, "u_const0", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_const1, "u_const1", m_pShader, Sys_ErrorPopup))
			return false;
	}

	return true;
}

//=============================================
//
//=============================================
void CCAS::ClearGL( void )
{
	if(m_pShader)
	{
		delete m_pShader;
		m_pShader = nullptr;
	}

	if(m_pInputRTT)
	{
		gRTTCache.Free(m_pInputRTT);
		m_pInputRTT = nullptr;
	}
}

//=============================================
//
//=============================================
bool CCAS::InitGame( void )
{
	return true;
}

//=============================================
//
//=============================================
void CCAS::ClearGame( void )
{
	if(m_pInputRTT)
	{
		gRTTCache.Free(m_pInputRTT);
		m_pInputRTT = nullptr;
	}
}

//=============================================
//
//=============================================
bool CCAS::Apply( void )
{
	if(!m_pCvarCAS || m_pCvarCAS->GetValue() < 1.0f)
		return true;

	if(!m_pShader)
		return false;

	Uint32 width = rns.screenwidth;
	Uint32 height = rns.screenheight;

	m_pInputRTT = gRTTCache.Alloc(width, height, false, GL_RGBA16F);
	CFBOCache::cache_fbo_t* pOutputFBO = gFBOCache.Alloc(width, height, false);

	if(!m_pInputRTT || !pOutputFBO)
	{
		if(m_pInputRTT)
		{
			gRTTCache.Free(m_pInputRTT);
			m_pInputRTT = nullptr;
		}
		if(pOutputFBO)
			gFBOCache.Free(pOutputFBO);
		return false;
	}

	R_GrabScreenToTexture(m_pInputRTT->palloc, width, height, false);

	if(!m_pShader->EnableShader())
	{
		gRTTCache.Free(m_pInputRTT);
		gFBOCache.Free(pOutputFBO);
		m_pInputRTT = nullptr;
		return false;
	}

	varAU4(const0);
	varAU4(const1);
	Float sharpness = m_pCvarSharpness ? m_pCvarSharpness->GetValue() : 0.5f;
	Float flWidth = static_cast<Float>(width);
	Float flHeight = static_cast<Float>(height);

	CasSetup(const0, const1, sharpness, flWidth, flHeight, flWidth, flHeight);

	Float c0[4];
	Float c1[4];
	for (Int32 i = 0; i < 4; i++)
	{
		memcpy(&c0[i], &const0[i], sizeof(Float));
		memcpy(&c1[i], &const1[i], sizeof(Float));
	}

	m_pShader->SetUniform4f(m_attribs.u_const0, c0[0], c0[1], c0[2], c0[3]);
	m_pShader->SetUniform4f(m_attribs.u_const1, c1[0], c1[1], c1[2], c1[3]);

	m_pShader->ResetSamplerIndex();
	m_pShader->SetUniform1i(m_attribs.u_input, 0);
	R_Bind2DTexture(GL_TEXTURE0, m_pInputRTT->palloc->gl_index);

	glBindImageTexture(1, pOutputFBO->fbo.ptexture1->gl_index, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

	Int32 groupX = (width + 15) / 16;
	Int32 groupY = (height + 15) / 16;
	glDispatchCompute(groupX, groupY, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

	glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	m_pShader->DisableShader();

	GLuint dstFBO = (rns.pboundfbo) ? rns.pboundfbo->fboid : rns.mainfbo.fboid;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, pOutputFBO->fbo.fboid);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);

	gRTTCache.Free(m_pInputRTT);
	gFBOCache.Free(pOutputFBO);
	m_pInputRTT = nullptr;

	R_ClearBinds();
	return true;
}