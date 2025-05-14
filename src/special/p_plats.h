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


#ifndef __P_PLATS__
#define __P_PLATS__

#include "d_think.h"
#include "m_fixed.h"
#include "r_defs.h"

#define PLATWAIT  3
#define PLATSPEED FRACUNIT
#define MAXPLATS  30


typedef enum
{
    up,
    down,
    waiting,
    in_stasis
} plat_e;

typedef enum
{
    // A platform which forever moves a floor between two different heights
    // unless suspended via a "stop platform" action. This type of platform
    // moves between the lowest and highest surrounding floors, reversing its
    // motion whenever the current destination height is reached after a short
    // pause. Whether such a floor initially moves up or down is random.
    // The speed of this type of platform is one unit per tic.
    perpetualRaise,

    // A platform that moves from its starting height down to the lowest
    // surrounding floor height (if no such sector exists, no movement takes
    // place). All such platforms wait for a time period of 3 seconds before
    // returning to their original starting height. The speed of this type of
    // platform is 4 units per tic.
    downWaitUpStay,

    // Platform that implements simple floor-raising behavior which changes
    // the properties of the sector it affects. The speed of this type of
    // platform is 0.5 unit per tic.
    raiseAndChange,

    // A "raise to nearest and change" platform action raises its floor to the
    // next highest neighboring floor, adapts the floor texture on the first
    // side of the activating switch linedef, and sets the sector special to
    // zero. A "raise and change" platform action raises its floor by a set
    // amount depending on the action type, adapts the floor texture on the
    // first side of the switch, and does not change the sector's special.
    // The speed of this type of platform is 0.5 unit per tic.
    raiseToNearestAndChange,

    // This platform is exactly the same as "downWaitUpStay" platforms, with
    // the only difference the speed is 8 units/tic instead of 4 units/tic.
    blazeDWUS
} plattype_e;

typedef struct
{
    thinker_t thinker;
    sector_t *sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int wait;
    int count;
    plat_e status;
    plat_e oldstatus;
    bool crush;
    int tag;
    plattype_e type;
} plat_t;

extern plat_t* activeplats[MAXPLATS];

bool EV_DoPlat(const line_t *line, plattype_e type, int amount);
void EV_StopPlat(const line_t *line);
void P_AddActivePlat(plat_t *plat);
void T_PlatRaise(plat_t *plat);

#endif
