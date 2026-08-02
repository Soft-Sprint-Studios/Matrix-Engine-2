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
#include "funcvideo.h"

// Link the entity to its class
LINK_ENTITY_TO_CLASS(func_video, CFuncVideo);

//=============================================
// @brief
//
//=============================================
CFuncVideo::CFuncVideo( edict_t* pedict ):
	CFuncWall(pedict)
{
}

//=============================================
// @brief
//
//=============================================
CFuncVideo::~CFuncVideo( void )
{
}

//=============================================
// @brief
//
//=============================================
bool CFuncVideo::KeyValue( const keyvalue_t& kv )
{
	if (!qstrcmp(kv.keyname, "skin"))
	{
		m_pState->skin = SDL_atoi(kv.value);
		return true;
	}
	else
		return CFuncWall::KeyValue(kv);
}

//=============================================
// @brief
//
//=============================================
bool CFuncVideo::Spawn( void )
{
	if (!CFuncWall::Spawn())
		return false;

	m_pState->rendertype = RT_VIDEO;

	if (HasSpawnFlag(FL_LOOP))
		m_pState->body |= (1<<0);

	if (!HasSpawnFlag(FL_START_OFF))
		m_pState->iuser1 = 1;

	return true;
}

//=============================================
// @brief
//
//=============================================
void CFuncVideo::CallUse( CBaseEntity* pActivator, CBaseEntity* pCaller, usemode_t useMode, Float value )
{
	switch (useMode)
	{
	case USE_ON:
	case USE_TOGGLE:
		{
			m_pState->iuser1++;
		}
		break;
	case USE_OFF:
		{
			m_pState->effects |= EF_NODRAW;
		}
		break;
	}
}