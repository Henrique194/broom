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
//	Refresh of things, i.e. objects represented by sprites.
//


#include <stdlib.h>
#include "deh_str.h"
#include "doomdef.h"
#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
#include "doomstat.h"


#define MINZ        (FRACUNIT * 4)
#define BASEYCENTER (SCREENHEIGHT / 2)


//
// Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
// CLOCKWISE around the axis. This is not the same as the angle, which
// increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t pspritescale;
fixed_t pspriteiscale;

static lighttable_t** spritelights;

// constant arrays used for psprite clipping and initializing clipping
short negonearray[SCREENWIDTH];
short screenheightarray[SCREENWIDTH];


//
// INITIALIZATION FUNCTIONS
//

// variables used to look up and range check thing_t sprites patches
spritedef_t *sprites;
int numsprites;

static spriteframe_t sprtemp[29];
static int maxframe;
static const char *spritename;


static void R_SetSpriteLights(short light_level) {
    int light_num = (light_level >> LIGHTSEGSHIFT) + extralight;

    if (light_num < 0) {
        spritelights = scalelight[0];
    } else if (light_num >= LIGHTLEVELS) {
        spritelights = scalelight[LIGHTLEVELS - 1];
    } else {
        spritelights = scalelight[light_num];
    }
}

//
// R_InstallSpriteLump
// Local function for R_InitSprites.
// Notes the highest frame letter.
//
static void R_InstallSpriteLump(int lump, unsigned frame, unsigned rotation,
                                bool flipped)
{
    if (frame >= 29 || rotation > 8) {
        I_Error("R_InstallSpriteLump: Bad frame characters in lump %i", lump);
    }
    if ((int) frame > maxframe) {
        maxframe = (int) frame;
    }

    spriteframe_t* sprite = &sprtemp[frame];
		
    if (rotation == 0) {
	// The lump should be used for all rotations.
	if (sprite->rotate == false) {
            I_Error("R_InitSprites: Sprite %s frame %c has multip rot=0 lump",
                    spritename, 'A' + frame);
        }
	if (sprite->rotate == true) {
            I_Error("R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump",
                    spritename, 'A' + frame);
        }
        sprite->rotate = false;
	for (int r = 0; r < 8; r++) {
            sprite->lump[r] = (short) (lump - firstspritelump);
            sprite->flip[r] = (byte) flipped;
	}
	return;
    }
	
    // The lump is only used for one rotation.
    if (sprite->rotate == false) {
        I_Error(
            "R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump",
            spritename, 'A' + frame);
    }
    sprite->rotate = true;
    // Make 0 based.
    rotation--;		
    if (sprite->lump[rotation] != -1) {
        I_Error("R_InitSprites: Sprite %s : %c : %c has two lumps mapped to it",
                spritename, 'A' + frame, '1' + rotation);
    }
    sprite->lump[rotation] = (short) (lump - firstspritelump);
    sprite->flip[rotation] = (byte) flipped;
}

//
// Allocate space for the frames present and copy sprtemp to it
//
static void R_SetSpriteFrames(spritedef_t* sprite) {
    int numframes = maxframe + 1;
    int size_alloc = numframes * sizeof(*(sprite->spriteframes));

    sprite->numframes = numframes;
    sprite->spriteframes = Z_Malloc(size_alloc, PU_STATIC, NULL);
    // Copy parsed sprite frames
    memcpy(sprite->spriteframes, sprtemp, size_alloc);
}

static void R_CheckForMalformedSpriteFrames() {
    for (int frame = 0; frame <= maxframe; frame++) {
        const spriteframe_t* sprite_frame = &sprtemp[frame];

        if ((int)sprite_frame->rotate == -1) {
            // not initialized: no rotations were found for that frame at all
            I_Error("R_InitSprites: No patches found for %s frame %c", spritename, frame + 'A');
        }

        if (sprite_frame->rotate == false) {
            // only the first rotation is needed
            continue;
        }

        // must have all 8 frames
        for (int rotation = 0; rotation < 8 ; rotation++) {
            if (sprite_frame->lump[rotation] == -1) {
                I_Error("R_InitSprites: Sprite %s frame %c is missing rotations", spritename, frame + 'A');
            }
        }
    }
}

//
// Scan the lumps, filling in the frames for whatever is found.
//
static void R_ParseSpriteFrames() {
    for (int l = firstspritelump; l <= lastspritelump; l++) {
        const lumpinfo_t* lump = lumpinfo[l];

        // All sprites names have a four-character prefix that identifies it.
        if (strncasecmp(lump->name, spritename, 4)) {
            // Lump name does not match sprite name.
            continue;
        }

        // The fifth character of a sprite graphic name gives the animation
        // frame ordinal, starting at A and proceeding through the alphabet
        // (the sequence continues past Z, if necessary, using ASCII encoding).
        int frame = lump->name[4] - 'A';
        // The sixth character of a sprite graphic name gives the rotation.
        // If it is 0, the graphic is used for all viewing angles.
        int rotation = lump->name[5] - '0';
        int lump_num = (modifiedgame ? W_GetNumForName(lump->name) : l);
        R_InstallSpriteLump(lump_num, frame, rotation, false);

        // It is possible to use a single graphic for two rotations.
        // For example, the name TROOA2A8 defines a graphic with prefix TROO
        // for animation frame A rot 2. The same graphic, drawn in mirror image,
        // is used for animation frame A rot 8.
        if (lump->name[6]) {
            frame = lump->name[6] - 'A';
            rotation = lump->name[7] - '0';
            R_InstallSpriteLump(l, frame, rotation, true);
        }
    }
}

//
// Count the number of sprite names
//
static int R_CountSpritesNames(const char **namelist) {
    const char **check = namelist;
    while (*check != NULL) {
        check++;
    }
    return check - namelist;
}

static void R_InitSpriteFrameParsing(const char *name) {
    spritename = DEH_String(name);
    memset(sprtemp, -1, sizeof(sprtemp));
    maxframe = -1;
}

//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account for horizontally flipped
// sprites. Will report an error if the lumps are inconsistant. Only called
// at startup.
//
// Sprite lump names are 4 characters for the actor, a letter for the frame,
// and a number for the rotation. A sprite that is flippable will have an
// additional letter/number appended. The rotation character can be 0 to
// signify no rotations.
//
static void R_InitSpriteDefs(const char **namelist) {
    numsprites = R_CountSpritesNames(namelist);
    if (!numsprites) {
        return;
    }
    sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, NULL);

    for (int i = 0; i < numsprites; i++) {
        // Parse sprite frames associated with "spritename"
        R_InitSpriteFrameParsing(namelist[i]);
        R_ParseSpriteFrames();

	if (maxframe == -1) {
            // No sprite frames associated with "spritename"
	    sprites[i].numframes = 0;
	} else {
            R_CheckForMalformedSpriteFrames();
            R_SetSpriteFrames(&sprites[i]);
        }
    }
}




//
// GAME FUNCTIONS
//
#define MAXVISSPRITES 128
static vissprite_t vissprites[MAXVISSPRITES];
static vissprite_t* vissprite_p;


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites(const char** namelist) {
    for (int i = 0; i < SCREENWIDTH; i++) {
	negonearray[i] = -1;
    }
    R_InitSpriteDefs(namelist);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites(void) {
    vissprite_p = vissprites;
}


//
// R_PushVisSprite
//
vissprite_t overflowsprite;

static vissprite_t* R_PushVisSprite(void) {
    if (vissprite_p == &vissprites[MAXVISSPRITES]) {
        return &overflowsprite;
    }
    vissprite_t* new_sprite = vissprite_p;
    vissprite_p++;

    return new_sprite;
}



short* mfloorclip;
short* mceilingclip;

fixed_t spryscale;
fixed_t sprtopscreen;

//
// Calculate unclipped screen coordinates for post.
//
static void R_ProjectScreen(const column_t* column) {
    int topscreen = sprtopscreen + (spryscale * column->topdelta);
    int bottomscreen = topscreen + (spryscale * column->length);

    // Round up top screen.
    dc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
    if (dc_yl <= mceilingclip[dc_x]) {
        dc_yl = mceilingclip[dc_x] + 1;
    }

    // Round down bottom screen.
    // Note that FRACUNIT will be mapped to 0.
    dc_yh = (bottomscreen - 1) >> FRACBITS;
    if (dc_yh >= mfloorclip[dc_x]) {
        dc_yh = mfloorclip[dc_x] - 1;
    }
}

//
// R_DrawMaskedColumn
// Drawn by either R_DrawColumn or (SHADOW) R_DrawFuzzColumn.
//
void R_DrawMaskedColumn(const column_t* column) {
    fixed_t basetexturemid = dc_texturemid;

    while (column->topdelta != END_COLUMN) {
        R_ProjectScreen(column);

        if (dc_yl <= dc_yh) {
            dc_source = column->data;
            dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
            colfunc();
        }

        column = NEXT_COLUMN(column);
    }

    dc_texturemid = basetexturemid;
}


static column_t* R_GetSpriteColumn(patch_t* patch, fixed_t frac) {
    int texturecolumn = frac >> FRACBITS;
    if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width)) {
        I_Error("R_GetSpriteColumn: bad texturecolumn");
    }
    return GET_COLUMN(patch, texturecolumn);
}

static void R_PrepareSpriteRender(vissprite_t* vis) {
    dc_colormap = vis->colormap;
    dc_iscale = abs(vis->xiscale) >> detailshift;
    dc_texturemid = vis->texturemid;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

    if (dc_colormap == NULL) {
        // NULL colormap = shadow draw
        colfunc = fuzzcolfunc;
    } else if (vis->mobjflags & MF_TRANSLATION) {
        colfunc = transcolfunc;
        dc_translation =
            translationtables - 256
            + ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
    }
}


//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
static void R_DrawVisSprite(vissprite_t* vis) {
    R_PrepareSpriteRender(vis);

    lumpindex_t sprite_lump = firstspritelump + vis->patch;
    patch_t* patch = (patch_t *) W_CacheLumpNum(sprite_lump, PU_CACHE);
    fixed_t frac = vis->startfrac;
    dc_x = vis->x1;

    while (dc_x <= vis->x2) {
        column_t* column = R_GetSpriteColumn(patch, frac);
        R_DrawMaskedColumn(column);

        dc_x++;
        frac += vis->xiscale;
    }

    colfunc = basecolfunc;
}

static spriteframe_t* R_GetThingSpriteFrame(const mobj_t* thing) {
    spritedef_t* sprdef = &sprites[thing->sprite];
    spriteframe_t* sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];
    return sprframe;
}

static unsigned R_CalculateThingSpriteRotation(const mobj_t* thing) {
    const spriteframe_t* sprframe = R_GetThingSpriteFrame(thing);

    if (sprframe->rotate) {
        // choose a different rotation based on player view
        angle_t ang = R_PointToAngle(thing->x, thing->y);
        return (ang - thing->angle + (unsigned)(ANG45/2) * 9) >> 29;
    }
    // use single rotation for all views
    return 0;
}

static int R_GetThingSpriteLump(const mobj_t* thing) {
    const spriteframe_t* sprframe = R_GetThingSpriteFrame(thing);
    unsigned rot = R_CalculateThingSpriteRotation(thing);
    return sprframe->lump[rot];
}

//
// Get light level.
//
static lighttable_t* R_CalculateVisibleSpriteColorMap(const mobj_t* thing,
                                                      fixed_t xscale)
{
    if (thing->flags & MF_SHADOW) {
        // shadow draw
        return NULL;
    }
    if (fixedcolormap) {
        // fixed map
        return fixedcolormap;
    }
    if (thing->frame & FF_FULLBRIGHT) {
        // full bright
        return colormaps;
    }
    // diminished light
    int index = xscale >> (LIGHTSCALESHIFT - detailshift);
    if (index >= MAXLIGHTSCALE) {
        index = MAXLIGHTSCALE - 1;
    }
    return spritelights[index];
}

static void R_CalculateVisibleSpriteStep(vissprite_t* vis, int x1,
                                         fixed_t xscale, int lump, int flip)
{
    fixed_t iscale = FixedDiv(FRACUNIT, xscale);

    if (flip) {
        vis->startfrac = spritewidth[lump] - 1;
        vis->xiscale = -iscale;
    } else {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }
    if (vis->x1 > x1) {
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    }
}

static void R_PrepareDrawThingSprite(const mobj_t* thing,
                                     const vissprite_t* vis)
{
    unsigned rot = R_CalculateThingSpriteRotation(thing);
    const spriteframe_t* sprframe = R_GetThingSpriteFrame(thing);

    int lump = sprframe->lump[rot];
    bool flip = (bool) sprframe->flip[rot];
    int x1 = vis->x1;
    int x2 = vis->x2;
    fixed_t xscale = vis->scale;

    vissprite_t* new_vis = R_PushVisSprite();
    new_vis->mobjflags = thing->flags;
    new_vis->gx = thing->x;
    new_vis->gy = thing->y;
    new_vis->gz = thing->z;
    new_vis->gzt = thing->z + spritetopoffset[lump];
    new_vis->colormap = R_CalculateVisibleSpriteColorMap(thing, xscale);
    new_vis->patch = lump;
    new_vis->scale = xscale << detailshift;
    new_vis->texturemid = new_vis->gzt - viewz;
    new_vis->x1 = (x1 < 0) ? 0 : x1;
    new_vis->x2 = (x2 >= viewwidth) ? viewwidth - 1 : x2;
    R_CalculateVisibleSpriteStep(new_vis, x1, xscale, lump, flip);
}

//
// Calculate edges of the shape.
// Returns true if the projected sprite is visible, false otherwise.
//
static bool R_ProjectThingSpriteScreenSpace(const mobj_t* thing,
                                               vissprite_t* vis)
{
    // Two transformations are applied here to facilitate the
    // sprite's projection into screen space:
    // 1. Rotation: The world is rotated such that the player's angle
    // becomes 0 degrees. This simplifies the projection of the sprite into
    // screen space by aligning it with the player's viewpoint.
    // 2. Reflection: A reflection across the x-axis is applied to ensure
    // consistency in screen coordinates. This ensures that any object to the
    // right of the screen's center has a positive value, while objects to the
    // left of the center have a negative value.
    //
    // The combination of these two transformations can be summarized by the
    // following formulas:
    //  tz = x' = (tr_x * viewcos) + (tr_y * viewsin)
    //  tx = y' = (tr_x * viewsin) - (tr_y * viewcos)
    //
    // After these transformations, obtaining the sprite's x-coordinate in
    // screen space is simply a matter of scaling "tx" and adding it to the
    // x-coordinate of the screen's center (i.e., "centerxfrac").
    fixed_t tr_x = thing->x - viewx;
    fixed_t tr_y = thing->y - viewy;
    fixed_t tz = FixedMul(tr_x, viewcos) + FixedMul(tr_y, viewsin);
    fixed_t tx = FixedMul(tr_x, viewsin) - FixedMul(tr_y, viewcos);
    // thing is behind view plane or too far off the side
    if (tz < MINZ || abs(tx) > (tz << 2)) {
        return false;
    }

    int lump = R_GetThingSpriteLump(thing);
    fixed_t world_x1 = tx - spriteoffset[lump];
    fixed_t world_x2 = world_x1 + spritewidth[lump];
    fixed_t xscale = FixedDiv(projection, tz);

    fixed_t projection_x1 = centerxfrac + FixedMul(world_x1, xscale);
    fixed_t projection_x2 = centerxfrac + FixedMul(world_x2, xscale);

    vis->x1 = projection_x1 >> FRACBITS;
    vis->x2 = (projection_x2 >> FRACBITS) - 1;
    vis->scale = xscale;

    return vis->x1 <= viewwidth && vis->x2 >= 0;
}

static void R_CheckInvalidThingSprite(const mobj_t* thing) {
    if ((unsigned int) thing->sprite >= (unsigned int) numsprites) {
        I_Error("R_ProjectSprite: invalid sprite number %i ", thing->sprite);
    }

    const spritedef_t* sprdef = &sprites[thing->sprite];
    if ( (thing->frame & FF_FRAMEMASK) >= sprdef->numframes) {
        I_Error("R_ProjectSprite: invalid sprite frame %i : %i ", thing->sprite, thing->frame);
    }
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
static void R_ProjectSprite(const mobj_t* thing) {
    R_CheckInvalidThingSprite(thing);

    vissprite_t avis;
    vissprite_t* vis = &avis;
    bool is_visible = R_ProjectThingSpriteScreenSpace(thing, vis);
    if (is_visible) {
        // store information in a vissprite
        R_PrepareDrawThingSprite(thing, vis);
    }
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites(sector_t* sec) {
    if (sec->validcount == validcount) {
        // BSP is traversed by subsector. A sector might have been split
        // into several subsectors during BSP building. Thus, we check
        // whether it's already added.
        return;
    }
    // Well, now it will be done.
    sec->validcount = validcount;
    R_SetSpriteLights(sec->lightlevel);
    // Handle all things in sector.
    for (mobj_t* thing = sec->thinglist; thing; thing = thing->snext) {
        R_ProjectSprite(thing);
    }
}


#define PLAYER_SPRITE_ANGLE 0

static lighttable_t* R_CalculatePlayerSpriteColorMap(const pspdef_t* plr_sprite) {
    int invisible = viewplayer->powers[pw_invisibility];

    if (invisible > 4*32 || invisible & 8) {
        // shadow draw
        return NULL;
    }
    if (fixedcolormap) {
        // fixed color
        return fixedcolormap;
    }
    if (plr_sprite->state->frame & FF_FULLBRIGHT) {
        // full bright
        return colormaps;
    }
    // local light
    return spritelights[MAXLIGHTSCALE-1];
}

static void R_CalculatePlayerSpriteStep(vissprite_t* vis, const spriteframe_t* sprframe, int x1) {
    int lump = sprframe->lump[PLAYER_SPRITE_ANGLE];
    int flip = (bool) sprframe->flip[PLAYER_SPRITE_ANGLE];

    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = spritewidth[lump] - 1;
    } else {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }
    if (vis->x1 > x1) {
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    }
}

static spriteframe_t* R_GetPlayerSpriteFrame(const pspdef_t* psp) {
    const state_t* psp_state = psp->state;
    const spritedef_t* sprdef = &sprites[psp_state->sprite];
    return &sprdef->spriteframes[psp_state->frame & FF_FRAMEMASK];
}

//
// Store information in a vissprite.
//
static void R_PrepareDrawPlayerSprite(vissprite_t* vis,
                                      const pspdef_t* plr_sprite)
{
    const spriteframe_t* sprframe = R_GetPlayerSpriteFrame(plr_sprite);
    int lump = sprframe->lump[PLAYER_SPRITE_ANGLE];
    int x1 = vis->x1;
    int x2 = vis->x2;

    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
    vis->mobjflags = 0;
    vis->texturemid = (BASEYCENTER << FRACBITS)
                      + FRACUNIT / 2
                      - (plr_sprite->sy - spritetopoffset[lump]);
    vis->scale = pspritescale << detailshift;
    R_CalculatePlayerSpriteStep(vis, sprframe, x1);
    vis->patch = lump;
    vis->colormap = R_CalculatePlayerSpriteColorMap(plr_sprite);
}

static bool R_IsPlayerSpriteVisible(const vissprite_t* vis) {
    return vis->x1 <= viewwidth && vis->x2 >= 0;
}

static void R_CalculatePlayerSpriteEdges(vissprite_t* vis,
                                         const pspdef_t* plr_sprite)
{
    const spriteframe_t* sprframe = R_GetPlayerSpriteFrame(plr_sprite);
    int lump = sprframe->lump[PLAYER_SPRITE_ANGLE];

    fixed_t tx = plr_sprite->sx - (SCREENWIDTH/2)*FRACUNIT;
    tx -= spriteoffset[lump];
    vis->x1 = (centerxfrac + FixedMul(tx,pspritescale)) >> FRACBITS;

    tx += spritewidth[lump];
    vis->x2 = ((centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS) - 1;
}

static void R_CheckInvalidPlayerSprite(const pspdef_t* plr_sprite) {
    const state_t* psp_state = plr_sprite->state;

    if ((unsigned) psp_state->sprite >= (unsigned int) numsprites) {
        I_Error("R_ProjectSprite: invalid sprite number %i ", psp_state->sprite);
    }
    const spritedef_t* sprdef = &sprites[psp_state->sprite];
    if ((psp_state->frame & FF_FRAMEMASK) >= sprdef->numframes) {
        I_Error("R_ProjectSprite: invalid sprite frame %i : %i ", psp_state->sprite, psp_state->frame);
    }
}

//
// R_DrawPlayerprite
//
static void R_DrawPlayerSprite(const pspdef_t* plr_sprite) {
    vissprite_t avis;
    vissprite_t* vis = &avis;

    R_CheckInvalidPlayerSprite(plr_sprite);
    R_CalculatePlayerSpriteEdges(vis, plr_sprite);

    if (R_IsPlayerSpriteVisible(vis)) {
        R_PrepareDrawPlayerSprite(vis, plr_sprite);
        R_DrawVisSprite(vis);
    }
}

//
// Add all active psprites
//
static void R_DrawActivePlayerSprites() {
    for (int i = 0; i < NUMPSPRITES; i++) {
        const pspdef_t* psp = &viewplayer->psprites[i];
        if (psp->state) {
            R_DrawPlayerSprite(psp);
        }
    }
}

//
// clip to screen bounds
//
static void R_SetPlayerSpriteScreenBounds() {
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;
}

static short clipbot[SCREENWIDTH];
static short cliptop[SCREENWIDTH];

static void R_SetThingSpriteScreenBounds() {
    mfloorclip = clipbot;
    mceilingclip = cliptop;
}


//
// get light level
//
static void R_SetPlayerSpriteLighting() {
    const mobj_t* plr_mo = viewplayer->mo;
    const sector_t* plr_sector = plr_mo->subsector->sector;
    R_SetSpriteLights(plr_sector->lightlevel);
}

static void R_CheckUnclippedThingSpriteColumns(const vissprite_t* spr) {
    for (int x = spr->x1; x <= spr->x2; x++) {
        if (clipbot[x] == -2) {
            clipbot[x] = (short) viewheight;
        }
        if (cliptop[x] == -2) {
            cliptop[x] = -1;
        }
    }
}

//
// Clip sprite that is behind segment.
//
static void R_ClipSpritePiece(const vissprite_t* spr, const drawseg_t* ds,
                              int r1, int r2)
{
    int silhouette = ds->silhouette;

    if (spr->gz >= ds->bsilheight) {
        silhouette &= ~SIL_BOTTOM;
    }
    if (spr->gzt <= ds->tsilheight) {
        silhouette &= ~SIL_TOP;
    }

    switch (silhouette) {
        case SIL_BOTTOM:
            for (int x = r1; x <= r2; x++) {
                if (clipbot[x] == -2) {
                    clipbot[x] = ds->sprbottomclip[x];
                }
            }
            break;
        case SIL_TOP:
            for (int x = r1; x <= r2; x++) {
                if (cliptop[x] == -2) {
                    cliptop[x] = ds->sprtopclip[x];
                }
            }
            break;
        case SIL_BOTH:
            for (int x = r1; x <= r2; x++) {
                if (clipbot[x] == -2) {
                    clipbot[x] = ds->sprbottomclip[x];
                }
                if (cliptop[x] == -2) {
                    cliptop[x] = ds->sprtopclip[x];
                }
            }
            break;
        default:
            break;
    }
}

//
// The first drawseg that has a greater scale is the clip seg.
//
static bool R_IsInFrontSprite(const drawseg_t* ds, const vissprite_t* spr) {
    fixed_t min_scale;
    fixed_t max_scale;
    if (ds->scale1 <= ds->scale2) {
        min_scale = ds->scale1;
        max_scale = ds->scale2;
    } else {
        min_scale = ds->scale2;
        max_scale = ds->scale1;
    }

    if (max_scale < spr->scale) {
        // Sprite scale is bigger than segment scale, so that means sprite is
        // closer than segment, so segment is behind sprite.
        return false;
    }
    if (min_scale >= spr->scale) {
        // Segment scale is bigger than sprite scale, so that means segment is
        // closer than sprite, so segment is in front of sprite.
        return true;
    }
    return R_PointOnSegSide(spr->gx, spr->gy, ds->curline);
}

//
// Determine if the drawseg obscures the sprite.
//
static bool R_OverlapSprite(const drawseg_t* ds, const vissprite_t* spr) {
    if (ds->x1 > spr->x2) {
        // Off the right edge
        return false;
    }
    if (ds->x2 < spr->x1) {
        // Off the left edge
        return false;
    }
    if (ds->silhouette != SIL_NONE) {
        return true;
    }
    return ds->maskedtexturecol != NULL;
}

//
// Iterate through the drawsegs array in reverse (from end to start) to check
// for any segments that obscure the sprite and clip accordingly.
//
static void R_ClipThingSprite(const vissprite_t* spr) {
    int end_seg = (ds_p - 1) - drawsegs;

    for (int i = end_seg; i >= 0; i--) {
        const drawseg_t* ds = &drawsegs[i];

        // Determine the range of the segment that overlaps with the sprite.
        int r1 = (spr->x1 > ds->x1) ? spr->x1 : ds->x1;
        int r2 = (spr->x2 < ds->x2) ? spr->x2 : ds->x2;

        if (!R_OverlapSprite(ds, spr)) {
            // Segment does not overlap sprite, so no clipping needed.
            continue;
        }
        if (R_IsInFrontSprite(ds, spr)) {
            // Segment is in front of sprite, so clip sprite.
            R_ClipSpritePiece(spr, ds, r1, r2);
            continue;
        }
        if (ds->maskedtexturecol) {
            // The segment is partially transparent (masked) and located behind
            // the sprite, so render the part of the segment that lies behind
            // the sprite to maintain proper visual stacking order and ensure
            // sprites can be interleaved with the segments, preventing segments
            // from incorrectly appearing in front of the sprite.
            // The sprite will be drawn on top of it afterward, preserving the
            // proper depth order.
            R_RenderMaskedSegRange(ds, r1, r2);
        }
    }
}

static void R_PrepareThingSpriteClipArrays(const vissprite_t* spr) {
    for (int x = spr->x1; x <= spr->x2; x++) {
        clipbot[x] = -2;
        cliptop[x] = -2;
    }
}

//
// R_DrawThingSprite
//
static void R_DrawThingSprite(vissprite_t* spr) {
    R_PrepareThingSpriteClipArrays(spr);
    R_ClipThingSprite(spr);
    R_CheckUnclippedThingSpriteColumns(spr);
    R_SetThingSpriteScreenBounds();
    R_DrawVisSprite(spr);
}

//
// Draw the psprites on top of everything but does not draw on side views
//
static void R_DrawPlayer() {
    if (viewangleoffset == 0) {
        R_SetPlayerSpriteLighting();
        R_SetPlayerSpriteScreenBounds();
        R_DrawActivePlayerSprites();
    }
}

//
// Some masked segments may have been only partially rendered or not rendered
// at all while rendering the things sprites, so we need to render any
// remaining masked mid textures.
//
static void R_DrawRemainingMaskedTextures() {
    for (const drawseg_t* ds = ds_p - 1; ds >= drawsegs; ds--) {
        if (ds->maskedtexturecol) {
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);
        }
    }
}

//
// R_SortThingsSprites
//
static vissprite_t vsprsortedhead;
static vissprite_t unsorted;

static vissprite_t* R_FindSpriteWithSmallestScale() {
    vissprite_t* best = unsorted.next;

    fixed_t bestscale = INT_MAX;
    vissprite_t* ds = best;
    while (ds != &unsorted) {
        if (ds->scale < bestscale) {
            bestscale = ds->scale;
            best = ds;
        }
        ds = ds->next;
    }

    return best;
}

static void R_InitSortedList() {
    vsprsortedhead.next = &vsprsortedhead;
    vsprsortedhead.prev = &vsprsortedhead;
}

static void R_InitUnsortedList() {
    unsorted.next = &unsorted;
    unsorted.prev = &unsorted;

    for (vissprite_t* ds = vissprites; ds < vissprite_p; ds++) {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }

    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p - 1)->next = &unsorted;
    unsorted.prev = vissprite_p - 1;
}

static void R_SortThingsSprites(void) {
    int count = vissprite_p - vissprites;

    R_InitUnsortedList();
    R_InitSortedList();

    for (int i = 0; i < count; i++) {
        // pull the vissprites out by scale
        vissprite_t* best = R_FindSpriteWithSmallestScale();

        // Remove from unsorted list
        best->next->prev = best->prev;
        best->prev->next = best->next;

        // Push in sorted list
        best->next = &vsprsortedhead;
        best->prev = vsprsortedhead.prev;
        vsprsortedhead.prev->next = best;
        vsprsortedhead.prev = best;
    }
}

//
// Draw all vissprites back to front
//
static void R_DrawThingsSprites() {
    if (vissprite_p <= vissprites) {
        // No sprites to render
        return;
    }

    R_SortThingsSprites();

    vissprite_t* spr = vsprsortedhead.next;
    while (spr != &vsprsortedhead) {
        R_DrawThingSprite(spr);
        spr = spr->next;
    }
}

//
// R_DrawMasked
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored in posts/runs of opaque pixels.
//
void R_DrawMasked(void) {
    R_DrawThingsSprites();
    R_DrawRemainingMaskedTextures();
    R_DrawPlayer();
}
