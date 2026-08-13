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
#include "envmodelphysics.h"

// Link the entity to its class
LINK_ENTITY_TO_CLASS(env_model_physics, CEnvModelPhysics);

//=============================================
// @brief
//
//=============================================
CEnvModelPhysics::CEnvModelPhysics( edict_t* pedict ):
	CEnvModelBreakable(pedict)
{
}

//=============================================
// @brief
//
//=============================================
CEnvModelPhysics::~CEnvModelPhysics( void )
{
}

//=============================================
// @brief
//
//=============================================
void CEnvModelPhysics::SetSpawnProperties( void )
{
	m_pState->movetype = MOVETYPE_PHYSICS;
	m_pState->solid = SOLID_BBOX;
}

//=============================================
// @brief
//
//=============================================
bool CEnvModelPhysics::KeyValue( const keyvalue_t& kv )
{
	if(!qstrcmp(kv.keyname, "mass"))
	{
		m_pState->fuser1 = SDL_atof(kv.value);
		return true;
	}
	else
		return CEnvModelBreakable::KeyValue(kv);
}

//=============================================
// @brief
//
//=============================================
bool CEnvModelPhysics::Spawn( void )
{
	if(!CEnvModelBreakable::Spawn())
		return false;

	return true;
}