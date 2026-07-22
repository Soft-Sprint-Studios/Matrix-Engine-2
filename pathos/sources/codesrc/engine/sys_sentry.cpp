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
#include "sys_sentry.h"
#include "console.h"
#include "cvar.h"
#include "system.h"
#include <sentry.h>

CSysSentryManager gSentry;

CCVar* g_pCvarSentryEnabled = nullptr;
CCVar* g_pCvarSentryDSN = nullptr;

//=============================================
// @brief Constructor
//
//=============================================
CSysSentryManager::CSysSentryManager( void ):
	m_isInitialized(false)
{
}

//=============================================
// @brief Destructor
//
//=============================================
CSysSentryManager::~CSysSentryManager( void )
{
	Shutdown();
}

//=============================================
//
//
//=============================================
bool CSysSentryManager::Init( void )
{
	if (m_isInitialized)
		return true;

	g_pCvarSentryEnabled = gConsole.CreateCVar(CVAR_FLOAT, (FL_CV_CLIENT|FL_CV_SAVE), "sv_sentry", "1", "Toggle Sentry crash reporting.");
	g_pCvarSentryDSN = gConsole.CreateCVar(CVAR_STRING, (FL_CV_CLIENT | FL_CV_SAVE), "sentry_dsn", "https://f737baca1820d6922ef0da6f0d4aabca@o4505736231124992.ingest.us.sentry.io/4511779312631808", "Sentry DSN string.");

	if (g_pCvarSentryEnabled->GetValue() < 1)
		return true;

	const Char* pstrDSN = g_pCvarSentryDSN->GetStrValue();
	if (!pstrDSN || !qstrlen(pstrDSN))
	{
		Con_Printf("%s - Sentry enabled but no DSN provided.\n", __FUNCTION__);
		return true;
	}

	sentry_options_t *options = sentry_options_new();
	sentry_options_set_dsn(options, pstrDSN);
	sentry_options_set_database_path(options, ".sentry-native");
	sentry_options_set_release(options, "matrix-engine2@1.0.0");

#ifdef _DEBUG
	sentry_options_set_environment(options, "debug");
	sentry_options_set_debug(options, 1);
#else
	sentry_options_set_environment(options, "release");
#endif

	if (sentry_init(options) != 0)
	{
		Con_EPrintf("Failed to initialize Sentry SDK.\n");
		return false;
	}

	Con_Printf("Sentry crash reporting initialized.\n");
	m_isInitialized = true;
	return true;
}

//=============================================
//
//
//=============================================
void CSysSentryManager::Shutdown( void )
{
	if (!m_isInitialized)
		return;

	sentry_close();
	m_isInitialized = false;
}