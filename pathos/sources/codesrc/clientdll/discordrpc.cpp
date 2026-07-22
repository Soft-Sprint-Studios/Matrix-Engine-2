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
#include "discordrpc.h"
#include "clientdll.h"
#include "discord_rpc.h"
#include "cache_model.h"
#include <ctime>

CDiscordRPC g_discordRpc;

//=============================================
// @brief Constructor
//
//=============================================
CDiscordRPC::CDiscordRPC( void ) :
	m_isInitialized( false ),
	m_pCvarEnabled( nullptr ),
	m_pCvarClientId( nullptr ),
	m_startTime( 0 )
{
}

//=============================================
// @brief Destructor
//
//=============================================
CDiscordRPC::~CDiscordRPC( void )
{
	Shutdown();
}

//=============================================
//
//
//=============================================
void CDiscordRPC::Init( void )
{
	m_pCvarEnabled = cl_engfuncs.pfnCreateCVar( CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "cl_discord_enabled", "1", "Toggles Discord Rich Presence." );
	m_pCvarClientId = cl_engfuncs.pfnCreateCVar( CVAR_STRING, (FL_CV_CLIENT|FL_CV_SAVE), "cl_discord_clientid", "1526988768144261140", "Discord Application Client ID." );
	
	m_startTime = time( nullptr );
}

//=============================================
//
//
//=============================================
void CDiscordRPC::Shutdown( void )
{
	if ( m_isInitialized )
	{
		Discord_Shutdown();
		m_isInitialized = false;
		m_currentClientId.clear();
	}
}

//=============================================
//
//
//=============================================
void CDiscordRPC::Frame( void )
{
	if ( !m_pCvarEnabled || !m_pCvarClientId )
		return;

	bool enabled = m_pCvarEnabled->GetValue() > 0;
	const Char* pstrClientId = m_pCvarClientId->GetStrValue();

	if ( enabled )
	{
		if (!m_isInitialized || !(m_currentClientId == pstrClientId))
		{
			if ( m_isInitialized )
			{
				Discord_Shutdown();
				m_isInitialized = false;
			}

			if ( pstrClientId && pstrClientId[0] != '\0' )
			{
				DiscordEventHandlers handlers;
				memset( &handlers, 0, sizeof( handlers ) );

				Discord_Initialize( pstrClientId, &handlers, 1, nullptr );
				m_isInitialized = true;
				m_currentClientId = pstrClientId;

				UpdatePresence( "Main Menu", "Idle" );
			}
		}

		if ( m_isInitialized )
		{
			Discord_RunCallbacks();
		}
	}
	else
	{
		if ( m_isInitialized )
		{
			Shutdown();
		}
	}
}

//=============================================
//
//
//=============================================
void CDiscordRPC::OnLevelInit(void)
{
	const cache_model_t* pmodel = cl_engfuncs.pfnGetModel(WORLD_MODEL_INDEX);
	if (pmodel && !pmodel->name.empty())
	{
		CString mapName = pmodel->name;

		Int32 prefixPos = mapName.find(0, "maps/");
		if (prefixPos != CString::CSTRING_NO_POSITION)
		{
			mapName.erase(0, 5);
		}

		Int32 suffixPos = mapName.find(0, ".bsp");
		if (suffixPos != CString::CSTRING_NO_POSITION)
		{
			mapName.erase(suffixPos, 4);
		}

		CString details;
		details << "Map: " << mapName;
		UpdatePresence("In-Game", details.c_str());
	}
	else
	{
		UpdatePresence("In-Game", "Loading...");
	}
}

//=============================================
//
//
//=============================================
void CDiscordRPC::UpdatePresence( const Char* pstrDetails, const Char* pstrState, const Char* pstrLargeImageKey, const Char* pstrLargeImageText )
{
	if ( !m_isInitialized )
		return;

	DiscordRichPresence discordPresence;
	memset( &discordPresence, 0, sizeof( discordPresence ) );
	discordPresence.state = pstrState;
	discordPresence.details = pstrDetails;
	discordPresence.startTimestamp = m_startTime;
	discordPresence.largeImageKey = pstrLargeImageKey ? pstrLargeImageKey : "icon";
	discordPresence.largeImageText = pstrLargeImageText ? pstrLargeImageText : "Matrix Engine 2";

	Discord_UpdatePresence( &discordPresence );
}