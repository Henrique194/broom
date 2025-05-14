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


#ifndef __P_SPEC__
#define __P_SPEC__


//
// End-level timer (-TIMER option)
//
extern bool levelTimer;
extern int levelTimeCount;


// at game start
void P_InitPicAnims(void);
void P_CrossSpecialLine(int linenum, int side, mobj_t *thing);
fixed_t P_FindHighestCeilingSurrounding(const sector_t *sec);
fixed_t P_FindHighestFloorSurrounding(sector_t *sec);
fixed_t P_FindLowestCeilingSurrounding(const sector_t *sec);
fixed_t P_FindLowestFloorSurrounding(sector_t *sec);
int P_FindMinSurroundingLight(const sector_t *sector, int max);
fixed_t P_FindNextHighestFloor(const sector_t *sec, int currentheight);
int P_FindSectorFromLineTag(const line_t *line, int start);
void P_PlayerInSpecialSector(player_t *player);
void P_ShootSpecialLine(mobj_t *thing, line_t *line);
// at map load
void P_SpawnSpecials(void);
// every tic
void P_UpdateSpecials(void);
// when needed
bool P_UseSpecialLine(mobj_t *thing, line_t *line, int side);

sector_t *getNextSector(line_t *line, const sector_t *sec);
sector_t *getSector(int currentSector, int line, int side);
side_t *getSide(int currentSector, int line, int side);
int twoSided(int sector, int line);

//
// SPECIAL
//
int EV_DoDonut(line_t *line);

#endif
