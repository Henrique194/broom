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
//
// DESCRIPTION:  the automap code
//


#include "am_responder.h"

#include "deh_str.h"
#include "m_controls.h"
#include "m_misc.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"

#include "am_map.h"

extern int followplayer;
extern mpoint_t m_paninc;
extern fixed_t mtof_zoommul;
extern fixed_t ftom_zoommul;
extern mpoint_t f_oldloc;
extern player_t *plr;
extern int grid;
extern int cheating;
extern int markpointnum;

extern fixed_t m_x;
extern fixed_t m_y;
extern fixed_t m_x2;
extern fixed_t m_y2;
extern fixed_t m_w;
extern fixed_t m_h;
extern int f_w;

// old stuff for recovery later
extern fixed_t old_m_w;
extern fixed_t old_m_h;
extern fixed_t old_m_x;
extern fixed_t old_m_y;

static int bigstate = 0;


static void AM_StopZoom() {
    mtof_zoommul = FRACUNIT;
    ftom_zoommul = FRACUNIT;
}

static void AM_ZoomIn() {
    mtof_zoommul = M_ZOOMIN;
    ftom_zoommul = M_ZOOMOUT;
}

static void AM_ZoomOut() {
    mtof_zoommul = M_ZOOMOUT;
    ftom_zoommul = M_ZOOMIN;
}

static void AM_StopVerticalPan() {
    m_paninc.y = 0;
}

static void AM_StopHorizontalPan() {
    m_paninc.x = 0;
}

static void AM_PanDown() {
    m_paninc.y = -FTOM(F_PANINC);
}

static void AM_PanUp() {
    m_paninc.y = FTOM(F_PANINC);
}

static void AM_PanLeft() {
    m_paninc.x = -FTOM(F_PANINC);
}

static void AM_PanRight() {
    m_paninc.x = FTOM(F_PANINC);
}

static bool AM_UsedAutoMapCheatCode(const event_t* ev) {
    if (deathmatch && gameversion > exe_doom_1_8) {
        return false;
    }
    return cht_CheckCheat(&cheat_amap, (char) ev->data2);
}

static void AM_ClearAllMarks() {
    AM_clearMarks();
    plr->message = DEH_String(AMSTR_MARKSCLEARED);
}

static void AM_MarkSpot() {
    static char buffer[20];

    const char* message = DEH_String(AMSTR_MARKEDSPOT);
    M_snprintf(buffer, sizeof(buffer), "%s %d", message, markpointnum);
    plr->message = buffer;

    AM_addMark();
}

static void AM_saveScaleAndLoc(void) {
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

static void AM_restoreScaleAndLoc(void) {
    m_w = old_m_w;
    m_h = old_m_h;

    if (followplayer) {
        m_x = plr->mo->x - m_w/2;
        m_y = plr->mo->y - m_h/2;
    } else {
        m_x = old_m_x;
        m_y = old_m_y;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w << FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

static void AM_ToggleMaxZoom() {
    bigstate = !bigstate;
    if (bigstate) {
        AM_saveScaleAndLoc();
        AM_minOutWindowScale();
    } else {
        AM_restoreScaleAndLoc();
    }
}

static void AM_ToggleGrid() {
    grid = !grid;
    if (grid) {
        plr->message = DEH_String(AMSTR_GRIDON);
    } else {
        plr->message = DEH_String(AMSTR_GRIDOFF);
    }
}

static void AM_ToggleFollowPlayer() {
    followplayer = !followplayer;
    f_oldloc.x = INT_MAX;
    if (followplayer) {
        plr->message = DEH_String(AMSTR_FOLLOWON);
    } else {
        plr->message = DEH_String(AMSTR_FOLLOWOFF);
    }
}

static void AM_ToggleAutoMap() {
    if (automapactive) {
        bigstate = 0;
        viewactive = true;
        AM_Stop();
    } else {
        AM_Start();
        viewactive = false;
    }
}

static void AM_UpdateJoyWait() {
    joywait = I_GetTime() + 5;
}

static bool AM_ResponderInactive(const event_t* ev) {
    switch (ev->type) {
        case ev_joystick:
            if (joybautomap >= 0 && (ev->data1 & (1 << joybautomap)) != 0) {
                AM_UpdateJoyWait();
                AM_ToggleAutoMap();
                return true;
            }
            return false;
        case ev_keydown:
            if (ev->data1 == key_map_toggle) {
                AM_ToggleAutoMap();
                return true;
            }
            return false;
        default:
            return false;
    }
}

static void AM_HandleKeyUp(const event_t* ev) {
    int key = ev->data1;
    if (key == key_map_zoomout || key == key_map_zoomin) {
        AM_StopZoom();
        return;
    }
    if (followplayer) {
        return;
    }
    if (key == key_map_east || key == key_map_west) {
        AM_StopHorizontalPan();
    } else if (key == key_map_north || key == key_map_south) {
        AM_StopVerticalPan();
    }
}

static bool AM_HandleZoomEvent(const event_t* ev) {
    int key = ev->data1;
    if (key == key_map_zoomout) {
        AM_ZoomOut();
        return true;
    }
    if (key == key_map_zoomin) {
        AM_ZoomIn();
        return true;
    }
    return false;
}

static bool AM_HandlePanEvent(const event_t* ev) {
    if (followplayer) {
        return false;
    }

    int key = ev->data1;
    if (key == key_map_east) {
        AM_PanRight();
        return true;
    }
    if (key == key_map_west) {
        AM_PanLeft();
        return true;
    }
    if (key == key_map_north) {
        AM_PanUp();
        return true;
    }
    if (key == key_map_south) {
        AM_PanDown();
        return true;
    }
    return false;
}

//
// Handles events related to the automap when a key is pressed.
// Returns true if the event was successfully processed (i.e., consumed).
//
static bool AM_HandleKeyDownMapEvent(const event_t* ev) {
    int key = ev->data1;
    if (AM_HandlePanEvent(ev)) {
        return true;
    }
    if (AM_HandleZoomEvent(ev)) {
        return true;
    }
    if (key == key_map_toggle) {
        AM_ToggleAutoMap();
        return true;
    }
    if (key == key_map_maxzoom) {
        AM_ToggleMaxZoom();
        return true;
    }
    if (key == key_map_follow) {
        AM_ToggleFollowPlayer();
        return true;
    }
    if (key == key_map_grid) {
        AM_ToggleGrid();
        return true;
    }
    if (key == key_map_mark) {
        AM_MarkSpot();
        return true;
    }
    if (key == key_map_clearmark) {
        AM_ClearAllMarks();
        return true;
    }
    return false;
}

//
// Handle key press events.
// Returns true if event was consumed.
//
static bool AM_HandleKeyDown(event_t* ev) {
    bool handled_event = AM_HandleKeyDownMapEvent(ev);
    if (AM_UsedAutoMapCheatCode(ev)) {
        cheating = (cheating + 1) % 3;
        return false;
    }
    return handled_event;
}

static bool AM_ResponderActive(event_t* ev) {
    switch (ev->type) {
        case ev_joystick:
            if (joybautomap >= 0 && (ev->data1 & (1 << joybautomap)) != 0) {
                AM_UpdateJoyWait();
                AM_ToggleAutoMap();
                return true;
            }
            return false;
        case ev_keydown:
            return AM_HandleKeyDown(ev);
        case ev_keyup:
            AM_HandleKeyUp(ev);
            return false;
        default:
            return false;
    }
}

bool AM_Responder(event_t* ev) {
    if (automapactive) {
        return AM_ResponderActive(ev);
    }
    return AM_ResponderInactive(ev);
}
