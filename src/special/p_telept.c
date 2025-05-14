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
//	Teleportation.
//


#include "p_telept.h"

#include "doomstat.h"
#include "p_local.h"
#include "s_sound.h"


// Old position of thing before teleport, so we can spawn fog after teleporting.
static fixed_t old_x;
static fixed_t old_y;
static fixed_t old_z;


static void EV_SpawnTeleportFog(fixed_t x, fixed_t y, fixed_t z) {
    mobj_t* fog = P_SpawnMobj(x, y, z, MT_TFOG);
    S_StartSound(fog, sfx_telept);
}

static void EV_SpawnDestinationFog(const mobj_t* thing, const mobj_t* teleport)
{
    fixed_t x = teleport->x + (20 * COS(teleport->angle));
    fixed_t y = teleport->y + (20 * SIN(teleport->angle));
    fixed_t z = thing->z;
    EV_SpawnTeleportFog(x, y, z);
}

static void EV_SpawnSourceFog() {
    EV_SpawnTeleportFog(old_x, old_y, old_z);
}

//
// Spawn teleport fog at source and destination.
//
static void EV_SpawnTeleportFogs(const mobj_t* thing, const mobj_t* teleport) {
    EV_SpawnSourceFog();
    EV_SpawnDestinationFog(thing, teleport);
}

static void EV_TryTeleportThing(mobj_t* thing, const mobj_t* teleport) {
    // Store the current position of the thing before teleporting,
    // so we can spawn a teleport fog at the old location.
    old_x = thing->x;
    old_y = thing->y;
    old_z = thing->z;

    if (!P_TeleportMove(thing, teleport->x, teleport->y)) {
        // Blocked move.
        return;
    }
    if (gameversion != exe_final) {
        // The first Final Doom executable does not set thing->z
        // when teleporting. This quirk is unique to this
        // particular version; the later version included in
        // some versions of the Id Anthology fixed this.
        thing->z = thing->floorz;
    }
    if (thing->player) {
        // Adjust camera height view.
        thing->player->viewz = thing->z + thing->player->viewheight;
        // Don't move for a bit after teleport.
        thing->reactiontime = 18;
    }
    thing->angle = teleport->angle;
    thing->momx = 0;
    thing->momy = 0;
    thing->momz = 0;

    EV_SpawnTeleportFogs(thing, teleport);
}

static mobj_t* EV_FindTeleportExit(int tag, int sec_num) {
    if (sectors[sec_num].tag != tag) {
        return NULL;
    }

    for (thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next) {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker) {
            // Not a map object.
            continue;
        }
        mobj_t* mobj = (mobj_t *) th;
        if (mobj->type != MT_TELEPORTMAN) {
            // Not a teleport exit.
            continue;
        }
        const sector_t* sector = mobj->subsector->sector;
        if (sector - sectors != sec_num) {
            // Not in the selected sector.
            continue;
        }
        return mobj;
    }

    return NULL;
}

static bool EV_CanTeleportThing(int side, const mobj_t* thing) {
    if (thing->flags & MF_MISSILE) {
        // Don't teleport missiles.
        return false;
    }
    if (side == 1) {
        // Don't teleport if hit back of line,
        // so you can get out of teleporter.
        return false;
    }
    return true;
}

//
// TELEPORTATION
//
void EV_Teleport(const line_t* line, int side, mobj_t* thing) {
    if (!EV_CanTeleportThing(side, thing)) {
        return;
    }

    for (int sec = 0; sec < numsectors; sec++) {
        const mobj_t* teleport = EV_FindTeleportExit(line->tag, sec);
        if (teleport) {
            EV_TryTeleportThing(thing, teleport);
            break;
        }
    }
}
