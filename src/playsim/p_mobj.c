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
//	Moving object handling. Spawn functions.
//


#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

#include "st_stuff.h"
#include "hu_stuff.h"

#include "s_sound.h"

#include "doomstat.h"


void G_PlayerReborn(int playernum);


//
// Use a heuristic approach to detect infinite state cycles: Count the number
// of times the loop in P_SetMobjState() executes and exit with an error once
// an arbitrary very large limit is reached.
//
#define MOBJ_CYCLE_LIMIT 1000000

//
// P_SetMobjState
// Returns true if the mobj is still present.
//
bool P_SetMobjState(mobj_t* mobj, statenum_t	state) {
    int	cycle_counter = 0;

    do {
	if (state == S_NULL) {
	    mobj->state = (state_t *) S_NULL;
	    P_RemoveMobj(mobj);
	    return false;
	}

        state_t* st = &states[state];
	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	// Modified handling.
	// Call action functions when the state is set
	if (st->action.acp1) {
            st->action.acp1(mobj);
        }
	
	state = st->nextstate;

	if (cycle_counter++ > MOBJ_CYCLE_LIMIT) {
	    I_Error("P_SetMobjState: Infinite state cycle detected!");
	}
    } while (mobj->tics == 0);
				
    return true;
}


//
// P_ExplodeMissile  
//
static void P_ExplodeMissile(mobj_t* mo) {
    mo->momx = 0;
    mo->momy = 0;
    mo->momz = 0;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
    mo->tics -= P_Random() & 3;
    if (mo->tics < 1) {
        mo->tics = 1;
    }
    mo->flags &= ~MF_MISSILE;
    if (mo->info->deathsound) {
        S_StartSound(mo, mo->info->deathsound);
    }
}


//
// P_XYMovement  
//
#define STOPSPEED 0x1000
#define FRICTION  0xe800

static bool P_IsSpeedInRange(const mobj_t *mo, fixed_t speed) {
    return (mo->momx > -speed && mo->momx < speed)
           && (mo->momy > -speed && mo->momy < speed);
}

static bool P_ShouldStopMoving(const mobj_t* mo) {
    const player_t* player = mo->player;
    if (!P_IsSpeedInRange(mo, STOPSPEED)) {
        // If the mobj's speed is too high, it cannot stop yet.
        return false;
    }
    if (!player) {
        // Stop all enemies within stop speed range.
        return true;
    }
    // Check if the player is not providing any movement input.
    // If there's no input, the player is likely not trying to
    // accelerate, and the movement will stop.
    return player->cmd.forwardmove == 0 && player->cmd.sidemove == 0;
}

static void P_ApplyFriction(mobj_t* mo) {
    if (P_ShouldStopMoving(mo)) {
        // If in a walking frame, stop moving.
        const player_t* player = mo->player;
        if (player && (unsigned)((player->mo->state - states)- S_PLAY_RUN1) < 4) {
            P_SetMobjState(player->mo, S_PLAY);
        }
        mo->momx = 0;
        mo->momy = 0;
    } else {
        mo->momx = FixedMul(mo->momx, FRICTION);
        mo->momy = FixedMul(mo->momy, FRICTION);
    }
}

static bool P_CanApplyFriction(const mobj_t* mo) {
    if (mo->flags & (MF_MISSILE | MF_SKULLFLY)) {
        // No friction for missiles ever.
        return false;
    }
    if (mo->z > mo->floorz) {
        // No friction when airborne.
        return false;
    }
    if (mo->flags & MF_CORPSE) {
        // Do not stop sliding if halfway off a step with some momentum.
        int speed = FRACUNIT / 4;
        if (mo->momx > speed || mo->momx < -speed
            || mo->momy > speed || mo->momy < -speed) {
            if (mo->floorz != mo->subsector->sector->floorheight) {
                return false;
            }
        }
    }
    return true;
}

static bool P_HasHitSky() {
    // Vanilla Bug: sometimes the projectile will directly hit the ceiling that
    // uses a sky texture, rather than a linedef. Because there is no logic
    // that checks if the projectile is directly hitting a "sky" ceiling, the
    // projectile will consider to hit a solid object and explode. Also, this
    // code does not handle sky floors.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Projectiles_explode_on_impact_with_%22sky%22
    return ceilingline
           && ceilingline->backsector
           && ceilingline->backsector->ceilingpic == sky_flat;
}

//
// Returns true if mobj continues to exist after collision, false otherwise.
//
static bool P_XYCollide(mobj_t* mo) {
    if (mo->player) {
        // Try to slide along it.
        P_SlideMove(mo);
        return true;
    }
    if (mo->flags & MF_MISSILE) {
        // Explode a missile.
        if (P_HasHitSky()) {
            // Hack to prevent missiles exploding against the sky.
            P_RemoveMobj(mo);
            return false;
        }
        P_ExplodeMissile(mo);
        return true;
    }
    mo->momx = 0;
    mo->momy = 0;
    return true;
}

static void P_ClipThingMomentum(mobj_t* mo) {
    if (mo->momx > MAXMOVE) {
        mo->momx = MAXMOVE;
    } else if (mo->momx < -MAXMOVE) {
        mo->momx = -MAXMOVE;
    }
    if (mo->momy > MAXMOVE) {
        mo->momy = MAXMOVE;
    } else if (mo->momy < -MAXMOVE) {
        mo->momy = -MAXMOVE;
    }
}

//
// Returns true if mobj continues to exist after movement, false otherwise.
//
static bool P_DoXYMovement(mobj_t* mo) {
    P_ClipThingMomentum(mo);

    fixed_t move_x = mo->momx;
    fixed_t move_y = mo->momy;
    fixed_t new_x;
    fixed_t new_y;
    do {
        // Apply movement sub stepping to increase collision accuracy.
        // Vanilla Bug: we do not check for negative movement, so south or
        // west direction ends up being less precise because no sub stepping
        // is applied. This bug is noticeable when shooting rockets into the
        // sky after picking up the green armor in E1M1, where some rockets
        // may explode unexpectedly.
        // More info on this bug can be found here:
        // https://doomwiki.org/wiki/Mancubus_fireball_clipping
        if (move_x > MAXMOVE/2 || move_y > MAXMOVE/2) {
            new_x = mo->x + move_x/2;
            new_y = mo->y + move_y/2;
            move_x >>= 1;
            move_y >>= 1;
        } else {
            new_x = mo->x + move_x;
            new_y = mo->y + move_y;
            move_x = 0;
            move_y = 0;
        }

        if (P_TryMove(mo, new_x, new_y)) {
            // Not blocked, so continue to move.
            continue;
        }
        if (!P_XYCollide(mo)) {
            // mobj removed after collision.
            return false;
        }
    } while (move_x || move_y);

    // mobj still exists after movement.
    return true;
}

static void P_StopSkullFlight(mobj_t* mo) {
    mo->flags &= ~MF_SKULLFLY;
    mo->momx = 0;
    mo->momy = 0;
    mo->momz = 0;
    // Vanilla Bug: this code puts the lost soul in its spawn state, causing
    // the lost soul to apparently forget its target after hitting a wall.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Lost_soul_target_amnesia
    P_SetMobjState(mo, mo->info->spawnstate);
}

static void P_XYMovement(mobj_t* mo) {
    if (mo->momx == 0 && mo->momy == 0) {
        if (mo->flags & MF_SKULLFLY) {
            // The skull slammed into something.
            P_StopSkullFlight(mo);
        }
        return;
    }
    if (!P_DoXYMovement(mo)) {
        // mobj removed after move.
        return;
    }

    // Slow down.
    if (mo->player && (mo->player->cheats & CF_NOMOMENTUM)) {
        // Debug option for no sliding at all.
        mo->momx = 0;
        mo->momy = 0;
        return;
    }
    if (P_CanApplyFriction(mo)) {
        P_ApplyFriction(mo);
    }
}

static bool P_CollisionEnabled(const mobj_t* thing) {
    return (thing->flags & MF_NOCLIP) == 0;
}

static bool P_GravityEnabled(const mobj_t* thing) {
    return (thing->flags & MF_NOGRAVITY) == 0;
}

static bool P_CanFloat(const mobj_t* thing) {
    return (thing->flags & MF_FLOAT) != 0;
}

static void P_HitCeiling(mobj_t* mo) {
    if (mo->momz > 0) {
        mo->momz = 0;
    }
    mo->z = mo->ceilingz - mo->height;
    if (mo->flags & MF_SKULLFLY) {
        // the skull slammed into something
        mo->momz = -mo->momz;
    }
}

static void P_ApplyGravity(mobj_t* thing) {
    if (thing->momz == 0) {
        thing->momz = -GRAVITY * 2;
    } else {
        thing->momz -= GRAVITY;
    }
}

//
// Squat down.
// Decrease viewheight for a moment after hitting
// the ground (hard), and utter appropriate sound.
//
static void P_SquatDown(mobj_t* mo) {
    mo->player->deltaviewheight = mo->momz >> 3;
    S_StartSound(mo, sfx_oof);
}

static void P_HitFloor(mobj_t* mo) {
    // Note (id):
    //  somebody left this after the setting momz to 0, kinda useless there.
    //
    // cph - This was the a bug in the linuxdoom-1.10 source which
    //  caused it not to sync Doom 2 v1.9 demos. Someone
    //  added the above comment and moved up the following code. So
    //  demos would desync in close lost soul fights.
    // Note that this only applies to original Doom 1 or Doom2 demos - not
    //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
    //  gamemission. (Note we assume that Doom1 is always Ult Doom, which
    //  seems to hold for most published demos.)
    //
    //  fraggle - cph got the logic here slightly wrong. There are three
    //  versions of Doom 1.9:
    //
    //  * The version used in registered doom 1.9 + doom2 - no bounce
    //  * The version used in ultimate doom - has bounce
    //  * The version used in final doom - has bounce
    //
    // So we need to check that this is either retail or commercial
    // (but not doom2)

    bool correct_lost_soul_bounce = gameversion >= exe_ultimate;
    if (correct_lost_soul_bounce && mo->flags & MF_SKULLFLY) {
        // the skull slammed into something
        mo->momz = -mo->momz;
    }
    if (mo->momz < 0) {
        if (mo->player && mo->momz < -GRAVITY * 8) {
            P_SquatDown(mo);
        }
        mo->momz = 0;
    }
    mo->z = mo->floorz;
    if (!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY) {
        // See lost soul bouncing comment above. We need this here
        // for bug compatibility with original Doom2 v1.9.
        // If a soul is charging and hit by a raising floor this
        // incorrectly reverses its Y momentum.
        mo->momz = -mo->momz;
    }
}

//
// Float down towards target if too close
//
static void P_FloatTowardsTarget(mobj_t* mo) {
    fixed_t dist =
        P_ApproxDistance(mo->x - mo->target->x, mo->y - mo->target->y);
    fixed_t delta = (mo->target->z + (mo->height >> 1)) - mo->z;

    if (delta < 0 && dist < -(delta * 3)) {
        mo->z -= FLOATSPEED;
    } else if (delta > 0 && dist < (delta * 3)) {
        mo->z += FLOATSPEED;
    }
}

static bool P_CanFloatTowardsTarget(const mobj_t* mo) {
    return P_CanFloat(mo)
           && mo->target
           && !(mo->flags & MF_SKULLFLY)
           && !(mo->flags & MF_INFLOAT);
}

static void P_DoSmoothStepUp(mobj_t* mo) {
    if (mo->z >= mo->floorz) {
        // Only apply when climbing stairs.
        return;
    }
    player_t* player = mo->player;
    player->viewheight -= (mo->floorz - mo->z);
    player->deltaviewheight = (VIEWHEIGHT - player->viewheight) / 8;
}

//
// P_ZMovement
//
void P_ZMovement(mobj_t* mo) {
    // Check for smooth step up.
    if (mo->player) {
        P_DoSmoothStepUp(mo);
    }

    // adjust height
    mo->z += mo->momz;

    if (P_CanFloatTowardsTarget(mo)) {
        P_FloatTowardsTarget(mo);
    }
    if (mo->z <= mo->floorz) {
        P_HitFloor(mo);
	if ((mo->flags & MF_MISSILE) && P_CollisionEnabled(mo)) {
	    P_ExplodeMissile(mo);
	    return;
	}
    } else if (P_GravityEnabled(mo)) {
        P_ApplyGravity(mo);
    }
    if (mo->z + mo->height > mo->ceilingz) {
        P_HitCeiling(mo);
	if ((mo->flags & MF_MISSILE) && P_CollisionEnabled(mo)) {
	    P_ExplodeMissile(mo);
	    return;
	}
    }
}

static void P_RespawnMonster(mobj_t* old_body, fixed_t x, fixed_t y) {
    const mapthing_t* mthing = &old_body->spawnpoint;
    fixed_t z =
        (old_body->info->flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ);

    // Inherit attributes from deceased one.
    mobj_t* new_body = P_SpawnMobj(x, y, z, old_body->type);
    new_body->spawnpoint = old_body->spawnpoint;
    new_body->angle = ANG45 * (mthing->angle / 45);
    if (mthing->options & MTF_AMBUSH) {
        new_body->flags |= MF_AMBUSH;
    }
    new_body->reactiontime = 18;

    // Remove the old monster.
    P_RemoveMobj(old_body);
}

//
// Spawn a teleport fog at both old and new spot.
//
static void P_MonsterRespawnFog(const mobj_t* old_body, fixed_t x, fixed_t y) {
    // Spawn a teleport fog at old spot because of removal of the body.
    fixed_t z = old_body->subsector->sector->floorheight;
    mobj_t* mo = P_SpawnMobj(old_body->x, old_body->y, z, MT_TFOG);
    S_StartSound(mo, sfx_telept);

    // Spawn a teleport fog at the new spot.
    const subsector_t* ss = R_PointInSubsector(x, y);
    z = ss->sector->floorheight;
    mo = P_SpawnMobj(x, y, z, MT_TFOG);
    S_StartSound(mo, sfx_telept);
}

//
// P_NightmareRespawn
//
static void P_NightmareRespawn(mobj_t* mobj) {
    fixed_t x = mobj->spawnpoint.x << FRACBITS;
    fixed_t y = mobj->spawnpoint.y << FRACBITS;
    if (P_CheckPosition(mobj, x, y)) {
        P_MonsterRespawnFog(mobj, x, y);
        P_RespawnMonster(mobj, x, y);
    }
}

static bool P_CanRespawnMonster(mobj_t* mobj) {
    if (!(mobj->flags & MF_COUNTKILL)) {
        return false;
    }
    if (!respawnmonsters) {
        return false;
    }

    mobj->movecount++;
    if (mobj->movecount < 12*TICRATE) {
        return false;
    }

    if (leveltime & 31) {
        return false;
    }
    if (P_Random() > 4) {
        return false;
    }
    return true;
}

//
// Cycle through states, calling action functions at transitions.
//
static void P_CycleThroughStates(mobj_t* mobj) {
    mobj->tics--;

    // you can cycle through multiple states in a tic
    if (mobj->tics == 0) {
        P_SetMobjState(mobj, mobj->state->nextstate);
    }
}

static bool P_IsMovingZ(mobj_t* mobj) {
    return (mobj->z != mobj->floorz) || mobj->momz;
}

static bool P_IsMovingXY(mobj_t* mobj) {
    return mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY);
}

//
// P_MobjThinker
//
void P_MobjThinker(mobj_t* mobj) {
    if (P_IsMovingXY(mobj)) {
	P_XYMovement(mobj);
        if (P_IsThinkerRemoved(&mobj->thinker)) {
            return;
        }
    }
    if (P_IsMovingZ(mobj)) {
	P_ZMovement(mobj);
	if (P_IsThinkerRemoved(&mobj->thinker)) {
            return;
        }
    }
    if (mobj->tics != -1) {
        P_CycleThroughStates(mobj);
        return;
    }
    if (P_CanRespawnMonster(mobj)) {
        P_NightmareRespawn(mobj);
    }
}

static void P_SetMobjZ(mobj_t* mobj, fixed_t z) {
    const sector_t* mobj_sector = mobj->subsector->sector;
    mobj->floorz = mobj_sector->floorheight;
    mobj->ceilingz = mobj_sector->ceilingheight;

    if (z == ONFLOORZ) {
        mobj->z = mobj->floorz;
    } else if (z == ONCEILINGZ) {
        mobj->z = mobj->ceilingz - mobj->info->height;
    } else {
        mobj->z = z;
    }
}

static void P_SetInitialMobjState(mobj_t* mobj) {
    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet
    state_t* state = &states[mobj->info->spawnstate];
    mobj->state = state;
    mobj->tics = state->tics;
    mobj->sprite = state->sprite;
    mobj->frame = state->frame;
}

static void P_SetMobjTypeData(mobj_t* mobj, mobjtype_t type) {
    mobjinfo_t* info = &mobjinfo[type];
    mobj->type = type;
    mobj->info = info;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->health = info->spawnhealth;
    if (gameskill != sk_nightmare) {
        mobj->reactiontime = info->reactiontime;
    }
    P_SetInitialMobjState(mobj);
}

//
// P_SpawnMobj
//
mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type) {
    mobj_t* mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));

    mobj->x = x;
    mobj->y = y;
    mobj->lastlook = P_Random() % MAXPLAYERS;
    mobj->thinker.function.acp1 = (actionf_p1) P_MobjThinker;

    P_SetMobjTypeData(mobj, type);
    P_SetThingPosition(mobj);
    P_SetMobjZ(mobj, z);
    P_AddThinker(&mobj->thinker);

    return mobj;
}


//
// P_RemoveMobj
//
mapthing_t itemrespawnque[ITEMQUESIZE];
int itemrespawntime[ITEMQUESIZE];
int iquehead;
int iquetail;

static bool P_IsSpecialRespawnQueueEmpty() {
    return iquehead == iquetail;
}

static void P_PopSpecialRespawnQueue() {
    iquetail = (iquetail + 1) % ITEMQUESIZE;
}

static void P_PushSpecialRespawnQueue(const mobj_t* mobj) {
    itemrespawnque[iquehead] = mobj->spawnpoint;
    itemrespawntime[iquehead] = leveltime;
    iquehead = (iquehead + 1) % ITEMQUESIZE;

    // lose one off the end?
    if (iquehead == iquetail) {
        iquetail = (iquetail + 1) % ITEMQUESIZE;
    }
}

static bool P_IsRespawnable(const mobj_t* mobj) {
    return !(mobj->flags & MF_DROPPED)
           && (mobj->type != MT_INV)
           && (mobj->type != MT_INS);
}

static bool P_IsSpecial(const mobj_t* mobj) {
    return (mobj->flags & MF_SPECIAL) != 0;
}

void P_RemoveMobj(mobj_t* mobj) {
    if (P_IsSpecial(mobj) && P_IsRespawnable(mobj)) {
        P_PushSpecialRespawnQueue(mobj);
    }
    // Unlink from sector and block lists.
    P_UnsetThingPosition(mobj);
    // Stop any playing sound.
    S_StopSound(mobj);
    // Free block.
    P_RemoveThinker((thinker_t*) mobj);
}

static void P_SpawnTeleport(const mapthing_t* mthing) {
    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    const subsector_t* subsector = R_PointInSubsector(x, y);
    fixed_t z = subsector->sector->floorheight;

    mobj_t* mo = P_SpawnMobj(x, y, z, MT_IFOG);
    S_StartSound(mo, sfx_itmbk);
}

static mobjtype_t P_FindThingType(const mapthing_t* mthing) {
    for (int i = 0; i < NUMMOBJTYPES; i++) {
        if (mthing->type == mobjinfo[i].doomednum) {
            return i;
        }
    }

    I_Error("P_FindThingType: Failed to find mobj type with doomednum "
            "%d when respawning thing. This would cause a buffer overrun "
            "in vanilla Doom", mthing->type);
}

static void P_RespawnThing(const mapthing_t* mthing) {
    mobjtype_t type = P_FindThingType(mthing);
    const mobjinfo_t* info = &mobjinfo[type];

    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    fixed_t z;
    if (info->flags & MF_SPAWNCEILING) {
        z = ONCEILINGZ;
    } else {
        z = ONFLOORZ;
    }

    mobj_t* mo = P_SpawnMobj(x, y, z, type);
    mo->spawnpoint = *mthing;
    mo->angle = ANG45 * (mthing->angle / 45);
}

static bool P_CanRespawn() {
    // Only respawn items if:
    // 1. In deathmatch
    // 2. Queue is not empty
    // 3. It has passed at least 30 seconds
    return deathmatch == 2
           && !P_IsSpecialRespawnQueueEmpty()
           && leveltime - itemrespawntime[iquetail] < 30*TICRATE;
}

//
// P_RespawnSpecials
//
void P_RespawnSpecials() {
    if (P_CanRespawn()) {
        const mapthing_t* thing = &itemrespawnque[iquetail];
        // Spawn a teleport fog at the new spot.
        P_SpawnTeleport(thing);
        // Spawn it.
        P_RespawnThing(thing);
        // Pull it from the queue.
        P_PopSpecialRespawnQueue();
    }
}

static void P_SetupCards(player_t* player) {
    // give all cards in death match mode
    if (deathmatch) {
        for (int i = 0; i < NUMCARDS; i++) {
            player->cards[i] = true;
        }
    }
}

static void P_ResetPlayerData(player_t* player) {
    player->playerstate = PST_LIVE;
    player->refire = 0;
    player->message = NULL;
    player->damagecount = 0;
    player->bonuscount = 0;
    player->extralight = 0;
    player->fixedcolormap = 0;
    player->viewheight = VIEWHEIGHT;
}

static void P_SetPlayerMobj(player_t* player, const mapthing_t* mthing) {
    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    fixed_t z = ONFLOORZ;
    mobj_t* mobj = P_SpawnMobj(x, y, z, MT_PLAYER);

    int player_idx = player - players;
    if (player_idx > 0) {
        // set color translations for player sprites
        mobj->flags |= player_idx << MF_TRANSSHIFT;
    }
    mobj->angle	= ANG45 * (mthing->angle / 45);
    mobj->player = player;
    mobj->health = player->health;

    player->mo = mobj;
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
void P_SpawnPlayer(const mapthing_t* mthing) {
    int player_idx = mthing->type - 1;

    // Invalid index or not playing.
    if (player_idx == -1 || !playeringame[player_idx]) {
        return;
    }

    player_t* player = &players[player_idx];
    if (player->playerstate == PST_REBORN) {
        G_PlayerReborn(player_idx);
    }
    P_SetPlayerMobj(player, mthing);
    P_ResetPlayerData(player);
    P_SetupPsprites(player);    // setup gun psprite
    P_SetupCards(player);
    if (player_idx == consoleplayer) {
	ST_Start();    // wake up the status bar
	HU_Start();    // wake up the heads up text
    }
}

static void P_SpawnThing(const mapthing_t* mthing, mobjtype_t type) {
    fixed_t x = mthing->x << FRACBITS;
    fixed_t y = mthing->y << FRACBITS;
    fixed_t z;
    if (mobjinfo[type].flags & MF_SPAWNCEILING) {
        z = ONCEILINGZ;
    } else {
        z = ONFLOORZ;
    }

    mobj_t* mobj = P_SpawnMobj(x, y, z, type);
    mobj->spawnpoint = *mthing;
    mobj->angle = ANG45 * (mthing->angle / 45);
    if (mthing->options & MTF_AMBUSH) {
        mobj->flags |= MF_AMBUSH;
    }
    if (mobj->tics > 0) {
        mobj->tics = 1 + (P_Random() % mobj->tics);
    }

    if (mobj->flags & MF_COUNTKILL) {
        totalkills++;
    }
    if (mobj->flags & MF_COUNTITEM) {
        totalitems++;
    }
}

static bool P_IsOnGameSkillLevel(const mapthing_t* mthing) {
    // Bit 0 => Thing is on skill levels 1 & 2.
    // Bit 1 => Thing is on skill level 3.
    // Bit 2 => Thing is on skill levels 4 & 5.
    int bit;
    switch (gameskill) {
        case sk_baby:
            bit = 1;
            break;
        case sk_nightmare:
            bit = 4;
            break;
        default:
            bit = 1 << (gameskill - 1);
            break;
    }
    return (mthing->options & bit) != 0;
}

static bool P_IsOnGameMode(const mapthing_t* mthing) {
    if (netgame) {
        return true;
    }
    // If bit 4 is present, thing is not in single player.
    return (mthing->options & 16) == 0;
}

static bool P_CanSpawnThing(const mapthing_t* mthing, mobjtype_t type) {
    const mobjinfo_t* thinginfo = &mobjinfo[type];

    if (!P_IsOnGameMode(mthing)) {
        // Thing not present in selected game mode.
        return false;
    }
    if (!P_IsOnGameSkillLevel(mthing)) {
        // Thing is not present in selected skill level.
        return false;
    }
    if (deathmatch && thinginfo->flags & MF_NOTDMATCH) {
        // Don't spawn keycards and players in deathmatch.
        return false;
    }
    if (nomonsters && (type == MT_SKULL || (thinginfo->flags & MF_COUNTKILL))) {
        // Don't spawn any monsters if -nomonsters.
        return false;
    }
    return true;
}

static void P_SpawnMapPlayer(const mapthing_t* mthing) {
    // save spots for respawning in network games
    playerstarts[mthing->type - 1] = *mthing;
    playerstartsingame[mthing->type - 1] = true;
    if (!deathmatch) {
        P_SpawnPlayer(mthing);
    }
}

static void P_SpawnDeathmatchStart(const mapthing_t* mthing) {
    if (deathmatch_p < &deathmatchstarts[MAX_DM_STARTS]) {
        memcpy(deathmatch_p, mthing, sizeof(*mthing));
        deathmatch_p++;
    }
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void P_SpawnMapThing(const mapthing_t* mthing) {
    // Count deathmatch start positions.
    if (mthing->type == 11) {
        P_SpawnDeathmatchStart(mthing);
	return;
    }
    if (mthing->type <= 0) {
        // Thing type 0 is actually "player -1 start".  
        // For some reason, Vanilla Doom accepts/ignores this.
        return;
    }
    // Check for players specially.
    if (mthing->type <= 4) {
        P_SpawnMapPlayer(mthing);
	return;
    }
    mobjtype_t type = P_FindThingType(mthing);
    if (P_CanSpawnThing(mthing, type)) {
        // Spawn it.
        P_SpawnThing(mthing, type);
    }
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//
void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z) {
    z += (P_SubRandom() << 10);
    mobj_t* th = P_SpawnMobj(x, y, z, MT_PUFF);
    th->momz = FRACUNIT;
    th->tics -= P_Random() & 3;
    if (th->tics < 1) {
        th->tics = 1;
    }
    // Don't make punches spark on the wall.
    if (attackrange == MELEERANGE) {
        P_SetMobjState(th, S_PUFF3);
    }
}



//
// P_SpawnBlood
// 
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage) {
    z += (P_SubRandom() << 10);
    mobj_t* th = P_SpawnMobj(x, y, z, MT_BLOOD);
    th->momz = FRACUNIT * 2;
    th->tics -= P_Random() & 3;
    if (th->tics < 1) {
        th->tics = 1;
    }
    if (damage <= 12 && damage >= 9) {
        P_SetMobjState(th, S_BLOOD2);
    } else if (damage < 9) {
        P_SetMobjState(th, S_BLOOD3);
    }
}



//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void P_CheckMissileSpawn(mobj_t* th)
{
    th->tics -= P_Random() & 3;
    if (th->tics < 1) {
        th->tics = 1;
    }
    
    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->x += (th->momx >> 1);
    th->y += (th->momy >> 1);
    th->z += (th->momz >> 1);

    if (!P_TryMove(th, th->x, th->y)) {
        P_ExplodeMissile (th);
    }
}

//
// Certain functions assume that a mobj_t pointer is non-NULL, causing a crash
// in some situations where it is NULL. Vanilla Doom did not crash because of
// the lack of proper memory  protection. This function substitutes NULL
// pointers for pointers to a dummy mobj, to avoid a crash.
//
mobj_t* P_SubstNullMobj(mobj_t *mobj) {
    static mobj_t dummy_mobj;

    if (mobj) {
        return mobj;
    }
    dummy_mobj.x = 0;
    dummy_mobj.y = 0;
    dummy_mobj.z = 0;
    dummy_mobj.flags = 0;
    return &dummy_mobj;
}

//
// P_SpawnMissile
//
mobj_t* P_SpawnMissile(mobj_t* source, const mobj_t* dest, mobjtype_t type) {
    mobj_t* th =
        P_SpawnMobj(source->x, source->y, source->z + 4 * 8 * FRACUNIT, type);

    if (th->info->seesound) {
        S_StartSound(th, th->info->seesound);
    }

    // Where it came from.
    th->target = source;

    th->angle = R_PointToAngle2(source->x, source->y, dest->x, dest->y);
    if (dest->flags & MF_SHADOW) {
        // Fuzzy player.
        th->angle += P_SubRandom() << 20;
    }

    th->momx = FixedMul(th->info->speed, COS(th->angle));
    th->momy = FixedMul(th->info->speed, SIN(th->angle));

    int dist = P_ApproxDistance(dest->x - source->x, dest->y - source->y);
    dist = dist / th->info->speed;
    if (dist < 1) {
        dist = 1;
    }
    th->momz = (dest->z - source->z) / dist;

    P_CheckMissileSpawn(th);
    return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void P_SpawnPlayerMissile(mobj_t* source, mobjtype_t type) {
    mobj_t *th;

    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t slope;

    // see which target is to be aimed at
    angle_t an = source->angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

    if (!linetarget) {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if (!linetarget) {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }
        if (!linetarget) {
            an = source->angle;
            slope = 0;
        }
    }

    x = source->x;
    y = source->y;
    z = source->z + (4 * 8 * FRACUNIT);

    th = P_SpawnMobj(x, y, z, type);

    if (th->info->seesound) {
        S_StartSound(th, th->info->seesound);
    }

    th->target = source;
    th->angle = an;
    th->momx = FixedMul(th->info->speed, COS(th->angle));
    th->momy = FixedMul(th->info->speed, SIN(th->angle));
    th->momz = FixedMul(th->info->speed, slope);

    P_CheckMissileSpawn(th);
}
