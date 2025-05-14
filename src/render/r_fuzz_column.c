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
//     Spectre/Invisibility.
//     Framebuffer postprocessing. Creates a fuzzy image by copying pixels from
//     adjacent ones to left and right. Used with an all black colormap, this
//     could create the SHADOW effect, i.e. spectres and invisible players.
//


#include "doomdef.h"
#include "i_system.h"
#include "r_local.h"

#define FUZZTABLE 50
#define FUZZOFF   1

// Colormap #6 (of 0-31, a bit brighter than average).
#define FUZZ_COLORMAP 6

static int fuzzoffset[FUZZTABLE] = {
    FUZZOFF,  -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF,
    FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF,
    FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF,
    FUZZOFF,  -FUZZOFF, -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  FUZZOFF,
    -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF, -FUZZOFF,
    FUZZOFF,  FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,
    FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF,
    FUZZOFF
};

static int fuzzpos = 0;


void R_DrawFuzzColumn() {
    // Zero length.
    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }

    // Adjust borders. Low...
    if (dc_yl < FUZZOFF) {
        dc_yl = FUZZOFF;
    }
    // .. and high.
    if (dc_yh >= viewheight - FUZZOFF) {
        dc_yh = viewheight - FUZZOFF - 1;
    }

    // Looks like an attempt at dithering.
    for (int y = dc_yl; y <= dc_yh; y++) {
        // Lookup framebuffer, and retrieve a pixel that is
        // either one row up or down of the current one.
        int offset = fuzzoffset[fuzzpos];
        pixel_t pixel = R_GetPixel(dc_x, y + offset);

        // Add index from colormap to index.
        int spot = pixel + (FUZZ_COLORMAP * 256);
        lighttable_t color = colormaps[spot];

        // Draw!
        R_DrawPixel(dc_x, y, color);

        // Clamp table lookup index.
        fuzzpos = (fuzzpos + 1) % FUZZTABLE;
    }
} 

//
// low detail mode version
//
void R_DrawFuzzColumnLow() {
    // Low detail mode, need to multiply by 2.
    int x = dc_x << 1;

    // Zero length.
    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }

    // Adjust borders. Low...
    if (dc_yl < FUZZOFF) {
        dc_yl = FUZZOFF;
    }
    // .. and high.
    if (dc_yh >= viewheight - FUZZOFF) {
        dc_yh = viewheight - FUZZOFF - 1;
    }

    // Looks like an attempt at dithering.
    for (int y = dc_yl; y <= dc_yh; y++) {
        // Lookup framebuffer, and retrieve a pixel that is either
        // one row up or down of the current one.
        int offset = fuzzoffset[fuzzpos];
        pixel_t pixel1 = R_GetPixel(x, y + offset);
        pixel_t pixel2 = R_GetPixel(x + 1, y + offset);

        // Add index from colormap to index.
        int spot1 = pixel1 + (FUZZ_COLORMAP * 256);
        int spot2 = pixel2 + (FUZZ_COLORMAP * 256);
        lighttable_t color1 = colormaps[spot1];
        lighttable_t color2 = colormaps[spot2];

        // Draw!
        R_DrawPixel(x, y, color1);
        R_DrawPixel(x + 1, y, color2);

        // Clamp table lookup index.
        fuzzpos = (fuzzpos + 1) % FUZZTABLE;
    }
}
