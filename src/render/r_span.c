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
//     With DOOM style restrictions on view orientation, the floors and ceilings
//     consist of horizontal slices or spans with constant z depth. However,
//     rotation around the world z axis is possible, thus this mapping, while
//     simpler and faster than perspective correct texture mapping, has to
//     traverse the texture at an angle in all but a few cases. In consequence,
//     flats are not stored by column (like walls), and the inner loop has to
//     step in texture space u and v.
//
//


#include "doomdef.h"
#include "i_system.h"
#include "r_local.h"


// Every flat must be 64x64
#define FLAT_WIDTH 64
#define FLAT_HEIGHT 64

int ds_y;
int ds_x1;
int ds_x2;

lighttable_t *ds_colormap;

fixed_t ds_xfrac;
fixed_t ds_yfrac;
fixed_t ds_xstep;
fixed_t ds_ystep;

// start of a 64*64 tile image
byte* ds_source;


static int R_RemEuclid(int a, int b) {
    int rem = a % b;
    if (rem < 0) {
        rem += b;
    }
    return rem;
}

static int R_GetFlatVCoordinate(int frac_y) {
    int int_y = frac_y >> FRACBITS;
    return R_RemEuclid(int_y, FLAT_HEIGHT);
}

static int R_GetFlatUCoordinate(int frac_x) {
    int int_x = frac_x >> FRACBITS;
    return R_RemEuclid(int_x, FLAT_WIDTH);
}

//
// Draws the actual span.
//
void R_DrawSpan() {
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || (unsigned)ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }

    for (int x = ds_x1; x <= ds_x2; x++) {
        // Linear interpolate texture coordinates.
        int dx = x - ds_x1;
        fixed_t texture_frac_x = ds_xfrac + (dx * ds_xstep);
        fixed_t texture_frac_y = ds_yfrac + (dx * ds_ystep);

        // Project in texture space.
        int texture_x = R_GetFlatUCoordinate(texture_frac_x);
        int texture_y = R_GetFlatVCoordinate(texture_frac_y);

        // Index texture and retrieve color.
        int texture_spot = texture_x + (texture_y * FLAT_WIDTH);
        byte color = ds_source[texture_spot];

        // Re-index using light/colormap.
        color = ds_colormap[color];

        // Draw!
        R_DrawPixel(x, ds_y, color);
    }
}

void R_DrawSpanLow() {
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH ||
        (unsigned) ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }

    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    for (int x = ds_x1; x <= ds_x2; x += 2) {
        // Linear interpolate texture coordinates. Each 2-pixel step in
        // screen space corresponds to a 1-pixel step in texture space.
        int dx = (x - ds_x1) >> 1;
        fixed_t texture_frac_x = ds_xfrac + (dx * ds_xstep);
        fixed_t texture_frac_y = ds_yfrac + (dx * ds_ystep);

        // Project in texture space.
        int texture_x = R_GetFlatUCoordinate(texture_frac_x);
        int texture_y = R_GetFlatVCoordinate(texture_frac_y);

        // Index texture and retrieve color.
        int texture_spot = texture_x + (texture_y * FLAT_WIDTH);
        byte color = ds_source[texture_spot];

        // Re-index using light/colormap.
        color = ds_colormap[color];

        // Draw!
        R_DrawPixel(x, ds_y, color);
        R_DrawPixel(x + 1, ds_y, color);
    }
}
