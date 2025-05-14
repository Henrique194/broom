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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//


#include <stdlib.h>
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "doomstat.h"
#include "r_local.h"
#include "r_sky.h"

//
// opening
//

// Here comes the obnoxious "visplane".
#define MAXVISPLANES 128
static visplane_t visplanes[MAXVISPLANES];
static visplane_t* lastvisplane;
visplane_t* floorplane;
visplane_t* ceilingplane;

// ?
#define MAXOPENINGS SCREENWIDTH * 64
static short openings[MAXOPENINGS];
short* lastopening;


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short floorclip[SCREENWIDTH];
short ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span initialized to 0 at start
//
static int spanstart[SCREENHEIGHT];

//
// texture mapping
//
static lighttable_t** planezlight;
static fixed_t planeheight;


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes() {
    // opening / clipping determination
    for (int i = 0; i < viewwidth; i++) {
	floorclip[i] = (short) viewheight;
	ceilingclip[i] = -1;
    }

    lastvisplane = visplanes;
    lastopening = openings;
}


static visplane_t* R_NewVisplane(fixed_t height, int pic, int light) {
    if (lastvisplane - visplanes == MAXVISPLANES) {
        I_Error("R_NewVisplane: no more visplanes");
    }
    visplane_t*	new_plane = lastvisplane;
    lastvisplane++;

    new_plane->height = height;
    new_plane->picnum = pic;
    new_plane->lightlevel = light;
    new_plane->minx = SCREENWIDTH;
    new_plane->maxx = -1;
    memset(new_plane->top, 0xff, sizeof(new_plane->top));

    return new_plane;
}

//
// Tries to find the first and oldest compatible visplane with
// given height, texture and light level.
//
static visplane_t* R_FindCompatiblePlane(fixed_t height, int picnum, int lightlevel) {
    for (visplane_t* pl = visplanes; pl < lastvisplane; pl++) {
        if (height == pl->height && picnum == pl->picnum && lightlevel == pl->lightlevel) {
            return pl;
        }
    }
    return NULL;
}

//
// R_FindPlane
//
visplane_t* R_FindPlane(fixed_t height, int picnum, int lightlevel) {
    if (picnum == sky_flat) {
        // All skies map together.
	height = 0;
	lightlevel = 0;
    }
    visplane_t*	pl = R_FindCompatiblePlane(height, picnum, lightlevel);
    if (pl != NULL) {
        return pl;
    }
    return R_NewVisplane(height, picnum, lightlevel);
}

//
// Determines whether a visplane can be used to draw vertical columns
// over the horizontal range [start, stop].
//
// For each X position in the intersection of [start, stop] and the visplane's
// horizontal bounds [minx, maxx], checks if the column at position X
// has already been drawn.
//
// Returns true if all positions in the range are unused and safe to draw;
// otherwise, returns false.
//
static bool R_IsPlaneRangeClear(const visplane_t* pl, int start, int stop) {
    int intrl = (pl->minx > start) ? pl->minx : start;
    int intrh = (pl->maxx < stop) ? pl->maxx : stop;
    for (int x = intrl; x <= intrh; x++) {
        if (pl->top[x] != 0xff) {
            // Column already used at position X; cannot reuse this plane.
            return false;
        }
    }
    return true;
}

//
// Returns a visplane suitable for drawing in the range [start, stop].
//
// If the existing visplane `pl` is clear (unused) in the specified range,
// it is reused and its horizontal bounds (minx/maxx) are updated as needed.
//
// If any part of the range is already occupied, a new visplane is created
// with the same visual properties (height, texture, light level) but with
// the specified bounds.
//
// Returns either the original or a newly allocated visplane.
//
visplane_t* R_GetPlane(visplane_t* pl, int start, int stop) {
    if (R_IsPlaneRangeClear(pl, start, stop)) {
        // Use the same one.
        pl->minx = (start < pl->minx) ? start : pl->minx;
        pl->maxx = (stop > pl->maxx) ? stop : pl->maxx;
        return pl;
    }
    // Make a new visplane.
    visplane_t* new_plane = R_NewVisplane(pl->height, pl->picnum, pl->lightlevel);
    new_plane->minx = start;
    new_plane->maxx = stop;
    return new_plane;
}


static void R_SetColorMap(fixed_t distance) {
    if (fixedcolormap) {
        ds_colormap = fixedcolormap;
        return;
    }

    unsigned index = distance >> LIGHTZSHIFT;
    if (index >= MAXLIGHTZ) {
        index = MAXLIGHTZ - 1;
    }
    ds_colormap = planezlight[index];
}

static void R_SetTextureCoordinates(int x1, fixed_t distance) {
    angle_t x1_angle = xtoviewangle[x1];

    // Calculate length: the hypotenuse with respect to the virtual screen
    fixed_t cosadj = abs(COS(x1_angle));
    fixed_t distance_scale = FixedDiv(FRACUNIT, cosadj);
    fixed_t length = FixedMul(distance, distance_scale);

    angle_t angle = viewangle + x1_angle;

    ds_xfrac = viewx + FixedMul(COS(angle), length);
    ds_yfrac = -viewy - FixedMul(SIN(angle), length);
}

static void R_SetTextureSteps(fixed_t distance) {
    // left to right mapping
    angle_t angle = viewangle - ANG90;

    // scale will be unit scale at SCREENWIDTH/2 distance
    fixed_t base_x_scale = FixedDiv(COS(angle), centerxfrac);
    fixed_t base_y_scale = -FixedDiv(SIN(angle), centerxfrac);

    ds_xstep = FixedMul(distance, base_x_scale);
    ds_ystep = FixedMul(distance, base_y_scale);
}

static void R_SetTextureRenderParams(int y, int x1, int x2, fixed_t distance) {
    R_SetTextureSteps(distance);
    R_SetTextureCoordinates(x1, distance);

    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;
}

static fixed_t R_CalculatePlaneDistance(int y) {
    fixed_t dy = ((y - centery) << FRACBITS) + FRACUNIT/2;
    dy = abs(dy);

    int scaled_width = viewwidth << detailshift;
    fixed_t proj_plane_dist = scaled_width/2 * FRACUNIT;

    fixed_t slope = FixedDiv(proj_plane_dist, dy);

    return FixedMul(planeheight, slope);
}

static void R_DrawPlane(int y, int x1, int x2) {
    fixed_t distance = R_CalculatePlaneDistance(y);
    R_SetTextureRenderParams(y, x1, x2, distance);
    R_SetColorMap(distance);

    // high or low detail
    spanfunc();
}

//
// R_MakeSpans
//
static void R_MakeSpans(int x, int t1, int b1, int t2, int b2) {
    while (t1 < t2 && t1 <= b1) {
        R_DrawPlane(t1, spanstart[t1], x - 1);
        t1++;
    }
    while (b1 > b2 && b1 >= t1) {
        R_DrawPlane(b1, spanstart[b1], x - 1);
        b1--;
    }

    while (t2 < t1 && t2 <= b2) {
	spanstart[t2] = x;
	t2++;
    }
    while (b2 > b1 && b2 >= t2) {
	spanstart[b2] = x;
	b2--;
    }
}

static void R_SetPlaneLighting(const visplane_t* pl) {
    int light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;
    if (light >= LIGHTLEVELS) {
        light = LIGHTLEVELS - 1;
    }
    if (light < 0) {
        light = 0;
    }

    planezlight = zlight[light];
}

static void R_SetPlaneHeight(const visplane_t* pl) {
    planeheight = abs(pl->height - viewz);
}

static void R_SetPlaneTexture(int lumpnum) {
    ds_source = W_CacheLumpNum(lumpnum, PU_STATIC);
}

static void R_DrawFlat(visplane_t* pl) {
    int plane_tex = firstflat + flattranslation[pl->picnum];
    R_SetPlaneTexture(plane_tex);
    R_SetPlaneHeight(pl);
    R_SetPlaneLighting(pl);

    pl->top[pl->maxx + 1] = 0xff;
    pl->top[pl->minx - 1] = 0xff;

    for (int x = pl->minx; x <= pl->maxx + 1; x++) {
        int t1 = pl->top[x - 1];
        int b1 = pl->bottom[x - 1];
        int t2 = pl->top[x];
        int b2 = pl->bottom[x];
        // Even though visplanes are stored as columns, they are
        // rendered as spans. This is done as a performance trick,
        // because spans are constant distant from the view point,
        // so we can do perspective correction once per span.
        R_MakeSpans(x, t1, b1, t2, b2);
    }

    W_ReleaseLumpNum(plane_tex);
}

static void R_CheckOverflow() {
    if (ds_p - drawsegs > MAXDRAWSEGS) {
        I_Error("R_CheckOverflow: drawsegs overflow (%td)", ds_p - drawsegs);
    }
    if (lastvisplane - visplanes > MAXVISPLANES) {
        I_Error("R_CheckOverflow: visplane overflow (%td)", lastvisplane - visplanes);
    }
    if (lastopening - openings > MAXOPENINGS) {
        I_Error("R_CheckOverflow: opening overflow (%td)", lastopening - openings);
    }
}

//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes() {
    R_CheckOverflow();

    for (visplane_t* pl = visplanes; pl < lastvisplane; pl++) {
	if (pl->minx > pl->maxx) {
            continue;
        }
	if (pl->picnum == sky_flat) {
            // Sky flat.
            R_DrawSky(pl);
	} else {
            // Regular flat.
            R_DrawFlat(pl);
        }
    }
}
