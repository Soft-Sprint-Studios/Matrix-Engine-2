/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include "includes.h"
#include "filewriterthread.h"
#include "file.h"
#include "system.h"

// Data object for file writer thread
writerthread_t g_fileThreadData;

// File writer thread function
static int SDLCALL FileWriterThread( void* lpParam );

//=============================================
// @brief
//
//=============================================
void FWT_Init( void )
{
	g_fileThreadData.mutex = SDL_CreateMutex();
	if(!g_fileThreadData.mutex)
	{
		Con_Printf("Failed to initialize file writer mutex: %s\n", SDL_GetError());
		return;
	}

	g_fileThreadData.condition = SDL_CreateCondition();
	if(!g_fileThreadData.condition)
	{
		Con_Printf("Failed to initialize file writer condition variable: %s\n", SDL_GetError());
		SDL_DestroyMutex(g_fileThreadData.mutex);
		g_fileThreadData.mutex = nullptr;
		return;
	}

	g_fileThreadData.threadhandle = SDL_CreateThread(FileWriterThread, "FileWriterThread", &g_fileThreadData);
	if(!g_fileThreadData.threadhandle)
	{
		Con_Printf("Failed to initialize file writer thread: %s\n", SDL_GetError());
		SDL_DestroyCondition(g_fileThreadData.condition);
		SDL_DestroyMutex(g_fileThreadData.mutex);
		g_fileThreadData.condition = nullptr;
		g_fileThreadData.mutex = nullptr;
		return;
	}

	// Mark file writer as available
	g_fileThreadData.available = true;
}

//=============================================
// @brief
//
//=============================================
void FWT_Shutdown( void )
{
	if(!g_fileThreadData.available)
		return;

	SDL_LockMutex(g_fileThreadData.mutex);
	g_fileThreadData.exit = true;
	SDL_UnlockMutex(g_fileThreadData.mutex);

	SDL_SignalCondition(g_fileThreadData.condition);

	SDL_WaitThread(g_fileThreadData.threadhandle, nullptr);
	g_fileThreadData.threadhandle = nullptr;

	if(g_fileThreadData.condition)
	{
		SDL_DestroyCondition(g_fileThreadData.condition);
		g_fileThreadData.condition = nullptr;
	}

	if(g_fileThreadData.mutex)
	{
		SDL_DestroyMutex(g_fileThreadData.mutex);
		g_fileThreadData.mutex = nullptr;
	}

	g_fileThreadData.available = false;
}

//=============================================
// @brief
//
//=============================================
bool FWT_AddFile( const Char* pstrFilename, const byte* pData, Uint32 dataSize, bool incremental, bool prompt, bool append )
{
	if(!g_fileThreadData.available)
		return false;

	if(incremental && CString(pstrFilename).find(0, "%number%") == CString::CSTRING_NO_POSITION)
	{
		Con_Printf("%s - Incremental file's path '%s' missing '%number%' token.\n", __FUNCTION__, pstrFilename);
		return false;
	}

	threadfile_t* pnew = new threadfile_t();

	// Copy data
	pnew->pdata = new byte[dataSize];
	memcpy(pnew->pdata, pData, sizeof(byte)*dataSize);

	pnew->datasize = dataSize;
	pnew->incremental = incremental;
	pnew->prompt = prompt;
	pnew->append = append;
	pnew->filename = pstrFilename;

	// Add to list
	SDL_LockMutex(g_fileThreadData.mutex);
	g_fileThreadData.fileslist.radd(pnew);
	SDL_UnlockMutex(g_fileThreadData.mutex);

	// Wake writer thread
	SDL_SignalCondition(g_fileThreadData.condition);

	return true;
}

//=============================================
// @brief
//
//=============================================
static int SDLCALL FileWriterThread( void* lpParam )
{
	writerthread_t* pThreadData = static_cast<writerthread_t*>(lpParam);

	while(true)
	{
		// Enter critical section (lock mutex)
		SDL_LockMutex(pThreadData->mutex);

		// Wait until there is work or we are exiting
		while (pThreadData->fileslist.empty() && !pThreadData->exit)
		{
			SDL_WaitCondition(pThreadData->condition, pThreadData->mutex);
		}

		if(pThreadData->exit && pThreadData->fileslist.empty())
		{
			SDL_UnlockMutex(pThreadData->mutex);
			break;
		}

		if(!pThreadData->fileslist.empty())
		{
			pThreadData->fileslist.begin();
			while(!pThreadData->fileslist.end())
			{
				// Get file pointer and remove from list
				threadfile_t* pfile = pThreadData->fileslist.get();
				pThreadData->fileslist.remove(pThreadData->fileslist.get_link());

				// Temporarily unlock mutex while doing I/O operations to prevent blocking main thread additions for too long
				SDL_UnlockMutex(pThreadData->mutex);

				CString filename;
				if(!pfile->incremental)
				{
					// Just use supplied filename
					filename = pfile->filename;
				}
				else
				{
					Uint32 i = 0;
					while(true)
					{
						// Build new name
						filename = pfile->filename;
						
						// Get position of token
						Int32 pos = filename.find(0, "%number%");
						if(pos == CString::CSTRING_NO_POSITION)
						{
							filename.clear();
							FWT_Con_Printf(pThreadData, "%s - Filename '%s' missing '%number%' token, file not writen.\n");
							break;
						}
						else
						{
							CString numstr;
							numstr << i;

							filename.erase(pos, 8);
							filename.insert(pos, numstr.c_str());

							if(!FL_FileExists(filename.c_str()))
								break;

							i++;
						}
					}
				}

				if(!filename.empty())
				{
					// Write file to disk
					if(!FL_WriteFile(pfile->pdata, pfile->datasize, filename.c_str(), pfile->append))
						FWT_Con_Printf(pThreadData, "%s - Failed to write file '%s'.\n", __FUNCTION__, filename.c_str());
					else if(pfile->prompt)
						FWT_Con_Printf(pThreadData, "Wrote '%s'.\n", filename.c_str());
				}

				// Delete file object
				delete pfile;

				// Re-acquire lock for list iteration
				SDL_LockMutex(pThreadData->mutex);

				// Move onto next file
				pThreadData->fileslist.next();
			}
		}

		SDL_UnlockMutex(pThreadData->mutex);
	}

	return 0;
}

//=============================================
// @brief Prints a formatted string to the console buffer for thread
//
// @param pThreadData Thread data pointer
// @param fmt String describing the format
// @param ... Additional format input parameters
//=============================================
void FWT_Con_Printf( writerthread_t* pThreadData, const Char *fmt, ... )
{
	va_list	vArgPtr;
	static Char cMsg[PRINT_MSG_BUFFER_SIZE];
	
	va_start(vArgPtr,fmt);
	vsprintf_s(cMsg, fmt, vArgPtr);
	va_end(vArgPtr);

	// Enter critical section
	SDL_LockMutex(pThreadData->mutex);
	pThreadData->consoleprints.radd(cMsg);
	SDL_UnlockMutex(pThreadData->mutex);
}

//=============================================
// @brief Returns the elements to print to console in an array
//
// @param fmt String describing the format
// @param ... Additional format input parameters
//=============================================
void FWT_GetConsolePrints( CArray<CString>& destArray )
{
	if(!g_fileThreadData.available)
		return;

	// Enter critical section
	SDL_LockMutex(g_fileThreadData.mutex);

	if(g_fileThreadData.consoleprints.empty())
	{
		SDL_UnlockMutex(g_fileThreadData.mutex);
		return;
	}

	destArray.reserve(g_fileThreadData.consoleprints.size());

	// Add elements from linked list to output array
	g_fileThreadData.consoleprints.begin();
	while(!g_fileThreadData.consoleprints.end())
	{
		destArray.push_back(g_fileThreadData.consoleprints.get());
		g_fileThreadData.consoleprints.next();
	}
	g_fileThreadData.consoleprints.clear();

	// Leave critical section
	SDL_UnlockMutex(g_fileThreadData.mutex);
}