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
//     Draws the border around the screen when resizing.
//
//


#include "doomdef.h"
#include "deh_str.h"
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
// Needs access to LFB (guess what).
#include "v_video.h"
// State.
#include "doomstat.h"


//
// Backing buffer containing the bezel drawn around the screen and
// surrounding background.
//
static pixel_t* background_buffer = NULL;


static void R_DrawBeveledEdge() {
    V_UseBuffer(background_buffer);

    patch_t* patch = W_CacheLumpName(DEH_String("brdr_tl"), PU_CACHE);
    V_DrawPatch(viewwindowx - 8, viewwindowy - 8, patch);

    patch = W_CacheLumpName(DEH_String("brdr_tr"), PU_CACHE);
    V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy - 8, patch);

    patch = W_CacheLumpName(DEH_String("brdr_bl"), PU_CACHE);
    V_DrawPatch(viewwindowx - 8, viewwindowy + viewheight, patch);

    patch = W_CacheLumpName(DEH_String("brdr_br"), PU_CACHE);
    V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + viewheight, patch);

    V_RestoreBuffer();
}

static void R_DrawBackScreenRightBorder() {
    V_UseBuffer(background_buffer);

    patch_t* patch = W_CacheLumpName(DEH_String("brdr_r"), PU_CACHE);
    for (int y = 0; y < viewheight; y += 8) {
        V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + y, patch);
    }

    V_RestoreBuffer();
}

static void R_DrawBackScreenLeftBorder() {
    V_UseBuffer(background_buffer);

    patch_t* patch = W_CacheLumpName(DEH_String("brdr_l"), PU_CACHE);
    for (int y = 0; y < viewheight; y += 8) {
        V_DrawPatch(viewwindowx - 8, viewwindowy + y, patch);
    }

    V_RestoreBuffer();
}

static void R_DrawBackScreenBottomBorder() {
    V_UseBuffer(background_buffer);

    patch_t* patch = W_CacheLumpName(DEH_String("brdr_b"), PU_CACHE);
    for (int x = 0; x < scaledviewwidth; x += 8) {
        V_DrawPatch(viewwindowx + x, viewwindowy + viewheight, patch);
    }

    V_RestoreBuffer();
}

static void R_DrawBackScreenTopBorder() {
    V_UseBuffer(background_buffer);

    patch_t* patch = W_CacheLumpName(DEH_String("brdr_t"), PU_CACHE);
    for (int x = 0; x < scaledviewwidth; x += 8) {
        V_DrawPatch(viewwindowx + x, viewwindowy - 8, patch);
    }

    V_RestoreBuffer();
}

//
// Draw screen and bezel; this is done to a separate screen buffer.
//
static void R_DrawBackScreenBorder() {
    R_DrawBackScreenTopBorder();
    R_DrawBackScreenBottomBorder();
    R_DrawBackScreenLeftBorder();
    R_DrawBackScreenRightBorder();
    R_DrawBeveledEdge();
}

static const byte* R_GetBackScreenTexture() {
    const char* name = (gamemode == commercial) ? DEH_String("GRNROCK")
                                                : DEH_String("FLOOR7_2");
    return W_CacheLumpName(name, PU_CACHE);
}

static void R_FillBackScreenWithTexture() {
    pixel_t* dest = background_buffer;
    const byte* texture = R_GetBackScreenTexture();

    for (int y = 0; y < SCREENHEIGHT - SBARHEIGHT; y++) {
        const byte* src = texture + ((y & 63) << 6);
        for (int x = 0; x < SCREENWIDTH / 64; x++) {
            memcpy(dest, src, 64);
            dest += 64;
        }
        if (SCREENWIDTH & 63) {
            memcpy(dest, src, SCREENWIDTH & 63);
            dest += (SCREENWIDTH & 63);
        }
    }
}

static void R_AllocBackgroundScreen() {
    unsigned int count = SCREENWIDTH * (SCREENHEIGHT - SBARHEIGHT);
    unsigned int size = count * sizeof(*background_buffer);
    int tag = PU_STATIC;
    void* user = NULL;
    background_buffer = Z_Malloc((int) size, tag, user);
}

//
// R_FillBackScreen
// Fills the back screen with a pattern for variable screen sizes.
// Also draws a beveled edge.
//
void R_FillBackScreen() {
    // If we are running full screen, there is no need to do any of this,
    // and the background buffer can be freed if it was previously in use.
    if (scaledviewwidth == SCREENWIDTH) {
        if (background_buffer != NULL) {
            Z_Free(background_buffer);
            background_buffer = NULL;
        }
        return;
    }

    if (background_buffer == NULL) {
        R_AllocBackgroundScreen();
    }
    R_FillBackScreenWithTexture();
    R_DrawBackScreenBorder();
}


//
// Copy a screen buffer.
//
void R_VideoErase(unsigned ofs, int count) {
    // LFB copy.
    // This might not be a good idea if memcpy is not optiomal,
    // e.g. byte by byte on a 32bit CPU, as GNU GCC/Linux libc did at one point.
    if (background_buffer != NULL) {
        byte* dst = I_VideoBuffer + ofs;
        const byte* src = background_buffer + ofs;
        size_t size = count * sizeof(*I_VideoBuffer);
        memcpy(dst, src, size);
    }
}


//
// R_DrawViewBorder
// Draws the border around the view for different size windows?
//
void R_DrawViewBorder() {
    int top;
    int side;
    int ofs;

    if (scaledviewwidth == SCREENWIDTH) {
        return;
    }

    top = (SCREENHEIGHT - SBARHEIGHT - viewheight) / 2;
    side = (SCREENWIDTH - scaledviewwidth) / 2;

    // copy top and one line of left side
    R_VideoErase(0, top * SCREENWIDTH + side);

    // copy one line of right side and bottom
    ofs = (viewheight + top) * SCREENWIDTH - side;
    R_VideoErase(ofs, top * SCREENWIDTH + side);

    // copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (int i = 1; i < viewheight; i++) {
        R_VideoErase(ofs, side);
        ofs += SCREENWIDTH;
    }
}
