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


#ifndef __P_LIGHTS__
#define __P_LIGHTS__

#include "d_think.h"
#include "r_defs.h"

#define GLOWSPEED    8
#define STROBEBRIGHT 5
#define FASTDARK     15
#define SLOWDARK     35


typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int maxlight;
    int minlight;
} fireflicker_t;

typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
} lightflash_t;

typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
} strobe_t;

typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int minlight;
    int maxlight;
    int direction;
} glow_t;

void EV_LightTurnOn(const line_t* line, int bright);
void EV_StartLightStrobing(const line_t* line);
void EV_TurnTagLightsOff(const line_t* line);
void P_SpawnFireFlicker(sector_t* sector);
void P_SpawnGlowingLight(sector_t* sector);
void P_SpawnLightFlash(sector_t* sector);
void P_SpawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync);
void T_Glow(glow_t* g);
void T_LightFlash(lightflash_t* flash);
void T_StrobeFlash(strobe_t* flash);

#endif
