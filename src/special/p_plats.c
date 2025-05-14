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
//	Plats (i.e. elevator platforms) code, raising/lowering.
//


#include "p_plats.h"

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "p_floor.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"


plat_t* activeplats[MAXPLATS];


void P_AddActivePlat(plat_t* plat) {
    for (int i = 0; i < MAXPLATS; i++) {
        if (activeplats[i] == NULL) {
            activeplats[i] = plat;
            return;
        }
    }
    I_Error("P_AddActivePlat: no more plats!");
}

static void P_RemoveActivePlat(const plat_t* plat) {
    for (int i = 0; i < MAXPLATS; i++) {
        if (plat == activeplats[i]) {
            (activeplats[i])->sector->specialdata = NULL;
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return;
        }
    }
    I_Error("P_RemoveActivePlat: can't find plat!");
}

static void T_UpdateWaitingPlat(plat_t* plat) {
    plat->count--;
    if (plat->count != 0) {
        // Not ready to move.
        return;
    }
    if (plat->sector->floorheight == plat->low) {
        plat->status = up;
    } else {
        plat->status = down;
    }
    S_StartSound(&plat->sector->soundorg, sfx_pstart);
}

static void T_MovePlatDown(plat_t* plat) {
    result_e res =
        T_MovePlane(plat->sector, plat->speed, plat->low, false, 0, -1);

    if (res == pastdest) {
        plat->count = plat->wait;
        plat->status = waiting;
        S_StartSound(&plat->sector->soundorg, sfx_pstop);
    }
}

static void T_MovePlatUp(plat_t* plat) {
    result_e res =
        T_MovePlane(plat->sector, plat->speed, plat->high, plat->crush, 0, 1);

    if (plat->type == raiseAndChange ||
        plat->type == raiseToNearestAndChange)
    {
        if (!(leveltime & 7)) {
            S_StartSound(&plat->sector->soundorg, sfx_stnmov);
        }
    }

    switch (res) {
        case crushed:
            if (!plat->crush) {
                plat->count = plat->wait;
                plat->status = down;
                S_StartSound(&plat->sector->soundorg, sfx_pstart);
            }
            break;
        case pastdest:
            plat->count = plat->wait;
            plat->status = waiting;
            S_StartSound(&plat->sector->soundorg, sfx_pstop);
            switch (plat->type) {
                case blazeDWUS:
                case downWaitUpStay:
                case raiseAndChange:
                case raiseToNearestAndChange:
                    P_RemoveActivePlat(plat);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

//
// Move a plat up and down
//
void T_PlatRaise(plat_t* plat) {
    switch (plat->status) {
        case up:
            T_MovePlatUp(plat);
            break;
        case down:
            T_MovePlatDown(plat);
            break;
        case waiting:
            T_UpdateWaitingPlat(plat);
            break;
        default:
            break;
    }
}

static void EV_InitPlatTypeData(plat_t* plat, sector_t* sec, const line_t *line,
                                plattype_e type, int amount)
{
    // Find lowest & highest floors around sector
    switch (type) {
        case perpetualRaise:
            plat->speed = PLATSPEED;
            plat->low = P_FindLowestFloorSurrounding(sec);
            if (plat->low > sec->floorheight) {
                plat->low = sec->floorheight;
            }
            plat->high = P_FindHighestFloorSurrounding(sec);
            if (plat->high < sec->floorheight) {
                plat->high = sec->floorheight;
            }
            plat->wait = TICRATE * PLATWAIT;
            plat->status = P_Random() & 1;
            S_StartSound(&sec->soundorg, sfx_pstart);
            break;
        case downWaitUpStay:
            plat->speed = PLATSPEED * 4;
            plat->low = P_FindLowestFloorSurrounding(sec);
            if (plat->low > sec->floorheight) {
                plat->low = sec->floorheight;
            }
            plat->high = sec->floorheight;
            plat->wait = TICRATE * PLATWAIT;
            plat->status = down;
            S_StartSound(&sec->soundorg, sfx_pstart);
            break;
        case raiseAndChange:
            plat->speed = PLATSPEED / 2;
            sec->floorpic = line->frontsector->floorpic;
            plat->high = sec->floorheight + amount * FRACUNIT;
            plat->wait = 0;
            plat->status = up;
            S_StartSound(&sec->soundorg, sfx_stnmov);
            break;
        case raiseToNearestAndChange:
            plat->speed = PLATSPEED / 2;
            sec->floorpic = line->frontsector->floorpic;
            plat->high = P_FindNextHighestFloor(sec, sec->floorheight);
            plat->wait = 0;
            plat->status = up;
            // No more damage, if applicable.
            sec->special = 0;
            S_StartSound(&sec->soundorg, sfx_stnmov);
            break;
        case blazeDWUS:
            plat->speed = PLATSPEED * 8;
            plat->low = P_FindLowestFloorSurrounding(sec);
            if (plat->low > sec->floorheight) {
                plat->low = sec->floorheight;
            }
            plat->high = sec->floorheight;
            plat->wait = TICRATE * PLATWAIT;
            plat->status = down;
            S_StartSound(&sec->soundorg, sfx_pstart);
            break;
    }
}

static void EV_AddNewPlat(sector_t* sec, const line_t* line, plattype_e type,
                          int amount)
{
    plat_t* plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, NULL);

    plat->type = type;
    plat->sector = sec;
    plat->sector->specialdata = plat;
    plat->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
    plat->crush = false;
    plat->tag = line->tag;
    EV_InitPlatTypeData(plat, sec, line, type, amount);

    P_AddThinker(&plat->thinker);
    P_AddActivePlat(plat);
}

static void P_ActivateInStasis(int tag) {
    for (int i = 0; i < MAXPLATS; i++) {
        plat_t* plat = activeplats[i];
        if (plat && plat->tag == tag && plat->status == in_stasis) {
            plat->status = plat->oldstatus;
            plat->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
        }
    }
}

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
bool EV_DoPlat(const line_t* line, plattype_e type, int amount) {
    if (type == perpetualRaise) {
        // Activate all plats that are in_stasis.
        P_ActivateInStasis(line->tag);
    }

    bool rtn = false;
    int sec_num = -1;

    while ((sec_num = P_FindSectorFromLineTag(line, sec_num)) >= 0) {
        sector_t* sec = &sectors[sec_num];
        if (!sec->specialdata) {
            rtn = true;
            EV_AddNewPlat(sec, line, type, amount);
        }
    }

    return rtn;
}

void EV_StopPlat(const line_t *line) {
    for (int i = 0; i < MAXPLATS; i++) {
        plat_t* plat = activeplats[i];
        if (plat && plat->status != in_stasis && plat->tag == line->tag) {
            plat->oldstatus = plat->status;
            plat->status = in_stasis;
            plat->thinker.function.acv = (actionf_v) NULL;
        }
    }
}
