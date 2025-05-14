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


#ifndef __P_CEILING__
#define __P_CEILING__

#include "d_think.h"
#include "r_defs.h"


#define CEILSPEED   FRACUNIT
#define MAXCEILINGS 30

typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise
} ceiling_e;

typedef struct
{
    thinker_t thinker;
    ceiling_e type;
    sector_t *sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    bool crush;

    // 1 = up, 0 = waiting, -1 = down
    int direction;

    // ID
    int tag;
    int olddirection;
} ceiling_t;

extern ceiling_t *activeceilings[MAXCEILINGS];

int EV_CeilingCrushStop(line_t *line);
int EV_DoCeiling(line_t *line, ceiling_e type);
void P_ActivateInStasisCeiling(line_t *line);
void P_AddActiveCeiling(ceiling_t *c);
void P_RemoveActiveCeiling(ceiling_t *c);
void T_MoveCeiling(ceiling_t *ceiling);

#endif
