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


#ifndef __P_DOORS__
#define __P_DOORS__

#include "r_defs.h"
#include "p_mobj.h"

#define VDOORSPEED FRACUNIT * 2
#define VDOORWAIT  150


typedef enum
{
    vld_normal,
    vld_close30ThenOpen,
    vld_close,
    vld_open,
    vld_raiseIn5Mins,
    vld_blazeRaise,
    vld_blazeOpen,
    vld_blazeClose
} vldoor_e;

typedef struct
{
    thinker_t thinker;
    vldoor_e type;
    sector_t *sector;
    fixed_t topheight;
    fixed_t speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int direction;

    // tics to wait at the top
    int topwait;

    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int topcountdown;
} vldoor_t;


bool EV_DoDoor(const line_t *line, vldoor_e type);
int EV_DoLockedDoor(const line_t *line, vldoor_e type, mobj_t *thing);
void EV_VerticalDoor(line_t *line, mobj_t *thing);
void P_SpawnDoorCloseIn30(sector_t *sec);
void P_SpawnDoorRaiseIn5Mins(sector_t *sec);
void T_VerticalDoor(vldoor_t *door);

#endif
