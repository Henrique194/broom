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
//      Preparation of data for rendering, generation of lookups, caching,
//      retrieval by name.
//
//      Graphics.
//      DOOM graphics for walls and sprites is stored in vertical runs of opaque
//      pixels (posts). A column is composed of zero or more posts, a patch or
//      sprite is composed of zero or more columns.
//
//      MAPTEXTURE_T CACHING
//      When a texture is first needed, it counts the number of composite
//      columns required in the texture and allocates space for a column
//      directory and any new columns. The directory will simply point inside
//      other patches if there is only one patch in a given column, but any
//      columns with multiple patches will have new column_ts generated.
//
//
//

#include <stdio.h>
#include <math.h>

#include "deh_str.h"
#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"


#include "w_wad.h"

#include "doomdef.h"
#include "m_misc.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"


#include "r_data.h"


//
// Texture definition.
// Each texture is composed of one or more patches, with patches being lumps
// stored in the WAD. The lumps are referenced by number, and patched into the
// rectangular texture space using origin and possibly other attributes.
//
typedef PACKED_STRUCT({
    // A short int defining the horizontal offset of the patch
    // relative to the upper-left of the texture.
    short originx;

    // A short int defining the vertical offset of the patch
    // relative to the upper-left of the texture.
    short originy;

    // A short int defining the patch number (as listed in PNAMES) to draw.
    short patch;

    // Unused.
    // A short int possibly intended to define if the
    // patch was to be drawn normally or mirrored.
    short stepdir;

    // Unused.
    // A short int possibly intended to define a special
    // colormap to draw the patch with, like a brightmap.
    short colormap;
}) mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef PACKED_STRUCT({
    // An ASCII string defining the name of the map texture. When a string is
    // less than 8 bytes long, it should be null-padded to the eighth byte.
    char name[8];

    // Unused.
    int masked;

    // A short integer defining the total width of the map texture.
    short width;

    // A short integer defining the total height of the map texture.
    short height;

    // Unused.
    int columndirectory;

    // The number of map patches that make up this map texture.
    short patchcount;

    // Array with the map patch structures for this texture.
    mappatch_t patches[1];
}) maptexture_t;


// A single patch from a texture definition, basically a rectangular area
// within the texture rectangle. This structure is derived from a mappatch_t
// when reading the TEXTURES1/TEXTURES2 lumps.
typedef struct
{
    // A short int defining the horizontal offset of the patch
    // relative to the upper-left of the texture.
    short originx;

    // A short int defining the vertical offset of the patch
    // relative to the upper-left of the texture
    short originy;

    // A short int defining the patch number (as listed in PNAMES) to draw.
    int patch;
} texpatch_t;


// A texture_t describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches. This structure
// is derived from a maptexture_t when reading the TEXTURES1/TEXTURES2 lumps.
typedef struct texture_s
{
    // Keep name for switch changing, etc.
    char name[8];
    short width;
    short height;

    // Index in textures list
    int index;

    // Next in hash table chain
    struct texture_s* next;

    // All the patches[patchcount] are drawn back to front
    // into the cached texture.
    short patchcount;
    texpatch_t patches[1];
} texture_t;


int firstflat;
int lastflat;
int numflats;

int firstspritelump;
int lastspritelump;
int numspritelumps;

static int numtextures;
static texture_t **textures;
static texture_t **textures_hashtable;


static int *texturewidthmask;
// needed for texture pegging
fixed_t *textureheight;
static int* texturecompositesize;
static short **texturecolumnlump;
static unsigned short **texturecolumnofs;
static byte** texturecomposite;

// for global animation
int *flattranslation;
int *texturetranslation;

// needed for pre rendering
fixed_t *spritewidth;
fixed_t *spriteoffset;
fixed_t *spritetopoffset;

lighttable_t *colormaps;


//
// R_DrawColumnInCache
// Clip and draw a column from a patch into a cached post.
//
static void R_DrawColumnInCache(column_t* column, byte* cache, int originy,
                                int cacheheight)
{
    while (column->topdelta != END_COLUMN) {
        int count = column->length;
        int position = originy + column->topdelta;

        if (position < 0) {
            // Vanilla Bug: The "position" is not used to correctly index
            // column data. This results in incorrect handling of negative
            // offsets, causing the column to be drawn from the top to bottom,
            // instead of starting from the correct offset. Note how position
            // is set to 0 here, instead of the correct offset.
            count += position;
            position = 0;
        }
        if (position + count > cacheheight) {
            count = cacheheight - position;
        }

        if (count > 0) {
            memcpy(cache + position, column->data, count);
        }

        column = NEXT_COLUMN(column);
    }
}

//
// R_GenerateComposite
// Using the texture definition, the composite texture is created from the
// patches, and each column is cached.
//
static void R_GenerateComposite(int texnum) {
    const texture_t* texture = textures[texnum];
    const short* collump = texturecolumnlump[texnum];
    const unsigned short* colofs = texturecolumnofs[texnum];

    byte* block = Z_Malloc(texturecompositesize[texnum], PU_STATIC,
                           &texturecomposite[texnum]);

    // Composite the columns together.
    for (int i = 0; i < texture->patchcount; i++) {
        const texpatch_t* texture_patch = &texture->patches[i];
        patch_t* patch = W_CacheLumpNum(texture_patch->patch, PU_CACHE);

        int x1 = texture_patch->originx;
        int x2 = x1 + SHORT(patch->width);
        int x = (x1 < 0) ? 0 : x1;
        if (x2 > texture->width) {
            x2 = texture->width;
        }

        for (; x < x2; x++) {
            if (collump[x] >= 0) {
                // Column does not have multiple patches.
                continue;
            }

            column_t* column = GET_COLUMN(patch, x - x1);
            byte* cache = block + colofs[x];
            int originy = texture_patch->originy;
            int cacheheight = texture->height;

            R_DrawColumnInCache(column, cache, originy, cacheheight);
        }
    }

    // Now that the texture has been built in column cache,
    // it is purgable from zone memory.
    Z_ChangeTag(block, PU_CACHE);
}

static void R_CheckCompositeColumns(int texnum, const byte* patchcount) {
    const texture_t* texture = textures[texnum];
    short* collump = texturecolumnlump[texnum];
    unsigned short* colofs = texturecolumnofs[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = NULL;
    texturecompositesize[texnum] = 0;

    for (int x = 0; x < texture->width; x++) {
        if (patchcount[x] == 0) {
            printf("R_CheckCompositeColumns: column without a patch (%s)\n",
                   texture->name);
            return;
        }
        if (patchcount[x] > 1) {
            // Use the cached block.
            collump[x] = -1;
            colofs[x] = (unsigned short) texturecompositesize[texnum];
            texturecompositesize[texnum] += texture->height;
            if (texturecompositesize[texnum] > 0x10000) {
                I_Error("R_CheckCompositeColumns: texture %i is >64k", texnum);
            }
        }
    }
}

//
// Count the number of columns that are covered by more than one patch.
// Fill in the lump / offset, so columns with only a single patch are all done.
//
static void R_CountNumPatches(int texnum, byte* patchcount) {
    const texture_t* texture = textures[texnum];
    short* collump = texturecolumnlump[texnum];
    unsigned short* colofs = texturecolumnofs[texnum];

    for (int i = 0; i < texture->patchcount; i++) {
        const texpatch_t* patch = &texture->patches[i];
        const patch_t* realpatch = W_CacheLumpNum(patch->patch, PU_CACHE);

        int x1 = patch->originx;
        int x2 = x1 + SHORT(realpatch->width);
        int x = (x1 < 0) ? 0 : x1;
        if (x2 > texture->width) {
            x2 = texture->width;
        }

        for (; x < x2; x++) {
            int real_patch_col = x - x1;
            int offset = LONG(realpatch->columnofs[real_patch_col]);
            patchcount[x]++;
            collump[x] = (short) patch->patch;
            colofs[x] = (unsigned short) offset;
        }
    }
}

//
// R_GenerateLookup
//
static void R_GenerateLookup(int texnum) {
    const texture_t* texture = textures[texnum];
    short width = texture->width;

    byte* patchcount = (byte *) Z_Malloc(width, PU_STATIC, &patchcount);
    memset(patchcount, 0, width);

    R_CountNumPatches(texnum, patchcount);
    R_CheckCompositeColumns(texnum, patchcount);

    Z_Free(patchcount);
}


//
// R_GetColumn
//
const byte* R_GetColumn(int tex, int col) {
    col &= texturewidthmask[tex];
    int lump = texturecolumnlump[tex][col];
    int ofs = texturecolumnofs[tex][col];

    if (lump > 0) {
        const byte* lump_data = (byte *) W_CacheLumpNum(lump, PU_CACHE);
        const column_t* column = (const column_t *) &lump_data[ofs];
        return column->data;
    }
    if (texturecomposite[tex] == NULL) {
        R_GenerateComposite(tex);
    }
    return &texturecomposite[tex][ofs];
}


static void GenerateTextureHashTable() {
    size_t size = sizeof(texture_t *) * numtextures;
    textures_hashtable = Z_Malloc((int) size, PU_STATIC, NULL);
    memset(textures_hashtable, 0, size);

    // Add all textures to hash table
    for (int i = 0; i < numtextures; ++i) {
        // Store index
        textures[i]->index = i;

        // Vanilla Doom does a linear search of the textures array and stops at
        // the first entry it finds.  If there are two entries with the same
        // name, the first one in the array wins. The new entry must therefore
        // be added at the end of the hash chain, so that earlier entries win.
        int key = W_LumpNameHash(textures[i]->name) % numtextures;
        texture_t** rover = &textures_hashtable[key];
        while (*rover != NULL) {
            rover = &(*rover)->next;
        }

        // Hook into hash table
        textures[i]->next = NULL;
        *rover = textures[i];
    }
}

//
// Create translation table for global animation.
//
static void R_InitTranslationTable() {
    size_t size = (numtextures + 1) * sizeof(*texturetranslation);
    texturetranslation = Z_Malloc((int) size, PU_STATIC, NULL);

    for (int i = 0; i < numtextures; i++) {
        texturetranslation[i] = i;
    }
}

static void R_AllocTexturesData() {
    size_t size = numtextures * sizeof(*textures);
    textures = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*texturecolumnlump);
    texturecolumnlump = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*texturecolumnofs);
    texturecolumnofs = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*texturecomposite);
    texturecomposite = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*texturecompositesize);
    texturecompositesize = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*texturewidthmask);
    texturewidthmask = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numtextures * sizeof(*textureheight);
    textureheight = Z_Malloc((int) size, PU_STATIC, NULL);
}

//
// Load the patch names from pnames.lmp.
//
static int* R_LoadPatchNamesLump() {
    const char* lump_name = DEH_String("PNAMES");
    char* lump = W_CacheLumpName(lump_name, PU_STATIC);
    // The first 4 bytes of the lump data represent the number of patches.
    int nummappatches = LONG(*((int *) lump));
    // The remaining data in the lump contains the names of the patches.
    // Each patch name is a string of at most 8 characters.
    const char* patch_names = lump + 4;

    int* patchlookup = NULL;
    size_t size = nummappatches * sizeof(*patchlookup);
    patchlookup = Z_Malloc((int) size, PU_STATIC, NULL);
    for (int i = 0; i < nummappatches; i++) {
        char name[9] = {0};
        const char* patch_name = patch_names + (8 * i);
        M_StringCopy(name, patch_name, sizeof(name));
        patchlookup[i] = W_CheckNumForName(name);
    }

    W_ReleaseLumpName(lump_name);

    return patchlookup;
}

// Classic vanilla "filling up the box" effect, which uses
// backspace to "step back" inside the box.
static void R_PrintBox() {
    int temp1 = W_GetNumForName(DEH_String("S_START")); // P_???????
    int temp2 = W_GetNumForName(DEH_String("S_END")) - 1;
    int temp3 = ((temp2 - temp1 + 63) / 64) + ((numtextures + 63) / 64);

    printf("[");
    for (int i = 0; i < temp3 + 9; i++) {
        printf(" ");
    }
    printf("]");

    for (int i = 0; i < temp3 + 10; i++) {
        printf("\b");
    }
}

static void R_InitTexturePatches(texture_t* texture,
                                 const maptexture_t* mtexture,
                                 const int* patchlookup)
{
    for (int i = 0; i < texture->patchcount; i++) {
        const mappatch_t* mpatch = &mtexture->patches[i];
        texpatch_t* patch = &texture->patches[i];

        patch->originx = SHORT(mpatch->originx);
        patch->originy = SHORT(mpatch->originy);
        patch->patch = patchlookup[SHORT(mpatch->patch)];

        if (patch->patch == -1) {
            I_Error("R_InitTexturePatches: Missing patch in texture %s",
                    texture->name);
        }
    }
}

static void R_InitTextureData(int tex_num, texture_t* texture) {
    short width = texture->width;

    // Save texture in textures array
    textures[tex_num] = texture;

    // Allocate lumps needed for each texture column
    size_t size = width * sizeof(**texturecolumnlump);
    texturecolumnlump[tex_num] = Z_Malloc((int) size, PU_STATIC, NULL);

    // Allocate texture offsets
    size = width * sizeof(**texturecolumnofs);
    texturecolumnofs[tex_num] = Z_Malloc((int) size, PU_STATIC, NULL);

    // Set texture width mask
    // Nearest power of two
    int j = 1 << ((int) log2(width));
    texturewidthmask[tex_num] = j - 1;

    // Set texture height
    textureheight[tex_num] = texture->height << FRACBITS;
}

static void R_InitTexture(int tex_num, const maptexture_t* mtexture,
                          const int* patchlookup)
{
    size_t num_patches = SHORT(mtexture->patchcount) - 1;
    size_t size = sizeof(texture_t) + (sizeof(texpatch_t) * num_patches);

    texture_t* texture = Z_Malloc((int) size, PU_STATIC, NULL);
    texture->width = SHORT(mtexture->width);
    texture->height = SHORT(mtexture->height);
    texture->patchcount = SHORT(mtexture->patchcount);
    memcpy(texture->name, mtexture->name, sizeof(texture->name));
    R_InitTexturePatches(texture, mtexture, patchlookup);

    R_InitTextureData(tex_num, texture);
}

// TEXTURE1
static int* maptex1 = NULL;
static int numtextures1 = 0;
static int maxoff = 0;

// TEXTURE2
static int* maptex2 = NULL;
static int numtextures2 = 0;
static int maxoff2 = 0;

static const maptexture_t* R_GetMapTexture(int map_tex_num, const int* maptex,
                                           int maxoffset)
{
    const int* directory = maptex + 1;
    int offset = LONG(directory[map_tex_num]);
    if (offset > maxoffset) {
        I_Error("R_GetMapTexture: bad texture directory");
    }
    const byte* maptexdata = (const byte *) maptex;
    return (const maptexture_t *) &maptexdata[offset];
}

static void R_InitCommercialTextures(const int* patchlookup) {
    if (maptex2 == NULL) {
        return;
    }

    for (int i = 0; i < numtextures2; i++) {
        int tex_num = i + numtextures1;
        if (tex_num % 64 == 0) {
            printf(".");
        }
        const maptexture_t* mtexture = R_GetMapTexture(i, maptex2, maxoff2);
        R_InitTexture(tex_num, mtexture, patchlookup);
    }
}

static void R_InitSharewareTextures(const int* patchlookup) {
    for (int i = 0; i < numtextures1; i++) {
        if (i % 64 == 0) {
            printf(".");
        }
        const maptexture_t* mtexture = R_GetMapTexture(i, maptex1, maxoff);
        R_InitTexture(i, mtexture, patchlookup);
    }
}

static void R_FreeTextureDefinitions() {
    W_ReleaseLumpName(DEH_String("TEXTURE1"));
    if (maptex2) {
        W_ReleaseLumpName(DEH_String("TEXTURE2"));
    }
    maptex1 = NULL;
    maptex2 = NULL;
}

static void R_AllocCommercialTextures() {
    const char* lump_name = DEH_String("TEXTURE2");
    if (W_CheckNumForName(lump_name) != -1) {
        maptex2 = W_CacheLumpName(lump_name, PU_STATIC);
        numtextures2 = LONG(*maptex2);
        maxoff2 = W_LumpLength(W_GetNumForName(lump_name));
    }
}

static void R_AllocSharewareTextures() {
    const char* lump_name = DEH_String("TEXTURE1");
    maptex1 = W_CacheLumpName(lump_name, PU_STATIC);
    numtextures1 = LONG(*maptex1);
    maxoff = W_LumpLength(W_GetNumForName(lump_name));
}

//
// Load the map texture definitions from textures.lmp. The data is contained in
// one or two lumps, TEXTURE1 for shareware, plus TEXTURE2 for commercial.
//
static void R_AllocTextureDefinitions() {
    R_AllocSharewareTextures();
    R_AllocCommercialTextures();
    numtextures = numtextures1 + numtextures2;
}

//
// R_InitTextures
// Initializes the texture list with the textures from the world map.
//
static void R_InitTextures() {
    R_AllocTextureDefinitions();
    R_AllocTexturesData();
    if (I_ConsoleStdout()) {
        // If stdout is a real console (not a file),
        // print a box around the information.
        R_PrintBox();
    }
    int* patchlookup = R_LoadPatchNamesLump();
    R_InitSharewareTextures(patchlookup);
    R_InitCommercialTextures(patchlookup);
    R_FreeTextureDefinitions();
    Z_Free(patchlookup);

    // Precalculate whatever possible.
    for (int i = 0; i < numtextures; i++) {
        R_GenerateLookup(i);
    }

    R_InitTranslationTable();
    GenerateTextureHashTable();
}

//
// Create translation table for global animation.
//
static void R_InitFlatTranslationTable() {
    size_t size = (numflats + 1) * sizeof(*flattranslation);
    flattranslation = Z_Malloc((int) size, PU_STATIC, 0);
    for (int i = 0; i < numflats; i++) {
        flattranslation[i] = i;
    }
}

//
// R_InitFlats
//
static void R_InitFlats() {
    firstflat = W_GetNumForName(DEH_String("F_START")) + 1;
    lastflat = W_GetNumForName(DEH_String("F_END")) - 1;
    numflats = lastflat - firstflat + 1;
    R_InitFlatTranslationTable();
}


static void R_AllocSpriteData() {
    firstspritelump = W_GetNumForName(DEH_String("S_START")) + 1;
    lastspritelump = W_GetNumForName(DEH_String("S_END")) - 1;
    numspritelumps = lastspritelump - firstspritelump + 1;

    size_t size = numspritelumps * sizeof(*spritewidth);
    spritewidth = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numspritelumps * sizeof(*spriteoffset);
    spriteoffset = Z_Malloc((int) size, PU_STATIC, NULL);

    size = numspritelumps * sizeof(*spritetopoffset);
    spritetopoffset = Z_Malloc((int) size, PU_STATIC, NULL);
}

//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad, so the sprite
// does not need to be cached completely just for having the header info
// ready during rendering.
//
static void R_InitSpriteLumps(void) {
    R_AllocSpriteData();
	
    for (int i = 0; i < numspritelumps; i++) {
        if (i % 64 == 0) {
            printf (".");
        }
        lumpindex_t patch_lump = firstspritelump + i;
        const patch_t* patch = W_CacheLumpNum(patch_lump, PU_CACHE);
	spritewidth[i] = SHORT(patch->width) << FRACBITS;
	spriteoffset[i] = SHORT(patch->leftoffset) << FRACBITS;
	spritetopoffset[i] = SHORT(patch->topoffset) << FRACBITS;
    }
}



//
// R_InitColormaps
//
static void R_InitColormaps() {
    // Load in the light tables, 256 byte align tables.
    int lump = W_GetNumForName(DEH_String("COLORMAP"));
    colormaps = W_CacheLumpNum(lump, PU_STATIC);
}



//
// R_InitData
// Locates all the lumps that will be used by all views.
// Must be called after W_Init.
//
void R_InitData() {
    R_InitTextures();
    printf(".");
    R_InitFlats();
    printf(".");
    R_InitSpriteLumps();
    printf(".");
    R_InitColormaps();
}


//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName(const char* name) {
    int i = W_CheckNumForName(name);
    if (i == -1) {
        char namet[9] = {0};
        memcpy(namet, name, 8);
        I_Error("R_FlatNumForName: %s not found", namet);
    }
    return i - firstflat;
}


//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int R_CheckTextureNumForName(const char* name) {
    if (name[0] == '-') {
        // "NoTexture" marker.
        return 0;
    }
    int key = (int) (W_LumpNameHash(name) % numtextures);
    texture_t* texture = textures_hashtable[key];
    while (texture != NULL) {
        if (!strncasecmp(texture->name, name, 8)) {
            return texture->index;
        }
        texture = texture->next;
    }
    return -1;
}


//
// R_TextureNumForName
// Calls R_CheckTextureNumForName, aborts with error message.
//
int R_TextureNumForName(const char *name) {
    int i = R_CheckTextureNumForName(name);
    if (i == -1) {
        I_Error("R_TextureNumForName: %s not found", name);
    }
    return i;
}


//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
static void R_PrecacheSprites() {
    char* spritepresent = Z_Malloc(numsprites, PU_STATIC, NULL);
    memset(spritepresent, 0, numsprites);
    thinker_t* th = thinkercap.next;
    while (th != &thinkercap) {
        if (th->function.acp1 == (actionf_p1) P_MobjThinker) {
            spritenum_t sprite = ((mobj_t *) th)->sprite;
            spritepresent[sprite] = 1;
        }
        th = th->next;
    }

    for (int i = 0; i < numsprites; i++) {
        if (spritepresent[i] == 0) {
            continue;
        }
        for (int j = 0; j < sprites[i].numframes; j++) {
            const spriteframe_t* sf = &sprites[i].spriteframes[j];
            for (int k = 0; k < 8; k++) {
                int lump = firstspritelump + sf->lump[k];
                W_CacheLumpNum(lump, PU_CACHE);
            }
        }
    }

    Z_Free(spritepresent);
}

static void R_PrecacheTextures() {
    char* texturepresent = Z_Malloc(numtextures, PU_STATIC, NULL);
    memset(texturepresent, 0, numtextures);
    for (int i = 0; i < numsides; i++) {
        texturepresent[sides[i].toptexture] = 1;
        texturepresent[sides[i].midtexture] = 1;
        texturepresent[sides[i].bottomtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to indicate a sky floor/ceiling
    // as a flat, while the sky texture is stored like a wall texture, with
    // an episode dependent name.
    texturepresent[sky_tex] = 1;

    for (int i = 0; i < numtextures; i++) {
        if (texturepresent[i] == 0) {
            continue;
        }
        const texture_t* texture = textures[i];
        for (int j = 0; j < texture->patchcount; j++) {
            int lump = texture->patches[j].patch;
            W_CacheLumpNum(lump, PU_CACHE);
        }
    }

    Z_Free(texturepresent);
}

//
// Precache flats.
//
static void R_PrecacheFlats() {
    char* flatpresent = Z_Malloc(numflats, PU_STATIC, NULL);
    memset(flatpresent, 0, numflats);

    for (int i = 0; i < numsectors; i++) {
        flatpresent[sectors[i].floorpic] = 1;
        flatpresent[sectors[i].ceilingpic] = 1;
    }

    for (int i = 0; i < numflats; i++) {
        if (flatpresent[i]) {
            int lump = firstflat + i;
            W_CacheLumpNum(lump, PU_CACHE);
        }
    }

    Z_Free(flatpresent);
}

void R_PrecacheLevel() {
    if (demoplayback) {
        return;
    }
    R_PrecacheFlats();
    R_PrecacheTextures();
    R_PrecacheSprites();
}
