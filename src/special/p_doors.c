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
// DESCRIPTION: Door animation code (opening/closing)
//


#include "p_doors.h"

#include "z_zone.h"
#include "doomdef.h"
#include "deh_str.h"
#include "p_local.h"
#include "i_system.h"
#include "p_plats.h"
#include "p_floor.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"
#include "sounds.h"



//
// VERTICAL DOORS
//

static result_e T_MoveDoorUp(vldoor_t* door) {
    sector_t* sector = door->sector;
    fixed_t speed = door->speed;
    fixed_t dest = door->topheight;
    bool crush = false;
    int floorOrCeiling = 1;
    int direction = door->direction;
    return T_MovePlane(sector, speed, dest, crush, floorOrCeiling, direction);
}

static result_e T_MoveDoorDown(vldoor_t* door) {
    sector_t* sector = door->sector;
    fixed_t speed = door->speed;
    fixed_t dest = sector->floorheight;
    bool crush = false;
    int floorOrCeiling = 1;
    int direction = door->direction;
    return T_MovePlane(sector, speed, dest, crush, floorOrCeiling, direction);
}

static void T_VerticalInitialWaitDoor(vldoor_t* door) {
    door->topcountdown--;
    if (door->topcountdown) {
        // Not ready to change direction.
        return;
    }
    if (door->type == vld_raiseIn5Mins) {
        door->direction = 1;
        door->type = vld_normal;
        S_StartSound(&door->sector->soundorg, sfx_doropn);
    }
}

static void T_VerticalUpDoor(vldoor_t* door) {
    result_e res = T_MoveDoorUp(door);
    if (res != pastdest) {
        return;
    }
    switch (door->type) {
        case vld_blazeRaise:
        case vld_normal:
            // Wait at top.
            door->direction = 0;
            door->topcountdown = door->topwait;
            break;
        case vld_close30ThenOpen:
        case vld_blazeOpen:
        case vld_open:
            door->sector->specialdata = NULL;
            P_RemoveThinker(&door->thinker); // unlink and free
            break;
        default:
            break;
    }
}

static void T_VerticalWaitingDoor(vldoor_t* door) {
    door->topcountdown--;
    if (door->topcountdown) {
        // Not ready to change direction.
        return;
    }
    switch (door->type) {
        case vld_blazeRaise:
            // Time to go back down.
            door->direction = -1;
            S_StartSound(&door->sector->soundorg, sfx_bdcls);
            break;
        case vld_normal:
            // Time to go back down.
            door->direction = -1;
            S_StartSound(&door->sector->soundorg, sfx_dorcls);
            break;
        case vld_close30ThenOpen:
            door->direction = 1;
            S_StartSound(&door->sector->soundorg, sfx_doropn);
            break;
        default:
            break;
    }
}

static void T_DoorHitObject(vldoor_t* door) {
    switch(door->type) {
        case vld_blazeClose:
        case vld_close:
            // Do not go back up!
            break;
        default:
            door->direction = 1;
            // Vanilla Bug: there is no test to check the type of the door, so
            // fast doors incorrectly use the normal door-opening sound.
            // More info on this bug here:
            // https://doomwiki.org/wiki/Fast_doors_reopening_with_wrong_sound
            S_StartSound(&door->sector->soundorg, sfx_doropn);
            break;
    }
}

static void T_StopDoorMove(vldoor_t* door) {
    switch (door->type) {
        case vld_blazeRaise:
        case vld_blazeClose:
            door->sector->specialdata = NULL;
            P_RemoveThinker(&door->thinker); // unlink and free
            // Vanilla Bug: this line causes fast doors to make two closing
            // sounds. The is no need to this, as the sound is already played
            // when the door starts to close.
            // More info on this bug here:
            // https://doomwiki.org/wiki/Fast_doors_make_two_closing_sounds
            S_StartSound(&door->sector->soundorg, sfx_bdcls);
            break;
        case vld_normal:
        case vld_close:
            door->sector->specialdata = NULL;
            P_RemoveThinker(&door->thinker); // unlink and free
            break;
        case vld_close30ThenOpen:
            door->direction = 0;
            door->topcountdown = (30 * TICRATE);
            break;
        default:
            break;
    }
}

static void T_VerticalDownDoor(vldoor_t* door) {
    switch (T_MoveDoorDown(door)) {
        case pastdest:
            T_StopDoorMove(door);
            break;
        case crushed:
            T_DoorHitObject(door);
            break;
        default:
            break;
    }
}


//
// T_VerticalDoor
//
void T_VerticalDoor(vldoor_t* door) {
    switch (door->direction) {
        case -1:
            T_VerticalDownDoor(door);
            break;
        case 0:
            T_VerticalWaitingDoor(door);
            break;
        case 1:
            T_VerticalUpDoor(door);
            break;
        case 2:
            T_VerticalInitialWaitDoor(door);
            break;
        default:
            break;
    }
}

//
// Returns true if player can open blocked door, false otherwise.
//
static bool EV_TryOpenBlockedDoor(const line_t* line, player_t* p) {
    switch (line->special) {
    // Blue Lock
        case 99:
        case 133:
            if (!p->cards[it_bluecard] && !p->cards[it_blueskull]) {
                p->message = DEH_String(PD_BLUEO);
                S_StartSound(NULL, sfx_oof);
                return false;
            }
            return true;
    // Red Lock
        case 134:
        case 135:
            if (!p->cards[it_redcard] && !p->cards[it_redskull]) {
                p->message = DEH_String(PD_REDO);
                S_StartSound(NULL, sfx_oof);
                return false;
            }
            return true;
    // Yellow Lock
        case 136:
        case 137:
            if (!p->cards[it_yellowcard] && !p->cards[it_yellowskull]) {
                p->message = DEH_String(PD_YELLOWO);
                S_StartSound(NULL, sfx_oof);
                return false;
            }
            return true;
        default:
            return true;
    }
}

//
// EV_DoLockedDoor
// Move a locked door up/down.
//
int EV_DoLockedDoor(const line_t* line, vldoor_e type, mobj_t* thing) {
    if (!thing->player) {
        // Not a player, so can't open door.
        return 0;
    }
    if (!EV_TryOpenBlockedDoor(line, thing->player)) {
        // Player cannot open door.
        return 0;
    }
    return EV_DoDoor(line, type);
}

static void EV_AddNewDoor(sector_t* sec, vldoor_e type) {
    vldoor_t* door = Z_Malloc(sizeof(*door), PU_LEVSPEC, NULL);

    P_AddThinker(&door->thinker);
    sec->specialdata = door;

    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->type = type;
    door->topwait = VDOORWAIT;
    door->speed = VDOORSPEED;

    switch (type) {
        case vld_blazeClose:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            door->speed = VDOORSPEED * 4;
            S_StartSound(&door->sector->soundorg, sfx_bdcls);
            break;
        case vld_close:
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->direction = -1;
            S_StartSound(&door->sector->soundorg, sfx_dorcls);
            break;
        case vld_close30ThenOpen:
            door->topheight = sec->ceilingheight;
            door->direction = -1;
            S_StartSound(&door->sector->soundorg, sfx_dorcls);
            break;
        case vld_blazeRaise:
        case vld_blazeOpen:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            door->speed = VDOORSPEED * 4;
            if (door->topheight != sec->ceilingheight)
                S_StartSound(&door->sector->soundorg, sfx_bdopn);
            break;
        case vld_normal:
        case vld_open:
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec);
            door->topheight -= 4 * FRACUNIT;
            if (door->topheight != sec->ceilingheight)
                S_StartSound(&door->sector->soundorg, sfx_doropn);
            break;
        default:
            break;
    }
}

bool EV_DoDoor(const line_t* line, vldoor_e type) {
    bool rtn = false;
    int sec_num = -1;

    while ((sec_num = P_FindSectorFromLineTag(line, sec_num)) >= 0) {
        sector_t* sec = &sectors[sec_num];
        if (!sec->specialdata) {
            rtn = true;
            EV_AddNewDoor(sec, type);
        }
    }

    return rtn;
}

static void EV_AddDoorThinker(line_t* line, sector_t* sec) {
    vldoor_t *door = Z_Malloc(sizeof(*door), PU_LEVSPEC, NULL);
    sec->specialdata = door;

    P_AddThinker(&door->thinker);
    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->direction = 1;
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;

    switch (line->special) {
        case 1:
        case 26:
        case 27:
        case 28:
            door->type = vld_normal;
            break;
        case 31:
        case 32:
        case 33:
        case 34:
            door->type = vld_open;
            line->special = 0;
            break;
        case 117: // blazing door raise
            door->type = vld_blazeRaise;
            door->speed = VDOORSPEED * 4;
            break;
        case 118: // blazing door open
            door->type = vld_blazeOpen;
            line->special = 0;
            door->speed = VDOORSPEED * 4;
            break;
        default:
            break;
    }

    // find the top and bottom of the movement range
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
}

static void EV_StartDoorSound(const line_t* line, sector_t* sec) {
    switch (line->special) {
        case 117: // BLAZING DOOR RAISE
        case 118: // BLAZING DOOR OPEN
            S_StartSound(&sec->soundorg, sfx_bdopn);
            break;
        case 1: // NORMAL DOOR SOUND
        case 31:
            S_StartSound(&sec->soundorg, sfx_doropn);
            break;
        default: // LOCKED DOOR SOUND
            S_StartSound(&sec->soundorg, sfx_doropn);
            break;
    }
}

static void EV_UpdateDoorDirection(vldoor_t* door, mobj_t* thing) {
    if (door->direction == -1) {
        // go back up
        door->direction = 1;
        return;
    }
    if (!thing->player) {
        // JDC: bad guys never close doors
        return;
    }

    // When is a door not a door?
    // In Vanilla, door->direction is set, even though
    // "specialdata" might not actually point at a door.

    if (door->thinker.function.acp1 == (actionf_p1) T_VerticalDoor) {
        // start going down immediately
        door->direction = -1;
        return;
    }
    if (door->thinker.function.acp1 == (actionf_p1) T_PlatRaise) {
        // Erm, this is a plat, not a door.
        // This notably causes a problem in ep1-0500.lmp where a plat and
        // a door are cross-referenced; the door doesn't open on 64-bit.
        // The direction field in vldoor_t corresponds to the wait field in
        // plat_t. Let's set that to -1 instead.
        plat_t* plat = (plat_t *) door;
        plat->wait = -1;
        return;
    }

    // This isn't a door OR a plat.  Now we're in trouble.
    fprintf(stderr, "EV_VerticalDoor: Tried to close something that wasn't a door.\n");
    // Try closing it anyway. At least it will work on 32-bit machines.
    door->direction = -1;
}

static bool EV_IsRaiseDoor(const line_t* line) {
    short special = line->special;
    return special == 1
           || special == 26
           || special == 27
           || special == 28
           || special == 117;
}

static bool EV_CanOpenDoor(const line_t* line, mobj_t* thing) {
    player_t* player = thing->player;

    switch (line->special) {
    // Blue Door
        case 26:
        case 32:
            if (!player) {
                return false;
            }
            if (!player->cards[it_bluecard] && !player->cards[it_blueskull]) {
                player->message = DEH_String(PD_BLUEK);
                S_StartSound(NULL,sfx_oof);
                return false;
            }
            return true;
    // Yellow Door
        case 27:
        case 34:
            if (!player) {
                return false;
            }
            if (!player->cards[it_yellowcard] && !player->cards[it_yellowskull]) {
                player->message = DEH_String(PD_YELLOWK);
                S_StartSound(NULL,sfx_oof);
                return false;
            }
            return true;
    // Red Door
        case 28:
        case 33:
            if (!player) {
                return false;
            }
            if (!player->cards[it_redcard] && !player->cards[it_redskull]) {
                player->message = DEH_String(PD_REDK);
                S_StartSound(NULL,sfx_oof);
                return false;
            }
            return true;
        default:
            return true;
    }
}

//
// Open a door manually, no tag value.
//
void EV_VerticalDoor(line_t* line, mobj_t* thing) {
    //	Check for locks.
    if (!EV_CanOpenDoor(line, thing)) {
        return;
    }
    if (line->sidenum[1] == -1) {
        // Every door must have two sides.
        I_Error("EV_VerticalDoor: DR special type on 1-sided linedef");
    }
    side_t* side = &sides[line->sidenum[1]];
    sector_t* sec = side->sector;

    // If the sector has an active thinker, use it.
    if (sec->specialdata && EV_IsRaiseDoor(line)) {
        EV_UpdateDoorDirection(sec->specialdata, thing);
        return;
    }
    // For proper sound.
    EV_StartDoorSound(line, sec);
    // New door thinker.
    EV_AddDoorThinker(line, sec);
}


//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30(sector_t* sec) {
    vldoor_t *door = Z_Malloc(sizeof(*door), PU_LEVSPEC, NULL);

    P_AddThinker(&door->thinker);

    sec->specialdata = door;
    sec->special = 0;

    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->direction = 0;
    door->type = vld_normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * TICRATE;
}

//
// Spawn a door that opens after 5 minutes
//
void P_SpawnDoorRaiseIn5Mins(sector_t* sec) {
    vldoor_t *door;

    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);

    P_AddThinker(&door->thinker);

    sec->specialdata = door;
    sec->special = 0;

    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->direction = 2;
    door->type = vld_raiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * TICRATE;
}
