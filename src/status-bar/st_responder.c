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
//	Status bar responder code.
//




#include "am_map.h"
#include "deh_str.h"
#include "deh_misc.h"
#include "g_game.h"
#include "m_misc.h"
#include "p_inter.h"
#include "s_sound.h"
#include "st_responder.h"
#include "st_stuff.h"
#include "d_mode.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"


static bool ST_IsAutoMapMessage(const event_t* ev) {
    return (ev->data1 & 0xffff0000) == AM_MSGHEADER;
}

static bool ST_CheckNoClipCheat(char key) {
    // For Doom 1, use the idspipsopd cheat;
    // For all others, use idclip.
    if (logical_gamemission == doom) {
        return cht_CheckCheat(&cheat_noclip, key);
    }
    return cht_CheckCheat(&cheat_commercial_noclip, key);
}

//
// Check invalid maps.
//
static bool ST_CheckInvalidMap(int epsd, int map) {
    if (gamemode == commercial) {
        return map < 1 || map > 40;
    }
    if (gameversion < exe_ultimate && epsd == 4) {
        return false;
    }
    return epsd < 1 || epsd > 4 || map < 1 || map > 9;
}

static void ST_GetIdclevMap(int* epsd, int* map) {
    char buf[3];
    cht_GetParam(&cheat_clev, buf);

    if (gamemode == commercial) {
        *epsd = 0;
        *map = (buf[0] - '0') * 10 + buf[1] - '0';
        return;
    }

    *epsd = buf[0] - '0';
    *map = buf[1] - '0';
    // Chex.exe always warps to episode 1.
    if (gameversion == exe_chex) {
        if (*epsd > 1) {
            *epsd = 1;
        }
        if (*map > 5) {
            *map = 5;
        }
    }
}

//
// 'idclev' for change-level cheat.
//
static void ST_DoLevelChangeCheat() {
    int epsd = 0;
    int map = 0;

    ST_GetIdclevMap(&epsd, &map);
    if (ST_CheckInvalidMap(epsd, map)) {
        return;
    }
    // So be it.
    st_plyr->message = DEH_String(STSTR_CLEV);
    G_DeferedInitNew(gameskill, epsd, map);
}

//
// 'idmypos' for player position.
//
static void ST_DoPlayerPositionCheat() {
    static char buf[ST_MSGWIDTH];

    const mobj_t* plr_mo = players[consoleplayer].mo;
    M_snprintf(buf, sizeof(buf), "ang=0x%x;x,y=(0x%x,0x%x)",
               plr_mo->angle, plr_mo->x, plr_mo->y);
    st_plyr->message = buf;
}

//
// 'idchoppers' invulnerability & chainsaw.
//
static void ST_DoChoppersCheat() {
    st_plyr->weaponowned[wp_chainsaw] = true;
    st_plyr->powers[pw_invulnerability] = true;
    st_plyr->message = DEH_String(STSTR_CHOPPERS);
}

//
// 'idbeholdl' power-up menu.
//
static void ST_DoBeholdCheat() {
    st_plyr->message = DEH_String(STSTR_BEHOLD);
}

//
// 'behold?' power-up cheats
//
static void ST_CheckPowerUpCheats(char key) {
    for (int i = 0; i < 6; i++) {
        if (cht_CheckCheat(&cheat_powerup[i], key)) {
            if (st_plyr->powers[i] == 0) {
                P_GivePower(st_plyr, i);
            } else if (i != pw_strength) {
                st_plyr->powers[i] = 1;
            } else {
                st_plyr->powers[i] = 0;
            }
            st_plyr->message = DEH_String(STSTR_BEHOLDX);
        }
    }
}

//
// Noclip cheat.
//
static void ST_DoNoClipCheat() {
    st_plyr->cheats ^= CF_NOCLIP;
    if (st_plyr->cheats & CF_NOCLIP) {
        st_plyr->message = DEH_String(STSTR_NCON);
    } else {
        st_plyr->message = DEH_String(STSTR_NCOFF);
    }
}

//
// 'idmus' cheat for changing music.
//
static void ST_DoMusicChangeCheat() {
    char buf[3];

    st_plyr->message = DEH_String(STSTR_MUS);
    cht_GetParam(&cheat_mus, buf);

    // Note: The original v1.9 had a bug that tried to play back
    // the Doom II music regardless of gamemode.  This was fixed
    // in the Ultimate Doom executable so that it would work for
    // the Doom 1 music as well.

    if (gamemode == commercial || gameversion < exe_ultimate) {
        int mus_offset = (buf[0] - '0') * 10 + (buf[1] - '1');
        int musnum = mus_runnin + mus_offset;

        if (mus_offset <= 34 || gameversion < exe_doom_1_8) {
            S_ChangeMusic(musnum, true);
        } else {
            st_plyr->message = DEH_String(STSTR_NOMUS);
        }
        return;
    }

    int mus_offset = (buf[0] - '1') * 9 + (buf[1] - '1');
    int musnum = mus_e1m1 + mus_offset;

    if (mus_offset <= 31) {
        S_ChangeMusic(musnum, true);
    } else {
        st_plyr->message = DEH_String(STSTR_NOMUS);
    }
}

//
// 'idkfa' cheat for key full ammo.
//
static void ST_DoKeysFullArsenalCheat() {
    st_plyr->armorpoints = deh_idkfa_armor;
    st_plyr->armortype = deh_idkfa_armor_class;
    for (int i = 0; i < NUMWEAPONS; i++) {
        st_plyr->weaponowned[i] = true;
    }
    for (int i = 0; i < NUMAMMO; i++) {
        st_plyr->ammo[i] = st_plyr->maxammo[i];
    }
    for (int i = 0; i < NUMCARDS; i++) {
        st_plyr->cards[i] = true;
    }
    st_plyr->message = DEH_String(STSTR_KFAADDED);
}

//
// 'idfa' cheat for killer fucking arsenal.
//
static void ST_DoFullArsenalCheat() {
    st_plyr->armorpoints = deh_idfa_armor;
    st_plyr->armortype = deh_idfa_armor_class;
    for (int i = 0; i < NUMWEAPONS; i++) {
        st_plyr->weaponowned[i] = true;
    }
    for (int i = 0; i < NUMAMMO; i++) {
        st_plyr->ammo[i] = st_plyr->maxammo[i];
    }
    st_plyr->message = DEH_String(STSTR_FAADDED);
}

//
// 'iddqd' cheat for toggleable god mode.
//
static void ST_DoInvulnerableCheat() {
    st_plyr->cheats ^= CF_GODMODE;
    if (st_plyr->cheats & CF_GODMODE) {
        if (st_plyr->mo) {
            st_plyr->mo->health = deh_god_mode_health;
        }
        st_plyr->health = deh_god_mode_health;
        st_plyr->message = DEH_String(STSTR_DQDON);
    } else {
        st_plyr->message = DEH_String(STSTR_DQDOFF);
    }
}

static void ST_CheckNonNightmareCheats(char key) {
    if (cht_CheckCheat(&cheat_god, key)) {
        ST_DoInvulnerableCheat();
    } else if (cht_CheckCheat(&cheat_ammonokey, key)) {
        ST_DoFullArsenalCheat();
    } else if (cht_CheckCheat(&cheat_ammo, key)) {
        ST_DoKeysFullArsenalCheat();
    } else if (cht_CheckCheat(&cheat_mus, key)) {
        ST_DoMusicChangeCheat();
    } else if (ST_CheckNoClipCheat(key)) {
        ST_DoNoClipCheat();
    }

    ST_CheckPowerUpCheats(key);
    if (cht_CheckCheat(&cheat_powerup[6], key)) {
        ST_DoBeholdCheat();
        return;
    }
    if (cht_CheckCheat(&cheat_choppers, key)) {
        ST_DoChoppersCheat();
        return;
    }
    if (cht_CheckCheat(&cheat_mypos, key)) {
        ST_DoPlayerPositionCheat();
    }
}

static void ST_CheckCheats(const event_t* ev) {
    char key = (char) ev->data2;
    if (gameskill != sk_nightmare) {
        ST_CheckNonNightmareCheats(key);
    }
    if (cht_CheckCheat(&cheat_clev, key)) {
        ST_DoLevelChangeCheat();
    }
}

//
// if a user keypress...
//
static void ST_HandleKeyDown(const event_t* ev) {
    // Disable cheat codes in multiplayer
    if (!netgame) {
        ST_CheckCheats(ev);
    }
}

static void ST_HandleKeyUp(const event_t* ev) {
    // Filter automap on/off.
    if (ST_IsAutoMapMessage(ev) && ev->data1 == AM_MSGENTERED) {
        st_firsttime = true;
    }
}

//
// Respond to keyboard input events, intercept cheats.
// Does not consume the event.
//
bool ST_Responder(const event_t* ev) {
    switch (ev->type) {
        case ev_keyup:
            ST_HandleKeyUp(ev);
            break;
        case ev_keydown:
            ST_HandleKeyDown(ev);
            break;
        default:
            break;
    }

    return false;
}
