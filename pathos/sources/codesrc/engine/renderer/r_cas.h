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
#ifndef R_CAS_H
#define R_CAS_H

#include "r_glsl.h"
#include "r_rttcache.h"
#include "r_fbocache.h"
#include "textures_shared.h"

class CCVar;

struct cas_attribs_t
{
	cas_attribs_t():
		u_input(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_output(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_const0(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_const1(CGLSLShader::PROPERTY_UNAVAILABLE)
	{}

	Int32 u_input;
	Int32 u_output;
	Int32 u_const0;
	Int32 u_const1;
};

/*
====================
CCAS

====================
*/
class CCAS
{
public:
	CCAS( void );
	~CCAS( void );

public:
	bool Init( void );
	void Shutdown( void );

	bool InitGL( void );
	void ClearGL( void );

	bool InitGame( void );
	void ClearGame( void );

	bool Apply( void );

private:
	CGLSLShader* m_pShader;
	cas_attribs_t m_attribs;

	CCVar* m_pCvarCAS;
	CCVar* m_pCvarSharpness;

	rtt_texture_t* m_pInputRTT;
};

extern CCAS gCAS;

#endif // R_CAS_H