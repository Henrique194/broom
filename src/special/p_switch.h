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
// DESCRIPTION:  none
//	Implements special effects:
//	Texture animation, height or lighting changes according to adjacent
//	sectors, respective utility functions, etc.
//


#ifndef __P_SWITCH__
#define __P_SWITCH__

#include "r_defs.h"

// max # of wall switches in a level
#define MAXSWITCHES 50

// 4 players, 4 buttons each at once, max.
#define MAXBUTTONS 16

// 1 second, in ticks.
#define BUTTONTIME 35


typedef struct
{
    char name1[9];
    char name2[9];
    short episode;
} switchlist_t;

typedef enum
{
    top,
    middle,
    bottom
} bwhere_e;

typedef struct
{
    line_t *line;
    bwhere_e where;
    int btexture;
    int btimer;
    degenmobj_t *soundorg;
} button_t;

extern button_t buttonlist[MAXBUTTONS];

void P_InitSwitchList(void);
void P_ChangeSwitchTexture(line_t *line, int useAgain);

#endif
