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
//     Used to draw player sprites with the green color ramp mapped to others.
//     Could be used with different translation tables, e.g. the lighter colored
//     version of the BaronOfHell, the HellKnight, uses identical sprites,
//     kinda brightened up.
//
//


#include "doomdef.h"
#include "i_system.h"
#include "z_zone.h"
#include "r_local.h"

byte* translationtables;
byte* dc_translation;


void R_DrawTranslatedColumn() {
    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }

    // Here we do an additional index re-mapping.
    for (int y = dc_yl; y <= dc_yh; y++) {
        // Linear interpolate texture coordinate
        int dy = y - centery;
        fixed_t texture_frac_y = dc_texturemid + (dy * dc_iscale);

        // Index texture and retrieve color
        int texture_y = texture_frac_y >> FRACBITS;
        byte color = dc_source[texture_y];

        // Translation tables are used to map certain color ramps to
        // other ones, used with PLAY sprites. Thus, the "green" ramp
        // of the player 0 sprite is mapped to gray, red, black/indigo.
        color = dc_translation[color];

        // Re-map color indices from wall texture column
        // using a lighting/special effects LUT.
        color = dc_colormap[color];

        // Draw!
        R_DrawPixel(dc_x, y, color);
    }
}

void R_DrawTranslatedColumnLow() {
    // Low detail, need to scale by 2.
    int x = dc_x << 1;

    if (dc_yh < dc_yl) {
        return;
    }
    if ((unsigned) x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, x);
    }

    // Here we do an additional index re-mapping.
    for (int y = dc_yl; y <= dc_yh; y++) {
        // Linear interpolate texture coordinate
        int dy = y - centery;
        fixed_t texture_frac_y = dc_texturemid + (dy * dc_iscale);

        // Index texture and retrieve color
        int texture_y = texture_frac_y >> FRACBITS;
        byte color = dc_source[texture_y];

        // Translation tables are used to map certain colorramps
        // to other ones, used with PLAY sprites. Thus the "green"
        // ramp of the player 0 sprite is mapped to gray, red, black/indigo.
        color = dc_translation[color];

        // Re-map color indices from wall texture column
        // using a lighting/special effects LUT.
        color = dc_colormap[color];

        // Draw!
        R_DrawPixel(x, y, color);
        R_DrawPixel(x + 1, y, color);
    }
}

//
// R_InitTranslationTables
// Creates the translation tables to map the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables() {
    translationtables = Z_Malloc(256 * 3, PU_STATIC, NULL);

    // translate just the 16 green colors
    for (int i = 0; i < 256; i++) {
        if (i >= 0x70 && i <= 0x7f) {
            // map green ramp to gray, brown, red
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        } else {
            // Keep all other colors as is.
            translationtables[i] = (byte) i;
            translationtables[i + 256] = (byte) i;
            translationtables[i + 512] = (byte) i;
        }
    }
}
