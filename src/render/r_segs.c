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
//	All the clipping: columns, horizontal spans, sky columns.
//


#include <stdio.h>
#include <stdlib.h>
#include "i_system.h"
#include "doomstat.h"
#include "r_local.h"


// OPTIMIZE: closed two sided lines as single sided


angle_t rw_normalangle;
// angle to line origin
int rw_angle1;

//
// regular wall
//
fixed_t rw_distance;


lighttable_t** walllights;


// True if any of the segs textures might be visible.
static bool segtextured;

// False if the back side is the same plane.
static bool markfloor;
static bool markceiling;

static bool maskedtexture;
static int toptexture;
static int bottomtexture;
static int midtexture;

static fixed_t rw_scalestep;
static fixed_t rw_midtexturemid;
static fixed_t rw_toptexturemid;
static fixed_t rw_bottomtexturemid;

static int worldtop;
static int worldbottom;
static int worldhigh;
static int worldlow;

static fixed_t pixhigh;
static fixed_t pixlow;
static fixed_t pixhighstep;
static fixed_t pixlowstep;

static fixed_t topfrac;
static fixed_t topstep;

static fixed_t bottomfrac;
static fixed_t bottomstep;

static int rw_x;
static int rw_stopx;
static fixed_t rw_offset;
static fixed_t rw_scale;
static fixed_t rw_scale2;

static short* maskedtexturecol;


//
// Calculate inverse scale: from screen space to world space.
//
static void R_CalculateInverseScale(fixed_t scale) {
    // This should be FixedDiv(FRACUNIT, scale), but in order to avoid a
    // possible overflow, we exploit the wrapping behaviour of unsigned int
    // and use this hack instead.
    dc_iscale = (fixed_t) (0xffffffffu / (unsigned) scale);
}

static const column_t* R_GetMaskedColumn(int x) {
    int tex = texturetranslation[curline->sidedef->midtexture];
    const byte* col_data = R_GetColumn(tex, maskedtexturecol[x]);
    return (const column_t *) (col_data - 3);
}

static void R_CalculateMaskedColumnColormap() {
    if (fixedcolormap) {
        dc_colormap = fixedcolormap;
        return;
    }
    unsigned int index = spryscale >> LIGHTSCALESHIFT;
    if (index >=  MAXLIGHTSCALE) {
        index = MAXLIGHTSCALE - 1;
    }
    dc_colormap = walllights[index];
}

static void R_PrepareMaskedColumnRender(const drawseg_t* ds, int x) {
    // Linear interpolate sprite scale
    int dx = x - ds->x1;
    spryscale = ds->scale1 + (dx * rw_scalestep);

    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
    dc_x = x;

    R_CalculateMaskedColumnColormap();
    R_CalculateInverseScale(spryscale);
}

static void R_CalculateMaskedMidTexture() {
    if (curline->linedef->flags & ML_DONTPEGBOTTOM) {
        if (frontsector->floorheight > backsector->floorheight) {
            dc_texturemid = frontsector->floorheight;
        } else {
            dc_texturemid = backsector->floorheight;
        }
        int texnum = texturetranslation[curline->sidedef->midtexture];
        dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
    } else {
        if (frontsector->ceilingheight < backsector->ceilingheight) {
            dc_texturemid = frontsector->ceilingheight;
        } else {
            dc_texturemid = backsector->ceilingheight;
        }
        dc_texturemid = dc_texturemid - viewz;
    }

    dc_texturemid += curline->sidedef->rowoffset;
}

static void R_CalculateWallLights() {
    int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT) + extralight;
    if (curline->v1->y == curline->v2->y) {
        lightnum--;
    } else if (curline->v1->x == curline->v2->x) {
        lightnum++;
    }

    if (lightnum < 0) {
        walllights = scalelight[0];
    } else if (lightnum >= LIGHTLEVELS) {
        walllights = scalelight[LIGHTLEVELS-1];
    } else {
        walllights = scalelight[lightnum];
    }
}

static void R_PrepareMaskedSegmentRender(const drawseg_t* ds) {
    curline = ds->curline;
    frontsector = curline->frontsector;
    backsector = curline->backsector;

    R_CalculateWallLights();

    maskedtexturecol = ds->maskedtexturecol;
    rw_scalestep = ds->scalestep;
    mceilingclip = ds->sprtopclip;
    mfloorclip = ds->sprbottomclip;

    // find positioning
    R_CalculateMaskedMidTexture();
}


//
// R_RenderMaskedSegRange
//
void R_RenderMaskedSegRange(const drawseg_t* ds, int x1, int x2) {
    R_PrepareMaskedSegmentRender(ds);

    // Draw the columns.
    for (int x = x1; x <= x2; x++) {
        if (maskedtexturecol[x] == SHRT_MAX) {
            continue;
        }
        // Draw the texture.
        R_PrepareMaskedColumnRender(ds, x);
        const column_t* col = R_GetMaskedColumn(x);
        R_DrawMaskedColumn(col);
        maskedtexturecol[x] = SHRT_MAX;
    }
}


#define HEIGHTBITS 12
#define HEIGHTUNIT (1 << HEIGHTBITS)

static void R_RenderBottomTexture(int x, int yh, fixed_t tex_col) {
    if (bottomtexture == 0) {
        // no bottom wall
        if (markfloor) {
            floorclip[x] = (short) (yh + 1);
        }
        return;
    }

    // bottom wall
    int yl = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
    pixlow += pixlowstep;

    if (yl <= ceilingclip[x]) {
        // no space above wall
        yl = ceilingclip[x] + 1;
    }
    if (yl > yh) {
        floorclip[x] = (short) (yh + 1);
        return;
    }
    floorclip[x] = (short) yl;

    dc_yl = yl;
    dc_yh = yh;
    dc_texturemid = rw_bottomtexturemid;
    dc_source = R_GetColumn(bottomtexture, tex_col);
    colfunc();
}

static void R_RenderTopTexture(int x, int yl, fixed_t tex_col) {
    if (toptexture == 0) {
        // no top wall
        if (markceiling) {
            ceilingclip[x] = (short) (yl - 1);
        }
        return;
    }

    // top wall
    int yh = pixhigh >> HEIGHTBITS;
    pixhigh += pixhighstep;

    if (floorclip[x] <= yh) {
        yh = floorclip[x] - 1;
    }
    if (yh < yl) {
        ceilingclip[x] = (short) (yl - 1);
        return;
    }
    ceilingclip[x] = (short) yh;

    dc_yl = yl;
    dc_yh = yh;
    dc_texturemid = rw_toptexturemid;
    dc_source = R_GetColumn(toptexture, tex_col);
    colfunc();
}

static void R_RenderMidTexture(int x, int yl, int yh, fixed_t tex_col) {
    dc_yl = yl;
    dc_yh = yh;
    dc_texturemid = rw_midtexturemid;
    dc_source = R_GetColumn(midtexture, tex_col);
    colfunc();
    // The following lines are technically redundant because the depth clipping
    // stage ensures that no additional lines will be drawn in the same screen
    // column once it has been filled with a single-sided line (solid wall).
    ceilingclip[x] = (short) viewheight;
    floorclip[x] = -1;
}

//
// calculate lighting
//
static void R_CalculateColormap(fixed_t scale) {
    unsigned index = scale >> LIGHTSCALESHIFT;
    if (index >= MAXLIGHTSCALE) {
        index = MAXLIGHTSCALE - 1;
    }
    dc_colormap = walllights[index];
}

static void R_PrepareColumnRender(int start_x, int x) {
    if (segtextured) {
        int dx = x - start_x;
        fixed_t scale = rw_scale + (dx * rw_scalestep);
        R_CalculateColormap(scale);
        R_CalculateInverseScale(scale);
        dc_x = x;
    }
}

//
// Calculate the texture
//
static fixed_t R_CalculateTextureColumn(int x) {
    if (!segtextured) {
        return 0;
    }
    angle_t texture_offset_angle = (viewangle + xtoviewangle[x]) - rw_normalangle;
    fixed_t tan = finetangent[(texture_offset_angle + ANG90) >> ANGLETOFINESHIFT];
    fixed_t tex_col = rw_offset - FixedMul(tan, rw_distance);
    tex_col >>= FRACBITS;
    return tex_col;
}

static void R_RenderColumn(int start_x, int x, int yl, int yh) {
    fixed_t tex_col = R_CalculateTextureColumn(x);
    R_PrepareColumnRender(start_x, x);

    if (midtexture) {
        // Single sided line.
        R_RenderMidTexture(x, yl, yh, tex_col);
        return;
    }
    // Two-sided line.
    R_RenderTopTexture(x, yl, tex_col);
    R_RenderBottomTexture(x, yh, tex_col);
    if (maskedtexture) {
        // save texturecol for backdrawing of masked mid texture
        maskedtexturecol[x] = (short) tex_col;
    }
}

static void R_MarkFloorPlane(int x, int yh) {
    int bottom = floorclip[x] - 1;
    int top = yh + 1;
    if (top <= ceilingclip[x]) {
        top = ceilingclip[x] + 1;
    }

    if (top <= bottom) {
        floorplane->top[x] = (byte) top;
        floorplane->bottom[x] = (byte) bottom;
    }
}

static void R_MarkCeilingPlane(int x, int yl) {
    int top = ceilingclip[x] + 1;
    int bottom = yl - 1;
    if (bottom >= floorclip[x]) {
        bottom = floorclip[x] - 1;
    }

    if (top <= bottom) {
        ceilingplane->top[x] = (byte) top;
        ceilingplane->bottom[x] = (byte) bottom;
    }
}

static void R_MarkPlanes(int x, int yl, int yh) {
    if (markceiling) {
        R_MarkCeilingPlane(x, yl);
    }
    if (markfloor) {
        R_MarkFloorPlane(x, yh);
    }
}

static int R_ProjectBottomScreen(int start_x, int x) {
    // Linear interpolate bottom.
    int dx = x - start_x;
    fixed_t bottom = bottomfrac + (dx * bottomstep);

    // Round down yh and project in screen space.
    int yh = bottom >> HEIGHTBITS;
    if (yh >= floorclip[x]) {
        yh = floorclip[x] - 1;
    }
    return yh;
}

static int R_ProjectTopScreen(int start_x, int x) {
    // Linear interpolate top.
    int dx = x - start_x;
    fixed_t top = topfrac + (dx * topstep);

    // Round up yl and project in screen space.
    int yl = (top + HEIGHTUNIT - 1) >> HEIGHTBITS;
    if (yl < ceilingclip[x] + 1) {
        // No space above wall.
        yl = ceilingclip[x] + 1;
    }
    return yl;
}

//
// R_RenderSegment
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
//
static void R_RenderSegment() {
    int start = rw_x;

    for (int x = start; x < rw_stopx; x++) {
        int yl = R_ProjectTopScreen(start, x);
        int yh = R_ProjectBottomScreen(start, x);

        R_MarkPlanes(x, yl, yh);
        R_RenderColumn(start, x, yl, yh);
    }
}


static void R_UpdateOpening(int start_x, const short clip[SCREENWIDTH]) {
    int dx = rw_stopx - start_x;
    size_t size = dx * sizeof(*lastopening);
    memcpy(lastopening, &clip[start_x], size);
    lastopening += dx;
}

static void R_SetSpriteBottomSilhouetteClip(int start_x) {
    if (backsector == NULL) {
        ds_p->sprbottomclip = negonearray;
        return;
    }
    if (backsector->ceilingheight <= frontsector->floorheight) {
        ds_p->sprbottomclip = negonearray;
        return;
    }
    if ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) {
        ds_p->sprbottomclip = lastopening - start_x;
        R_UpdateOpening(start_x, floorclip);
        return;
    }
    ds_p->sprbottomclip = NULL;
}

static void R_SetSpriteTopSilhouetteClip(int start_x) {
    if (backsector == NULL) {
        ds_p->sprtopclip = screenheightarray;
        return;
    }
    if (backsector->floorheight >= frontsector->ceilingheight) {
        ds_p->sprtopclip = screenheightarray;
        return;
    }
    if ((ds_p->silhouette & SIL_TOP) || maskedtexture) {
        ds_p->sprtopclip = lastopening - start_x;
        R_UpdateOpening(start_x, ceilingclip);
        return;
    }
    ds_p->sprtopclip = NULL;
}

static void R_SetSpriteSilhouetteClip(int start_x) {
    R_SetSpriteTopSilhouetteClip(start_x);
    R_SetSpriteBottomSilhouetteClip(start_x);
}

static void R_SetSpriteBottomSilhouette() {
    if (backsector == NULL) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
        return;
    }
    if (frontsector->floorheight > backsector->floorheight) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = frontsector->floorheight;
    } else if (backsector->floorheight > viewz) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }
    if (backsector->ceilingheight <= frontsector->floorheight) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }
    if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM)) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }
}

static void R_SetSpriteTopSilhouette() {
    if (backsector == NULL) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
        return;
    }
    if (frontsector->ceilingheight < backsector->ceilingheight) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = frontsector->ceilingheight;
    } else if (backsector->ceilingheight < viewz) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }
    if (backsector->floorheight >= frontsector->ceilingheight) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }
    if (maskedtexture && !(ds_p->silhouette & SIL_TOP)) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }
}

static void R_SetSpriteSilhouette() {
    ds_p->silhouette = SIL_NONE;
    R_SetSpriteTopSilhouette();
    R_SetSpriteBottomSilhouette();
}

static void R_SetSpriteMaskedTextureColumn() {
    if (backsector != NULL && sidedef->midtexture) {
        ds_p->maskedtexturecol = maskedtexturecol;
    } else {
        ds_p->maskedtexturecol = NULL;
    }
}

static void R_SetSpriteScales(int start, int stop) {
    ds_p->x1 = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    ds_p->scale1 = rw_scale;
    ds_p->scale2 = rw_scale2;
    ds_p->scalestep = rw_scalestep;
}

static void R_SaveSpriteClippingInfo(int start, int stop) {
    R_SetSpriteScales(start, stop);
    R_SetSpriteMaskedTextureColumn();
    R_SetSpriteSilhouette();
    R_SetSpriteSilhouetteClip(start);

    ds_p++;
}


static void R_SetPlanes() {
    // If a floor/ceiling plane is on the wrong side of the view plane,
    // it is definitely invisible and doesn't need to be marked.
    if (frontsector->floorheight >= viewz) {
        // Above view plane.
        markfloor = false;
    }
    if (frontsector->ceilingheight <= viewz && frontsector->ceilingpic != sky_flat) {
        // Below view plane.
        markceiling = false;
    }

    if (markceiling) {
        ceilingplane = R_GetPlane(ceilingplane, rw_x, rw_stopx - 1);
    }
    if (markfloor) {
        floorplane = R_GetPlane(floorplane, rw_x, rw_stopx - 1);
    }
}

//
// Calculate incremental stepping values for texture edges.
//
static void R_CalculateSteps() {
    // Convert 16.16 to 20.12 fixed point format
    worldtop >>= 4;
    worldbottom >>= 4;

    topstep = -FixedMul(rw_scalestep, worldtop);
    topfrac = (centeryfrac >> 4) - FixedMul(worldtop, rw_scale);

    bottomstep = -FixedMul(rw_scalestep, worldbottom);
    bottomfrac = (centeryfrac >> 4) - FixedMul(worldbottom, rw_scale);

    if (backsector) {
        // Convert 16.16 to 20.12 fixed point format
        worldhigh >>= 4;
        worldlow >>= 4;

        if (worldhigh < worldtop) {
            pixhigh = (centeryfrac >> 4) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(rw_scalestep, worldhigh);
        }
        if (worldlow > worldbottom) {
            pixlow = (centeryfrac >> 4) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(rw_scalestep, worldlow);
        }
    }
}

static int R_ApplyFakeContrast(int light_level) {
    if (curline->v1->y == curline->v2->y) {
        // Walls parallel to the East-West axis are darker.
        return light_level - 1;
    }
    if (curline->v1->x == curline->v2->x) {
        // Walls parallel to the North-South axis are brighter.
        return light_level + 1;
    }
    return light_level;
}

static int R_CalculateLightLevel() {
    int sector_light = frontsector->lightlevel >> LIGHTSEGSHIFT;
    int light_level = sector_light + extralight;
    light_level = R_ApplyFakeContrast(light_level);
    if (light_level < 0) {
        return 0;
    }
    if (light_level >= LIGHTLEVELS) {
        return LIGHTLEVELS - 1;
    }
    return light_level;
}

// calculate light table use different light tables
// for horizontal / vertical / diagonal
// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
static void R_CalculateLightTable() {
    if (fixedcolormap) {
        // Already calculated.
        return;
    }
    int light_level = R_CalculateLightLevel();
    walllights = scalelight[light_level];
}

static void R_CalculateTextureOffset() {
    if (!segtextured) {
        // Only needed for textured lines.
        return;
    }
    angle_t offset_angle = rw_normalangle - rw_angle1;
    if (offset_angle > ANG180) {
        offset_angle = -offset_angle;
    }
    if (offset_angle > ANG90) {
        offset_angle = ANG90;
    }
    fixed_t sine_val = SIN(offset_angle);
    fixed_t hyp = R_PointToDist(curline->v1->x, curline->v1->y);
    rw_offset = FixedMul(hyp, sine_val);
    if (rw_normalangle - rw_angle1 < ANG180) {
        rw_offset = -rw_offset;
    }
    // At this point, rw_offset is the distance along the wall's infinite line
    // between the segment's start vertex and the point on the line closest to
    // the player. This value is used later to determine which column of the
    // texture should be drawn for a given horizontal screen position. However,
    // we must still adjust for the segment’s position within the full linedef
    // to ensure the texture remain visually continuous across split segments.
    rw_offset += curline->offset;
    // Also add any additional texture shift defined by the map designer.
    rw_offset += sidedef->textureoffset;
}

static void R_CalculateTextureOneSide() {
    midtexture = texturetranslation[sidedef->midtexture];
    // A single sided line is terminal, so it must mark ends.
    markfloor = true;
    markceiling = true;
    if (linedef->flags & ML_DONTPEGBOTTOM) {
        // Bottom of texture at bottom.
        // If the "lower unpegged" flag is set on the linedef, the texture
        // is "pegged" to the bottom of the wall. That is, the bottom of the
        // texture is located at the floor. The texture is drawn upwards from
        // here. This is commonly used in the jambs in doors; because Doom's
        // engine treats each door as a "rising ceiling", a doorjamb pegged to
        // the top of the wall would "rise up" with the door.
        fixed_t vtop = frontsector->floorheight + textureheight[sidedef->midtexture];
        rw_midtexturemid = vtop - viewz;
    } else {
        // Top of texture at top.
        // The texture is "pegged" to the top of the wall. That is to say,
        // the top of the texture is at the ceiling and continues downward
        // to the floor.
        rw_midtexturemid = worldtop;
    }
    rw_midtexturemid += sidedef->rowoffset;
}

static void R_CalculateTextureTwoSide() {
    worldhigh = backsector->ceilingheight - viewz;
    worldlow = backsector->floorheight - viewz;

    // Hack to allow height changes in outdoor areas,
    if (frontsector->ceilingpic == sky_flat && backsector->ceilingpic == sky_flat) {
        worldtop = worldhigh;
    }

    if (worldlow != worldbottom
        || backsector->floorpic != frontsector->floorpic
        || backsector->lightlevel != frontsector->lightlevel) {
        markfloor = true;
    } else {
        // same plane on both sides
        markfloor = false;
    }

    if (worldhigh != worldtop
        || backsector->ceilingpic != frontsector->ceilingpic
        || backsector->lightlevel != frontsector->lightlevel) {
        markceiling = true;
    } else {
        // same plane on both sides
        markceiling = false;
    }

    if (backsector->ceilingheight <= frontsector->floorheight
        || backsector->floorheight >= frontsector->ceilingheight) {
        // closed door
        markceiling = true;
        markfloor = true;
    }

    if (worldhigh < worldtop) {
        // top texture
        toptexture = texturetranslation[sidedef->toptexture];
        if (linedef->flags & ML_DONTPEGTOP) {
            // Top of texture at top.
            // If the "upper unpegged" flag is set on the linedef, the upper
            // texture will begin at the higher ceiling and be drawn downwards.
            rw_toptexturemid = worldtop;
        } else {
            // Bottom of texture.
            fixed_t vtop = backsector->ceilingheight + textureheight[sidedef->toptexture];
            rw_toptexturemid = vtop - viewz;
        }
    }
    if (worldlow > worldbottom) {
        // bottom texture
        bottomtexture = texturetranslation[sidedef->bottomtexture];

        if (linedef->flags & ML_DONTPEGBOTTOM) {
            // Bottom of texture at bottom.
            // Top of texture at top.
            // If the "lower unpegged" flag is set on the linedef, the lower
            // texture will be drawn as if it began at the higher ceiling and
            // continued downwards.
            rw_bottomtexturemid = worldtop;
        } else {
            // top of texture at top
            rw_bottomtexturemid = worldlow;
        }
    }
    rw_toptexturemid += sidedef->rowoffset;
    rw_bottomtexturemid += sidedef->rowoffset;

    // allocate space for masked texture tables
    if (sidedef->midtexture) {
        // masked midtexture
        maskedtexture = true;
        maskedtexturecol = lastopening - rw_x;
        lastopening += rw_stopx - rw_x;
    }
}

// calculate rw_distance for scale calculation
static void R_CalculateRwDistance() {
    rw_normalangle = curline->angle + ANG90;
    angle_t offsetangle = abs((int)rw_normalangle - (int)rw_angle1);

    if (offsetangle > ANG90) {
        offsetangle = ANG90;
    }

    angle_t dist_angle = ANG90 - offsetangle;
    fixed_t sine_val = SIN(dist_angle);
    fixed_t hyp = R_PointToDist(curline->v1->x, curline->v1->y);
    rw_distance = FixedMul(hyp, sine_val);
}

//
// Calculate scale: from world space to screen space
//
static void R_CalculateScales(int start, int stop) {
    R_CalculateRwDistance();

    rw_x = start;
    rw_stopx = stop + 1;

    // Calculate scale at both ends and step.
    rw_scale = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[start]);
    if (stop > start) {
        rw_scale2 = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[stop]);
        rw_scalestep = (rw_scale2 - rw_scale) / (stop - start);
    } else {
        rw_scale2 = rw_scale;
        rw_scalestep = 0;
    }
}

static void R_CalculateTexture(int start, int stop) {
    R_CalculateScales(start, stop);

    // calculate texture boundaries and decide if floor / ceiling marks are needed
    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    if (backsector) {
        // Two-sided line.
        R_CalculateTextureTwoSide();
    } else {
        // Single-sided line.
        R_CalculateTextureOneSide();
    }
    segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

    R_CalculateTextureOffset();
    R_CalculateSteps();
}

//
// Mark the segment as visible for auto map
//
static void R_MarkLineAsVisible(seg_t* line) {
    line->linedef->flags |= ML_MAPPED;
}

static void R_SetLineParams(seg_t* line) {
    sidedef = line->sidedef;
    linedef = line->linedef;
}

static void R_PrepareSegmentRender(int start, int stop) {
    R_SetLineParams(curline);
    R_MarkLineAsVisible(curline);
    R_CalculateTexture(start, stop);
    R_CalculateLightTable();
    R_SetPlanes();
}


//
// R_RenderWallRange
// A wall segment will be drawn between start and stop pixels (inclusive).
//
void R_RenderWallRange(int start, int stop) {
    if (ds_p == &drawsegs[MAXDRAWSEGS]) {
        // Can't save more sprite clipping info.
        // Don't overflow and crash.
        return;
    }
    if (start >= viewwidth || start > stop) {
        I_Error("Bad R_RenderWallRange: %i to %i", start , stop);
    }

    R_PrepareSegmentRender(start, stop);
    R_RenderSegment();
    R_SaveSpriteClippingInfo(start, stop);
}
