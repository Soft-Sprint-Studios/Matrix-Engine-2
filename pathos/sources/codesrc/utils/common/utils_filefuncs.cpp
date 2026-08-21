/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/
#include <SDL3/SDL.h>

#include "includes.h"
#include "utils_filefuncs.h"
#include "file_interface.h"
file_interface_t g_fileInterface = 
{
	FL_LoadFile,				//pfnLoadFile
	FL_WriteFile,				//pfnWriteFile
	FL_WriteLogFile,			//pfnWriteLogFile
	FL_FreeFile,				//pfnFreeFile
	FL_FileExists,				//pfnFileExists
	FL_DeleteFile,				//pfnDeleteFile
	FL_CreateDirectory,			//pfnCreateDirectory
	FL_GetFileDate,				//pfnGetFileDate
	FL_CompareFileDates,		//pfnCompareFileDates
	nullptr,					//pfnGetGameDirectory
};

//=============================================
// @brief Loads in data from a file
//
// @param pstrpath Pointer to path string
// @param psize Pointer to variable that holds the size
// @return Pointer to file data
//=============================================
const byte* FL_LoadFile( const Char* pstrpath, Uint32* psize )
{
	SDL_IOStream* pf = SDL_IOFromFile(pstrpath, "rb");
	if(!pf)
	{
		SDL_ClearError();
		return nullptr;
	}

	SDL_SeekIO(pf, 0, SDL_IO_SEEK_END);
	Int32 size = (Int32)SDL_TellIO(pf);
	SDL_SeekIO(pf, 0, SDL_IO_SEEK_SET);

	byte* pbuffer = new byte[size+1];
	size_t numbytes = SDL_ReadIO(pf, pbuffer, size);
	SDL_CloseIO(pf);

	if(numbytes != size)
	{
		delete[] pbuffer;
		pbuffer =  nullptr;
		return nullptr;
	}

	// null terminate all files
	pbuffer[size] = '\0';

	if(psize)
		*psize = size;

	return pbuffer;
}

//=============================================
// @brief Frees memory for a file
//
// @param pfile Pointer to file data in memory
//=============================================
void FL_FreeFile( const void* pfile )
{
	delete[] pfile;
}

//=============================================
// @brief Writes data to a specified file
//
// @param pdata Pointer to data to write
// @param size The size of the data block to write
// @param pstrpath File path to write to
// @param append Tells if we should append to the file
// @return TRUE if successful, FALSE otherwise
//=============================================
bool FL_WriteFile( const byte* pdata, Uint32 size, const Char* pstrpath, bool append )
{
	SDL_IOStream* pf = SDL_IOFromFile(pstrpath, append ? "ab" : "wb");
	if(!pf)
	{
		SDL_ClearError();
		return false;
	}

	size_t numbytes = SDL_WriteIO(pf, pdata, size);
	SDL_CloseIO(pf);

	return (numbytes == size) ? true : false;
}

//=============================================
// @brief Writes data to a specified log file
//
// @param pdata Pointer to data block to be written
// @param size Size of data to be written
// @param pstrpath Path to write the file to
// @param append If true, the data will be appeneded to a file with this name if it already exists
// @return TRUE if write was successful, FALSE otherwise
//=============================================
bool FL_WriteLogFile( const byte* pdata, Uint32 size, const Char* pstrpath, bool append )
{
	SDL_IOStream* pf = SDL_IOFromFile(pstrpath, append ? "ab" : "wb");
	if(!pf)
	{
		SDL_ClearError();
		return false;
	}

	size_t numbytes = SDL_WriteIO(pf, pdata, size);
	SDL_CloseIO(pf);

	return (numbytes == size) ? true : false;
}

//=============================================
// @brief Compares the dates of two file date structures
//
// @param d1 First file date info structure
// @param d2 Second file date info structure
// @return 0 if dates match, 1 if d1 is earlier than d1, -1 if d1 is later than d2
//=============================================
Int32 FL_CompareFileDates( const file_dateinfo_t& d1, const file_dateinfo_t& d2 )
{
	if(d1.year == d2.year
		&& d1.month == d2.month
		&& d1.day == d2.day
		&& d1.hour == d2.hour
		&& d1.minute == d2.minute
		&& d1.second == d2.second)
		return 0;

	if(d1.year < d2.year)
		return 1;

	if(d1.year == d2.year 
		&& d1.month < d2.month)
		return 1;

	if(d1.year == d2.year
		&& d1.month == d2.month
		&& d1.day < d2.day)
		return 1;

	if(d1.year == d2.year
		&& d1.month == d2.month
		&& d1.day == d2.day
		&& d1.hour < d2.hour)
		return 1;

	if(d1.year == d2.year
		&& d1.month == d2.month
		&& d1.day == d2.day
		&& d1.hour == d2.hour
		&& d1.minute < d2.minute)
		return 1;

	if(d1.year == d2.year
		&& d1.month == d2.month
		&& d1.day == d2.day
		&& d1.hour == d2.hour
		&& d1.minute == d2.minute
		&& d1.second < d2.second)
		return 1;

	return -1;
}

//=============================================
// @brief Returns the date of a file in dateinfo
//
// @param pstrFile Path to file
// @param dateinfo Date info structure to hold our result
// @return TRUE if successful, FALSE otherwise
//=============================================
bool FL_GetFileDate( const Char* pstrFile, file_dateinfo_t& dateinfo )
{
	SDL_PathInfo info;
	if(!SDL_GetPathInfo(pstrFile, &info))
		return false;

	time_t modtime = (time_t)(info.modify_time / SDL_NS_PER_SECOND);
	struct tm* t = localtime(&modtime);
	if(!t)
		return false;

	dateinfo.year = t->tm_year + 1900;
	dateinfo.month = t->tm_mon + 1;
	dateinfo.day = t->tm_mday;
	dateinfo.hour = t->tm_hour;
	dateinfo.minute = t->tm_min;
	dateinfo.second = t->tm_sec;

	return true;
}

//=============================================
// @brief Creates a directory in the path specified
//
// @param pstrpath Path to directory to create
// @return TRUE if successful FALSE otherwise
//=============================================
bool FL_CreateDirectory( const Char* pstrpath )
{
	return SDL_CreateDirectory(pstrpath);
}

//=============================================
// @brief Deletes a file at a given location
//
// @param pstrpath Path to file
// @return TRUE if successful FALSE otherwise
//=============================================
bool FL_DeleteFile( const Char* pstrpath )
{
	return true;
}

//=============================================
// @brief Checks if a file exists at the given path
//
// pstrpath Path to the file to check
// @return TRUE if file exists, FALSE otherwise
//=============================================
bool FL_FileExists( const Char* pstrpath )
{
	SDL_IOStream* pf = SDL_IOFromFile(pstrpath, "rb");
	if(!pf)
		return false;

	SDL_CloseIO(pf);
	return true;
}
