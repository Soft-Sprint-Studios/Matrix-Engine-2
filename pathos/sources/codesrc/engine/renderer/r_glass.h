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
#ifndef R_GLASS_H
#define R_GLASS_H

#include "ref_params.h"
#include "r_glsl.h"

struct cl_entity_t;
struct msurface_t;
struct en_texalloc_t;

struct cl_glass_t
{
	cl_glass_t() :
		pentity(nullptr),
		psurface(nullptr),
		start_vertex(0),
		num_vertexes(0),
		pnormalmap(nullptr),
		amount(0.05f)
	{}

	cl_entity_t* pentity;

	Vector mins;
	Vector maxs;
	Vector origin;
	msurface_t* psurface;

	Uint32 start_vertex;
	Uint32 num_vertexes;

	en_texalloc_t* pnormalmap;
	Float amount;
};

struct glass_vertex_t
{
	glass_vertex_t()
	{
		memset(origin, 0, sizeof(origin));
		memset(normal, 0, sizeof(normal));
		memset(tangent, 0, sizeof(tangent));
		memset(binormal, 0, sizeof(binormal));
		memset(texcoord, 0, sizeof(texcoord));
		memset(padding, 0, sizeof(padding));
	}

	vec4_t origin;
	Vector normal;
	Vector tangent;
	Vector binormal;
	Float texcoord[2];
	byte padding[4];
};

struct glass_attribs_t
{
	glass_attribs_t() :
		a_vertex(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_normal(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_tangent(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_binormal(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_texcoord(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_modelview(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_normalmatrix(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_refract_texture(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_normal_texture(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_glass_params(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_fogcolor(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_fogparams(CGLSLShader::PROPERTY_UNAVAILABLE),
		d_fog(CGLSLShader::PROPERTY_UNAVAILABLE)
	{}

	Int32 a_vertex;
	Int32 a_normal;
	Int32 a_tangent;
	Int32 a_binormal;
	Int32 a_texcoord;

	Int32 u_modelview;
	Int32 u_projection;
	Int32 u_normalmatrix;

	Int32 u_refract_texture;
	Int32 u_normal_texture;
	Int32 u_glass_params;

	Int32 u_fogcolor;
	Int32 u_fogparams;

	Int32 d_fog;
};

/*
====================
CGlassManager

====================
*/
class CGlassManager
{
public:
	CGlassManager( void );
	~CGlassManager( void );

public:
	bool Init( void );
	void Shutdown( void );

	bool InitGL( void );
	void ClearGL( void );

	bool InitGame( void );
	void ClearGame( void );

	bool DrawGlass( void );
	void AllocNewGlass( cl_entity_t* pentity );

private:
	cl_glass_t* m_pCurrentGlass;
	CArray<cl_glass_t*> m_glassArray;

	class CGLSLShader* m_pShader;
	class CVBO* m_pVBO;

	glass_attribs_t m_attribs;
};

extern CGlassManager gGlassManager;
#endif // R_GLASS_H