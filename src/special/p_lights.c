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
//	Handle Sector base lighting effects.
//	Muzzle flash?
//


#include "p_lights.h"

#include "z_zone.h"
#include "m_random.h"

#include "p_local.h"


//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker(fireflicker_t* flick) {
    if (--flick->count) {
        // Not ready to change light level.
        return;
    }

    int amount = (P_Random() & 3) * 16;
    if (flick->sector->lightlevel - amount < flick->minlight) {
        flick->sector->lightlevel = (short) flick->minlight;
    } else {
        flick->sector->lightlevel = (short) (flick->maxlight - amount);
    }

    flick->count = 4;
}


//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker(sector_t* sector) {
    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    sector->special = 0;

    fireflicker_t* flick = Z_Malloc(sizeof(*flick), PU_LEVSPEC, NULL);

    P_AddThinker(&flick->thinker);

    flick->thinker.function.acp1 = (actionf_p1) T_FireFlicker;
    flick->sector = sector;
    flick->count = 4;
    flick->maxlight = sector->lightlevel;
    flick->minlight =
        P_FindMinSurroundingLight(sector, sector->lightlevel) + 16;
}


//
// BROKEN LIGHT FLASHING
//


//
// T_LightFlash
// Do flashing lights.
//
void T_LightFlash(lightflash_t* flash) {
    if (--flash->count) {
        return;
    }

    if (flash->sector->lightlevel == flash->maxlight) {
        flash->sector->lightlevel = (short) flash->minlight;
        flash->count = (P_Random() & flash->mintime) + 1;
    } else {
        flash->sector->lightlevel = (short) flash->maxlight;
        flash->count = (P_Random() & flash->maxtime) + 1;
    }
}


//
// P_SpawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnLightFlash(sector_t* sector) {
    // Nothing special about it during gameplay.
    sector->special = 0;

    lightflash_t *flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, NULL);

    P_AddThinker(&flash->thinker);

    flash->thinker.function.acp1 = (actionf_p1) T_LightFlash;
    flash->sector = sector;
    flash->maxlight = sector->lightlevel;
    flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (P_Random() & flash->maxtime) + 1;
}


//
// STROBE LIGHT FLASHING
//


//
// T_StrobeFlash
//
void T_StrobeFlash(strobe_t *flash) {
    if (--flash->count) {
        return;
    }

    if (flash->sector->lightlevel == flash->minlight) {
        flash->sector->lightlevel = (short) flash->maxlight;
        flash->count = flash->brighttime;
    } else {
        flash->sector->lightlevel = (short) flash->minlight;
        flash->count = flash->darktime;
    }
}


//
// P_SpawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync) {
    strobe_t* flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, NULL);

    P_AddThinker(&flash->thinker);

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->thinker.function.acp1 = (actionf_p1) T_StrobeFlash;
    flash->maxlight = sector->lightlevel;

    flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
    if (flash->minlight == flash->maxlight) {
        flash->minlight = 0;
    }

    // Nothing special about it during gameplay.
    sector->special = 0;
    flash->count = (inSync ? 1 : (P_Random() & 7) + 1);
}


//
// Start strobing lights (usually from a trigger)
//
void EV_StartLightStrobing(const line_t* line) {
    int secnum = -1;
    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0) {
        sector_t* sec = &sectors[secnum];
        if (!sec->specialdata) {
            P_SpawnStrobeFlash(sec, SLOWDARK, 0);
        }
    }
}

static void EV_TurnSectorLightsOff(sector_t* sector) {
    short min = sector->lightlevel;
    for (int i = 0; i < sector->linecount; i++) {
        line_t* line = sector->lines[i];
        const sector_t* sec = getNextSector(line, sector);
        if (sec && sec->lightlevel < min) {
            min = sec->lightlevel;
        }
    }
    sector->lightlevel = min;
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void EV_TurnTagLightsOff(const line_t* line) {
    for (int i = 0; i < numsectors; i++) {
        sector_t* sector = &sectors[i];
        if (sector->tag == line->tag) {
            EV_TurnSectorLightsOff(sector);
        }
    }
}

static void EV_TurnSectorLightsOn(sector_t* sector, int bright) {
    // bright = 0 means to search for highest light level surrounding sector
    if (bright == 0) {
        for (int i = 0; i < sector->linecount; i++) {
            line_t* line = sector->lines[i];
            const sector_t* sec = getNextSector(line, sector);
            if (sec && sec->lightlevel > bright) {
                bright = sec->lightlevel;
            }
        }
    }
    sector->lightlevel = (short) bright;
}

//
// TURN LINE'S TAG LIGHTS ON
//
void EV_LightTurnOn(const line_t* line, int bright) {
    for (int i = 0; i < numsectors; i++) {
        sector_t* sector = &sectors[i];
        if (sector->tag == line->tag) {
            EV_TurnSectorLightsOn(sector, bright);
        }
    }
}


//
// Spawn glowing light
//

void T_Glow(glow_t* g) {
    switch (g->direction) {
        case -1:
            // DOWN
            g->sector->lightlevel -= GLOWSPEED;
            if (g->sector->lightlevel <= g->minlight) {
                g->sector->lightlevel += GLOWSPEED;
                g->direction = 1;
            }
            break;
        case 1:
            // UP
            g->sector->lightlevel += GLOWSPEED;
            if (g->sector->lightlevel >= g->maxlight) {
                g->sector->lightlevel -= GLOWSPEED;
                g->direction = -1;
            }
            break;
        default:
            break;
    }
}


void P_SpawnGlowingLight(sector_t* sector) {
    glow_t* g = Z_Malloc(sizeof(*g), PU_LEVSPEC, NULL);

    P_AddThinker(&g->thinker);

    g->sector = sector;
    g->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
    g->maxlight = sector->lightlevel;
    g->thinker.function.acp1 = (actionf_p1) T_Glow;
    g->direction = -1;

    sector->special = 0;
}
