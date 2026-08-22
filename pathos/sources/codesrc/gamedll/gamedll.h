/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef GAMEDLL_H
#define GAMEDLL_H

#if defined(_WIN32)
    #define DLLEXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
    #define DLLEXPORT __attribute__((visibility("default")))
#else
    #define DLLEXPORT
#endif

struct gdll_engfuncs_t;
struct trace_interface_t;
struct file_interface_t;
struct gamevars_t;
struct usercmd_t;
struct pm_info_t;
enum savefile_type_t : int;

// Declaration of gamedll enginefuncs struct
extern gdll_engfuncs_t gd_engfuncs;
// Declaration of traceline interface
extern trace_interface_t gd_tracefuncs;
// File functions
extern file_interface_t gd_filefuncs;
// Declaration of gamevars pointer
extern gamevars_t* g_pGameVars;

// VIS buffer for server
extern byte* g_pVISBuffer;
// VIS buffer size
extern Uint32 g_visBufferSize;

// TRUE if game initialization has occurred
extern bool g_gameInitializationDone;

extern bool GameDLLInit( void );
extern void GameDLLShutdown( void );
extern bool GameInit( void );
extern void GameShutdown( void );
extern void InitializeClientData( edict_t* pclient );
extern void GetHullSizes( Int32 hullIndex, Vector& pmins, Vector& pmaxs );
extern void ServerFrame( void );
extern void RunPlayerMovement( const usercmd_t& cmd, pm_info_t* pminfo );
extern void GetSaveGameTitle( Char* pstrBuffer, Int32 maxlength );
extern bool InconsistentFile( const Char* pstrFilename );
extern bool AreCheatsEnabled( void );
extern void DumpCheatCodes( void );
extern void StopMusic( void );
extern void AdjustLandmarkPVSData( edict_t* pLandmarkEdict, byte* pPVS, Uint32 pvsBufferSize );
extern void SetConnectionSaveFile( const Char* pstrLevelName, const Char* pstrLandmarkName, const Char* pstrSaveFileName );
extern void PrecacheResources( void );
extern bool CanSaveGame( enum savefile_type_t type );
extern bool CanLoadGame( void );
extern void ShowSaveGameMessage( void );
extern void ShowAutoSaveGameMessage( void );
extern void ShowSaveGameBlockedMessage( void );
extern void RestoreDecal( const Vector& origin, const Vector& normal, edict_t* pedict, const Char* pstrDecalTexture, Int32 decalflags );
extern void FormatKeyValue( const Char* pstrKeyValueName, Char* pstrValue, Uint32 maxLength );
extern void PostSpawnGame( void );
#endif //GAMEDLL_H
