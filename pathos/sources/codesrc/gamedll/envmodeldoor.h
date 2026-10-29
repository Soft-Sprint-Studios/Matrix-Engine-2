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
#ifndef ENVMODELDOOR_H
#define ENVMODELDOOR_H

#include "animatingentity.h"

//=============================================
//
//=============================================
class CEnvModelDoor : public CAnimatingEntity
{
public:
	enum
	{
		FL_START_OPEN = (1<<0),
		FL_NO_AUTO_RETURN = (1<<2),
		FL_NO_COLLISION = (1<<3)
	};

	enum door_state_t
	{
		DOOR_STATE_CLOSED = 0,
		DOOR_STATE_OPENING,
		DOOR_STATE_OPEN,
		DOOR_STATE_CLOSING
	};

public:
	// Default wait time to close
	static const Float DEFAULT_WAIT_TIME;

public:
	explicit CEnvModelDoor( edict_t* pedict );
	virtual ~CEnvModelDoor( void );

public:
	virtual void DeclareSaveFields( void ) override;
	virtual bool KeyValue( const keyvalue_t& kv ) override;
	virtual void Precache( void ) override;
	virtual bool Spawn( void ) override;
	virtual void CallUse( CBaseEntity* pActivator, CBaseEntity* pCaller, usemode_t useMode, Float value ) override;
	virtual bool CanEntityBeParented( void ) const override { return true; }

public:
	void OpenDoor( void );
	void EXPORTFN CloseDoor( void );
	void EXPORTFN DoorThink( void );
	void DoorHitTop( void );
	void DoorHitBottom( void );

private:
	string_t m_openSequence;
	string_t m_openSound;
	string_t m_closeSound;
	string_t m_lockedSound;
	string_t m_unlockedSound;
	string_t m_masterEntityName;

	Float m_waitTime;
	Int32 m_doorState;

	Double m_nextLockSoundTime;
	CEntityHandle m_activator;
};
#endif //ENVMODELDOOR_H