//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	System specific interface stuff.
//


#ifndef __I_SYSTEM__
#define __I_SYSTEM__


#include "doomtype.h"


typedef void (*atexit_func_t)(void);


// Called by startup code
// to get the ammount of memory to malloc
// for the zone management.
byte* I_ZoneBase(int* size);

bool I_ConsoleStdout(void);


// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.


// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void) NORETURN;

void I_Error (const char *error, ...) NORETURN PRINTF_ATTR(1, 2);

void *I_Realloc(void *ptr, size_t size);

bool I_GetMemoryValue(unsigned int offset, void *value, int size);

// Schedule a function to be called when the program exits.
// If run_if_error is true, the function is called if the exit
// is due to an error (I_Error)

void I_AtExit(atexit_func_t func, bool run_if_error);

// Print startup banner copyright message.

void I_PrintStartupBanner(const char *gamedescription);

// Print a centered text banner displaying the given string.

void I_PrintBanner(const char *text);

// Print a dividing line for startup banners.

void I_PrintDivider(void);

#endif

