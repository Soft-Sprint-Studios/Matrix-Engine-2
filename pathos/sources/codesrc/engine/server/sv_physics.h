/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef SV_PHYSICS_H
#define SV_PHYSICS_H

#include "sv_main.h"
#include "sv_world.h"

class CCVar;

// Allocation count for saved movement stack
static const Uint32 SAVEDMOVE_STACK_ALLOC_SIZE = 64;

struct saved_move_t
{
	saved_move_t():
		psave_edict(nullptr),
		restoreAngles(false)
	{}

	Vector saved_origin;
	Vector saved_angles;
	edict_t* psave_edict;
	bool restoreAngles;
};

struct savedmovestack_t
{
	savedmovestack_t():
		numsavedmovingents(0),
		pmoveredict(nullptr)
	{
		savedmovingentities.resize(SAVEDMOVE_STACK_ALLOC_SIZE);
	}

	void saveEdict( edict_t* pedict, const Vector& saveOrigin, const Vector& angularMove, const Vector* pSavedAngles )
	{
		if(numsavedmovingents == savedmovingentities.size())
			savedmovingentities.resize(savedmovingentities.size()+SAVEDMOVE_STACK_ALLOC_SIZE);

		saved_move_t& savedmove = savedmovingentities[numsavedmovingents];
		numsavedmovingents++;

		savedmove.psave_edict = pedict;
		savedmove.saved_origin = saveOrigin;
		
		if(pSavedAngles)
		{
			savedmove.saved_angles = (*pSavedAngles);
			savedmove.restoreAngles = true;
		}
		else
			savedmove.restoreAngles = false;
	}

	void restoreEdictPositions( const Vector& angularmove )
	{
		// Move back any entities we already moved
		for(Uint32 j = 0; j < numsavedmovingents; j++)
		{
			saved_move_t* psaved = &savedmovingentities[j];
			psaved->psave_edict->state.origin = psaved->saved_origin;

			if(psaved->restoreAngles)
				psaved->psave_edict->state.angles = psaved->saved_angles;
			else if(psaved->psave_edict->state.flags & FL_CLIENT)
				psaved->psave_edict->state.avelocity[1] = 0.0f;
			else if(psaved->psave_edict->state.movetype != MOVETYPE_PUSHSTEP)
				psaved->psave_edict->state.angles[YAW] -= angularmove[YAW];

			SV_LinkEdict(psaved->psave_edict, FALSE);
		}

		// Reset to 0
		numsavedmovingents = 0;
	}

	// Array of saved entity states
	CArray<saved_move_t> savedmovingentities;
	// Number of saved entities
	Uint32 numsavedmovingents;
	// Entity doing the movement
	edict_t* pmoveredict;
};

struct svphysics_t
{
	svphysics_t():
		touchlinksemaphore(false),
		currentstackindex(0)
		{}

	savedmovestack_t* getStackForIndex( Int32 stackindex )
	{
		if(stackindex >= psavestackarray.size())
			psavestackarray.resize(stackindex+1);

		if(!psavestackarray[stackindex])
			psavestackarray[stackindex] = new savedmovestack_t();

		savedmovestack_t* pstack = psavestackarray[stackindex];
		pstack->numsavedmovingents = 0;

		return pstack;
	}

	void resetStack( void )
	{
		if(!psavestackarray.empty())
		{
			for(Uint32 i = 0; i < psavestackarray.size(); i++)
				delete psavestackarray[i];

			psavestackarray.clear();
		}
		psavestackarray.resize(1);
		psavestackarray[0] = new savedmovestack_t();
		currentstackindex = 0;
	}

	// Saved movement stack array
	CArray<savedmovestack_t*> psavestackarray;
	// Current highest stack index
	Int32 currentstackindex;

	// Touch link semaphore for safety
	bool touchlinksemaphore;
};

extern CCVar* g_psv_maxvelocity;
extern CCVar* g_psv_gravity;
extern CCVar* g_psv_bounce;
extern CCVar* g_psv_stepsize;
extern CCVar* g_psv_friction;
extern CCVar* g_psv_stopspeed;

extern svphysics_t g_serverPhysics;

extern void SV_Physics_Init( void );
extern void SV_Physics( void );
extern void SV_Impact( edict_t* pentity1, edict_t* pentity2, const trace_t& trace );
extern bool SV_CheckBottom( edict_t* pedict );
extern bool SV_CheckWater( edict_t* pedict );
extern Float SV_Submerged( edict_t* pedict );
#endif //SV_PHYSICS_H