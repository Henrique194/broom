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
//     The actual span/column drawing functions. Here find the main potential
//     for optimization, e.g. inline assembly, different algorithms. All drawing
//     to the view buffer is accomplished through R_DrawPixel. The other refresh
//     files only know about coordinates, not the architecture of the frame
//     buffer. Conveniently, the frame buffer is a linear one, and we need only
//     the base address, and the total size == width*height*depth/8.
//


#include "r_local.h"


int viewwidth;
int scaledviewwidth;
int viewheight;
int viewwindowx;
int viewwindowy;


static int R_ScreenCoordinate(int x, int y) {
    // Apply viewport correction.
    x += viewwindowx;
    y += viewwindowy;
    return x + (y * SCREENWIDTH);
}

void R_DrawPixel(int x, int y, pixel_t color) {
    int screen_spot = R_ScreenCoordinate(x, y);
    I_VideoBuffer[screen_spot] = color;
}

pixel_t R_GetPixel(int x, int y) {
    int screen_spot = R_ScreenCoordinate(x, y);
    return I_VideoBuffer[screen_spot];
}

//
// R_UpdateViewWindow
// Init viewport correction to handle screen resize.
//
void R_UpdateViewWindow(int width, int height) {
    // Handle resize, e.g. smaller view windows with border and/or status bar.
    viewwindowx = (SCREENWIDTH - width) >> 1;

    // Same with base row offset.
    if (width == SCREENWIDTH) {
        viewwindowy = 0;
    } else {
        viewwindowy = (SCREENHEIGHT - SBARHEIGHT - height) >> 1;
    }
}
