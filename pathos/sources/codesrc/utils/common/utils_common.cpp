/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/


#include <SDL3/SDL.h>

#include "includes.h"
#include "utils_common.h"
#include "logfile.h"

// Log file ptr
CLogFile* g_pLogFile = nullptr;

// Size of buffer for message prints
static const Uint32 PRINT_MSG_BUFFER_SIZE = 16384;

//===============================================
// @brief Prints a generic message to the console and the log
//
// @param fmt Formatted string
// @param ... Parameters for string formatting
//===============================================
void Msg( const Char *fmt, ... )
{
	// compile the string result
	va_list	vArgPtr;
	static Char cMsg[PRINT_MSG_BUFFER_SIZE];
	
	va_start(vArgPtr,fmt);
	vsnprintf_safe(cMsg, PRINT_MSG_BUFFER_SIZE, fmt, vArgPtr);
	va_end(vArgPtr);

	printf(cMsg);

	if(g_pLogFile)
		g_pLogFile->Printf(cMsg);
}

//===============================================
// @brief Prints a generic message to the console and the log
//
// @param fmt Formatted string
// @param ... Parameters for string formatting
//===============================================
void WarningMsg( const Char *fmt, ... )
{
	// compile the string result
	va_list	vArgPtr;
	static Char cMsg[PRINT_MSG_BUFFER_SIZE];
	
	va_start(vArgPtr,fmt);
	vsnprintf_safe(cMsg, PRINT_MSG_BUFFER_SIZE, fmt, vArgPtr);
	va_end(vArgPtr);

	printf("Warning: %s", cMsg);

	if(g_pLogFile)
		g_pLogFile->Printf(cMsg);
}

//===============================================
// @brief Prints a generic message to the console and the log
//
// @param fmt Formatted string
// @param ... Parameters for string formatting
//===============================================
void ErrorMsg( const Char *fmt, ... )
{
	// compile the string result
	va_list	vArgPtr;
	static Char cMsg[PRINT_MSG_BUFFER_SIZE];
	
	va_start(vArgPtr,fmt);
	vsnprintf_safe(cMsg, PRINT_MSG_BUFFER_SIZE, fmt, vArgPtr);
	va_end(vArgPtr);

	printf("Error: %s", cMsg);

	if(g_pLogFile)
		g_pLogFile->Printf(cMsg);
}

//===============================================
// 
//
//===============================================
bool DirectoryExists( const Char* dirPath )
{
	SDL_PathInfo info;
	if (!SDL_GetPathInfo(dirPath, &info))
		return false;  // Something is wrong with your path or it doesn't exist!

	if (info.type == SDL_PATHTYPE_DIRECTORY)
		return true;   // This is a directory!

	return false;    // This is not a directory!
}