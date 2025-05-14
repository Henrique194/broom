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

#ifndef __AMMAP_RESPONDER_H__
#define __AMMAP_RESPONDER_H__

#include "d_event.h"


//
// Handle events (user inputs) in automap mode
//
bool AM_Responder(event_t* ev);

void AM_Start(void);

void AM_addMark(void);

void AM_minOutWindowScale(void);

void AM_clearMarks(void);


#endif
