//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard, Andrey Budko
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
//	Movement, collision handling.
//	Shooting and aiming.
//

#include <stdio.h>
#include <stdlib.h>

#include "deh_misc.h"

#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"
// Data.
#include "sounds.h"

// Spechit overrun magic value.
//
// This is the value used by PrBoom-plus.  I think the value below is 
// actually better and works with more demos.  However, I think
// it's better for the spechits emulation to be compatible with
// PrBoom-plus, at least so that the big spechits emulation list
// on Doomworld can also be used with Broom.

#define DEFAULT_SPECHIT_MAGIC 0x01C09C98

// This is from a post by myk on the Doomworld forums, 
// outputted from entryway's spechit_magic generator for
// s205n546.lmp.  The _exact_ value of this isn't too
// important; as long as it is in the right general
// range, it will usually work.  Otherwise, we can use
// the generator (hacked doom2.exe) and provide it 
// with -spechit.

//#define DEFAULT_SPECHIT_MAGIC 0x84f968e8


static fixed_t tmbbox[4];
static mobj_t *tmthing;
static int tmflags;
static fixed_t tmx;
static fixed_t tmy;


// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
bool floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
static fixed_t tmdropoffz;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t *ceilingline;

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

line_t *spechit[MAXSPECIALCROSS];
int numspechit;


//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
static bool PIT_StompThing(mobj_t* thing) {
    if (!(thing->flags & MF_SHOOTABLE)) {
        return true;
    }
    fixed_t blockdist = thing->radius + tmthing->radius;
    if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist) {
        // Didn't hit it.
        return true;
    }
    if (thing == tmthing) {
        // Don't clip against self.
        return true;
    }
    if (!tmthing->player && gamemap != 30) {
        // Monsters don't stomp things except on boss level.
        return false;
    }
    P_DamageMobj(thing, tmthing, tmthing, 10000);
    return true;
}


//
// P_TeleportMove
//
bool P_TeleportMove(mobj_t* thing, fixed_t x, fixed_t y) {
    // Kill anything occupying the position.
    tmthing = thing;
    tmflags = thing->flags;
    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    const subsector_t* newsubsec = R_PointInSubsector(x, y);
    ceilingline = NULL;

    // The base floor/ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    tmceilingz = newsubsec->sector->ceilingheight;
    tmfloorz = newsubsec->sector->floorheight;
    tmdropoffz = tmfloorz;

    validcount++;
    numspechit = 0;

    // Stomp on any things contacted.
    int xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    int xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    int yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    int yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            bool can_move = P_BlockThingsIterator(bx, by, PIT_StompThing);
            if (!can_move) {
                return false;
            }
        }
    }

    // The move is ok, so link the thing into its new position.
    P_UnsetThingPosition(thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    return true;
}


//
// MOVEMENT ITERATOR FUNCTIONS
//

static void SpechitOverrun(line_t *ld);

//
// Add special line to list to keep track of it to process later if the
// move is proven ok.
// NOTE: specials are NOT sorted by order, so two special lines that are
// only 8 pixels apart could be crossed in either order.
//
static void PIT_AddLineToSpecialList(line_t* ld) {
    spechit[numspechit] = ld;
    numspechit++;

    // fraggle: spechits overrun emulation code from prboom-plus
    if (numspechit > MAXSPECIALCROSS_ORIGINAL) {
        SpechitOverrun(ld);
    }
}

static void PIT_AdjustFloorCeilingHeights(line_t* ld) {
    P_LineOpening(ld);
    if (opentop < tmceilingz) {
        tmceilingz = opentop;
        ceilingline = ld;
    }
    if (openbottom > tmfloorz) {
        tmfloorz = openbottom;
    }
    if (lowfloor < tmdropoffz) {
        tmdropoffz = lowfloor;
    }
}

static bool PIT_IsLineBlocking(const line_t* ld) {
    if (!ld->backsector) {
        // One-sided lines always block move.
        return true;
    }
    if (tmthing->flags & MF_MISSILE) {
        // Do not block projectiles, as they may pass over/under the line.
        return false;
    }
    if (ld->flags & ML_BLOCKING) {
        // Explicitly blocking everything.
        return true;
    }
    if (!tmthing->player && ld->flags & ML_BLOCKMONSTERS) {
        // Block monsters only.
        return true;
    }
    return false;
}

static bool PIT_IsLineHit(const line_t* ld) {
    return tmbbox[BOXRIGHT] > ld->bbox[BOXLEFT]
           && tmbbox[BOXLEFT] < ld->bbox[BOXRIGHT]
           && tmbbox[BOXTOP] > ld->bbox[BOXBOTTOM]
           && tmbbox[BOXBOTTOM] < ld->bbox[BOXTOP]
           && P_BoxOnLineSide(tmbbox, ld) == -1;
}

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
// Returns true if not blocked by line, otherwise returns false.
//
static bool PIT_CheckLine(line_t* line) {
    if (!PIT_IsLineHit(line)) {
        // Did not hit line, so keep moving.
        return true;
    }
    // The moving thing's destination position will cross the given line.
    if (PIT_IsLineBlocking(line)) {
        // Move not allowed.
        return false;
    }
    PIT_AdjustFloorCeilingHeights(line);
    if (line->special) {
        // If contacted a special line, add it to the list.
        PIT_AddLineToSpecialList(line);
    }
    return true;
}

static bool P_IsDropped(const mobj_t* thing) {
    return (thing->flags & MF_DROPPED) != 0;
}

static bool P_IsShootable(const mobj_t* thing) {
    return (thing->flags & MF_SHOOTABLE) != 0;
}

static bool P_IsSpecial(const mobj_t* thing) {
    return (thing->flags & MF_SPECIAL) != 0;
}

static bool P_IsSolid(const mobj_t* thing) {
    return (thing->flags & MF_SOLID) != 0;
}


static bool PIT_CheckSpecial(mobj_t* thing) {
    if (tmflags & MF_PICKUP) {
        // Can remove thing.
        // Vanilla Bug: items are picked up without checking if the move
        // is valid. This can lead to situations where items are picked up
        // even if the level geometry would make this impossible.
        // More info on this bug here:
        // https://www.doomworld.com/forum/topic/87199-the-doom-movement-bible/?tab=comments#comment-1587751
        P_TouchSpecialThing(thing, tmthing);
    }
    return !P_IsSolid(thing);
}


static bool PIT_IsSameSpecies(const mobj_t* thing, const mobj_t* target) {
    mobjtype_t type = thing->type;
    mobjtype_t targettype = target->type;

    return targettype == type
        || (targettype == MT_KNIGHT && type == MT_BRUISER)
        || (targettype == MT_BRUISER && type == MT_KNIGHT);
}

static bool PIT_IsMonsterInFighting(const mobj_t* thing) {
    if (deh_species_infighting) {
        // override "monsters of the same species cant hurt each other"
        // behavior through dehacked patches.
        return false;
    }
    return thing->type != MT_PLAYER
           && tmthing->target
           && PIT_IsSameSpecies(thing, tmthing->target);
}

static bool PIT_ProjectileHitThing(const mobj_t* thing) {
    // see if it went over / under
    if (thing->z + thing->height < tmthing->z) {
        // overhead
        return false;
    }
    if (tmthing->z + tmthing->height < thing->z) {
        // underneath
        return false;
    }
    if (thing == tmthing->target) {
        // Don't self inflict damage.
        return false;
    }
    return true;
}

//
// Returns true if the projectile can keep moving, false otherwise.
//
static bool PIT_CheckProjectile(mobj_t* thing) {
    if (!PIT_ProjectileHitThing(thing)) {
        return true;
    }
    if (PIT_IsMonsterInFighting(thing)) {
        // explode, but do no damage.
        return false;
    }
    if (P_IsShootable(thing)) {
        // damage / explode
        int damage = ((P_Random() % 8) + 1) * tmthing->info->damage;
        P_DamageMobj(thing, tmthing, tmthing->target, damage);
        // don't traverse anymore
        return false;
    }
    // Explode if solid, but do no damage.
    return !P_IsSolid(thing);
}



static bool PIT_CheckSkull(mobj_t* thing) {
    // Vanilla Bug: this code does not check neither the height nor the
    // type of the thing being collided with. Thus, if the lost soul collide
    // with any item that may be picked up by a player, then it will stop and
    // start moving normally again as if nothing had happened.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Lost_soul_colliding_with_items
    int damage = ((P_Random() % 8) + 1) * tmthing->info->damage;
    P_DamageMobj(thing, tmthing, tmthing, damage);

    tmthing->flags &= ~MF_SKULLFLY;
    tmthing->momx = 0;
    tmthing->momy = 0;
    tmthing->momz = 0;

    P_SetMobjState(tmthing, tmthing->info->spawnstate);

    // Stop moving.
    return false;
}


static bool PIT_IsBlockingMove(const mobj_t* thing) {
    if (!P_IsSolid(thing) && !P_IsSpecial(thing) && !P_IsShootable(thing)) {
        return false;
    }
    fixed_t blockdist = thing->radius + tmthing->radius;
    if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist) {
        // Not in range: didn't hit it.
        return false;
    }
    if (thing == tmthing) {
        // Don't clip against self.
        return false;
    }
    return true;
}

//
// PIT_CheckThing
// Returns true if thing can keep moving, false otherwise.
//
static bool PIT_CheckThing(mobj_t* thing) {
    if (!PIT_IsBlockingMove(thing)) {
        return true;
    }
    // Check for skulls slamming into things.
    if (tmthing->flags & MF_SKULLFLY) {
        return PIT_CheckSkull(thing);
    }
    // Projectiles can hit other things.
    if (tmthing->flags & MF_MISSILE) {
        return PIT_CheckProjectile(thing);
    }
    // Check for special pickup.
    if (thing->flags & MF_SPECIAL) {
        return PIT_CheckSpecial(thing);
    }
    return !P_IsSolid(thing);
}


//
// MOVEMENT CLIPPING
//

static bool P_CanFloat(const mobj_t* thing) {
    return (thing->flags & MF_FLOAT) != 0;
}

static bool P_CanFallFromAnyHeight(const mobj_t* thing) {
    return (thing->flags & MF_DROPOFF) != 0;
}

static bool P_CanCrossLine(const mobj_t* thing) {
    return (thing->flags & MF_TELEPORT) == 0;
}

static bool P_CollisionEnabled(const mobj_t* thing) {
    return (thing->flags & MF_NOCLIP) == 0;
}

static bool P_IsBlockedByLines() {
    int xl = (tmbbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
    int xh = (tmbbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
    int yl = (tmbbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
    int yh = (tmbbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            bool can_move = P_BlockLinesIterator(bx, by, PIT_CheckLine);
            if (!can_move) {
                return true;
            }
        }
    }

    return false;
}

static bool P_IsBlockedByThings() {
    // The bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into
    // adjacent blocks by up to MAXRADIUS units.
    int xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    int xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    int yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    int yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            bool can_move = P_BlockThingsIterator(bx, by, PIT_CheckThing);
            if (!can_move) {
                return true;
            }
        }
    }

    return false;
}

static void P_SetupBoundingBoxHeight(fixed_t x, fixed_t y) {
    const subsector_t* newsubsec = R_PointInSubsector(x, y);
    ceilingline = NULL;

    // The base floor / ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    tmdropoffz = newsubsec->sector->floorheight;
    tmfloorz = tmdropoffz;
    tmceilingz = newsubsec->sector->ceilingheight;
}

static void P_SetupBoundingBox(fixed_t x, fixed_t y) {
    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;
    P_SetupBoundingBoxHeight(x, y);
}

static void P_SetupCollisionCheck(mobj_t* thing, fixed_t x, fixed_t y) {
    tmthing = thing;
    tmflags = thing->flags;
    tmx = x;
    tmy = y;
    P_SetupBoundingBox(x, y);
    validcount++;
    numspechit = 0;
}

//
// P_CheckPosition
// Returns true if "thing" can move to (x, y), false otherwise.
// This is purely informative, nothing is modified (except things picked up).
// 
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//  the lowest point contacted (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//
bool P_CheckPosition(mobj_t* thing, fixed_t x, fixed_t y) {
    P_SetupCollisionCheck(thing, x, y);
    if (P_CollisionEnabled(thing)) {
        // Check things first, possibly picking things up.
        return !P_IsBlockedByThings() && !P_IsBlockedByLines();
    }
    return true;
}

static void P_DoSpecialLineEffects(mobj_t* thing, fixed_t oldx, fixed_t oldy) {
    while (numspechit--) {
        // see if the line was crossed
        const line_t* line = spechit[numspechit];
        int side = P_PointOnLineSide(thing->x, thing->y, line);
        int old_side = P_PointOnLineSide(oldx, oldy, line);
        if (side != old_side && line->special) {
            P_CrossSpecialLine(line - lines, old_side, thing);
        }
    }
}

static bool P_HasTriggeredSpecialLines(const mobj_t* thing) {
    return P_CanCrossLine(thing) && P_CollisionEnabled(thing) && numspechit > 0;
}

static void P_UpdateThingPosition(mobj_t* thing, fixed_t x, fixed_t y) {
    P_UnsetThingPosition(thing);
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;
    P_SetThingPosition(thing);
}

#define MAX_STEP_UP  (24*FRACUNIT)
#define MAX_DROP_OFF (24*FRACUNIT)

static bool P_CanFallFromHeight(const mobj_t* thing) {
    return P_CanFallFromAnyHeight(thing)
           || (tmfloorz - tmdropoffz) <= MAX_DROP_OFF;
}

//
// Returns true if height difference is not an obstacle
// to "thing" movement, otherwise returns false.
//
static bool P_CheckHeight(const mobj_t* thing) {
    if (!P_CollisionEnabled(thing)) {
        return true;
    }

    floatok = tmceilingz >= tmfloorz + thing->height;
    if (!floatok) {
        // Doesn't fit.
        return false;
    }

    if (P_CanCrossLine(thing)) {
        if (tmceilingz < thing->z + thing->height) {
            // mobj must lower itself to fit.
            return false;
        }
        if (tmfloorz > thing->z + MAX_STEP_UP) {
            // Too big a step-up.
            return false;
        }
    }

    return P_CanFallFromHeight(thing) || P_CanFloat(thing);
}

//
// Return true if move is valid, false otherwise.
// A move is valid if:
// 1. It is not blocked by a solid wall or another object (thing).
// 2. The height difference caused by the move is within acceptable limits.
//
static bool P_CheckMove(mobj_t* thing, fixed_t x, fixed_t y) {
    // floatok is false by default, but it can be set true by P_CheckHeight
    floatok = false;
    return P_CheckPosition(thing, x, y) && P_CheckHeight(thing);
}


//
// P_TryMove
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT is set.
//
bool P_TryMove(mobj_t* thing, fixed_t x, fixed_t y) {
    if (!P_CheckMove(thing, x, y)) {
        // Cannot move.
        return false;
    }
    fixed_t oldx = thing->x;
    fixed_t oldy = thing->y;
    P_UpdateThingPosition(thing, x, y);
    if (P_HasTriggeredSpecialLines(thing)) {
        P_DoSpecialLineEffects(thing, oldx, oldy);
    }
    return true;
}


//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
// and possibly thing->z. This is called for all nearby monsters whenever a
// sector changes height. If the thing doesn't fit, the z will be set to the
// lowest value and false will be returned.
//
static bool P_ThingHeightClip(mobj_t* thing) {
    bool onfloor = (thing->z == thing->floorz);

    P_CheckPosition(thing, thing->x, thing->y);

    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    if (onfloor) {
        // walking monsters rise and fall with the floor
        thing->z = thing->floorz;
    } else if (thing->z + thing->height > thing->ceilingz) {
        // don't adjust a floating monster unless forced to
        thing->z = thing->ceilingz - thing->height;
    }

    return thing->ceilingz - thing->floorz >= thing->height;
}


//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
static fixed_t bestslidefrac;
static line_t* bestslideline;
static mobj_t* slidemo;
static fixed_t tmxmove;
static fixed_t tmymove;


//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
static void P_HitSlideLine(const line_t* ld) {
    if (ld->slopetype == ST_HORIZONTAL) {
        tmymove = 0;
        return;
    }
    if (ld->slopetype == ST_VERTICAL) {
        tmxmove = 0;
        return;
    }

    int side = P_PointOnLineSide(slidemo->x, slidemo->y, ld);
    angle_t lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);
    if (side == 1) {
        lineangle += ANG180;
    }

    angle_t moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    angle_t deltaangle = moveangle - lineangle;
    if (deltaangle > ANG180) {
        deltaangle += ANG180;
//        I_Error ("SlideLine: ang>ANG180");
    }

    fixed_t movelen = P_ApproxDistance(tmxmove, tmymove);
    fixed_t newlen = FixedMul(movelen, COS(deltaangle));

    tmxmove = FixedMul(newlen, COS(lineangle));
    tmymove = FixedMul(newlen, SIN(lineangle));
}


//
// PTR_SlideTraverse
//
static bool PTR_SlideTraverse(intercept_t* in) {
    if (!in->isaline) {
        I_Error("PTR_SlideTraverse: not a line?");
    }

    line_t* li = in->d.line;

    if (!(li->flags & ML_TWOSIDED)) {
        if (P_PointOnLineSide(slidemo->x, slidemo->y, li)) {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // Set openrange, opentop, openbottom.
    P_LineOpening(li);

    if (openrange < slidemo->height) {
        // Doesn't fit.
        goto isblocking;
    }
    if (opentop - slidemo->z < slidemo->height) {
        // mobj is too high.
        goto isblocking;
    }
    if (openbottom - slidemo->z > 24 * FRACUNIT) {
        // Too big a step up.
        goto isblocking;
    }

    // This line doesn't block movement.
    return true;

isblocking:
    // The line does block movement, see if it is closer than best so far.
    if (in->frac < bestslidefrac) {
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    // Stop.
    return false;
}


//
// P_SlideMove
// The momx / momy move is bad, so try to slide along a wall.
// Find the first line hit, move flush to it, and slide along it.
//
// This is a kludgy mess.
//
void P_SlideMove(mobj_t* mo) {
    fixed_t leadx;
    fixed_t leady;
    fixed_t trailx;
    fixed_t traily;
    fixed_t newx;
    fixed_t newy;
    int hitcount;

    slidemo = mo;
    hitcount = 0;

retry:
    if (++hitcount == 3) {
        // don't loop forever
        goto stairstep;
    }


    // trace along the three leading corners
    if (mo->momx > 0) {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    } else {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }
    if (mo->momy > 0) {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    } else {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    bestslidefrac = FRACUNIT + 1;

    P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailx, leady, trailx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadx, traily, leadx + mo->momx, traily + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);

    // move up to the wall
    if (bestslidefrac == FRACUNIT + 1) {
        // the move most have hit the middle, so stairstep
    stairstep:
        if (!P_TryMove(mo, mo->x, mo->y + mo->momy)) {
            P_TryMove(mo, mo->x + mo->momx, mo->y);
        }
        return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if (bestslidefrac > 0) {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        if (!P_TryMove(mo, mo->x + newx, mo->y + newy)) {
            goto stairstep;
        }
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

    if (bestslidefrac > FRACUNIT) {
        bestslidefrac = FRACUNIT;
    }

    if (bestslidefrac <= 0) {
        return;
    }

    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    P_HitSlideLine(bestslideline); // clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;

    if (!P_TryMove(mo, mo->x + tmxmove, mo->y + tmymove)) {
        goto retry;
    }
}


//
// P_LineAttack
//
mobj_t* linetarget; // who got hit (or NULL)
mobj_t* shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t shootz;

int la_damage;
fixed_t attackrange;

fixed_t aimslope;

//
// Check if shot hit a thing.
// Returns true if shot hit thing, false otherwise.
//
static bool PTR_CheckShotHitThing(const intercept_t* in) {
    mobj_t* th = in->d.thing;

    if (th == shootthing) {
        // can't shoot self
        return false;
    }
    if (!(th->flags & MF_SHOOTABLE)) {
        // corpse or something
        return false;
    }

    // check angles to see if the thing can be aimed at
    fixed_t dist = FixedMul(attackrange, in->frac);

    fixed_t thingtopslope = FixedDiv(th->z + th->height - shootz, dist);
    if (thingtopslope < bottomslope) {
        // shot over the thing
        return false;
    }

    fixed_t thingbottomslope = FixedDiv(th->z - shootz, dist);
    if (thingbottomslope > topslope) {
        // shot under the thing
        return false;
    }

    // this thing can be hit!
    if (thingtopslope > topslope) {
        thingtopslope = topslope;
    }
    if (thingbottomslope < bottomslope) {
        thingbottomslope = bottomslope;
    }
    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;
    return true;
}


static bool PTR_IsTwoSided(const line_t* li) {
    return (li->flags & ML_TWOSIDED) != 0;
}

//
// Crosses a two sided line.
// A two sided line will restrict the possible target ranges.
//
static bool PTR_CheckShotHitTwoSidedLine(const intercept_t* in,
                                            const line_t* li)
{
    const sector_t* backsector = li->backsector;
    const sector_t* frontsector = li->frontsector;

    P_LineOpening(li);

    if (openbottom >= opentop) {
        // No opening, so shot hit line
        return true;
    }

    fixed_t dist = FixedMul(attackrange, in->frac);
    if (backsector == NULL ||
        frontsector->floorheight != backsector->floorheight)
    {
        fixed_t slope = FixedDiv(openbottom - shootz, dist);
        if (slope > bottomslope) {
            bottomslope = slope;
        }
    }
    if (backsector == NULL ||
        frontsector->ceilingheight != backsector->ceilingheight)
    {
        fixed_t slope = FixedDiv(opentop - shootz, dist);
        if (slope < topslope) {
            topslope = slope;
        }
    }

    // If no opening, shot hit line
    return topslope <= bottomslope;
}

static bool PTR_CheckShotHitLine(const intercept_t* in) {
    const line_t* li = in->d.line;

    if (PTR_IsTwoSided(li)) {
        return PTR_CheckShotHitTwoSidedLine(in, li);
    }
    // Shot always hit a solid wall
    return true;
}

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
// Returns true if shot did not hit anything, false otherwise.
//
static bool PTR_AimTraverse(intercept_t* in) {
    if (in->isaline) {
        return !PTR_CheckShotHitLine(in);
    }
    return !PTR_CheckShotHitThing(in);
}


//
// Spawn bullet puffs or blood spots, depending on target type.
//
static void PTR_SpawnThingHitEffect(const intercept_t* in) {
    // Position a bit closer.
    fixed_t frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

    fixed_t x = trace.x + FixedMul(trace.dx, frac);
    fixed_t y = trace.y + FixedMul(trace.dy, frac);
    fixed_t z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

    if (in->d.thing->flags & MF_NOBLOOD) {
        P_SpawnPuff(x, y, z);
        return;
    }
    P_SpawnBlood(x, y, z, la_damage);
}

static void PTR_ShootThing(const intercept_t* in) {
    PTR_SpawnThingHitEffect(in);
    if (la_damage) {
        mobj_t* th = in->d.thing;
        P_DamageMobj(th, shootthing, shootthing, la_damage);
    }
}

static bool PTR_HasHitThing(const intercept_t* in) {
    const mobj_t* th = in->d.thing;

    if (th == shootthing) {
        // Can't shoot self.
        return false;
    }
    if (!P_IsShootable(th)) {
        // Corpse or something.
        return false;
    }

    // Check angles to see if the thing can be aimed at.
    fixed_t dist = FixedMul(attackrange, in->frac);
    fixed_t thing_top_slope = FixedDiv(th->z + th->height - shootz, dist);
    if (thing_top_slope < aimslope) {
        // Shot over the thing
        return false;
    }
    fixed_t thing_bottom_slope = FixedDiv(th->z - shootz, dist);
    if (thing_bottom_slope > aimslope) {
        // Shot under the thing.
        return false;
    }

    return true;
}

static bool PTR_TryHitThing(const intercept_t* in) {
    if (PTR_HasHitThing(in)) {
        PTR_ShootThing(in);
        // Shot hit thing, so don't go any farther.
        return false;
    }
    // Shot continues.
    return true;
}

static void PTR_TrySpawnPuff(const intercept_t* in) {
    const line_t* line = in->d.line;
    const sector_t* back_sector = line->backsector;
    const sector_t* front_sector = line->frontsector;

    // Position a bit closer.
    fixed_t frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
    fixed_t x = trace.x + FixedMul(trace.dx, frac);
    fixed_t y = trace.y + FixedMul(trace.dy, frac);
    fixed_t z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

    if (front_sector->ceilingpic == sky_flat) {
        if (z > front_sector->ceilingheight) {
            // Don't shoot the sky!
            return;
        }
        if (back_sector && back_sector->ceilingpic == sky_flat) {
            // It's a sky hack wall.
            return;
        }
    }

    // Spawn bullet puffs.
    P_SpawnPuff(x, y, z);
}

static bool PTR_HasHitTwoSidedLine(const intercept_t* in) {
    const line_t* li = in->d.line;
    P_LineOpening(li);
    fixed_t dist = FixedMul(attackrange, in->frac);
    const sector_t* back_sector = li->backsector;
    const sector_t* front_sector = li->frontsector;
    fixed_t slope;

    // Emulation of missed back side on two-sided lines.
    // backsector can be NULL when emulating missing back side.
    if (back_sector == NULL) {
        slope = FixedDiv(openbottom - shootz, dist);
        if (slope > aimslope) {
            return true;
        }
        slope = FixedDiv(opentop - shootz, dist);
        if (slope < aimslope) {
            return true;
        }
        return false;
    }
    if (front_sector->floorheight != back_sector->floorheight) {
        slope = FixedDiv(openbottom - shootz, dist);
        if (slope > aimslope) {
            return true;
        }
    }
    if (front_sector->ceilingheight != back_sector->ceilingheight) {
        slope = FixedDiv(opentop - shootz, dist);
        if (slope < aimslope) {
            return true;
        }
    }
    return false;
}

static bool PTR_HasHitLine(const intercept_t* in) {
    if (PTR_IsTwoSided(in->d.line)) {
        // Crosses a two-sided line.
        return PTR_HasHitTwoSidedLine(in);
    }
    // Shot always hit solid walls.
    return true;
}

static bool PTR_TryHitLine(intercept_t* in) {
    line_t* line = in->d.line;
    if (line->special) {
        P_ShootSpecialLine(shootthing, line);
    }
    if (PTR_HasHitLine(in)) {
        PTR_TrySpawnPuff(in);
        // Shot hit line, so don't go any farther.
        return false;
    }
    // Shot continues.
    return true;
}

//
// PTR_ShootTraverse
//
static bool PTR_ShootTraverse(intercept_t* in) {
    if (in->isaline) {
        return PTR_TryHitLine(in);
    }
    return PTR_TryHitThing(in);
}


//
// P_AimLineAttack
//
fixed_t P_AimLineAttack(mobj_t* t1, angle_t angle, fixed_t distance) {
    t1 = P_SubstNullMobj(t1);

    shootthing = t1;

    fixed_t x2 = t1->x + (distance >> FRACBITS) * COS(angle);
    fixed_t y2 = t1->y + (distance >> FRACBITS) * SIN(angle);
    shootz = t1->z + (t1->height >> 1) + (8 * FRACUNIT);

    // can't shoot outside view angles
    topslope = (SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);
    bottomslope = -(SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_AimTraverse);

    if (linetarget) {
        return aimslope;
    }
    return 0;
}


//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage)
{
    shootthing = t1;
    la_damage = damage;
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
    attackrange = distance;
    aimslope = slope;

    fixed_t x1 = t1->x;
    fixed_t y1 = t1->y;
    fixed_t x2 = x1 + (distance >> FRACBITS) * COS(angle);
    fixed_t y2 = y1 + (distance >> FRACBITS) * SIN(angle);
    int flags = PT_ADDLINES | PT_ADDTHINGS;

    P_PathTraverse(x1, y1, x2, y2, flags, PTR_ShootTraverse);
}


//
// USE LINES
//
static mobj_t* usething;

static bool PTR_UseTraverse(intercept_t* in) {
    line_t* line = in->d.line;
    if (line->special) {
        int side = P_PointOnLineSide(usething->x, usething->y, line);
        P_UseSpecialLine(usething, line, side);
        // Can't use for than one special line in a row.
        return false;
    }
    P_LineOpening(line);
    if (openrange <= 0) {
        // Do OOF!
        S_StartSound(usething, sfx_noway);
        // Can't use through a wall.
        return false;
    }
    // Not a special line, but keep checking.
    return true;
}


//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void P_UseLines(player_t* player) {
    fixed_t x1 = player->mo->x;
    fixed_t y1 = player->mo->y;

    fixed_t x_projection = (USERANGE >> FRACBITS) * COS(player->mo->angle);
    fixed_t y_projection = (USERANGE >> FRACBITS) * SIN(player->mo->angle);
    fixed_t x2 = x1 + x_projection;
    fixed_t y2 = y1 + y_projection;

    usething = player->mo;
    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}


//
// RADIUS ATTACK
//
mobj_t* bombsource;
mobj_t* bombspot;
int bombdamage;

static int PIT_GetBombDistance(const mobj_t* thing) {
    fixed_t dx = abs(thing->x - bombspot->x);
    fixed_t dy = abs(thing->y - bombspot->y);
    fixed_t dist = dx > dy ? dx : dy;
    dist = (dist - thing->radius) >> FRACBITS;
    if (dist < 0) {
        dist = 0;
    }
    return dist;
}

static bool PIT_IsInExplosionRadius(const mobj_t* thing) {
    int dist = PIT_GetBombDistance(thing);
    return dist < bombdamage;
}

static bool PIT_IsDamagedByBomb(mobj_t* thing) {
    if (!P_IsShootable(thing)) {
        return false;
    }
    if (thing->type == MT_CYBORG || thing->type == MT_SPIDER) {
        // Boss spider and cyborg take no damage from concussion.
        return false;
    }
    if (!PIT_IsInExplosionRadius(thing)) {
        return false;
    }
    return P_CheckSight(thing, bombspot);
}


//
// PIT_RadiusAttack
// "bombsource" is the creature that caused the explosion at "bombspot".
//
static bool PIT_RadiusAttack(mobj_t* thing) {
    if (PIT_IsDamagedByBomb(thing)) {
        int dist = PIT_GetBombDistance(thing);
        P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }
    return true;
}


//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage) {
    fixed_t dist = (damage + MAXRADIUS) << FRACBITS;
    int yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
    int yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
    int xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
    int xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;

    bombspot = spot;
    bombsource = source;
    bombdamage = damage;

    for (int y = yl; y <= yh; y++) {
        for (int x = xl; x <= xh; x++) {
            P_BlockThingsIterator(x, y, PIT_RadiusAttack);
        }
    }
}


bool		crushchange;
bool		nofit;

static void PIT_CrushThing(mobj_t* thing) {
    P_DamageMobj(thing, NULL, NULL, 10);

    // spray blood in a random direction
    fixed_t x = thing->x;
    fixed_t y = thing->y;
    fixed_t z = thing->z + thing->height / 2;
    mobj_t* mo = P_SpawnMobj(x, y, z, MT_BLOOD);

    mo->momx = P_SubRandom() << 12;
    mo->momy = P_SubRandom() << 12;
}

//
// Crunch bodies to giblets.
//
static void PIT_CrunchToGiblets(mobj_t* thing) {
    P_SetMobjState(thing, S_GIBS);
    if (gameversion > exe_doom_1_2) {
        thing->flags &= ~MF_SOLID;
    }
    thing->height = 0;
    thing->radius = 0;
}

static void PIT_TryCrushThing(mobj_t* thing) {
    if (P_ThingHeightClip(thing)) {
        // thing fits in sector height, no crush applied.
        return;
    }
    if (thing->health <= 0) {
        PIT_CrunchToGiblets(thing);
        return;
    }
    if (P_IsDropped(thing)) {
        // crunch dropped items.
        P_RemoveMobj(thing);
        return;
    }
    if (!P_IsShootable(thing)) {
        // assume it is bloody gibs or something, no crush applied.
        return;
    }

    nofit = true;

    if (crushchange && (leveltime % 4 == 0)) {
        PIT_CrushThing(thing);
    }
}

//
// PIT_ChangeSector
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height, call this routine to
// adjust the positions of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage as they are being crushed.
// If crunch is false, you should set the sector height back the way it was and
// call P_ChangeSector again  to undo the changes.
//
static bool PIT_ChangeSector(mobj_t* thing) {
    PIT_TryCrushThing(thing);
    // keep checking (crush other things)
    return true;
}


//
// P_ChangeSector
//
bool P_ChangeSector(const sector_t* sector, bool crunch) {
    nofit = false;
    crushchange = crunch;

    int x_min = sector->blockbox[BOXLEFT];
    int x_max = sector->blockbox[BOXRIGHT];
    int y_min = sector->blockbox[BOXBOTTOM];
    int y_max = sector->blockbox[BOXTOP];

    // re-check heights for all things near the moving sector
    for (int x = x_min; x <= x_max; x++) {
        for (int y = y_min; y <= y_max; y++) {
            P_BlockThingsIterator(x, y, PIT_ChangeSector);
        }
    }

    return nofit;
}

// Code to emulate the behavior of Vanilla Doom when encountering an overrun
// of the spechit array.  This is by Andrey Budko (e6y) and comes from his
// PrBoom plus port.  A big thanks to Andrey for this.

static void SpechitOverrun(line_t *ld)
{
    static unsigned int baseaddr = 0;
    unsigned int addr;
   
    if (baseaddr == 0)
    {
        int p;

        // This is the first time we have had an overrun.  Work out
        // what base address we are going to use.
        // Allow a spechit value to be specified on the command line.

        //!
        // @category compat
        // @arg <n>
        //
        // Use the specified magic value when emulating spechit overruns.
        //

        p = M_CheckParmWithArgs("-spechit", 1);
        
        if (p > 0)
        {
            M_StrToInt(myargv[p+1], (int *) &baseaddr);
        }
        else
        {
            baseaddr = DEFAULT_SPECHIT_MAGIC;
        }
    }
    
    // Calculate address used in doom2.exe

    addr = baseaddr + (ld - lines) * 0x3E;

    switch(numspechit)
    {
        case 9: 
        case 10:
        case 11:
        case 12:
            tmbbox[numspechit-9] = addr;
            break;
        case 13: 
            crushchange = addr; 
            break;
        case 14: 
            nofit = addr; 
            break;
        default:
            fprintf(stderr, "SpechitOverrun: Warning: unable to emulate"
                            "an overrun where numspechit=%i\n",
                            numspechit);
            break;
    }
}

