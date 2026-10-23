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
#include "gameservercommand.h"

// Link the entity to it's class
LINK_ENTITY_TO_CLASS(game_servercommand, CGameServerCommand);

//=============================================
// @brief
//
//=============================================
CGameServerCommand::CGameServerCommand( edict_t* pedict ):
	CPointEntity(pedict)
{
}

//=============================================
// @brief
//
//=============================================
CGameServerCommand::~CGameServerCommand( void )
{
}

//=============================================
// @brief
//
//=============================================
void CGameServerCommand::DeclareSaveFields( void )
{
	CPointEntity::DeclareSaveFields();
}

//=============================================
// @brief
//
//=============================================
bool CGameServerCommand::Spawn( void )
{
	if(m_pFields->message == NO_STRING_VALUE)
	{
		Util::WarnEmptyEntity(m_pEdict);
		return false;
	}

	if(!CPointEntity::Spawn())
		return false;

	return true;
}

//=============================================
// @brief
//
//=============================================
void CGameServerCommand::CallUse( CBaseEntity* pActivator, CBaseEntity* pCaller, usemode_t useMode, Float value )
{
	const Char* pstrCommand = gd_engfuncs.pfnGetString(m_pFields->message);
	if(pstrCommand && qstrlen(pstrCommand) > 0)
	{
		CString cmd;
		cmd << pstrCommand << "\n";
		gd_engfuncs.pfnServerCommand(cmd.c_str());
	}
}