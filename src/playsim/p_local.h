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
//	Play functions, animation, global header.
//


#ifndef __P_LOCAL__
#define __P_LOCAL__

#include "r_local.h"

#define FLOATSPEED (FRACUNIT * 4)


#define MAXHEALTH  100
#define VIEWHEIGHT (41 * FRACUNIT)

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS 128
#define MAPBLOCKSIZE  (MAPBLOCKUNITS * FRACUNIT)
#define MAPBLOCKSHIFT (FRACBITS + 7)
#define MAPBTOFRAC    (MAPBLOCKSHIFT - FRACBITS)


// player radius for movement checking
#define PLAYERRADIUS 16 * FRACUNIT

// MAXRADIUS is for precalculated sector block boxes the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS 32 * FRACUNIT

#define GRAVITY FRACUNIT
#define MAXMOVE (30 * FRACUNIT)

#define USERANGE     (64 * FRACUNIT)
#define MELEERANGE   (64 * FRACUNIT)
#define MISSILERANGE (32 * 64 * FRACUNIT)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD 100


//
// P_TICK
//

// both the head and tail of the thinker list
extern thinker_t thinkercap;


void P_InitThinkers();
void P_AddThinker(thinker_t *thinker);
void P_RemoveThinker(thinker_t *thinker);
bool P_IsThinkerRemoved(thinker_t *thinker);


//
// P_PSPR
//
void P_SetupPsprites(player_t *curplayer);
void P_MovePsprites(player_t *curplayer);
void P_DropWeapon(player_t *player);


//
// P_USER
//
void P_PlayerThink(player_t *player);


//
// P_MOBJ
//
#define ONFLOORZ   INT_MIN
#define ONCEILINGZ INT_MAX

// Time interval for item respawning.
#define ITEMQUESIZE 128

extern mapthing_t itemrespawnque[ITEMQUESIZE];
extern int itemrespawntime[ITEMQUESIZE];
extern int iquehead;
extern int iquetail;


void P_RespawnSpecials(void);

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void P_RemoveMobj(mobj_t *th);
mobj_t *P_SubstNullMobj(mobj_t *th);
bool P_SetMobjState(mobj_t *mobj, statenum_t state);
void P_MobjThinker(mobj_t *mobj);

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t *P_SpawnMissile(mobj_t* source, const mobj_t* dest, mobjtype_t type);
void P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type);


//
// P_ENEMY
//
void P_NoiseAlert(mobj_t *target, mobj_t *emmiter);


//
// P_MAPUTL
//
typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;
} divline_t;

typedef struct
{
    fixed_t frac; // along trace line
    bool isaline;
    union
    {
        mobj_t *thing;
        line_t *line;
    } d;
} intercept_t;

// Extended MAXINTERCEPTS, to allow for intercepts overrun emulation.

#define MAXINTERCEPTS_ORIGINAL 128
#define MAXINTERCEPTS          (MAXINTERCEPTS_ORIGINAL + 61)

typedef bool (*traverser_t)(intercept_t *in);

fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy);
int P_PointOnLineSide(fixed_t x, fixed_t y, const line_t* line);
fixed_t P_InterceptVector(const divline_t* v2, const divline_t* v1);
int P_BoxOnLineSide(const fixed_t* tmbox, const line_t* ld);

extern fixed_t opentop;
extern fixed_t openbottom;
extern fixed_t openrange;
extern fixed_t lowfloor;

void P_LineOpening(const line_t* line);

bool P_BlockLinesIterator(int x, int y, bool (*func)(line_t *));
bool P_BlockThingsIterator(int x, int y, bool (*func)(mobj_t *));

#define PT_ADDLINES  1
#define PT_ADDTHINGS 2
#define PT_EARLYOUT  4

extern divline_t trace;

bool P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, bool (*trav)(intercept_t *));

void P_UnsetThingPosition(mobj_t *thing);
void P_SetThingPosition(mobj_t *thing);


//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern bool floatok;
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;


extern line_t *ceilingline;

// fraggle: I have increased the size of this buffer.  In the original Doom,
// overrunning past this limit caused other bits of memory to be overwritten,
// affecting demo playback.  However, in doing so, the limit was still
// exceeded.  So we have to support more than 8 specials.
//
// We keep the original limit, to detect what variables in memory were
// overwritten (see SpechitOverrun())

#define MAXSPECIALCROSS          20
#define MAXSPECIALCROSS_ORIGINAL 8

extern line_t *spechit[MAXSPECIALCROSS];
extern int numspechit;

bool P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
bool P_TryMove(mobj_t *thing, fixed_t x, fixed_t y);
bool P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y);
void P_SlideMove(mobj_t *mo);
bool P_CheckSight(const mobj_t* t1, const mobj_t* t2);
void P_UseLines(player_t *player);

bool P_ChangeSector(const sector_t *sector, bool crunch);

extern mobj_t *linetarget; // who got hit (or NULL)


extern fixed_t attackrange;

// slopes to top and bottom of target
extern fixed_t topslope;
extern fixed_t bottomslope;


fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance);

void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage);

void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);


//
// P_SETUP
//
extern byte* rejectmatrix;  // for fast sight rejection
extern short* blockmaplump; // offsets in blockmap are from here
extern short* blockmap;
extern int bmapwidth;
extern int bmapheight; // in mapblocks


extern fixed_t bmaporgx;
extern fixed_t bmaporgy;    // origin of block map
extern mobj_t** blocklinks; // for thing chains


//
// P_INTER
//
extern int maxammo[NUMAMMO];
extern int clipammo[NUMAMMO];

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);
void P_DamageMobj(mobj_t *target, const mobj_t *inflictor, mobj_t *source,
                  int damage);


//
// P_SPEC
//
#include "p_spec.h"


#endif // __P_LOCAL__
