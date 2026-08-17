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
#ifndef R_SSAO_H
#define R_SSAO_H

#include "r_glsl.h"
#include "r_vbo.h"
#include "r_rttcache.h"
#include "r_fbocache.h"

class CCVar;
struct en_texalloc_t;

enum ssao_pass_t
{
	SSAO_PASS_RAW = 0,
	SSAO_PASS_BLUR,
	SSAO_PASS_APPLY
};

struct ssao_attribs_t
{
	ssao_attribs_t() :
		a_position(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_texcoord(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_modelview(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_scene_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_scene_projection_inverse(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_depthMap(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_noise(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_ssaoTexture(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_noiseScale(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_sampleRad(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_intensity(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_kernel(CGLSLShader::PROPERTY_UNAVAILABLE),
		d_pass(CGLSLShader::PROPERTY_UNAVAILABLE)
		{}

	Int32 a_position;
	Int32 a_texcoord;

	Int32 u_modelview;
	Int32 u_projection;
	Int32 u_scene_projection;
	Int32 u_scene_projection_inverse;

	Int32 u_depthMap;
	Int32 u_noise;
	Int32 u_ssaoTexture;

	Int32 u_noiseScale;
	Int32 u_sampleRad;
	Int32 u_intensity;
	Int32 u_kernel;

	Int32 d_pass;
};

/*
====================
CSSAOManager

====================
*/
class CSSAOManager
{
public:
	static const Uint32 SSAO_KERNEL_SIZE = 16;
	static const Uint32 SSAO_NOISE_DIMENSION = 4;

public:
	CSSAOManager( void );
	~CSSAOManager( void );

public:
	// Initializes console variables and kernel points
	bool Init( void );
	// Performs complete shutdown
	void Shutdown( void );

	// Initializes OpenGL objects, compiles shaders, and sets VBO
	bool InitGL( void );
	// Clears OpenGL objects
	void ClearGL( void );

	// Initializes per-game-session resources
	bool InitGame( void );
	// Cleans up game resources on map unload
	void ClearGame( void );

	// Executes depth blit, SSAO generation, blur, and screen multiplicative blend
	bool DrawSSAO( void );

private:
	// Generates hemispherical distribution sampling kernel
	void GenerateKernel( void );
	// Generates 4x4 random rotation noise texture
	void CreateNoiseTexture( void );

private:
	// GLSL shader object
	CGLSLShader* m_pShader;
	// Fullscreen quad VBO
	CVBO* m_pVBO;
	// Shader attribute/uniform mappings
	ssao_attribs_t m_attribs;

	// 4x4 noise texture allocation
	en_texalloc_t* m_pNoiseTexture;

	// Precomputed hemisphere sample kernel points
	Vector m_kernel[SSAO_KERNEL_SIZE];

	// CVars
	CCVar* m_pCvarSSAO;
	CCVar* m_pCvarSSAORadius;
	CCVar* m_pCvarSSAOIntensity;
};

extern CSSAOManager gSSAO;

#endif // R_SSAO_H