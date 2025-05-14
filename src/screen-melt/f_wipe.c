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
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"

#include "f_wipe.h"

//
//                       SCREEN WIPE PACKAGE
//

static pixel_t*	wipe_scr_start;
static pixel_t*	wipe_scr_end;

// The position of each column in the screen when scrolling.
// (col_pos < 0 => not ready to scroll yet)
static int* col_pos;


//
// Setup initial column positions.
//
static void wipe_initColumnPositions() {
    size_t size = SCREENWIDTH * sizeof(*col_pos);
    col_pos = Z_Malloc((int) size, PU_STATIC, NULL);

    // The screen is divided into groups of two columns, where
    // each pair of columns moves together at the same speed.
    col_pos[0] = -(M_Random() % 16);
    col_pos[1] = col_pos[0];
    for (int i = 2; i < SCREENWIDTH; i += 2) {
        // Generate a random value of -1, 0, or 1.
        int r = (M_Random() % 3) - 1;
        int pos = col_pos[i - 1] + r;
        if (pos > 0) {
            // Ensure every column starts at 0.
            pos = 0;
        } else if (pos == -16) {
            // Limit max difference between column when scrolling.
            pos = -15;
        }
        col_pos[i] = pos;
        col_pos[i + 1] = pos;
    }
}

static void wipe_initWipeSrc() {
    // Copy start screen to main screen.
    size_t size = SCREENWIDTH * SCREENHEIGHT * sizeof(*I_VideoBuffer);
    memcpy(I_VideoBuffer, wipe_scr_start, size);
}

static void wipe_initMelt() {
    wipe_initWipeSrc();
    wipe_initColumnPositions();
}

static void wipe_moveStartScreen(int i, int dy) {
    col_pos[i] += dy;

    const pixel_t* src = wipe_scr_start;
    pixel_t* dst = I_VideoBuffer;

    int y0 = 0;
    int y1 = SCREENHEIGHT - col_pos[i];

    for (int y = y0; y < y1; y++) {
        int src_spot = i + (y * SCREENWIDTH);
        int dst_spot = i + ((y + col_pos[i]) * SCREENWIDTH);
        dst[dst_spot] = src[src_spot];
    }
}

static void wipe_moveEndScreen(int i, int dy) {
    const pixel_t* src = wipe_scr_end;
    pixel_t* dst = I_VideoBuffer;

    int y0 = col_pos[i];
    int y1 = y0 + dy;

    for (int y = y0; y < y1; y++) {
        int spot = i + (y * SCREENWIDTH);
        dst[spot] = src[spot];
    }
}

//
// Calculate how much a column should move.
//
static int wipe_CalculateDy(int i) {
    int pos = col_pos[i];
    int dy = (pos < 16) ? pos + 1 : 8;
    if (pos + dy >= SCREENHEIGHT) {
        dy = SCREENHEIGHT - pos;
    }
    return dy;
}

static void wipe_moveColumn(int col) {
    int dy = wipe_CalculateDy(col);
    wipe_moveEndScreen(col, dy);
    wipe_moveStartScreen(col, dy);
}

static bool wipe_moveColumns() {
    bool done = true;

    for (int i = 0; i < SCREENWIDTH; i++) {
        if (col_pos[i] < 0) {
            // A column will only start to move when col_pos >= 0.
            col_pos[i]++;
            done = false;
        } else if (col_pos[i] < SCREENHEIGHT) {
            wipe_moveColumn(i);
            done = false;
        }
    }

    return done;
}

static bool wipe_doMelt(int ticks) {
    bool done = true;
    while (ticks--) {
        done &= wipe_moveColumns();
    }
    return done;
}

static void wipe_exitMelt() {
    Z_Free(col_pos);
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
}

void wipe_StartScreen() {
    int size = SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_start);
    wipe_scr_start = Z_Malloc(size, PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_start);
}

void wipe_EndScreen() {
    // Copy current frame to wipe_scr_end
    int size = SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_end);
    wipe_scr_end = Z_Malloc(size, PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);

    // Copy wipe_scr_start (previous frame) to video screen.
    V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_start);
}

int wipe_ScreenWipe(int ticks) {
    static bool init = true;

    // Initial stuff.
    if (init) {
        init = false;
        wipe_initMelt();
    }
    // Do a piece of wipe-in.
    bool done = wipe_doMelt(ticks);
    // Final stuff.
    if (done) {
        init = true;
        wipe_exitMelt();
    }
    return init;
}
