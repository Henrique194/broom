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


#ifndef __P_FLOOR__
#define __P_FLOOR__

#include "m_fixed.h"
#include "r_defs.h"

#define FLOORSPEED FRACUNIT


typedef enum
{
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,
    donutRaise,
    raiseFloor512
} floor_e;

typedef enum
{
    // slowly build by 8
    build8,
    // quickly build by 16
    turbo16
} stair_e;

typedef struct
{
    thinker_t thinker;
    floor_e type;
    bool crush;
    sector_t *sector;
    int direction;
    int newspecial;
    short texture;
    fixed_t floordestheight;
    fixed_t speed;
} floormove_t;

typedef enum
{
    ok,
    // The door has hit an object.
    crushed,
    // The door has reached its destination.
    pastdest
} result_e;

int EV_BuildStairs(const line_t *line, stair_e type);
int EV_DoFloor(line_t *line, floor_e floortype);
void T_MoveFloor(floormove_t *floor);
result_e T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest, bool crush,
                     int floorOrCeiling, int direction);

#endif
