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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//


#include <stdlib.h>
#include "d_loop.h"
#include "m_menu.h"
#include "r_local.h"
#include "r_sky.h"


// Field of View.
// Fineangles in the SCREENWIDTH wide window.
#define FOV ANG90


int viewangleoffset;

// increment every time a check is made
int validcount = 1;


lighttable_t *fixedcolormap;

int centerx;
int centery;

fixed_t centerxfrac;
fixed_t centeryfrac;
fixed_t projection;


fixed_t viewx;
fixed_t viewy;
fixed_t viewz;

angle_t viewangle;

fixed_t viewcos;
fixed_t viewsin;

player_t *viewplayer;

// 0 = high, 1 = low
int detailshift;

//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
int viewangletox[FINEANGLES / 2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[SCREENWIDTH + 1];

lighttable_t* scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* scalelightfixed[MAXLIGHTSCALE];
lighttable_t* zlight[LIGHTLEVELS][MAXLIGHTZ];

// bumped light from gun blasts
int extralight;


void (*colfunc)(void);
void (*basecolfunc)(void);
void (*fuzzcolfunc)(void);
void (*transcolfunc)(void);
void (*spanfunc)(void);


//
// R_PointOnSide
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int R_PointOnSide(fixed_t x, fixed_t y, const node_t* node) {
    if (!node->dx) {
	if (x <= node->x) {
            return node->dy > 0;
        }
	return node->dy < 0;
    }
    if (!node->dy) {
	if (y <= node->y) {
            return node->dx < 0;
        }
	return node->dx > 0;
    }

    fixed_t dx = (x - node->x);
    fixed_t dy = (y - node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000) {
	if  ((node->dy ^ dx) & 0x80000000) {
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    fixed_t left = FixedMul(node->dy >> FRACBITS, dx);
    fixed_t right = FixedMul(dy, node->dx >> FRACBITS);

    if (right < left) {
	// front side
	return 0;
    }
    // back side
    return 1;			
}

//
// Returns true if the point (x, y) is on the back side of the line or on the
// line itself. Returns false if the point is on the front side of the line.
//
bool R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t* line) {
    fixed_t lx = line->v1->x;
    fixed_t ly = line->v1->y;

    fixed_t ldx = line->v2->x - lx;
    fixed_t ldy = line->v2->y - ly;

    if (ldx == 0) {
	if (x <= lx) {
            return ldy > 0;
        }
	return ldy < 0;
    }
    if (ldy == 0) {
	if (y <= ly) {
            return ldx < 0;
        }
	return ldx > 0;
    }

    fixed_t dx = (x - lx);
    fixed_t dy = (y - ly);

    // Try to quickly decide by looking at sign bits.
    if ((ldy ^ ldx ^ dx ^ dy) & 0x80000000) {
	if  ((ldy ^ dx) & 0x80000000) {
	    // (left is negative)
	    return true;
	}
	return false;
    }

    // Let L = (ldx, ldy) and D = (dx, dy) be vectors representing the line
    // and the point. The cross product of L and D is then:
    //
    //         | ldx  ldy |
    // L x D = |          | = (ldx * dy) - (ldy * dx) = det
    //         |  dy   dx |
    //
    // If det >= 0, the point lies on the left side (or on the line);
    // if det < 0, the point lies on the right side.
    fixed_t right = FixedMul(ldx >> FRACBITS, dy);
    fixed_t left = FixedMul(ldy >> FRACBITS, dx);

    if (right >= left) {
        // back side
        return true;
    }
    // front side
    return false;
}

angle_t R_PointToAngle(fixed_t x, fixed_t y) {
    x -= viewx;
    y -= viewy;
    return ATAN2(x, y);
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
    viewx = x1;
    viewy = y1;
    return R_PointToAngle(x2, y2);
}


fixed_t R_PointToDist(fixed_t x, fixed_t y) {
    fixed_t dx = abs(x - viewx);
    fixed_t dy = abs(y - viewy);
    if (dy > dx) {
        // We want the division dy/dx to never be bigger than FRACUNIT.
        // So swap dy and dy here.
        fixed_t temp = dx;
        dx = dy;
        dy = temp;
    }
    // Fix crashes in udm1.wad.
    fixed_t frac = (dx != 0) ? FixedDiv(dy, dx) : 0;
    int angle = (int) ATAN(frac, FRACUNIT);
    return FixedDiv(dx, COS(angle));
}

// 64
#define MAX_SCALE (64 * FRACUNIT)

// 0.00390625
#define MIN_SCALE (256)

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale for the current line (horizontal span)
// at the given angle. rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle(angle_t visangle) {
    angle_t anglea = visangle - viewangle;
    angle_t angleb = visangle - rw_normalangle;

    // The scale calculation is straightforward:
    // proj_plane_dist = projection / cos(anglea)
    // vertex_dist = rw_distance / cos(angleb)
    // scale = proj_plane_dist / vertex_dist
    // This simplifies to:
    // scale = (projection * cos(angleb)) / (rw_distance * cos(anglea))

    // Both cosines are always positive.
    fixed_t sinea = COS(anglea);
    fixed_t sineb = COS(angleb);

    fixed_t num = FixedMul(projection, sineb) << detailshift;
    fixed_t den = FixedMul(rw_distance, sinea);

    //
    // To calculate the scale, we need to perform FixedDiv(num, den). However,
    // before doing so, we must ensure that this operation doesn't overflow.
    // Let "inv_scale = FixedDiv(den, num)", which represents the inverse of
    // the desired scale. We can determine if calculating the scale will
    // overflow by checking if "inv_scale" is less than or equal to 1. This
    // works because the scale is computed as "scale = FRACUNIT / inv_scale",
    // so if "inv_scale" is 0 or 1, the calculation will overflow.
    //
    // But there's an additional concern:
    // what if the division to compute "inv_scale" itself overflows?
    //
    // This is a valid concern, and the only way to safely avoid overflow in
    // this case is to avoid using "FixedDiv" altogether and instead perform
    // the division using a low-level operation:
    //     inv_scale <= 1
    //     ((den << FRACBITS) / num) <= 1
    //     (den << FRACBITS) <= num
    //     den <= (num >> FRACBITS)
    //
    if (den <= (num >> FRACBITS)) {
        // Overflows, so return MAX_SCALE.
        return MAX_SCALE;
    }

    fixed_t scale = FixedDiv(num, den);
    if (scale > MAX_SCALE) {
        return MAX_SCALE;
    }
    if (scale < MIN_SCALE) {
        return MIN_SCALE;
    }
    return scale;
}

//
// R_InitTextureMapping
//
static void R_InitTextureMapping() {
    // Use tangent table to generate viewangletox:
    // viewangletox will give the next greatest x after the view angle.
    //
    // Calculate focallength so FOV angles covers SCREENWIDTH.
    fixed_t tan = TAN(FOV / 2);
    fixed_t focal_length = FixedDiv(centerxfrac, tan);

    for (int i = 0; i < FINEANGLES / 2; i++) {
        if (finetangent[i] > FRACUNIT * 2) {
            // Off the left edge
            viewangletox[i] = -1;
            continue;
        }
        if (finetangent[i] < -FRACUNIT * 2) {
            // Off the right edge
            viewangletox[i] = viewwidth + 1;
            continue;
        }

        fixed_t x_frac = centerxfrac - FixedMul(finetangent[i], focal_length);
        // Round up when converting from fixed-point to int.
        int x = (x_frac + FRACUNIT - 1) >> FRACBITS;
        if (x < -1) {
            x = -1;
        } else if (x > viewwidth + 1) {
            x = viewwidth + 1;
        }
        viewangletox[i] = x;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    // xtoviewangle will give the smallest view angle that maps to x.
    for (int x = 0; x <= viewwidth; x++) {
        int i = 0;
        while (viewangletox[i] > x) {
            i++;
        }
        xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
    }
    
    // Take out the fencepost cases from viewangletox.
    for (int i = 0; i < FINEANGLES/2; i++) {
        if (viewangletox[i] == -1) {
            viewangletox[i] = 0;
        } else if (viewangletox[i] == viewwidth + 1) {
            viewangletox[i] = viewwidth;
        }
    }
	
    clipangle = xtoviewangle[0];
}


//
// R_InitLightTables
// Only inits the zlight table, because the scalelight table changes with view size.
//
#define DISTMAP 2

void R_InitLightTables(void) {
    int level;
    int startmap;
    int scale;

    // Calculate the light levels to use for each level / distance combination.
    for (int i = 0; i < LIGHTLEVELS; i++) {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (int j = 0; j < MAXLIGHTZ; j++) {
            scale =
                FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (j + 1) << LIGHTZSHIFT);
            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale / DISTMAP;
            if (level < 0) {
                level = 0;
            }
	    if (level >= NUMCOLORMAPS) {
                level = NUMCOLORMAPS - 1;
            }

            zlight[i][j] = colormaps + level * 256;
        }
    }
}



//
// R_SetViewSize
// Do not really change anything here, because it might be in the
// middle of a refresh. The change will take effect next refresh.
//
bool setsizeneeded;
static int setblocks;
static int setdetail;

void R_SetViewSize(int blocks, int detail) {
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

//
// Calculate the light levels to use for each level / scale combination.
//
static void R_UpdateLight() {
    for (int i = 0; i < LIGHTLEVELS; i++) {
        int startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;

        for (int j = 0; j < MAXLIGHTSCALE; j++) {
            int level = startmap -
                    (j * SCREENWIDTH / (viewwidth << detailshift)) / DISTMAP;
            if (level < 0) {
                level = 0;
            }
            if (level >= NUMCOLORMAPS) {
                level = NUMCOLORMAPS - 1;
            }

            scalelight[i][j] = colormaps + level * 256;
        }
    }
}

static void R_UpdateThingClippingArray() {
    for (int i = 0; i < viewwidth; i++) {
        screenheightarray[i] = (short) viewheight;
    }
}

//
// psprite scales
//
static void R_UpdateSpriteScales() {
    pspritescale = FRACUNIT * viewwidth / SCREENWIDTH;
    pspriteiscale = FRACUNIT * SCREENWIDTH / viewwidth;
}

static void R_UpdateDrawFuncs() {
    if (detailshift) {
        colfunc = &R_DrawColumnLow;
        basecolfunc = &R_DrawColumnLow;
        fuzzcolfunc = &R_DrawFuzzColumnLow;
        transcolfunc = &R_DrawTranslatedColumnLow;
        spanfunc = &R_DrawSpanLow;
        return;
    }
    colfunc = &R_DrawColumn;
    basecolfunc = &R_DrawColumn;
    fuzzcolfunc = &R_DrawFuzzColumn;
    transcolfunc = &R_DrawTranslatedColumn;
    spanfunc = &R_DrawSpan;
}

static void R_UpdateProjectionPlane() {
    if (setblocks == 11) {
        scaledviewwidth = SCREENWIDTH;
        viewheight = SCREENHEIGHT;
    } else {
        scaledviewwidth = setblocks * 32;
        viewheight = (setblocks * 168 / 10) & ~7;
    }

    detailshift = setdetail;
    viewwidth = scaledviewwidth >> detailshift;

    centery = viewheight / 2;
    centerx = viewwidth / 2;
    centerxfrac = centerx << FRACBITS;
    centeryfrac = centery << FRACBITS;
    projection = centerxfrac;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize() {
    setsizeneeded = false;

    R_UpdateProjectionPlane();
    R_UpdateDrawFuncs();
    R_UpdateViewWindow(scaledviewwidth, viewheight);
    R_InitTextureMapping();
    R_UpdateSpriteScales();
    R_UpdateThingClippingArray();
    R_UpdateLight();
}

//
// R_Init
//
void R_Init(void) {
    R_InitData();
    printf(".");
    printf(".");
    printf(".");
    R_SetViewSize(screenblocks, detailLevel);
    printf(".");
    R_InitLightTables();
    printf(".");
    R_InitSkyMap();
    R_InitTranslationTables();
    printf(".");
}

//
// R_PointInSubsector
//
subsector_t* R_PointInSubsector(fixed_t x, fixed_t y) {
    // Single subsector is a special case.
    if (!numnodes) {
        return &subsectors[0];
    }

    int nodenum = numnodes - 1;
    while (!(nodenum & NF_SUBSECTOR)) {
        const node_t* node = &nodes[nodenum];
        int side = R_PointOnSide(x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// Clear buffers
//
static void R_CleanUpState() {
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();
}

    //
// R_SetupFrame
//
static void R_SetupFrame(player_t* player) {
    viewplayer = player;
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewangle = player->mo->angle + viewangleoffset;
    extralight = player->extralight;
    viewz = player->viewz;

//    viewx = 96993553;
//    viewy = 64204515;
//    viewz = 11599872;
//    viewangle = 3295150080;

    viewsin = SIN(viewangle);
    viewcos = COS(viewangle);

    if (player->fixedcolormap) {
	fixedcolormap = &colormaps[player->fixedcolormap * 256];
	walllights = scalelightfixed;
	for (int i = 0; i < MAXLIGHTSCALE; i++) {
            scalelightfixed[i] = fixedcolormap;
        }
    } else {
        fixedcolormap = NULL;
    }

    validcount++;
}

//
// R_RenderView
//
void R_RenderPlayerView(player_t* player) {
    R_SetupFrame(player);
    R_CleanUpState();

    // Check for new console commands.
    NetUpdate();

    // Render solid walls and portals (two-sided lines that connect sectors).
    // These are always perpendicular to the player's ground plane and
    // define the world boundary.
    R_RenderSectors();

    // Check for new console commands.
    NetUpdate();

    // Render floors/ceilings.
    // These are always perpendicular to the player's vertical plane.
    R_DrawPlanes();
    
    // Check for new console commands.
    NetUpdate();

    // Render map objects and partially transparent walls.
    R_DrawMasked();

    // Check for new console commands.
    NetUpdate();
}
