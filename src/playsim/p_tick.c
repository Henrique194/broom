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
//	Archiving: SaveGame I/O.
//	Thinker, Ticker.
//


#include "p_tick.h"

#include "doomstat.h"
#include "p_local.h"
#include "z_zone.h"


int leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc, so they can be operated on
// uniformly. The actual structures will vary in size, but the first element
// must be thinker_t.
//


// Both the head and tail of the thinker list.
thinker_t thinkercap;


//
// P_InitThinkers
//
void P_InitThinkers() {
    thinkercap.prev = &thinkercap;
    thinkercap.next  = &thinkercap;
}

//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker(thinker_t* thinker) {
    thinkercap.prev->next = thinker;
    thinker->next = &thinkercap;
    thinker->prev = thinkercap.prev;
    thinkercap.prev = thinker;
}

//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void P_RemoveThinker(thinker_t* thinker) {
    // FIXME: NOP.
    thinker->function.acv = (actionf_v)(-1);
}

bool P_IsThinkerRemoved(thinker_t* thinker) {
    return thinker->function.acv == (actionf_v) (-1);
}

//
// Check P_RemoveThinker for more information.
//
static void P_FreeThinker(thinker_t* thinker) {
    thinker->next->prev = thinker->prev;
    thinker->prev->next = thinker->next;
    Z_Free(thinker);
}

//
// P_RunThinkers
//
void P_RunThinkers() {
    thinker_t* current_thinker = thinkercap.next;
    thinker_t* next_thinker;

    while (current_thinker != &thinkercap) {
	if (P_IsThinkerRemoved(current_thinker)) {
	    // Time to free it.
            next_thinker = current_thinker->next;
            P_FreeThinker(current_thinker);
	} else {
	    if (current_thinker->function.acp1) {
                current_thinker->function.acp1(current_thinker);
            }
            // "acp1" can actually change the thinker list.
            next_thinker = current_thinker->next;
	}
        current_thinker = next_thinker;
    }
}

static void P_UpdateLevelTime() {
    // for par times
    leveltime++;
}

static void P_RunPlayersThinker() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i]) {
            P_PlayerThink(&players[i]);
        }
    }
}

static bool P_IsGamePaused() {
    // run the tic
    if (paused) {
        return true;
    }
    // pause if in menu and at least one tic has been run
    if (!netgame && menuactive && !demoplayback && players[consoleplayer].viewz != 1) {
        return true;
    }
    return false;
}

//
// P_Ticker
//
void P_Ticker() {
    if (P_IsGamePaused()) {
        return;
    }
    P_RunPlayersThinker();
    P_RunThinkers();
    P_UpdateSpecials();
    P_RespawnSpecials();
    P_UpdateLevelTime();
}
