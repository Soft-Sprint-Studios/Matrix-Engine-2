/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include "includes.h"
#include "com_math.h"
#include "edict.h"
#include "sv_main.h"
#include "sv_entities.h"
#include "sv_world.h"
#include "system.h"

#include "enginestate.h"
#include "edictmanager.h"
#include "brushmodel.h"
#include "modelcache.h"
#include "frustum.h"
#include "sv_move.h"

//=============================================
//
//=============================================
bool SV_SetModel( edict_t* pedict, const Char* pstrFilepath, bool setbounds )
{
	if(!pstrFilepath || !qstrlen(pstrFilepath))
	{
		Con_Printf("%s - Empty model name specified for entity '%s'.\n", __FUNCTION__, SV_GetString(pedict->fields.classname));
		return false;
	}

	sv_model_t* psvmodel = nullptr;
	CacheNameIndexMap_t::iterator it = svs.modelcachemap.find(pstrFilepath);
	if(it != svs.modelcachemap.end())
		psvmodel = &svs.modelcache[it->second];

	cache_model_t* pmodel = nullptr;
	if(!psvmodel)
	{
		Con_Printf("[flags=onlyonce_game]%s - Model '%s' not precached for entity '%s'.\n", __FUNCTION__, pstrFilepath, SV_GetString(pedict->fields.classname));

		CString fileextension(pstrFilepath+qstrlen(pstrFilepath)-4);
		fileextension.tolower();

		if(!qstrcmp(fileextension, ".spr"))
		{
			if(!svs.perrorsprite)
				return false;

			pmodel = svs.perrorsprite;
			pedict->fields.modelname = SV_AllocString(svs.perrorsprite->name.c_str());
			pedict->state.modelindex = svs.perrorsprite->cacheindex;
		}
		else if(!qstrcmp(fileextension, ".mdl"))
		{
			if(!svs.perrormodel)
				return false;

			pmodel = svs.perrormodel;
			pedict->fields.modelname = SV_AllocString(svs.perrormodel->name.c_str());
			pedict->state.modelindex = svs.perrormodel->cacheindex;
		}
		else
		{
			// Shouldn't happen
			return false;
		}
	}
	else
	{
		pmodel = gModelCache.GetModelByIndex(psvmodel->cache_index);
		if(!pmodel)
		{
			Con_Printf("[flags=onlyonce_game]%s - Could not load '%s'.\n", __FUNCTION__, pstrFilepath);
			return false;
		}

		pedict->fields.modelname = SV_AllocString(pstrFilepath);
		pedict->state.modelindex = pmodel->cacheindex;
	}

	if(setbounds)
	{
		// Set size from model
		SV_SetMinsMaxs(pedict, pmodel->mins, pmodel->maxs);
	}

	return true;
}

//=============================================
//
//=============================================
void SV_SetMinsMaxs( edict_t* pedict, const Vector& mins, const Vector& maxs )
{
	for(Uint32 i = 0; i < 3; i++)
	{
		if(mins[i] > maxs[i])
		{
			CString errormsg;
			errormsg << __FUNCTION__ << " - Backwards mins maxs on " << SV_GetString(pedict->fields.classname) << " entity";
			if(pedict->fields.targetname != NO_STRING_VALUE)
				errormsg << "(" << SV_GetString(pedict->fields.targetname) << ")\n";

			Con_EPrintf(errormsg.c_str());
			break;
		}
	}

	pedict->state.mins = mins;
	pedict->state.maxs = maxs;

	Math::VectorAdd(pedict->state.origin, pedict->state.mins, pedict->state.absmin);
	Math::VectorAdd(pedict->state.origin, pedict->state.maxs, pedict->state.absmax);

	Math::VectorSubtract(maxs, mins, pedict->state.size);

	// Update the leafnums
	pedict->leafnums.clear();
	SV_LinkEdict(pedict, false);
}

//=============================================
//
//=============================================
void SV_SetSize( edict_t* pedict, const Vector& size )
{
	for(Uint32 i = 0; i < 3; i++)
	{
		pedict->state.mins[i] = size[i] + 1;
		pedict->state.maxs[i] = -size[i] - 1;
	}

	Math::VectorAdd(pedict->state.origin, pedict->state.mins, pedict->state.absmin);
	Math::VectorAdd(pedict->state.origin, pedict->state.maxs, pedict->state.absmax);

	pedict->state.size = size;

	// Update the leafnums
	pedict->leafnums.clear();
	SV_LinkEdict(pedict, false);
}

//=============================================
//
//=============================================
void SV_ReadjustChildByMove( edict_t* pedict, edict_t* pmovingparent, const Vector& parentPrevOrigin, const Vector& parentCurOrigin )
{
	Vector localOrigin;
	Vector myPrevOrigin = pedict->state.origin;
	Math::VectorSubtract(myPrevOrigin, parentPrevOrigin, localOrigin);
	Math::VectorAdd(localOrigin, parentCurOrigin, pedict->state.origin);

	Math::VectorAdd(pedict->state.origin, pedict->state.mins, pedict->state.absmin);
	Math::VectorAdd(pedict->state.origin, pedict->state.maxs, pedict->state.absmax);

	// Update the leafnums
	pedict->leafnums.clear();
	SV_LinkEdict(pedict, false);

	// Have game dll do adjustments
	svs.dllfuncs.pfnOnParentEntitySetOrigin(pedict, pmovingparent, parentPrevOrigin, parentCurOrigin);

	// Ensure children are also set
	for(Uint32 i = 0; i < pedict->state.children.size(); i++)
	{
		entindex_t childindex = pedict->state.children[i];
		edict_t* pchildedict = SV_GetEdictByIndex(childindex);
		if(!pchildedict || pchildedict->free)
			continue;

		// Set origin for child and link to world
		SV_ReadjustChildByMove(pchildedict, pedict, parentPrevOrigin, parentCurOrigin);
	}
}

//=============================================
//
//=============================================
void SV_SetOrigin( edict_t* pedict, const Vector& origin, bool realignEntity )
{
	Vector prevOrigin = pedict->state.origin;
	Math::VectorCopy(origin, pedict->state.origin);

	Math::VectorAdd(pedict->state.origin, pedict->state.mins, pedict->state.absmin);
	Math::VectorAdd(pedict->state.origin, pedict->state.maxs, pedict->state.absmax);

	Vector parentMovement;
	Math::VectorSubtract(origin, prevOrigin, parentMovement);

	// Update the leafnums
	pedict->leafnums.clear();
	SV_LinkEdict(pedict, false);

	// Only do anything if the origin actually changed
	if(prevOrigin == origin)
		return;

	// Inform game dll if set
	svs.dllfuncs.pfnOnEntitySetOrigin(pedict, prevOrigin, origin, realignEntity);

	// Ensure children are also set
	for(Uint32 i = 0; i < pedict->state.children.size(); i++)
	{
		entindex_t childindex = pedict->state.children[i];
		edict_t* pchildedict = SV_GetEdictByIndex(childindex);
		if(!pchildedict || pchildedict->free)
			continue;

		// Set origin for child and link to world
		SV_ReadjustChildByMove(pchildedict, pedict, prevOrigin, origin);
	}
}

//=============================================
//
//=============================================
void SV_ReadjustChildByRotation( edict_t* pedict, edict_t* protatingparent, const Vector& parentAngularMovement, Float (*pPrevAngleMatrix)[4], Float (*pCurAngleMatrix)[4] )
{
	// Calculate new angle
	Math::VectorAdd(pedict->state.angles, parentAngularMovement, pedict->state.angles);

	Vector start;
	Math::VectorSubtract(pedict->state.origin, protatingparent->state.origin, start);

	// Calculate destination position
	Vector move, end;
	Math::VectorInverseRotate(start, pPrevAngleMatrix, move);
	Math::VectorRotate(move, pCurAngleMatrix, end);

	Math::VectorAdd(end, protatingparent->state.origin, pedict->state.origin);
	SV_LinkEdict(pedict, FALSE);

	// Calculate velocity rotation change
	Vector velocity = pedict->state.velocity;
	Float velocityLength = velocity.Length();
	velocity.Normalize();

	Vector refvelocity, endvelocity;
	Math::VectorInverseRotate(velocity, pPrevAngleMatrix, refvelocity);
	Math::VectorRotate(refvelocity, pCurAngleMatrix, endvelocity);
	Math::VectorScale(endvelocity, velocityLength, pedict->state.velocity);

	// Calculate base velocity rotation change
	velocity = pedict->state.basevelocity;
	velocityLength = velocity.Length();
	velocity.Normalize();

	Math::VectorInverseRotate(velocity, pPrevAngleMatrix, refvelocity);
	Math::VectorRotate(refvelocity, pCurAngleMatrix, endvelocity);
	Math::VectorScale(endvelocity, velocityLength, pedict->state.basevelocity);

	// Calculate angular velocity rotation change
	velocity = pedict->state.avelocity;
	velocityLength = velocity.Length();
	velocity.Normalize();

	Math::VectorInverseRotate(velocity, pPrevAngleMatrix, refvelocity);
	Math::VectorRotate(refvelocity, pCurAngleMatrix, endvelocity);
	Math::VectorScale(endvelocity, velocityLength, pedict->state.avelocity);

	// Calculate move direction velocity rotation change
	Vector refmovedir;
	Math::VectorInverseRotate(pedict->state.movedir, pPrevAngleMatrix, refmovedir);
	Math::VectorRotate(refmovedir, pCurAngleMatrix, pedict->state.movedir);

	// Have gamedll do adjustments too
	svs.dllfuncs.pfnOnParentEntitySetAngles(pedict, protatingparent, protatingparent->state.origin, pPrevAngleMatrix, pCurAngleMatrix, parentAngularMovement);

	// Propogate to children as well
	for(Uint32 i = 0; i < pedict->state.children.size(); i++)
	{
		entindex_t childindex = pedict->state.children[i];
		edict_t* pchildedict = SV_GetEdictByIndex(childindex);
		if(!pchildedict || pchildedict->free)
			continue;

		SV_ReadjustChildByRotation(pchildedict, protatingparent, parentAngularMovement, pPrevAngleMatrix, pCurAngleMatrix);
	}
}

//=============================================
//
//=============================================
void SV_SetAngles( edict_t* pedict, const Vector& angles, bool realignEntity )
{
	Vector prevAngles = pedict->state.angles;
	Math::VectorCopy(angles, pedict->state.angles);

	Math::VectorAdd(pedict->state.origin, pedict->state.mins, pedict->state.absmin);
	Math::VectorAdd(pedict->state.origin, pedict->state.maxs, pedict->state.absmax);

	if(prevAngles != pedict->state.angles)
	{
		Float prevAngleMatrix[3][4];
		Math::AngleMatrix(prevAngles, prevAngleMatrix);

		Float curAngleMatrix[3][4];
		Math::AngleMatrix(angles, curAngleMatrix);

		// Calculate angular movement of parent
		Vector angularMovement;
		Math::VectorSubtract(angles, prevAngles, angularMovement);

		// Inform game dll
		svs.dllfuncs.pfnOnEntitySetAngles(pedict, prevAngleMatrix, curAngleMatrix, angularMovement, realignEntity);

		// Ensure children get relatively rotated/moved
		for(Uint32 i = 0; i < pedict->state.children.size(); i++)
		{
			entindex_t childindex = pedict->state.children[i];
			edict_t* pchildedict = SV_GetEdictByIndex(childindex);
			if(!pchildedict || pchildedict->free)
				continue;

			SV_ReadjustChildByRotation(pchildedict, pedict, angularMovement, prevAngleMatrix, curAngleMatrix);
		}
	}

	// Update the leafnums
	pedict->leafnums.clear();
	SV_LinkEdict(pedict, false);
}

//=============================================
//
//=============================================
void SV_AddToTouched( entindex_t hitent, trace_t& trace, const Vector& velocity )
{
	if(hitent == NO_ENTITY_INDEX)
		return;

	for(Uint32 i = 0; i < svs.numpmovetraces; i++)
	{
		if(svs.pmovetraces[i].hitentity == hitent)
			return;
	}

	if(svs.numpmovetraces >= MAX_TOUCHENTS)
	{
		Con_Printf("%s - Exceeded MAX_TOUCHENTS on player %d.\n", __FUNCTION__, svs.pmoveplayerindex);
		return;
	}

	// Add to the stack
	Math::VectorCopy(velocity, trace.deltavelocity);
	svs.pmovetraces[svs.numpmovetraces] = trace;
	svs.numpmovetraces++;
}

//=============================================
//
//=============================================
Int32 SV_GetNumEdicts( void )
{
	return gEdicts.GetNbEdicts();
}

//=============================================
//
//=============================================
edict_t* SV_GetEdictByIndex( entindex_t entindex )
{
	if(entindex >= static_cast<Int32>(gEdicts.GetNbEdicts()))
	{
		Con_Printf("%s - Bogus entity index %d.\n", __FUNCTION__, entindex);
		return nullptr;
	}

	return gEdicts.GetEdict(entindex);
}

//=============================================
//
//=============================================
bool SV_InitPrivateData( edict_t* pedict, const Char* pstrClassname )
{
	// Init the gamedll interface
	pfnPrivateData_t pfn = static_cast<pfnPrivateData_t>(SDL_LoadFunction(svs.pdllhandle, pstrClassname));
	if(!pfn)
		return false;

	// Call the function
	pfn(pedict);

	return true;
}

//=============================================
//
//=============================================
edict_t* SV_CreateEntity( const Char* pstrClassName )
{
	if(ens.gamestate == GAME_INACTIVE)
	{
		Con_EPrintf("%s - Called on inactive game\n", __FUNCTION__);
		return nullptr;
	}

	if(!pstrClassName || !qstrlen(pstrClassName))
	{
		Con_EPrintf("%s - No classname specified.\n", __FUNCTION__);
		return nullptr;
	}

	// Allocate the edict
	edict_t* pedict = gEdicts.AllocEdict();
	if(!pedict)
	{
		Con_EPrintf("%s - Could not allocate edict for '%s'.\n", __FUNCTION__, pstrClassName);
		return nullptr;
	}

	if(!SV_InitPrivateData(pedict, pstrClassName))
	{
		Con_EPrintf("%s - Could not allocate private data for edict with classname '%s'.\n", __FUNCTION__, pstrClassName);
		gEdicts.FreeEdict(pedict, EDICT_REMOVED_AT_INIT);
		return nullptr;
	}

	// Set classname
	pedict->fields.classname = SV_AllocString(pstrClassName);

	return pedict;
}

//=============================================
//
//=============================================
void SV_RemoveEntity( edict_t* pentity )
{
	if(!pentity)
		return;

	gEdicts.FreeEdict(pentity, EDICT_REMOVED_KILLED);
}

//=============================================
//
//=============================================
bool SV_DropToFloor( edict_t* pentity )
{
	Vector traceEnd;
	traceEnd = pentity->state.origin - Vector(0, 0, 255);
	
	trace_t tr;
	Int32 traceFlags = FL_TRACE_NORMAL;
	if(pentity->state.flags & FL_NPC_CLIP)
		traceFlags |= FL_TRACE_NPC_CLIP;

	SV_Move(tr, pentity->state.origin, pentity->state.mins, pentity->state.maxs, traceEnd, traceFlags, pentity, static_cast<hull_types_t>(pentity->state.forcehull));
	if(tr.allSolid() || tr.startSolid() || tr.noHit())
		return false;
	
	SV_SetOrigin(pentity, tr.endpos);
	pentity->state.flags |= FL_ONGROUND;
	pentity->state.groundent = tr.hitentity;

	return true;
}

//=============================================
//
//=============================================
bool SV_SetEntityParent( edict_t* pentity, edict_t* pparent )
{
	// Check if it's already been added
	for(Uint32 i = 0; i < pparent->state.children.size(); i++)
	{
		if(pparent->state.children[i] == pentity->entindex)
			return false;
	}

	// Do not allow parenting to antyhing but MOVETYPE_PUSH
	if(pparent->state.movetype != MOVETYPE_PUSH)
	{
		const Char* pstrParentName = SV_GetString(pparent->fields.targetname);
		Con_EPrintf("%s - Parent entity '%s' movetype is not MOVETYPE_PUSH.\n", __FUNCTION__, pstrParentName);
		return false;
	}

	// Set parented flag
	entity_state_t& entitystate = pentity->state;
	entitystate.flags |= FL_PARENTED;
	entitystate.parent = pparent->entindex; // Set entindex of our parent

	// Ensure parent offset takes angles into account
	Float angleMatrix[3][4];
	Math::AngleMatrix(pparent->state.angles, angleMatrix);
		
	for(Uint32 i = 0; i < 3; i++)
		angleMatrix[i][3] = pparent->state.origin[i];

	// Calculate final position
	Vector parentoffset;
	Math::VectorTransform(entitystate.parentoffset, angleMatrix, parentoffset);

	// Calculate final angles
	Vector finalAngles;
	Math::VectorAdd(pentity->state.angles, pparent->state.angles, finalAngles);

	for(Uint32 i = 0; i < 3; i++)
		finalAngles[i] = Math::AngleMod(finalAngles[i]);

	SV_SetAngles(pentity, finalAngles);
	SV_SetOrigin(pentity, parentoffset);

	pparent->state.children.push_back(pentity->entindex);
	return true;
}

//=============================================
//
//=============================================
bool SV_NPC_WalkMove( edict_t* pentity, Float yaw, Float dist, walkmove_t movemode )
{
	if(!(pentity->state.flags & (FL_SWIM|FL_FLY|FL_ONGROUND)))
		return false;

	Vector move;
	move[0] = SDL_cos(yaw*2.0f * M_PI/360.0f) * dist;
	move[1] = SDL_sin(yaw*2.0f * M_PI/360.0f) * dist;

	bool result = false;
	switch(movemode)
	{
	case WALKMOVE_WORLDONLY:
		result = SV_NPC_MoveTest(pentity, move, true);
		break;
	case WALKMOVE_CHECKONLY:
		result = SV_NPC_MoveStep(pentity, move, false, false);
		break;
	case WALKMOVE_NO_NPCS:
		result = SV_NPC_MoveStep(pentity, move, false, true);
		break;
	case WALKMOVE_NORMAL:
	default:
		result = SV_NPC_MoveStep(pentity, move, true, false);
		break;
	}

	return result;
}