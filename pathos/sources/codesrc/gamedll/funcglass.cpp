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
#include "gd_includes.h"
#include "funcglass.h"

LINK_ENTITY_TO_CLASS(func_glass, CFuncGlass);

//=============================================
// @brief
//
//=============================================
CFuncGlass::CFuncGlass( edict_t* pedict ):
	CFuncWall(pedict)
{
}

//=============================================
// @brief
//
//=============================================
CFuncGlass::~CFuncGlass( void )
{
}

//=============================================
// @brief
//
//=============================================
bool CFuncGlass::KeyValue( const keyvalue_t& kv )
{
	if (!qstrcmp(kv.keyname, "normalid"))
	{
		m_pState->iuser1 = SDL_atoi(kv.value);
		return true;
	}
	else if (!qstrcmp(kv.keyname, "amount"))
	{
		m_pState->scale = SDL_atof(kv.value);
		return true;
	}
	else
		return CFuncWall::KeyValue(kv);
}

//=============================================
// @brief
//
//=============================================
bool CFuncGlass::Spawn( void )
{
	if (!CFuncWall::Spawn())
		return false;

	m_pState->rendertype = RT_GLASS;

	if (m_pState->scale <= 0.0f)
		m_pState->scale = 0.05f;

	return true;
}