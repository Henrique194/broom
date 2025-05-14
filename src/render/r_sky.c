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
//  Sky rendering logic. In DOOM, the sky is rendered as a
//  texture map, similar to a wall texture, that wraps horizontally
//  around the player. The default sky texture is 256 columns wide
//  and mapped across the player's entire field of view (FOV), which
//  is 90°. Since the screen width is 320 pixels, the texture is
//  stretched slightly to fit. The mapping ratio is thus: 256 columns
//  correspond to 90°, meaning 1024 columns represent 360°.
//  
//

#include "r_sky.h"
#include "r_main.h"
#include "r_column.h"
#include "r_things.h"

//
// sky mapping
//
int sky_flat;
int sky_tex;
int sky_tex_mid;


void R_DrawSky(const visplane_t* pl) {
    dc_iscale = pspriteiscale >> detailshift;
    dc_texturemid = sky_tex_mid;
    // Sky is always drawn full bright, i.e. colormaps[0] is used.
    // Because of this hack, sky is not affected by INVUL inverse mapping.
    dc_colormap = &colormaps[0];

    for (int x = pl->minx; x <= pl->maxx; x++) {
        dc_yl = pl->top[x];
        dc_yh = pl->bottom[x];

        if (dc_yl <= dc_yh) {
            angle_t col_angle = viewangle + xtoviewangle[x];
            int tex_col = (int) (col_angle >> ANGLETOSKYSHIFT);
            dc_source = R_GetColumn(sky_tex, tex_col);
            dc_x = x;
            colfunc();
        }
    }
}

//
// R_InitSkyMap
// Called whenever the view size changes.
//
void R_InitSkyMap() {
    sky_tex_mid = (SCREENHEIGHT / 2) * FRACUNIT;
}
