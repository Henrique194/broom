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
//     A column is a vertical slice/span from a wall texture that, given the
//     DOOM style restrictions on the view orientation, will always have
//     constant z depth. Thus a special case loop for very fast rendering can
//     be used. It has also been used with Wolfenstein 3D.
//
//


#include "i_system.h"
#include "r_local.h"


//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t* dc_colormap;
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;

// first pixel in a column (possibly virtual)
const byte* dc_source;


void R_DrawColumn() {
    // Zero length, column does not exceed a pixel.
    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }

    for (int y = dc_yl; y <= dc_yh; y++) {
        // Linear interpolate texture coordinate.
        int dy = y - centery;
        fixed_t texture_frac_y = dc_texturemid + (dy * dc_iscale);

        // Index texture and retrieve color.
        int texture_y = (texture_frac_y >> FRACBITS) & 127;
        byte color = dc_source[texture_y];

        // Re-map color indices from wall texture column
        // using a lighting/special effects LUT.
        color = dc_colormap[color];

        // Draw!
        R_DrawPixel(dc_x, y, color);
    }
}

//
// Low detail version of R_DrawColumn above.
//
void R_DrawColumnLow() {
    // Zero length.
    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }

    // Blocky mode, need to multiply by 2.
    int x = dc_x << 1;

    for (int y = dc_yl; y <= dc_yh; y++) {
        // Linear interpolate texture coordinate.
        int dy = y - centery;
        fixed_t texture_frac_y = dc_texturemid + (dy * dc_iscale);

        // Index texture and retrieve color.
        int texture_y = (texture_frac_y >> FRACBITS) & 127;
        byte color = dc_source[texture_y];

        // Re-map color indices from wall texture column
        // using a lighting/special effects LUT.
        color = dc_colormap[color];

        // Draw!
        R_DrawPixel(x, y, color);
        R_DrawPixel(x + 1, y, color);
    }
}
