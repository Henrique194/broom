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
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//


#include <stdlib.h>

#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "d_loop.h"
#include "p_local.h"
#include "p_floor.h"
#include "p_doors.h"

#include "s_sound.h"

#include "g_game.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"

#include "a_enemy.h"


typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;


//
// P_NewChaseDir related LUT.
//
static dirtype_t opposite[] = {
    DI_WEST,      DI_SOUTHWEST, DI_SOUTH,
    DI_SOUTHEAST, DI_EAST,      DI_NORTHEAST,
    DI_NORTH,     DI_NORTHWEST, DI_NODIR
};

static dirtype_t diags[] = {
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST,
    DI_SOUTHEAST
};


//
// ENEMY THINKING
// Enemies are allways spawned with targetplayer = -1 and threshold = 0.
// Most monsters are spawned unaware of all players,
// but some can be made pre-aware.
//


//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
static mobj_t *soundtarget;

static bool P_CanPropagateSound(const line_t* line, int sound_blocks) {
    if (!(line->flags & ML_TWOSIDED)) {
        // Solid walls block all sound.
        return false;
    }
    P_LineOpening(line);
    if (openrange <= 0) {
        // Closed doors block all sound.
        return false;
    }
    if (line->flags & ML_SOUNDBLOCK) {
        // Line is set to block sounds. Only allow propagation
        // if this is the first line to block the sound.
        return sound_blocks == 0;
    }
    return true;
}

static bool P_CanFloodSector(const sector_t* sec, int soundblocks) {
    if (sec->validcount != validcount) {
        // Not yet flooded in the current frame.
        return true;
    }
    // If we reached the sector through a different path
    // with no sound block, then flood it again.
    return soundblocks + 1 < sec->soundtraversed;
}

static void P_RecursiveSound(sector_t* sec, int soundblocks) {
    if (!P_CanFloodSector(sec, soundblocks)) {
        return;
    }

    sec->validcount = validcount;
    sec->soundtraversed = soundblocks + 1;
    // Wake up all monsters in this sector.
    sec->soundtarget = soundtarget;

    for (int i = 0; i < sec->linecount; i++) {
        const line_t* line = sec->lines[i];
        if (!P_CanPropagateSound(line, soundblocks)) {
            continue;
        }
        sector_t* other =
            (sec == line->frontsector) ? line->backsector : line->frontsector;
        if (line->flags & ML_SOUNDBLOCK) {
            // This is a sound-blocking line, but we do not block sound here.
            // Instead, we recursively call the function for the next
            // sound-blocking line to block the sound, and so simulate
            // sound attenuation. This makes the monsters' reactions to the
            // player more organic and realistic (checkout Doom 2 MAP01).
            P_RecursiveSound(other, 1);
        } else {
            P_RecursiveSound(other, soundblocks);
        }
    }
}


//
// P_NoiseAlert
// If a monster yells at a player, it will alert other monsters to the player.
//
void P_NoiseAlert(mobj_t* target, mobj_t* emitter) {
    soundtarget = target;
    validcount++;
    P_RecursiveSound(emitter->subsector->sector, 0);
}


//
// P_CheckMeleeRange
//
static bool P_CheckMeleeRange(const mobj_t* actor) {
    fixed_t range;

    if (!actor->target) {
        return false;
    }

    const mobj_t* pl = actor->target;
    fixed_t dist = P_ApproxDistance(pl->x - actor->x, pl->y - actor->y);

    if (gameversion <= exe_doom_1_2) {
        range = MELEERANGE;
    } else {
        range = MELEERANGE - (20 * FRACUNIT) + pl->info->radius;
    }

    if (dist >= range) {
        return false;
    }

    return P_CheckSight(actor, actor->target);
}

//
// P_CheckMissileRange
//
static bool P_CheckMissileRange(mobj_t* actor) {
    if (!P_CheckSight(actor, actor->target)) {
        return false;
    }
    if (actor->flags & MF_JUSTHIT) {
        // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }
    if (actor->reactiontime) {
        // Do not attack yet.
        return false;
    }

    // OPTIMIZE: get this from a global checksight
    fixed_t dx = actor->x - actor->target->x;
    fixed_t dy = actor->y - actor->target->y;
    fixed_t dist = P_ApproxDistance(dx, dy);

    if (actor->info->meleestate) {
        dist -= (64 * FRACUNIT);
    } else {
        // No melee attack, so fire more.
        dist -= (192 * FRACUNIT);
    }
    dist >>= FRACBITS;

    switch (actor->type) {
        case MT_VILE:
            if (dist > 896) {
                // Too far away.
                return false;
            }
            break;
        case MT_UNDEAD:
            if (dist < 196) {
                // Close for fist attack.
                return false;
            }
            dist >>= 1;
            break;
        case MT_CYBORG:
            dist >>= 1;
            if (dist > 160) {
                dist = 160;
            }
            break;
        case MT_SPIDER:
        case MT_SKULL:
            dist >>= 1;
            break;
        default:
            break;
    }

    if (dist > 200) {
        dist = 200;
    }

    return P_Random() >= dist;
}


static fixed_t xspeed[8] = {FRACUNIT,  47000,  0, -47000,
                            -FRACUNIT, -47000, 0, 47000};
static fixed_t yspeed[8] = {0, 47000,  FRACUNIT,  47000,
                            0, -47000, -FRACUNIT, -47000};

static bool P_TryOpenSpecials(mobj_t* actor) {
    if (actor->flags & MF_FLOAT && floatok) {
        // must adjust height
        if (actor->z < tmfloorz) {
            actor->z += FLOATSPEED;
        } else {
            actor->z -= FLOATSPEED;
        }
        actor->flags |= MF_INFLOAT;
        return true;
    }

    if (!numspechit) {
        return false;
    }

    actor->movedir = DI_NODIR;
    bool good = false;
    while (numspechit--) {
        line_t* ld = spechit[numspechit];
        // if the special is not a door that can be opened, return false
        if (P_UseSpecialLine(actor, ld, 0)) {
            good = true;
        }
    }
    return good;
}

//
// P_Move
// Move in the current direction, returns false if the move is blocked.
//
static bool P_Move(mobj_t* actor) {
    if (actor->movedir == DI_NODIR) {
        return false;
    }
    if ((unsigned) actor->movedir >= NUMDIRS) {
        I_Error("Weird actor->movedir!");
    }
    int speed = actor->info->speed;
    fixed_t new_x = actor->x + speed * xspeed[actor->movedir];
    fixed_t new_y = actor->y + speed * yspeed[actor->movedir];
    if (!P_TryMove(actor, new_x, new_y)) {
        return P_TryOpenSpecials(actor);
    }
    actor->flags &= ~MF_INFLOAT;
    if (!(actor->flags & MF_FLOAT)) {
        actor->z = actor->floorz;
    }
    return true;
}


//
// TryWalk
// Attempts to move actor on in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor returns FALSE.
// If move is either clear or blocked only by a door, returns TRUE and sets...
// If a door is in the way, an OpenDoor call is made to start it opening.
//
static bool P_TryWalk(mobj_t* actor) {
    if (!P_Move(actor)) {
        return false;
    }
    actor->movecount = P_Random() & 15;
    return true;
}

static bool P_TryNewDir(mobj_t* actor, int new_dir) {
    actor->movedir = new_dir;
    return P_TryWalk(actor);
}

//
// Randomly determine direction of search.
//
static bool P_TryWalkRandomDir(mobj_t* actor, dirtype_t turnaround) {
    int start;
    int stop;
    int inc;
    if (P_Random() & 1) {
        // Normal order.
        start = DI_EAST;
        stop = DI_SOUTHEAST + 1;
        inc = 1;
    } else {
        // Reverse order.
        start = DI_SOUTHEAST;
        stop = DI_EAST - 1;
        inc = -1;
    }
    for (int dir = start; dir != stop; dir += inc) {
        if (dir == (int) turnaround) {
            continue;
        }
        if (P_TryNewDir(actor, dir)) {
            return true;
        }
    }
    return false;
}

static dirtype_t P_FindDiagonalDir(fixed_t dx, fixed_t dy) {
    return diags[((dy < 0) << 1) + (dx > 0)];
}

static dirtype_t P_FindVerticalDir(fixed_t dy) {
    if (dy < -10 * FRACUNIT) {
        return DI_SOUTH;
    }
    if (dy > 10 * FRACUNIT) {
        return DI_NORTH;
    }
    return DI_NODIR;
}

static dirtype_t P_FindHorizontalDir(fixed_t dx) {
    if (dx > 10 * FRACUNIT) {
        return DI_EAST;
    }
    if (dx < -10 * FRACUNIT) {
        return DI_WEST;
    }
    return DI_NODIR;
}

//
// Try to get closer to target.
//
static bool P_TryWalkDirectDir(mobj_t* actor, dirtype_t turnaround) {
    dirtype_t olddir = actor->movedir;
    fixed_t dx = actor->target->x - actor->x;
    fixed_t dy = actor->target->y - actor->y;
    dirtype_t horizontal_dir = P_FindHorizontalDir(dx);
    dirtype_t vertical_dir = P_FindVerticalDir(dy);

    // Try direct route.
    if (horizontal_dir != DI_NODIR && vertical_dir != DI_NODIR) {
        actor->movedir = P_FindDiagonalDir(dx, dy);
        if (actor->movedir != (int) turnaround && P_TryWalk(actor)) {
            return true;
        }
    }

    // Try other directions.
    if (P_Random() > 200 || abs(dy) > abs(dx)) {
        int tdir = horizontal_dir;
        horizontal_dir = vertical_dir;
        vertical_dir = tdir;
    }
    // Heavily avoid turning 180°.
    if (horizontal_dir == turnaround) {
        horizontal_dir = DI_NODIR;
    }
    if (vertical_dir == turnaround) {
        vertical_dir = DI_NODIR;
    }

    // Try new directions.
    if (P_TryNewDir(actor, horizontal_dir)) {
        // Either moved forward or attacked.
        return true;
    }
    if (P_TryNewDir(actor, vertical_dir)) {
        return true;
    }
    // There is no direct path to the player, so pick another direction.
    return P_TryNewDir(actor, olddir);
}

//
// Function that picks another direction when enemy is chasing target.
//
static void P_NewChaseDir(mobj_t* actor) {
    if (!actor->target) {
        I_Error("P_NewChaseDir: called with no target");
    }
    dirtype_t turnaround = opposite[actor->movedir];
    if (P_TryWalkDirectDir(actor, turnaround)) {
        return;
    }
    if (P_TryWalkRandomDir(actor, turnaround)) {
        return;
    }
    if (P_TryNewDir(actor, turnaround)) {
        return;
    }
    // Cannot move.
    actor->movedir = DI_NODIR;
}

//
// If the allaround is true, actor will have a 360 degrees field of view.
//
static bool P_IsPlayerInFOV(const mobj_t* actor, const player_t* player,
                               bool allaround)
{
    if (allaround) {
        return true;
    }

    fixed_t actor_x = actor->x;
    fixed_t actor_y = actor->y;
    fixed_t plr_x = player->mo->x;
    fixed_t plr_y = player->mo->y;

    angle_t ang = R_PointToAngle2(actor_x, actor_y, plr_x, plr_y);
    angle_t ang_dist = ang - actor->angle;
    if (ang_dist <= ANG90 || ang_dist >= ANG270) {
        // player in actor's field of view
        return true;
    }
    // player is not in field of view,
    // but if he is real close, then react anyway
    fixed_t dist = P_ApproxDistance(plr_x - actor_x, plr_y - actor_y);
    return dist <= MELEERANGE;
}

static bool P_IsPlayerVisible(const mobj_t* actor, const player_t* player,
                                 bool allaround)
{
    if (P_CheckSight(actor, player->mo)) {
        // actor has a clear sight to player, but it does
        // not automatically mean the player is visible.
        return P_IsPlayerInFOV(actor, player, allaround);
    }
    // out of sight
    return false;
}

static bool P_CanTargetPlayer(const mobj_t* actor, const player_t* player,
                                 bool allaround)
{
    if (player->health <= 0) {
        // Dead.
        return false;
    }
    return P_IsPlayerVisible(actor, player, allaround);
}

//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
static bool P_LookForPlayers(mobj_t* actor, bool allaround) {
    int plr_checks = 0;
    int stop = (actor->lastlook - 1) & 3;

    while (true) {
        if (playeringame[actor->lastlook]) {
            if (actor->lastlook == stop || plr_checks == 2) {
                // done looking
                return false;
            }

            plr_checks += 1;
            player_t* player = &players[actor->lastlook];
            if (P_CanTargetPlayer(actor, player, allaround)) {
                actor->target = player->mo;
                return true;
            }
        }
        actor->lastlook = (actor->lastlook + 1) & 3;
    }
}


//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie(mobj_t* mo) {
    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for (thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next) {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker) {
            continue;
        }

        const mobj_t* mo2 = (mobj_t *) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0) {
            // other Keen not dead
            return;
        }
    }

    line_t junk;
    junk.tag = 666;
    EV_DoDoor(&junk, vld_open);
}


//
// ACTION ROUTINES
//

static void A_DoChaseSound(mobj_t* actor) {
    int sound;
    switch (actor->info->seesound) {
        case sfx_posit1:
        case sfx_posit2:
        case sfx_posit3:
            sound = sfx_posit1 + P_Random() % 3;
            break;
        case sfx_bgsit1:
        case sfx_bgsit2:
            sound = sfx_bgsit1 + P_Random() % 2;
            break;
        default:
            sound = actor->info->seesound;
            break;
    }

    if (actor->type == MT_SPIDER || actor->type == MT_CYBORG) {
        // full volume
        S_StartSound(NULL, sound);
        return;
    }
    S_StartSound(actor, sound);
}

static void A_StartChase(mobj_t* actor) {
    if (actor->info->seesound) {
        A_DoChaseSound(actor);
    }
    P_SetMobjState(actor, actor->info->seestate);
}

static bool A_HasFoundTarget(mobj_t* actor) {
    mobj_t* targ = actor->subsector->sector->soundtarget;
    actor->threshold = 0; // any shot will wake up

    if (targ == NULL || !(targ->flags & MF_SHOOTABLE)) {
        return P_LookForPlayers(actor, false);
    }

    actor->target = targ;
    if (actor->flags & MF_AMBUSH) {
        // When in ambush mode, only chase target if target is visible.
        return P_CheckSight(actor, actor->target)
               || P_LookForPlayers(actor, false);
    }
    return true;
}

//
// A_Look
// Stay in state until a player is sighted.
//
void A_Look(mobj_t* actor) {
    if (A_HasFoundTarget(actor)) {
        A_StartChase(actor);
    }
}

static void A_ChaseTarget(mobj_t* actor) {
    // Possibly choose another target
    if (netgame
        && actor->threshold == 0
        && !P_CheckSight(actor, actor->target)
        && P_LookForPlayers(actor, true))
    {
        // Got a new target.
        return;
    }

    // Chase towards player.
    actor->movecount--;
    if (actor->movecount < 0 || !P_Move(actor)) {
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if (actor->info->activesound && P_Random() < 3) {
        S_StartSound(actor, actor->info->activesound);
    }
}

static void A_DoMissileAttack(mobj_t* actor) {
    P_SetMobjState(actor, actor->info->missilestate);
    actor->flags |= MF_JUSTATTACKED;
}

static bool A_CanDoMissileAttack(mobj_t* actor) {
    if (!actor->info->missilestate) {
        return false;
    }
    if (gameskill < sk_nightmare && !fastparm && actor->movecount) {
        return false;
    }
    return P_CheckMissileRange(actor);
}

static void A_DoMeleeAttack(mobj_t* actor) {
    if (actor->info->attacksound) {
        S_StartSound(actor, actor->info->attacksound);
    }
    P_SetMobjState(actor, actor->info->meleestate);
}

static bool A_CanDoMeleeAttack(const mobj_t* actor) {
    return actor->info->meleestate && P_CheckMeleeRange(actor);
}

//
// Do not attack twice in a row.
//
static void A_AttackCooldown(mobj_t* actor) {
    actor->flags &= ~MF_JUSTATTACKED;
    if (gameskill != sk_nightmare && !fastparm) {
        P_NewChaseDir(actor);
    }
}

static void A_LookForNewTarget(mobj_t* actor) {
    if (P_LookForPlayers(actor, true)) {
        // Got a new target.
        return;
    }
    P_SetMobjState(actor, actor->info->spawnstate);
}

static bool A_HasNoTarget(const mobj_t* actor) {
    if (actor->target == NULL) {
        return true;
    }
    // Actor has target but can't damage it.
    return (actor->target->flags & MF_SHOOTABLE) == 0;
}

//
// Turn towards movement direction if not there yet.
//
static void A_UpdateAngle(mobj_t* actor) {
    if (actor->movedir >= DI_NODIR) {
        return;
    }
    actor->angle &= (7u << 29);
    int delta = (int) (actor->angle - (actor->movedir << 29));
    if (delta > 0) {
        actor->angle -= ANG90 / 2;
    } else if (delta < 0) {
        actor->angle += ANG90 / 2;
    }
}

static void A_UpdateTargetThreshold(mobj_t* actor) {
    if (actor->threshold == 0) {
        return;
    }
    if (gameversion <= exe_doom_1_2) {
        actor->threshold--;
        return;
    }
    if (actor->target && actor->target->health > 0) {
        // Has target and it is still alive.
        actor->threshold--;
        return;
    }
    actor->threshold = 0;
}

static void A_UpdateReactionTime(mobj_t* actor) {
    if (actor->reactiontime) {
        actor->reactiontime--;
    }
}

//
// A_Chase
// Actor has a melee attack, so it tries to close as fast as possible
//
void A_Chase(mobj_t* actor) {
    A_UpdateReactionTime(actor);
    A_UpdateTargetThreshold(actor);
    A_UpdateAngle(actor);
    if (A_HasNoTarget(actor)) {
        A_LookForNewTarget(actor);
        return;
    }
    if (actor->flags & MF_JUSTATTACKED) {
        A_AttackCooldown(actor);
        return;
    }
    if (A_CanDoMeleeAttack(actor)) {
        A_DoMeleeAttack(actor);
        return;
    }
    if (A_CanDoMissileAttack(actor)) {
        A_DoMissileAttack(actor);
        return;
    }
    A_ChaseTarget(actor);
}


//
// A_FaceTarget
//
void A_FaceTarget(mobj_t* actor) {
    if (actor->target == NULL) {
        return;
    }
    actor->flags &= ~MF_AMBUSH;
    actor->angle =
        R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
    if (actor->target->flags & MF_SHADOW) {
        actor->angle += P_SubRandom() << 21;
    }
}


//
// Zombieman attack.
//
void A_PosAttack(mobj_t* actor) {
    if (actor->target == NULL) {
        return;
    }

    A_FaceTarget(actor);
    int angle = actor->angle;
    int slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(actor, sfx_pistol);
    angle += P_SubRandom() << 20;
    int damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// Shotgun Guy attack.
//
void A_SPosAttack (mobj_t* actor)
{
    int		i;
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    for (i=0 ; i<3 ; i++)
    {
	angle = bangle + (P_SubRandom() << 20);
	damage = ((P_Random()%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}

//
// Chaingunner attack.
//
void A_CPosAttack (mobj_t* actor)
{
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    angle = bangle + (P_SubRandom() << 20);
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 40)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}


void A_SpidRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 10)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}

void A_BspiAttack (mobj_t *actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);

    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_ARACHPLAZ);
}


//
// A_TroopAttack
//
void A_TroopAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*3;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }

    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_TROOPSHOT);
}


void A_SargAttack (mobj_t* actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);

    if (gameversion > exe_doom_1_2)
    {
        if (!P_CheckMeleeRange (actor))
            return;
    }

    damage = ((P_Random()%10)+1)*4;

    if (gameversion <= exe_doom_1_2)
        P_LineAttack(actor, actor->angle, MELEERANGE, 0, damage);
    else
        P_DamageMobj (actor->target, actor, actor, damage);
}

void A_HeadAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = (P_Random()%6+1)*10;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_HEADSHOT);
}

void A_CyberAttack (mobj_t* actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    P_SpawnMissile (actor, actor->target, MT_ROCKET);
}


void A_BruisAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*10;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void A_SkelMissile (mobj_t* actor)
{	
    mobj_t*	mo;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    actor->z += 16*FRACUNIT;	// so missile spawns higher
    mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
    actor->z -= 16*FRACUNIT;	// back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor->target;
}

int	TRACEANGLE = 0xc000000;

void A_Tracer (mobj_t* actor)
{
    angle_t	exact;
    fixed_t	dist;
    fixed_t	slope;
    mobj_t*	dest;
    mobj_t*	th;
		
    if (gametic & 3)
	return;
    
    // spawn a puff of smoke behind the rocket		
    P_SpawnPuff (actor->x, actor->y, actor->z);
	
    th = P_SpawnMobj (actor->x-actor->momx,
		      actor->y-actor->momy,
		      actor->z, MT_SMOKE);
    
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	th->tics = 1;
    
    // adjust direction
    dest = actor->tracer;
	
    if (!dest || dest->health <= 0)
	return;
    
    // change angle	
    exact = R_PointToAngle2 (actor->x,
			     actor->y,
			     dest->x,
			     dest->y);

    if (exact != actor->angle)
    {
	if (exact - actor->angle > 0x80000000)
	{
	    actor->angle -= TRACEANGLE;
	    if (exact - actor->angle < 0x80000000)
		actor->angle = exact;
	}
	else
	{
	    actor->angle += TRACEANGLE;
	    if (exact - actor->angle > 0x80000000)
		actor->angle = exact;
	}
    }

    actor->momx = FixedMul (actor->info->speed, COS(actor->angle));
    actor->momy = FixedMul (actor->info->speed, SIN(actor->angle));
    
    // change slope
    dist = P_ApproxDistance(dest->x - actor->x, dest->y - actor->y);
    
    dist = dist / actor->info->speed;

    if (dist < 1)
	dist = 1;
    slope = (dest->z+40*FRACUNIT - actor->z) / dist;

    if (slope < actor->momz)
	actor->momz -= FRACUNIT/8;
    else
	actor->momz += FRACUNIT/8;
}


void A_SkelWhoosh (mobj_t*	actor)
{
    if (!actor->target)
	return;
    A_FaceTarget (actor);
    S_StartSound (actor,sfx_skeswg);
}

void A_SkelFist (mobj_t*	actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
	
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*6;
	S_StartSound (actor, sfx_skepch);
	P_DamageMobj (actor->target, actor, actor, damage);
    }
}



//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
static mobj_t* corpsehit;
static fixed_t viletryx;
static fixed_t viletryy;

static bool PIT_VileCheck(mobj_t* thing) {
    if (!(thing->flags & MF_CORPSE)) {
        // Not a monster.
        return true;
    }
    if (thing->tics != -1) {
        // Not lying still yet.
        return true;
    }
    if (thing->info->raisestate == S_NULL) {
        // Monster doesn't have a raise state.
        return true;
    }

    fixed_t dx = abs(thing->x - viletryx);
    fixed_t dy = abs(thing->y - viletryy);
    int max_dist = thing->info->radius + mobjinfo[MT_VILE].radius;
    if (dx > max_dist || dy > max_dist) {
        // Not actually touching.
        return true;
    }

    corpsehit = thing;
    corpsehit->momx = 0;
    corpsehit->momy = 0;
    corpsehit->height <<= 2;
    bool check = P_CheckPosition(corpsehit, corpsehit->x, corpsehit->y);
    corpsehit->height >>= 2;

    if (!check) {
        // Doesn't fit here.
        return true;
    }
    // Got one, so stop checking.
    return false;
}


static void A_ResurrectMonster(mobj_t* actor) {
    // Face corpse.
    mobj_t* temp = actor->target;
    actor->target = corpsehit;
    A_FaceTarget(actor);
    actor->target = temp;

    // Put archie in raise state.
    P_SetMobjState(actor, S_VILE_HEAL1);
    S_StartSound(corpsehit, sfx_slop);

    // Resurrect corpse.
    mobjinfo_t* info = corpsehit->info;
    P_SetMobjState(corpsehit, info->raisestate);
    corpsehit->height <<= 2;
    corpsehit->flags = info->flags;
    corpsehit->health = info->spawnhealth;
    corpsehit->target = NULL;
}

static bool A_TryResurrectMonsters(mobj_t* actor) {
    viletryx = actor->x + actor->info->speed * xspeed[actor->movedir];
    viletryy = actor->y + actor->info->speed * yspeed[actor->movedir];

    int xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
    int xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
    int yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
    int yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            // Call PIT_VileCheck to check whether object
            // is a corpse that can be raised.
            bool can_resurrect = !P_BlockThingsIterator(bx, by, PIT_VileCheck);
            if (can_resurrect) {
                // Got one!
                A_ResurrectMonster(actor);
                return true;
            }
        }
    }

    return false;
}

//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase(mobj_t* actor) {
    if (actor->movedir == DI_NODIR) {
        // Not moving, so return to normal attack.
        A_Chase(actor);
        return;
    }
    if (!A_TryResurrectMonsters(actor)) {
        // Could not resurrect any monster,
        // so return to normal attack.
        A_Chase(actor);
    }
}


//
// A_VileStart
//
void A_VileStart(mobj_t* actor) {
    S_StartSound(actor, sfx_vilatk);
}


void A_StartFire(mobj_t* actor) {
    S_StartSound(actor, sfx_flamst);
    A_Fire(actor);
}

void A_FireCrackle(mobj_t* actor) {
    S_StartSound(actor, sfx_flame);
    A_Fire(actor);
}

//
// A_Fire
// Keep fire in front of player unless out of sight.
//
void A_Fire(mobj_t* actor) {
    const mobj_t* dest = actor->tracer;
    if (!dest) {
        return;
    }

    const mobj_t* target = P_SubstNullMobj(actor->target);

    // Don't move it if the vile lost sight.
    if (!P_CheckSight(target, dest)) {
        return;
    }

    P_UnsetThingPosition(actor);
    actor->x = dest->x + FixedMul(24 * FRACUNIT, COS(dest->angle));
    actor->y = dest->y + FixedMul(24 * FRACUNIT, SIN(dest->angle));
    actor->z = dest->z;
    P_SetThingPosition(actor);
}


//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget(mobj_t* actor) {
    if (!actor->target) {
        return;
    }

    A_FaceTarget(actor);

    // Vanilla Bug: the fire from the arch-vile's attack can be spawned at
    // the wrong location. Note that in the second parameter, the actor's
    // target's x coordinate is used again where the code should instead use
    // the target's y coordinate.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Arch-vile_fire_spawned_at_the_wrong_location
    mobj_t* fog = P_SpawnMobj(actor->target->x, actor->target->x,
                              actor->target->z, MT_FIRE);

    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire(fog);
}


//
// Move the fire between the vile and the player.
//
static void A_MoveVileFire(mobj_t* actor) {
    mobj_t* fire = actor->tracer;
    if (!fire) {
        return;
    }
    fire->x = actor->target->x - FixedMul(24 * FRACUNIT, COS(actor->angle));
    fire->y = actor->target->y - FixedMul(24 * FRACUNIT, SIN(actor->angle));
    P_RadiusAttack(fire, actor, 70);
}

static void A_DoVileAttack(mobj_t* actor) {
    S_StartSound(actor, sfx_barexp);
    P_DamageMobj(actor->target, actor, actor, 20);
    actor->target->momz = 1000 * FRACUNIT / actor->target->info->mass;
}

//
// A_VileAttack
//
void A_VileAttack(mobj_t* actor) {
    if (!actor->target) {
        return;
    }
    A_FaceTarget(actor);
    if (P_CheckSight(actor, actor->target)) {
        // Only do attack if target is in line of sight.
        A_DoVileAttack(actor);
        A_MoveVileFire(actor);
    }
}


//
// Mancubus attack, firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it. 
//
#define FATSPREAD (ANG90 / 8)

void A_FatRaise(mobj_t* actor) {
    A_FaceTarget(actor);
    S_StartSound(actor, sfx_manatk);
}


void A_FatAttack1(mobj_t* actor) {
    mobj_t *mo;
    mobj_t *target;

    A_FaceTarget(actor);

    // Change direction  to ...
    actor->angle += FATSPREAD;
    target = P_SubstNullMobj(actor->target);
    P_SpawnMissile(actor, target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    mo->momx = FixedMul(mo->info->speed, COS(mo->angle));
    mo->momy = FixedMul(mo->info->speed, SIN(mo->angle));
}

void A_FatAttack2(mobj_t* actor) {
    mobj_t *mo;
    mobj_t *target;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    target = P_SubstNullMobj(actor->target);
    P_SpawnMissile(actor, target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, target, MT_FATSHOT);
    mo->angle -= FATSPREAD * 2;
    mo->momx = FixedMul(mo->info->speed, COS(mo->angle));
    mo->momy = FixedMul(mo->info->speed, SIN(mo->angle));
}

void A_FatAttack3(mobj_t* actor) {
    mobj_t *mo;
    mobj_t *target;

    A_FaceTarget(actor);

    target = P_SubstNullMobj(actor->target);

    mo = P_SpawnMissile(actor, target, MT_FATSHOT);
    mo->angle -= FATSPREAD / 2;
    mo->momx = FixedMul(mo->info->speed, COS(mo->angle));
    mo->momy = FixedMul(mo->info->speed, SIN(mo->angle));

    mo = P_SpawnMissile(actor, target, MT_FATSHOT);
    mo->angle += FATSPREAD / 2;
    mo->momx = FixedMul(mo->info->speed, COS(mo->angle));
    mo->momy = FixedMul(mo->info->speed, SIN(mo->angle));
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define SKULLSPEED (20 * FRACUNIT)

void A_SkullAttack(mobj_t* actor) {
    if (!actor->target) {
        return;
    }

    const mobj_t* dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor, actor->info->attacksound);
    A_FaceTarget(actor);

    // The XY speed is constant.
    actor->momx = FixedMul(SKULLSPEED, COS(actor->angle));
    actor->momy = FixedMul(SKULLSPEED, SIN(actor->angle));

    // The Z velocity (vertical movement) is variable, calculated based on the
    // time to reach the target. To determine the time to target, we use the XY
    // distance between the actor and the target. The height difference will
    // affect the Z velocity for proper vertical movement.
    // Vanilla Bug: Using a constant XY speed to calculate the arrival time
    // can cause the lost soul to travel at abnormal movement speeds when there
    // is a significant height difference between the actor and the target.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Quantum_lost_soul
    int dist = P_ApproxDistance(dest->x - actor->x, dest->y - actor->y);
    int time_arrival = dist / SKULLSPEED;
    if (time_arrival < 1) {
        time_arrival = 1;
    }
    actor->momz = (dest->z + (dest->height >> 1) - actor->z) / time_arrival;
}

static mobj_t* A_TrySpawnLostSoul(mobj_t* actor, angle_t angle) {
    int pre_step = (4 * FRACUNIT) +
                  3 * (actor->info->radius + mobjinfo[MT_SKULL].radius) / 2;

    fixed_t x = actor->x + FixedMul(pre_step, COS(angle));
    fixed_t y = actor->y + FixedMul(pre_step, SIN(angle));
    fixed_t z = actor->z + (8 * FRACUNIT);

    mobj_t* lost_soul = P_SpawnMobj(x, y, z, MT_SKULL);

    // Check if spawned in a valid location.
    if (!P_TryMove(lost_soul, lost_soul->x, lost_soul->y)) {
        // Not valid, so kill it immediately.
        P_DamageMobj(lost_soul, actor, actor, 10000);
        return NULL;
    }

    lost_soul->target = actor->target;
    return lost_soul;
}

//
// Check if thinker is a lost soul.
//
static bool A_IsLostSoul(const thinker_t* thinker) {
    if (thinker->function.acp1 != (actionf_p1) P_MobjThinker) {
        // Not a monster.
        return false;
    }
    return ((mobj_t *) thinker)->type == MT_SKULL;
}

//
// Count total number of skull currently on the level.
//
static int A_CountNumLostSouls() {
    int count = 0;

    thinker_t* thinker = thinkercap.next;
    while (thinker != &thinkercap) {
        if (A_IsLostSoul(thinker)) {
            count++;
        }
        thinker = thinker->next;
    }

    return count;
}

static bool A_CanSpawnLostSoul() {
    int count = A_CountNumLostSouls();
    // if there are already 20 skulls on the level, don't spit another one.
    return count <= 20;
}

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
static void A_PainShootSkull(mobj_t* actor, angle_t angle) {
    if (!A_CanSpawnLostSoul()) {
        return;
    }

    mobj_t* lost_soul = A_TrySpawnLostSoul(actor, angle);
    if (lost_soul != NULL) {
        // Shoot lost soul at the same target of the pain element.
        A_SkullAttack(lost_soul);
    }
}


//
// A_PainAttack
// Pain elemental attack.
// Spawn a lost soul and launch it at the target
//
void A_PainAttack(mobj_t *actor) {
    if (!actor->target) {
        return;
    }
    A_FaceTarget(actor);
    A_PainShootSkull(actor, actor->angle);
}


void A_PainDie(mobj_t* actor) {
    A_Fall(actor);
    A_PainShootSkull(actor, actor->angle + ANG90);
    A_PainShootSkull(actor, actor->angle + ANG180);
    A_PainShootSkull(actor, actor->angle + ANG270);
}


void A_Scream(mobj_t* actor) {
    int sound;
    switch (actor->info->deathsound) {
        case 0:
            return;
        case sfx_podth1:
        case sfx_podth2:
        case sfx_podth3:
            sound = sfx_podth1 + P_Random() % 3;
            break;
        case sfx_bgdth1:
        case sfx_bgdth2:
            sound = sfx_bgdth1 + P_Random() % 2;
            break;
        default:
            sound = actor->info->deathsound;
            break;
    }

    // Check for bosses.
    if (actor->type == MT_SPIDER || actor->type == MT_CYBORG) {
        // Full volume.
        S_StartSound(NULL, sound);
    } else {
        S_StartSound(actor, sound);
    }
}


void A_XScream(mobj_t* actor) {
    S_StartSound(actor, sfx_slop);
}

void A_Pain(mobj_t* actor) {
    if (actor->info->painsound) {
        S_StartSound(actor, actor->info->painsound);
    }
}


void A_Fall(mobj_t* actor) {
    // Actor is on ground, it can be walked over.
    // So change this if corpse objects are meant to be obstacles.
    actor->flags &= ~MF_SOLID;
}


//
// A_Explode
//
void A_Explode(mobj_t* actor) {
    P_RadiusAttack(actor, actor->target, 128);
}

static void A_VictoryNonCommercial() {
    line_t junk;
    if (gameepisode == 1) {
        junk.tag = 666;
        EV_DoFloor(&junk, lowerFloorToLowest);
        return;
    }
    if (gameepisode == 4 && gamemap == 6) {
        junk.tag = 666;
        EV_DoDoor(&junk, vld_blazeOpen);
        return;
    }
    if (gameepisode == 4 && gamemap == 8) {
        junk.tag = 666;
        EV_DoFloor(&junk, lowerFloorToLowest);
        return;
    }
    G_ExitLevel();
}

static void A_VictoryCommercial(const mobj_t* mo) {
    line_t junk;
    if (gamemap != 7) {
        G_ExitLevel();
        return;
    }
    if (mo->type == MT_FATSO) {
        junk.tag = 666;
        EV_DoFloor(&junk, lowerFloorToLowest);
        return;
    }
    if (mo->type == MT_BABY) {
        junk.tag = 667;
        EV_DoFloor(&junk, raiseToTexture);
        return;
    }
    G_ExitLevel();
}

static void A_Victory(const mobj_t* mo) {
    if (gamemode == commercial) {
        A_VictoryCommercial(mo);
        return;
    }
    A_VictoryNonCommercial();
}

//
// Check to see if all bosses in the level are dead.
//
static bool A_IsAllBossesDead(const mobj_t* boss) {
    thinker_t* th = thinkercap.next;

    while (th != &thinkercap) {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker) {
            // Not a monster.
            continue;
        }

        mobj_t* mo2 = (mobj_t *) th;
        if (mo2 != boss && mo2->type == boss->type && mo2->health > 0) {
            // Found other boss that is not dead.
            return false;
        }

        th = th->next;
    }

    return true;
}

//
// Check whether the death of the specified monster type is
// allowed to trigger the end of episode special action.
//
static bool A_CanMonsterTriggerBossEnd(const mobj_t* mo) {
    mobjtype_t motype = mo->type;

    // Doom 2.
    if (gamemode == commercial) {
        if (gamemap != 7) {
            return false;
        }
        return (motype == MT_FATSO) || (motype == MT_BABY);
    }

    // This behavior changed in v1.9, the most notable effect
    // of which was to break uac_dead.wad
    if (gameversion < exe_ultimate) {
        if (gamemap != 8) {
            return false;
        }
        // Baron death on later episodes is nothing special.
        return gameepisode == 1 || motype != MT_BRUISER;
    }

    // New logic that appeared in Ultimate Doom.
    // Looks like the logic was overhauled while adding in the
    // episode 4 support.  Now bosses only trigger on their
    // specific episode.
    switch (gameepisode) {
        case 1:
            return gamemap == 8 && motype == MT_BRUISER;
        case 2:
            return gamemap == 8 && motype == MT_CYBORG;
        case 3:
            return gamemap == 8 && motype == MT_SPIDER;
        case 4:
            return (gamemap == 6 && motype == MT_CYBORG)
                   || (gamemap == 8 && motype == MT_SPIDER);
        default:
            return gamemap == 8;
    }
}

//
// Check if there is a player alive for victory.
//
static bool A_IsAnyPlayerAlive() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i] && players[i].health > 0) {
            return true;
        }
    }
    // No one left alive, so do not end game.
    return false;
}

static bool A_CanTriggerBossEnd(const mobj_t* mo) {
    return A_CanMonsterTriggerBossEnd(mo)
           && A_IsAnyPlayerAlive()
           && A_IsAllBossesDead(mo);
}

//
// A_BossDeath
// Possibly trigger special effects if on first boss level.
//
void A_BossDeath(const mobj_t* mo) {
    if (A_CanTriggerBossEnd(mo)) {
        A_Victory(mo);
    }
}

void A_Hoof(mobj_t* mo) {
    S_StartSound(mo, sfx_hoof);
    A_Chase(mo);
}

void A_Metal(mobj_t* mo) {
    S_StartSound(mo, sfx_metal);
    A_Chase(mo);
}

void A_BabyMetal(mobj_t* mo) {
    S_StartSound(mo, sfx_bspwlk);
    A_Chase(mo);
}


static mobj_t* braintargets[32];
static int numbraintargets;
static int braintargeton = 0;

void A_BrainAwake(const mobj_t* mo) {
    // Find all the target spots.
    numbraintargets = 0;
    braintargeton = 0;

    for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker) {
            // Not a mobj.
            continue;
        }

        mobj_t* m = (mobj_t *) th;
        if (m->type == MT_BOSSTARGET) {
            braintargets[numbraintargets] = m;
            numbraintargets++;
        }
    }

    S_StartSound(NULL, sfx_bossit);
}

void A_BrainPain(mobj_t* mo) {
    S_StartSound(NULL, sfx_bospn);
}

void A_BrainScream(const mobj_t* mo) {
    fixed_t x0 = mo->x - (196 * FRACUNIT);
    fixed_t x1 = mo->x + (320 * FRACUNIT);
    fixed_t inc = (8 * FRACUNIT);

    for (fixed_t x = x0; x < x1; x += inc) {
        fixed_t y = mo->y - (320 * FRACUNIT);
        fixed_t z = 128 + (P_Random() * 2 * FRACUNIT);
        mobj_t* th = P_SpawnMobj(x, y, z, MT_ROCKET);

        th->momz = P_Random() * 512;
        P_SetMobjState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if (th->tics < 1) {
            th->tics = 1;
        }
    }

    S_StartSound(NULL, sfx_bosdth);
}

void A_BrainExplode(const mobj_t* mo) {
    fixed_t x = mo->x + P_SubRandom() * 2048;
    fixed_t y = mo->y;
    fixed_t z = 128 + (P_Random() * 2 * FRACUNIT);

    mobj_t* th = P_SpawnMobj(x, y, z, MT_ROCKET);
    th->momz = P_Random() * 512;
    P_SetMobjState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if (th->tics < 1) {
        th->tics = 1;
    }
}


void A_BrainDie(const mobj_t* mo) {
    G_ExitLevel();
}

void A_BrainSpit(mobj_t* mo) {
    static int easy = 0;
    easy ^= 1;

    if (gameskill <= sk_easy && (!easy)) {
        return;
    }
    if (numbraintargets == 0) {
        I_Error("A_BrainSpit: numbraintargets was 0 (vanilla crashes here)");
    }

    // Shoot a cube at current target.
    mobj_t* targ = braintargets[braintargeton];

    // Spawn brain missile.
    mobj_t* newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    newmobj->reactiontime =
        ((targ->y - mo->y) / newmobj->momy) / newmobj->state->tics;
    S_StartSound(NULL, sfx_bospit);

    // Next target.
    braintargeton = (braintargeton + 1) % numbraintargets;
}

//
// Travelling cube sound.
//
void A_SpawnSound(mobj_t* mo) {
    S_StartSound(mo, sfx_boscub);
    A_SpawnFly(mo);
}

//
// Randomly select monster to spawn.
//
static mobjtype_t A_RandomlySelectMonster() {
    // Randomly select monster to spawn.
    int r = P_Random();

    // Probability distribution (kind of :), decreasing likelihood.
    if (r < 50) {
        return MT_TROOP;
    }
    if (r < 90) {
        return MT_SERGEANT;
    }
    if (r < 120) {
        return MT_SHADOWS;
    }
    if (r < 130) {
        return MT_PAIN;
    }
    if (r < 160) {
        return MT_HEAD;
    }
    if (r < 162) {
        return MT_VILE;
    }
    if (r < 172) {
        return MT_UNDEAD;
    }
    if (r < 192) {
        return MT_BABY;
    }
    if (r < 222) {
        return MT_FATSO;
    }
    if (r < 246) {
        return MT_KNIGHT;
    }
    return MT_BRUISER;
}

void A_SpawnFly(mobj_t* mo) {
    mo->reactiontime--;
    if (mo->reactiontime) {
        // Still flying.
        return;
    }

    const mobj_t* targ = P_SubstNullMobj(mo->target);

    // First spawn teleport fog.
    mobj_t* fog = P_SpawnMobj(targ->x, targ->y, targ->z, MT_SPAWNFIRE);
    S_StartSound(fog, sfx_telept);

    // Then spawn new monster.
    mobjtype_t type = A_RandomlySelectMonster();
    mobj_t* newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
    if (P_LookForPlayers(newmobj, true)) {
        P_SetMobjState(newmobj, newmobj->info->seestate);
    }
    // Telefrag anything in this spot.
    P_TeleportMove(newmobj, newmobj->x, newmobj->y);

    // Remove self (i.e., cube).
    P_RemoveMobj(mo);
}
