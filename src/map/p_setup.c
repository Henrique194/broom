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
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//


#include "p_setup.h"

#include "z_zone.h"

#include "p_switch.h"

#include "deh_str.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_bbox.h"

#include "g_game.h"

#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "doomstat.h"


void P_SpawnMapThing(const mapthing_t *mthing);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int numvertexes;
vertex_t* vertexes;

int numsegs;
seg_t* segs;

int numsectors;
sector_t* sectors;

int numsubsectors;
subsector_t* subsectors;

int numnodes;
node_t* nodes;

int numlines;
line_t* lines;

int numsides;
side_t* sides;

static int totallines;

// BLOCKMAP
// Created from axis aligned bounding box of the map,
// a rectangular array of blocks of size ...
// Used to speed up collision detection by spatial subdivision in 2D.

//
// x coordinate of grid origin
//
fixed_t bmaporgx;
//
// y coordinate of grid origin
//
fixed_t bmaporgy;
//
// Number of columns
//
int bmapwidth;
//
// Number of rows
//
int bmapheight;


short* blockmap; // int for larger maps

// offsets in blockmap are from here
short* blockmaplump;

// for thing chains
mobj_t** blocklinks;


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
byte*		rejectmatrix;


// Maintain single and multi player starting spots.
#define MAX_DEATHMATCH_STARTS	10

mapthing_t	deathmatchstarts[MAX_DEATHMATCH_STARTS];
mapthing_t*	deathmatch_p;
mapthing_t	playerstarts[MAXPLAYERS];
bool     playerstartsingame[MAXPLAYERS];



static void P_AllocVertexes(int lump) {
    // Determine number of lumps:
    // total lump length / vertex record length.
    numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);

    // Allocate zone memory for buffer.
    vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, NULL);
}

//
// P_LoadVertexes
//
static void P_LoadVertexes(int lump) {
    // Load data into cache.
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapvertex_t* ml = (mapvertex_t *) data;

    P_AllocVertexes(lump);

    // Copy and convert vertex coordinates, internal representation as fixed.
    for (int i = 0; i < numvertexes; i++) {
        const mapvertex_t* vertex_def = &ml[i];
        vertex_t* vertex = &vertexes[i];
        vertex->x = SHORT(vertex_def->x) << FRACBITS;
        vertex->y = SHORT(vertex_def->y) << FRACBITS;
    }

    // Free buffer memory.
    W_ReleaseLumpNum(lump);
}

//
// GetSectorAtNullAddress
//
static sector_t* GetSectorAtNullAddress()
{
    static bool null_sector_is_initialized = false;
    static sector_t null_sector;

    if (!null_sector_is_initialized)
    {
        memset(&null_sector, 0, sizeof(null_sector));
        I_GetMemoryValue(0, &null_sector.floorheight, 4);
        I_GetMemoryValue(4, &null_sector.ceilingheight, 4);
        null_sector_is_initialized = true;
    }

    return &null_sector;
}

static void P_SetSegSectors(const mapseg_t* seg_def, seg_t* seg) {
    int side = SHORT(seg_def->side);
    const line_t* line = seg->linedef;
    int front_side = line->sidenum[side];
    int back_side = line->sidenum[side ^ 1];

    seg->frontsector = sides[front_side].sector;

    if ((line->flags & ML_TWOSIDED) == 0) {
        seg->backsector = NULL;
        return;
    }
    // If the back_side is out of range, this may be a "glass hack"
    // impassible window. Point at side #0 (this may not be the correct
    // Vanilla behavior; however, it seems to work for OTTAWAU.WAD, which
    // is the one place I've seen this trick used).
    if (back_side < 0 || back_side >= numsides) {
        seg->backsector = GetSectorAtNullAddress();
    } else {
        seg->backsector = sides[back_side].sector;
    }
}

static void P_SetSegSide(const mapseg_t* seg_def, seg_t* seg) {
    const line_t* line = seg->linedef;
    int side = SHORT(seg_def->side);
    unsigned int line_side = (unsigned) line->sidenum[side];

    // e6y: check for wrong indexes
    if (line_side >= (unsigned) numsides) {
        int seg_id = seg - segs;
        int line_id = line - lines;
        I_Error("P_SetSegSide: linedef %d for seg %d references a "
                "non-existent sidedef %d",
                line_id, seg_id, line_side);
    }

    seg->sidedef = &sides[line->sidenum[side]];
}

static void P_SetSegLine(const mapseg_t* seg_def, seg_t* seg) {
    seg->linedef = &lines[SHORT(seg_def->linedef)];
}

static void P_SetSideMetadata(const mapseg_t* seg_def, seg_t* seg) {
    seg->angle = (SHORT(seg_def->angle)) << FRACBITS;
    seg->offset = (SHORT(seg_def->offset)) << FRACBITS;
}

static void P_SetSideVertexes(const mapseg_t* seg_def, seg_t* seg) {
    seg->v1 = &vertexes[SHORT(seg_def->v1)];
    seg->v2 = &vertexes[SHORT(seg_def->v2)];
}

static void P_LoadSeg(const mapseg_t* seg_def, seg_t* seg) {
    P_SetSideVertexes(seg_def, seg);
    P_SetSideMetadata(seg_def, seg);
    P_SetSegLine(seg_def, seg);
    P_SetSegSide(seg_def, seg);
    P_SetSegSectors(seg_def, seg);
}

static void P_AllocSegs(int lump) {
    numsegs = W_LumpLength(lump) / sizeof(mapseg_t);
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
    memset(segs, 0, numsegs * sizeof(seg_t));
}

//
// P_LoadSegs
//
static void P_LoadSegs(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapseg_t* ml = (mapseg_t *) data;

    P_AllocSegs(lump);

    for (int i = 0; i < numsegs; i++) {
        P_LoadSeg(&ml[i], &segs[i]);
    }

    W_ReleaseLumpNum(lump);
}

static void P_AllocSubSectors(int lump) {
    numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_t);
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, NULL);
    memset(subsectors, 0, numsubsectors * sizeof(subsector_t));
}

//
// P_LoadSubSectors
//
static void P_LoadSubSectors(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapsubsector_t* ms = (mapsubsector_t *) data;

    P_AllocSubSectors(lump);

    for (int i = 0; i < numsubsectors; i++) {
        const mapsubsector_t* subsector_def = &ms[i];
        subsector_t* subsector = &subsectors[i];

        subsector->numlines = SHORT(subsector_def->numsegs);
        subsector->firstline = SHORT(subsector_def->firstseg);
    }

    W_ReleaseLumpNum(lump);
}

static void P_LoadSector(const mapsector_t* sector_def, sector_t* sector) {
    sector->floorheight = SHORT(sector_def->floorheight) << FRACBITS;
    sector->ceilingheight = SHORT(sector_def->ceilingheight) << FRACBITS;
    sector->floorpic = (short) R_FlatNumForName(sector_def->floorpic);
    sector->ceilingpic = (short) R_FlatNumForName(sector_def->ceilingpic);
    sector->lightlevel = SHORT(sector_def->lightlevel);
    sector->special = SHORT(sector_def->special);
    sector->tag = SHORT(sector_def->tag);
    sector->thinglist = NULL;
}

static void P_AllocSectors(int lump) {
    numsectors = W_LumpLength(lump) / sizeof(mapsector_t);
    sectors = Z_Malloc(numsectors * sizeof(sector_t), PU_LEVEL, NULL);
    memset(sectors, 0, numsectors * sizeof(sector_t));
}

//
// P_LoadSectors
//
static void P_LoadSectors(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapsector_t* ms = (mapsector_t *) data;

    P_AllocSectors(lump);

    for (int i = 0; i < numsectors; i++) {
        P_LoadSector(&ms[i], &sectors[i]);
    }

    W_ReleaseLumpNum(lump);
}

static void P_LoadNodeChildren(const mapnode_t* node_def, node_t* node) {
    for (int j = 0; j < 2; j++) {
        node->children[j] = SHORT(node_def->children[j]);
        for (int k = 0; k < 4; k++) {
            node->bbox[j][k] = SHORT(node_def->bbox[j][k]) << FRACBITS;
        }
    }
}

static void P_LoadNodePartitionLine(const mapnode_t* node_def, node_t* node) {
    node->x = SHORT(node_def->x) << FRACBITS;
    node->y = SHORT(node_def->y) << FRACBITS;
    node->dx = SHORT(node_def->dx) << FRACBITS;
    node->dy = SHORT(node_def->dy) << FRACBITS;
}

static void P_AllocNodes(int lump) {
    numnodes = W_LumpLength(lump) / sizeof(mapnode_t);
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, NULL);
}

//
// P_LoadNodes
//
static void P_LoadNodes(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapnode_t* mn = (mapnode_t *) data;

    P_AllocNodes(lump);

    for (int i = 0; i < numnodes; i++) {
        const mapnode_t* node_def = &mn[i];
        node_t* node = &nodes[i];

        P_LoadNodePartitionLine(node_def, node);
        P_LoadNodeChildren(node_def, node);
    }

    W_ReleaseLumpNum(lump);
}

static void P_CheckPlayersStart() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i] && !playerstartsingame[i]) {
            I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);
        }
        playerstartsingame[i] = false;
    }
}

static bool P_CanSpawnThing(const mapthing_t* mt) {
    if (gamemode == commercial) {
        return true;
    }
    // Do not spawn cool, new monsters if !commercial
    switch (SHORT(mt->type)) {
        case 68:    // Arachnotron
        case 64:    // Archvile
        case 88:    // Boss Brain
        case 89:    // Boss Shooter
        case 69:    // Hell Knight
        case 67:    // Mancubus
        case 71:    // Pain Elemental
        case 65:    // Former Human Commando
        case 66:    // Revenant
        case 84:    // Wolf SS
            return false;
        default:
            return true;
    }
}

//
// P_LoadThings
//
static void P_LoadThings(int lump) {
    byte *data = W_CacheLumpNum(lump, PU_STATIC);
    int numthings = W_LumpLength(lump) / sizeof(mapthing_t);
    const mapthing_t *mt = (mapthing_t *) data;

    for (int i = 0; i < numthings; i++, mt++) {
        if (!P_CanSpawnThing(mt)) {
            break;
        }
        mapthing_t spawnthing;
        spawnthing.x = SHORT(mt->x);
        spawnthing.y = SHORT(mt->y);
        spawnthing.angle = SHORT(mt->angle);
        spawnthing.type = SHORT(mt->type);
        spawnthing.options = SHORT(mt->options);
        P_SpawnMapThing(&spawnthing);
    }

    if (!deathmatch) {
        P_CheckPlayersStart();
    }

    W_ReleaseLumpNum(lump);
}

static void P_SetLineSectors(line_t* ld) {
    if (ld->sidenum[0] != -1) {
        ld->frontsector = sides[ld->sidenum[0]].sector;
    } else {
        ld->frontsector = NULL;
    }

    if (ld->sidenum[1] != -1) {
        ld->backsector = sides[ld->sidenum[1]].sector;
    } else {
        ld->backsector = NULL;
    }
}

static void P_SetLineSides(const maplinedef_t* line_def, line_t* ld) {
    ld->sidenum[0] = SHORT(line_def->sidenum[0]);
    ld->sidenum[1] = SHORT(line_def->sidenum[1]);
}

static void P_SetLineBoundingBox(line_t* ld) {
    const vertex_t* v1 = ld->v1;
    const vertex_t* v2 = ld->v2;

    if (v1->x < v2->x) {
        ld->bbox[BOXLEFT] = v1->x;
        ld->bbox[BOXRIGHT] = v2->x;
    } else {
        ld->bbox[BOXLEFT] = v2->x;
        ld->bbox[BOXRIGHT] = v1->x;
    }

    if (v1->y < v2->y) {
        ld->bbox[BOXBOTTOM] = v1->y;
        ld->bbox[BOXTOP] = v2->y;
    } else {
        ld->bbox[BOXBOTTOM] = v2->y;
        ld->bbox[BOXTOP] = v1->y;
    }
}

static void P_SetLineSlope(line_t* ld) {
    if (ld->dx == 0) {
        ld->slopetype = ST_VERTICAL;
        return;
    }
    if (ld->dy == 0) {
        ld->slopetype = ST_HORIZONTAL;
        return;
    }
    if (FixedDiv(ld->dy, ld->dx) > 0) {
        ld->slopetype = ST_POSITIVE;
        return;
    }
    ld->slopetype = ST_NEGATIVE;
}

static void P_SetLineVertexes(const maplinedef_t* line_def, line_t* ld) {
    vertex_t* v1 = &vertexes[SHORT(line_def->v1)];
    vertex_t* v2 = &vertexes[SHORT(line_def->v2)];

    ld->v1 = v1;
    ld->v2 = v2;
    ld->dx = v2->x - v1->x;
    ld->dy = v2->y - v1->y;
}

static void P_SetLineMetadata(const maplinedef_t* line_def, line_t* ld) {
    ld->flags = SHORT(line_def->flags);
    ld->special = SHORT(line_def->special);
    ld->tag = SHORT(line_def->tag);
}

static void P_LoadLineDef(const maplinedef_t* mld, line_t* ld) {
    P_SetLineMetadata(mld, ld);
    P_SetLineVertexes(mld, ld);
    P_SetLineSlope(ld);
    P_SetLineBoundingBox(ld);
    P_SetLineSides(mld, ld);
    P_SetLineSectors(ld);
}

static void P_AllocLines(int lump) {
    numlines = W_LumpLength(lump) / sizeof(maplinedef_t);
    lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, NULL);
    memset(lines, 0, numlines * sizeof(line_t));
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
static void P_LoadLineDefs(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const maplinedef_t* line_defs = (maplinedef_t *) data;

    P_AllocLines(lump);

    for (int i = 0; i < numlines; i++) {
        P_LoadLineDef(&line_defs[i], &lines[i]);
    }

    W_ReleaseLumpNum(lump);
}

static void P_AllocSides(int lump) {
    numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
    sides = Z_Malloc(numsides * sizeof(side_t), PU_LEVEL, NULL);
    memset(sides, 0, numsides * sizeof(side_t));
}

//
// P_LoadSideDefs
//
static void P_LoadSideDefs(int lump) {
    byte* data = W_CacheLumpNum(lump, PU_STATIC);
    const mapsidedef_t* msd = (mapsidedef_t *) data;

    P_AllocSides(lump);

    for (int i = 0; i < numsides; i++) {
        const mapsidedef_t* side_def = &msd[i];
        side_t* side = &sides[i];

        side->textureoffset = SHORT(side_def->textureoffset) << FRACBITS;
        side->rowoffset = SHORT(side_def->rowoffset) << FRACBITS;
        side->toptexture = (short) R_TextureNumForName(side_def->toptexture);
        side->bottomtexture = (short) R_TextureNumForName(side_def->bottomtexture);
        side->midtexture = (short) R_TextureNumForName(side_def->midtexture);
        side->sector = &sectors[SHORT(side_def->sector)];
    }

    W_ReleaseLumpNum(lump);
}

//
// Clear out mobj chains.
//
static void P_AllocBlockLinks() {
    size_t count = sizeof(*blocklinks) * bmapwidth * bmapheight;
    blocklinks = Z_Malloc((int) count, PU_LEVEL, NULL);
    memset(blocklinks, 0, count);
}

static void P_ReadBlockMapHeader() {
    bmaporgx = blockmaplump[0] << FRACBITS;
    bmaporgy = blockmaplump[1] << FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];
}

static void P_ReadBlockMapLump(int lump) {
    int lumplen = W_LumpLength(lump);
    int count = lumplen / 2;

    blockmaplump = Z_Malloc(lumplen, PU_LEVEL, NULL);
    W_ReadLump(lump, blockmaplump);
    // Swap all short integers to native byte ordering.
    for (int i = 0; i < count; i++) {
        blockmaplump[i] = SHORT(blockmaplump[i]);
    }

    blockmap = blockmaplump + 4;
}


//
// P_LoadBlockMap
//
static void P_LoadBlockMap(int lump) {
    P_ReadBlockMapLump(lump);
    P_ReadBlockMapHeader();
    P_AllocBlockLinks();
}

static void P_SetSectorBoundingBox(sector_t* sector, const fixed_t bbox[4]) {
    // Adjust bounding box to map blocks.

    int block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block >= bmapheight) ? bmapheight - 1 : block;
    sector->blockbox[BOXTOP] = block;

    block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block < 0) ? 0 : block;
    sector->blockbox[BOXBOTTOM] = block;

    block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block >= bmapwidth) ? bmapwidth - 1 : block;
    sector->blockbox[BOXRIGHT] = block;

    block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block < 0) ? 0 : block;
    sector->blockbox[BOXLEFT] = block;
}

static void P_SetSectorSoundOrigin(sector_t* sector, const fixed_t bbox[4]) {
    // Set the degenmobj_t to the middle of the bounding box.
    sector->soundorg.x = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
    sector->soundorg.y = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;
}

//
// Generate bounding boxes for sectors.
//
static void P_SetSectorsBoundingBox() {
    fixed_t bbox[4];

    for (int i = 0; i < numsectors; i++) {
        sector_t* sector = &sectors[i];

        // Build the bounding box.
        M_ClearBox(bbox);
        for (int j = 0; j < sector->linecount; j++) {
            const line_t* li = sector->lines[j];
            M_AddToBox(bbox, li->v1->x, li->v1->y);
            M_AddToBox(bbox, li->v2->x, li->v2->y);
        }

        P_SetSectorSoundOrigin(sector, bbox);
        P_SetSectorBoundingBox(sector, bbox);
    }
}

static void P_AssignLinesToSectors() {
    for (int i = 0; i < numlines; ++i) {
        line_t* line = &lines[i];

        if (line->frontsector) {
            sector_t* sector = line->frontsector;
            sector->lines[sector->linecount] = line;
            sector->linecount++;
        }
        if (line->backsector && line->frontsector != line->backsector) {
            sector_t* sector = line->backsector;
            sector->lines[sector->linecount] = line;
            sector->linecount++;
        }
    }
}

static void P_BuildSectorLineTable() {
    size_t size = totallines * sizeof(line_t *);
    line_t** linebuffer = Z_Malloc((int) size, PU_LEVEL, NULL);

    for (int i = 0; i < numsectors; ++i) {
        // Assign the line buffer for this sector.
        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;

        // Reset linecount to zero so in the next stage
        // we can count lines into the list.
        sectors[i].linecount = 0;
    }
}

//
// Count number of lines in each sector.
//
static void P_CountSectorLines() {
    totallines = 0;

    for (int i = 0; i < numlines; i++) {
        line_t* li = &lines[i];
        li->frontsector->linecount++;
        totallines++;
        if (li->backsector && li->backsector != li->frontsector) {
            li->backsector->linecount++;
            totallines++;
        }
    }
}

//
// Look up sector number for each subsector.
//
static void P_LinkSubSectorsToSectors() {
    for (int i = 0; i < numsubsectors; i++) {
        subsector_t* sub_sector = &subsectors[i];
        seg_t* seg = &segs[sub_sector->firstline];
        sub_sector->sector = seg->sidedef->sector;
    }
}

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
static void P_GroupLines() {
    P_LinkSubSectorsToSectors();
    P_CountSectorLines();
    P_BuildSectorLineTable();
    P_AssignLinesToSectors();
    P_SetSectorsBoundingBox();
}

//
// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.
//
static void PadRejectArray(byte* array, unsigned int len) {
    byte *dest;
    unsigned int padvalue;

    // Values to pad the REJECT array with:
    unsigned int rejectpad[4] = {
        0,       // Size
        0,       // Part of z_zone block header
        50,      // PU_LEVEL
        0x1d4a11 // DOOM_CONST_ZONEID
    };
    rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

    // Copy values from rejectpad into the destination array.
    dest = array;
    for (unsigned int i = 0; i < len && i < sizeof(rejectpad); ++i) {
        unsigned int byte_num = i % 4;
        *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
        ++dest;
    }

    // We only have a limited pad size.
    // Print a warning if the REJECT lump is too small.
    if (len > sizeof(rejectpad)) {
        fprintf(stderr,
                "PadRejectArray: REJECT lump too short to pad! (%u > %i)\n",
                len, (int) sizeof(rejectpad));

        // Pad remaining space with 0 (or 0xff, if specified on command line).
        if (M_CheckParm("-reject_pad_with_ff")) {
            padvalue = 0xff;
        } else {
            padvalue = 0x00;
        }

        memset(array + sizeof(rejectpad), padvalue, len - sizeof(rejectpad));
    }
}

static void P_LoadReject(int lumpnum) {
    // Calculate the size that the REJECT lump *should* be.
    int minlength = (numsectors * numsectors + 7) / 8;
    int lumplen = W_LumpLength(lumpnum);

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size and
    // pad it with appropriate data.
    if (lumplen >= minlength) {
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
        return;
    }
    rejectmatrix = Z_Malloc(minlength, PU_LEVEL, &rejectmatrix);
    W_ReadLump(lumpnum, rejectmatrix);
    PadRejectArray(&rejectmatrix[lumplen], minlength - lumplen);
}

// pointer to the current map lump info struct
lumpinfo_t *maplumpinfo;

static void P_ClearSpecialRespawnQueue() {
    iquehead = 0;
    iquetail = 0;
}

static void P_RandomlySpawnActivePlayers() {
    for (int i = 0; i < MAXPLAYERS; i++) {
        if (playeringame[i]) {
            players[i].mo = NULL;
            G_DeathMatchSpawnPlayer(i);
        }
    }
}

//
// find map name
//
static void P_GetLevelName(int episode, int map, char* lumpname) {
    if (gamemode != commercial) {
        lumpname[0] = 'E';
        lumpname[1] = (char) ('0' + episode);
        lumpname[2] = 'M';
        lumpname[3] = (char) ('0' + map);
        lumpname[4] = 0;
        return;
    }
    if (map < 10) {
        DEH_snprintf(lumpname, 9, "map0%i", map);
        return;
    }
    DEH_snprintf(lumpname, 9, "map%i", map);
}

static int P_GetLevelLump(int episode, int map) {
    char lumpname[9];
    P_GetLevelName(episode, map, lumpname);
    return W_GetNumForName(lumpname);
}

static void P_LoadLevelLumps(int episode, int map) {
    int lumpnum = P_GetLevelLump(episode, map);

    maplumpinfo = lumpinfo[lumpnum];
    leveltime = 0;
    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;

    P_LoadBlockMap(lumpnum + ML_BLOCKMAP);
    P_LoadVertexes(lumpnum + ML_VERTEXES);
    P_LoadSectors(lumpnum + ML_SECTORS);
    P_LoadSideDefs(lumpnum + ML_SIDEDEFS);
    P_LoadLineDefs(lumpnum + ML_LINEDEFS);
    P_LoadSubSectors(lumpnum + ML_SSECTORS);
    P_LoadNodes(lumpnum + ML_NODES);
    P_LoadSegs(lumpnum + ML_SEGS);
    P_GroupLines();
    P_LoadReject(lumpnum + ML_REJECT);
    P_LoadThings(lumpnum + ML_THINGS);
}

static void P_FreeMemoryForLevel() {
    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start();
    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);
}

//
// Initial height of PointOfView will be set by player think.
//
static void P_ResetPlayerViewZ() {
    players[consoleplayer].viewz = 1;
}

static void P_ClearPlayersWIStats() {
    totalkills = 0;
    totalitems = 0;
    totalsecret = 0;
    wminfo.maxfrags = 0;
    wminfo.partime = 180;

    for (int i = 0; i < MAXPLAYERS; i++) {
        players[i].killcount = 0;
        players[i].secretcount = 0;
        players[i].itemcount = 0;
    }
}

//
// P_SetupLevel
//
void P_SetupLevel(int episode, int map) {
    P_ClearPlayersWIStats();
    P_ResetPlayerViewZ();
    P_FreeMemoryForLevel();
    P_InitThinkers();
    // if working with a development map, reload it
    W_Reload();
    P_LoadLevelLumps(episode, map);
    if (deathmatch) {
        P_RandomlySpawnActivePlayers();
    }
    P_ClearSpecialRespawnQueue();
    // set up world state
    P_SpawnSpecials();
    if (precache) {
        // preload graphics
        R_PrecacheLevel();
    }
}


//
// P_Init
//
void P_Init() {
    P_InitSwitchList();
    P_InitPicAnims();
    R_InitSprites(sprnames);
}
