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
#include "envmodeldoor.h"

// Default wait time to close
const Float CEnvModelDoor::DEFAULT_WAIT_TIME = 4.0f;

// Link the entity to its class
LINK_ENTITY_TO_CLASS(env_model_door, CEnvModelDoor);

//=============================================
// @brief
//
//=============================================
CEnvModelDoor::CEnvModelDoor( edict_t* pedict ):
	CAnimatingEntity(pedict),
	m_openSequence(NO_STRING_VALUE),
	m_openSound(NO_STRING_VALUE),
	m_closeSound(NO_STRING_VALUE),
	m_lockedSound(NO_STRING_VALUE),
	m_unlockedSound(NO_STRING_VALUE),
	m_masterEntityName(NO_STRING_VALUE),
	m_waitTime(DEFAULT_WAIT_TIME),
	m_doorState(DOOR_STATE_CLOSED),
	m_nextLockSoundTime(0)
{
}

//=============================================
// @brief
//
//=============================================
CEnvModelDoor::~CEnvModelDoor( void )
{
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::DeclareSaveFields( void )
{
	// Call base class to do it first
	CAnimatingEntity::DeclareSaveFields();

	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_openSequence, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_openSound, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_closeSound, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_lockedSound, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_unlockedSound, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_masterEntityName, EFIELD_STRING));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_waitTime, EFIELD_FLOAT));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_doorState, EFIELD_INT32));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_nextLockSoundTime, EFIELD_TIME));
	DeclareSaveField(DEFINE_DATA_FIELD(CEnvModelDoor, m_activator, EFIELD_EHANDLE));
}

//=============================================
// @brief
//
//=============================================
bool CEnvModelDoor::KeyValue( const keyvalue_t& kv )
{
	if(!qstrcmp(kv.keyname, "opensequence"))
	{
		m_openSequence = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "opensound"))
	{
		m_openSound = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "closesound"))
	{
		m_closeSound = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "lockedsound"))
	{
		m_lockedSound = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "unlockedsound"))
	{
		m_unlockedSound = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "master"))
	{
		m_masterEntityName = gd_engfuncs.pfnAllocString(kv.value);
		return true;
	}
	else if(!qstrcmp(kv.keyname, "wait"))
	{
		m_waitTime = SDL_atof(kv.value);
		return true;
	}
	else
		return CAnimatingEntity::KeyValue(kv);
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::Precache( void )
{
	// Call base class to precache
	CAnimatingEntity::Precache();

	if(m_openSound != NO_STRING_VALUE)
		gd_engfuncs.pfnPrecacheSound(gd_engfuncs.pfnGetString(m_openSound));

	if(m_closeSound != NO_STRING_VALUE)
		gd_engfuncs.pfnPrecacheSound(gd_engfuncs.pfnGetString(m_closeSound));

	if(m_lockedSound != NO_STRING_VALUE)
		gd_engfuncs.pfnPrecacheSound(gd_engfuncs.pfnGetString(m_lockedSound));

	if(m_unlockedSound != NO_STRING_VALUE)
		gd_engfuncs.pfnPrecacheSound(gd_engfuncs.pfnGetString(m_unlockedSound));
}

//=============================================
// @brief
//
//=============================================
bool CEnvModelDoor::Spawn( void )
{
	if(!CAnimatingEntity::Spawn())
		return false;

	if(HasSpawnFlag(FL_NO_COLLISION))
		m_pState->solid = SOLID_NOT;
	else
		m_pState->solid = SOLID_BBOX;

	m_pState->movetype = MOVETYPE_NONE;

	ResetSequenceInfo();
	m_isSequenceLooped = false;
	InitBoneControllers();

	SetUse(&CEnvModelDoor::CallUse);

	if(HasSpawnFlag(FL_START_OPEN))
	{
		m_doorState = DOOR_STATE_OPEN;
		if(m_openSequence != NO_STRING_VALUE)
		{
			SetSequence(gd_engfuncs.pfnGetString(m_openSequence));
			ResetSequenceInfo();
			m_isSequenceLooped = false;
			m_pState->frame = 255;
		}
	}
	else
	{
		m_doorState = DOOR_STATE_CLOSED;
		m_pState->frame = 0;
	}

	m_pState->framerate = 0;

	SetSequenceBox(false);

	gd_engfuncs.pfnSetOrigin(m_pEdict, m_pState->origin, false);

	return true;
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::CallUse( CBaseEntity* pActivator, CBaseEntity* pCaller, usemode_t useMode, Float value )
{
	if(m_masterEntityName != NO_STRING_VALUE)
	{
		const Char* pstrMaster = gd_engfuncs.pfnGetString(m_masterEntityName);
		if(!Util::IsMasterTriggered(pstrMaster, pActivator, this))
		{
			if(m_nextLockSoundTime <= g_pGameVars->time && m_lockedSound != NO_STRING_VALUE)
			{
				Util::EmitEntitySound(this, gd_engfuncs.pfnGetString(m_lockedSound), SND_CHAN_BODY);
				m_nextLockSoundTime = g_pGameVars->time + 1.0f;
			}
			return;
		}
	}

	m_activator = pActivator;

	switch(useMode)
	{
	case USE_ON:
		if(m_doorState == DOOR_STATE_CLOSED || m_doorState == DOOR_STATE_CLOSING)
			OpenDoor();
		break;
	case USE_OFF:
		if(m_doorState == DOOR_STATE_OPEN || m_doorState == DOOR_STATE_OPENING)
			CloseDoor();
		break;
	case USE_TOGGLE:
	default:
		if(m_doorState == DOOR_STATE_CLOSED || m_doorState == DOOR_STATE_CLOSING)
			OpenDoor();
		else if(m_doorState == DOOR_STATE_OPEN || m_doorState == DOOR_STATE_OPENING)
			CloseDoor();
		break;
	}
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::OpenDoor( void )
{
	if(m_doorState == DOOR_STATE_OPEN || m_doorState == DOOR_STATE_OPENING)
		return;

	if(m_unlockedSound != NO_STRING_VALUE)
	{
		Util::EmitEntitySound(this, gd_engfuncs.pfnGetString(m_unlockedSound), SND_CHAN_BODY);
		m_unlockedSound = NO_STRING_VALUE;
	}

	if(m_openSequence != NO_STRING_VALUE)
	{
		SetSequence(gd_engfuncs.pfnGetString(m_openSequence));
		if(m_doorState != DOOR_STATE_CLOSING)
		{
			ResetSequenceInfo();
			m_pState->frame = 0;
		}
		m_isSequenceLooped = false;
		m_pState->framerate = 1.0f;
		m_pState->animtime = g_pGameVars->time;
	}

	if(m_openSound != NO_STRING_VALUE)
		Util::EmitEntitySound(this, gd_engfuncs.pfnGetString(m_openSound), SND_CHAN_BODY);

	m_doorState = DOOR_STATE_OPENING;

	SetThink(&CEnvModelDoor::DoorThink);
	m_pState->nextthink = g_pGameVars->time + 0.05f;
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::CloseDoor( void )
{
	if(m_doorState == DOOR_STATE_CLOSED || m_doorState == DOOR_STATE_CLOSING)
		return;

	if(m_openSequence != NO_STRING_VALUE)
	{
		SetSequence(gd_engfuncs.pfnGetString(m_openSequence));
		if(m_doorState != DOOR_STATE_OPENING)
		{
			ResetSequenceInfo();
			m_pState->frame = 255;
		}
		m_isSequenceLooped = false;
		m_pState->framerate = -1.0f;
		m_pState->animtime = g_pGameVars->time;
	}

	if(m_closeSound != NO_STRING_VALUE)
		Util::EmitEntitySound(this, gd_engfuncs.pfnGetString(m_closeSound), SND_CHAN_BODY);

	m_doorState = DOOR_STATE_CLOSING;

	SetThink(&CEnvModelDoor::DoorThink);
	m_pState->nextthink = g_pGameVars->time + 0.05f;
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::DoorThink( void )
{
	Double animFinishTime = 0;
	FrameAdvance(0, &animFinishTime);

	if(m_isSequenceFinished)
	{
		if(m_doorState == DOOR_STATE_OPENING)
			DoorHitTop();
		else if(m_doorState == DOOR_STATE_CLOSING)
			DoorHitBottom();

		return;
	}

	m_pState->nextthink = g_pGameVars->time + 0.05f;
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::DoorHitTop( void )
{
	m_doorState = DOOR_STATE_OPEN;
	m_pState->framerate = 0.0f;
	m_pState->frame = 255;

	UseTargets(m_activator, USE_TOGGLE, 0);

	SetSequenceBox(false);

	if(!HasSpawnFlag(FL_NO_AUTO_RETURN) && m_waitTime > 0)
	{
		SetThink(&CEnvModelDoor::CloseDoor);
		m_pState->nextthink = g_pGameVars->time + m_waitTime;
	}
	else
	{
		SetThink(nullptr);
		m_pState->nextthink = 0;
	}
}

//=============================================
// @brief
//
//=============================================
void CEnvModelDoor::DoorHitBottom( void )
{
	m_doorState = DOOR_STATE_CLOSED;
	m_pState->framerate = 0.0f;
	m_pState->frame = 0;

	SetSequenceBox(false);

	SetThink(nullptr);
	m_pState->nextthink = 0;
}