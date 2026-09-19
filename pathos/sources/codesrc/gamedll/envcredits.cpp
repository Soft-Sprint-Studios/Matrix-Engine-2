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
#include "envcredits.h"
#include "player.h"

// Default scroll speed
const Float CEnvCredits::DEFAULT_SCROLL_SPEED = 45.0f;

// Link the entity to its class
LINK_ENTITY_TO_CLASS(env_credits, CEnvCredits);

//=============================================
// @brief
//
//=============================================
CEnvCredits::CEnvCredits( edict_t* pedict ):
	CPointEntity(pedict),
	m_scrollSpeed(DEFAULT_SCROLL_SPEED)
{
}

//=============================================
// @brief
//
//=============================================
CEnvCredits::~CEnvCredits( void )
{
}

//=============================================
// @brief
//
//=============================================
void CEnvCredits::DeclareSaveFields( void )
{
	CPointEntity::DeclareSaveFields();

	DeclareSaveField(DEFINE_DATA_FIELD(CEnvCredits, m_scrollSpeed, EFIELD_FLOAT));
}

//=============================================
// @brief
//
//=============================================
bool CEnvCredits::KeyValue( const keyvalue_t& kv )
{
	if (!qstrcmp(kv.keyname, "scrollspeed"))
	{
		m_scrollSpeed = SDL_atof(kv.value);
		return true;
	}
	else
	{
		return CPointEntity::KeyValue(kv);
	}
}

//=============================================
// @brief
//
//=============================================
bool CEnvCredits::Spawn( void )
{
	if (!CPointEntity::Spawn())
		return false;

	if (m_scrollSpeed <= 0)
		m_scrollSpeed = DEFAULT_SCROLL_SPEED;

	return true;
}

//=============================================
// @brief
//
//=============================================
void CEnvCredits::CallUse( CBaseEntity* pActivator, CBaseEntity* pCaller, usemode_t useMode, Float value )
{
	gd_engfuncs.pfnUserMessageBegin(MSG_ALL, g_usermsgs.showcredits, nullptr, nullptr);
		gd_engfuncs.pfnMsgWriteFloat(m_scrollSpeed);
	gd_engfuncs.pfnUserMessageEnd();
}