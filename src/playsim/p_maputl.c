//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2005, 2006 Andrey Budko
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
//	Movement/collision utility functions, as used by function in p_map.c.
//	BLOCKMAP Iterator functions, and some PIT_* functions to use for iteration.
//



#include <stdlib.h>


#include "m_bbox.h"
#include "m_misc.h"

#include "doomstat.h"
#include "p_local.h"


// State.
#include "r_state.h"

//
// P_ApproxDistance
// This function approximates the Euclidean distance between two points
// using a truncated Taylor expansion of the square root function. The
// approximation is faster than computing the exact distance, but it is
// less precise. It is based on a common technique found in Graphics Gems 1
// (pp. 427-429), where the square root is approximated by its second-order
// Taylor series.
//
fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy) {
    dx = abs(dx);
    dy = abs(dy);
    if (dx < dy) {
        return (dx >> 1) + dy;
    }
    return dx + (dy >> 1);
}


//
// P_PointOnLineSide
// Returns 0 or 1
//
int P_PointOnLineSide(fixed_t x, fixed_t y, const line_t* line) {
    if (line->dx == 0) {
        if (x <= line->v1->x) {
            return line->dy > 0;
        }
        return line->dy < 0;
    }
    if (line->dy == 0) {
        if (y <= line->v1->y) {
            return line->dx < 0;
        }
        return line->dx > 0;
    }

    fixed_t dx = x - line->v1->x;
    fixed_t dy = y - line->v1->y;

    fixed_t left = FixedMul(line->dy >> FRACBITS, dx);
    fixed_t right = FixedMul(dy, line->dx >> FRACBITS);

    return right >= left;
}


//
// P_BoxOnLineSide
// Considers the line to be infinite.
// Returns side 0 or 1, -1 if box crosses the line.
//
int P_BoxOnLineSide(const fixed_t* tmbox, const line_t* ld) {
    int p1 = 0;
    int p2 = 0;

    switch (ld->slopetype) {
        case ST_HORIZONTAL:
            p1 = tmbox[BOXTOP] > ld->v1->y;
            p2 = tmbox[BOXBOTTOM] > ld->v1->y;
            if (ld->dx < 0) {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;
        case ST_VERTICAL:
            p1 = tmbox[BOXRIGHT] < ld->v1->x;
            p2 = tmbox[BOXLEFT] < ld->v1->x;
            if (ld->dy < 0) {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;
        case ST_POSITIVE:
            p1 = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld);
            p2 = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
            break;
        case ST_NEGATIVE:
            p1 = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
            p2 = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
            break;
    }

    if (p1 == p2) {
        return p1;
    }
    return -1;
}


//
// P_PointOnDivlineSide
// Returns 0 (front side) or 1 (back side).
//
static int P_PointOnDivlineSide(fixed_t x, fixed_t y, const divline_t* line) {
    if (line->dx == 0) {
        if (x <= line->x) {
            return line->dy > 0;
        }
        return line->dy < 0;
    }
    if (line->dy == 0) {
        if (y <= line->y) {
            return line->dx < 0;
        }
        return line->dx > 0;
    }

    fixed_t dx = x - line->x;
    fixed_t dy = y - line->y;

    // try to quickly decide by looking at sign bits
    if ((line->dy ^ line->dx ^ dx ^ dy) & 0x80000000) {
        if ((line->dy ^ dx) & 0x80000000) {
            // left is negative and right is positive
            return 1;
        }
        return 0;
    }

    fixed_t left = FixedMul(line->dy >> 8, dx >> 8);
    fixed_t right = FixedMul(dy >> 8, line->dx >> 8);

    return right >= left;
}


//
// P_MakeDivline
//
static void P_MakeDivline(const line_t* li, divline_t* dl) {
    dl->x = li->v1->x;
    dl->y = li->v1->y;
    dl->dx = li->dx;
    dl->dy = li->dy;
}


//
// P_InterceptVector
// Returns the fractional intercept point along the first divline.
// This is only called by the addthings and addlines traversers.
//
fixed_t P_InterceptVector(const divline_t* v2, const divline_t* v1) {
    fixed_t den = FixedMul(v1->dy >> 8, v2->dx) - FixedMul(v1->dx >> 8, v2->dy);

    if (den == 0) {
        return 0;
    }

    fixed_t num = FixedMul((v1->x - v2->x) >> 8, v1->dy)
          + FixedMul((v2->y - v1->y) >> 8, v1->dx);

    return FixedDiv(num, den);
}


fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t	lowfloor;

//
// P_LineOpening
// Sets opentop and openbottom to the window through a two sided line.
// OPTIMIZE: keep this precalculated
//
void P_LineOpening(const line_t* line) {
    if (line->sidenum[1] == -1) {
	// single sided line
	openrange = 0;
	return;
    }
    const sector_t* front = line->frontsector;
    const sector_t* back = line->backsector;
    if (front->ceilingheight < back->ceilingheight) {
        opentop = front->ceilingheight;
    } else {
        opentop = back->ceilingheight;
    }
    if (front->floorheight > back->floorheight) {
	openbottom = front->floorheight;
	lowfloor = back->floorheight;
    } else {
	openbottom = back->floorheight;
	lowfloor = front->floorheight;
    }
    openrange = opentop - openbottom;
}


//
// THING POSITION SETTING
//

static bool P_IsThingOnTheMap(int blockx, int blocky) {
    return blockx >= 0
           && blockx < bmapwidth
           && blocky >= 0
           && blocky < bmapheight;
}

static void P_AddToBlockList(mobj_t* thing) {
    int blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
    int blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;

    if (!P_IsThingOnTheMap(blockx, blocky)) {
        // Thing is off the map.
        thing->bnext = NULL;
        thing->bprev = NULL;
        return;
    }
    // Insert thing as new head in block links.
    int offset = blockx + (blocky * bmapwidth);
    mobj_t* head = blocklinks[offset];
    thing->bprev = NULL;
    thing->bnext = head;
    if (head) {
        head->bprev = thing;
    }
    blocklinks[offset] = thing;
}

static void P_RemoveFromBlockList(mobj_t* thing) {
    // inert things don't need to be in blockmap
    // unlink from block map
    if (thing->bnext) {
        thing->bnext->bprev = thing->bprev;
    }
    if (thing->bprev) {
        thing->bprev->bnext = thing->bnext;
        return;
    }
    int blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
    int blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;
    if (P_IsThingOnTheMap(blockx, blocky)) {
        int offset = blockx + (blocky * bmapwidth);
        blocklinks[offset] = thing->bnext;
    }
}

static bool P_IsInert(const mobj_t* thing) {
    return (thing->flags & MF_NOBLOCKMAP) != 0;
}

static void P_AddToSectorList(mobj_t* thing) {
    sector_t* sector = thing->subsector->sector;
    thing->sprev = NULL;
    thing->snext = sector->thinglist;
    if (sector->thinglist) {
        sector->thinglist->sprev = thing;
    }
    sector->thinglist = thing;
}

static void P_RemoveFromSectorList(mobj_t* thing) {
    // inert things don't need to be in blockmap?
    // unlink from subsector
    if (thing->snext) {
        thing->snext->sprev = thing->sprev;
    }
    if (thing->sprev) {
        thing->sprev->snext = thing->snext;
    } else {
        thing->subsector->sector->thinglist = thing->snext;
    }
}

static bool P_IsInvisible(const mobj_t* thing) {
    return (thing->flags & MF_NOSECTOR) != 0;
}

//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other lookups maintaining lists of
// things inside these structures need to be updated.
//
void P_UnsetThingPosition(mobj_t* thing) {
    if (!P_IsInvisible(thing)) {
        P_RemoveFromSectorList(thing);
    }
    if (!P_IsInert(thing)) {
        P_RemoveFromBlockList(thing);
    }
}

static void P_SetSubSector(mobj_t* thing) {
    thing->subsector = R_PointInSubsector(thing->x, thing->y);
}

//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on it's (x, y).
// Sets thing->subsector properly.
//
void P_SetThingPosition(mobj_t* thing) {
    P_SetSubSector(thing);
    if (!P_IsInvisible(thing)) {
        // Enable rendering for map object.
        P_AddToSectorList(thing);
    }
    if (!P_IsInert(thing)) {
        // Enable collision for map object.
        P_AddToBlockList(thing);
    }
}



//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock, call the passed PIT_* function.
// If the function returns false, exit with false without checking anything else.
//


//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines that
// are marked in multiple mapblocks, so increment validcount before
// the first call to P_BlockLinesIterator, then make one or more calls to it.
//
// Returns false if blocked, true otherwise.
//
bool P_BlockLinesIterator(int x, int y, bool (*func)(line_t *)) {
    if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight) {
        return true;
    }

    int offset = x + (y * bmapwidth);
    offset = blockmap[offset];

    for (const short* list = &blockmaplump[offset]; *list != -1; list++) {
        line_t* ld = &lines[*list];
        if (ld->validcount == validcount) {
            // Line has already been checked.
            continue;
        }
        ld->validcount = validcount;
        if (!func(ld)) {
            return false;
        }
    }

    // Everything was checked.
    return true;
}


//
// P_BlockThingsIterator
// Return false if blocked, true otherwise.
//
bool P_BlockThingsIterator(int x, int y, bool (*func)(mobj_t *)) {
    if (!P_IsThingOnTheMap(x, y)) {
        return true;
    }

    int block_map_pos = x + (y * bmapwidth);
    mobj_t* mobj = blocklinks[block_map_pos];
    LINKED_LIST_CHECK_NO_CYCLE(mobj_t, mobj, bnext);
    while (mobj) {
        if (!func(mobj)) {
            return false;
        }
        mobj = mobj->bnext;
    }
    return true;
}


//
// INTERCEPT ROUTINES
//
static intercept_t intercepts[MAXINTERCEPTS];
static intercept_t* intercept_p;

static bool earlyout;
divline_t trace;

//
// Intercepts Overrun emulation, from PrBoom-plus.
// Thanks to Andrey Budko (entryway) for researching this and his
// implementation of Intercepts Overrun emulation in PrBoom-plus
// which this is based on.
//
typedef struct
{
    int len;
    void* addr;
    bool int16_array;
} intercepts_overrun_t;

//
// Intercepts memory table. This is where various variables are located
// in memory in Vanilla Doom. When the intercepts table overflows, we
// need to write to them.
//
// Almost all the values to overwrite are 32-bit integers, except for
// playerstarts, which is effectively an array of 16-bit integers and
// must be treated differently.
//
static intercepts_overrun_t intercepts_overrun[] =
{
    {4,   NULL,                          false},
    {4,   NULL, /* &earlyout, */         false},
    {4,   NULL, /* &intercept_p, */      false},
    {4,   &lowfloor,                     false},
    {4,   &openbottom,                   false},
    {4,   &opentop,                      false},
    {4,   &openrange,                    false},
    {4,   NULL,                          false},
    {120, NULL, /* &activeplats, */      false},
    {8,   NULL,                          false},
    {4,   &bulletslope,                  false},
    {4,   NULL, /* &swingx, */           false},
    {4,   NULL, /* &swingy, */           false},
    {4,   NULL,                          false},
    {40,  &playerstarts,                 true},
    {4,   NULL, /* &blocklinks, */       false},
    {4,   &bmapwidth,                    false},
    {4,   NULL, /* &blockmap, */         false},
    {4,   &bmaporgx,                     false},
    {4,   &bmaporgy,                     false},
    {4,   NULL, /* &blockmaplump, */     false},
    {4,   &bmapheight,                   false},
    {0,   NULL,                          false},
};

//
// Overwrite a specific memory location with a value.
//
static void InterceptsMemoryOverrun(int location, int value) {
    int offset = 0;
    intercepts_overrun_t* overrun = intercepts_overrun;

    // Search down the array until we find the right entry.
    while (overrun->len != 0) {
        if (offset + overrun->len > location) {
            break;
        }
        offset += overrun->len;
        overrun++;
    }
    if (overrun->len == 0 || overrun->addr == NULL) {
        return;
    }
    // Write the value to the memory location.
    // 16-bit and 32-bit values are written differently.
    if (overrun->int16_array) {
        short* addr = (short *) overrun->addr;
        int index = (location - offset) / 2;
        addr[index] = (short) (value & 0xffff);
        addr[index + 1] = (short) ((value >> 16) & 0xffff);
    } else {
        int* addr = (int *) overrun->addr;
        int index = (location - offset) / 4;
        addr[index] = value;
    }
}

//
// Emulate overruns of the intercepts[] array.
//
static void InterceptsOverrun(int num_intercepts, intercept_t* intercept) {
    if (num_intercepts <= MAXINTERCEPTS_ORIGINAL) {
        // No overrun.
        return;
    }
    // Overwrite memory that is overwritten in Vanilla Doom, using
    // the values from the intercept structure.
    //
    // Note: the ->d.{thing,line} member should really have its
    // address translated into the correct address value for
    // Vanilla Doom.
    int location = (num_intercepts - MAXINTERCEPTS_ORIGINAL - 1) * 12;
    InterceptsMemoryOverrun(location, intercept->frac);
    InterceptsMemoryOverrun(location + 4, intercept->isaline);
    InterceptsMemoryOverrun(location + 8, (intptr_t) intercept->d.thing);
}

static bool PIT_LineInterceptTrace(const line_t* ld, fixed_t* frac) {
    int s1;
    int s2;
    divline_t dl;

    // avoid precision problems with two routines
    if (trace.dx > FRACUNIT * 16 || trace.dy > FRACUNIT * 16
        || trace.dx < -FRACUNIT * 16 || trace.dy < -FRACUNIT * 16)
    {
        s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
        s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
    } else {
        s1 = P_PointOnLineSide(trace.x, trace.y, ld);
        s2 = P_PointOnLineSide(trace.x + trace.dx, trace.y + trace.dy, ld);
    }

    if (s1 == s2) {
        // line isn't crossed
        return false;
    }

    // hit the line
    P_MakeDivline(ld, &dl);
    *frac = P_InterceptVector(&trace, &dl);

    // return true if the intersection is ahead of the starting
    // point of the trace (not behind the source)
    return *frac >= 0;
}

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
static bool PIT_AddLineIntercepts(line_t* ld) {
    fixed_t frac;
    if (!PIT_LineInterceptTrace(ld, &frac)) {
        return true;
    }
    // try to early out the check
    if (earlyout && frac < FRACUNIT && ld->backsector == NULL) {
        // stop checking
        return false;
    }
    // add new intercept line
    intercept_p->frac = frac;
    intercept_p->isaline = true;
    intercept_p->d.line = ld;
    InterceptsOverrun(intercept_p - intercepts, intercept_p);
    intercept_p++;
    // continue
    return true;
}

static divline_t PIT_GetThingCrossSection(const mobj_t* thing) {
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;
    bool tracepositive = (trace.dx ^ trace.dy) > 0;
    if (tracepositive) {
        x1 = thing->x - thing->radius;
        y1 = thing->y + thing->radius;
        x2 = thing->x + thing->radius;
        y2 = thing->y - thing->radius;
    } else {
        x1 = thing->x - thing->radius;
        y1 = thing->y - thing->radius;
        x2 = thing->x + thing->radius;
        y2 = thing->y + thing->radius;
    }

    divline_t dl = {.x = x1, .y = y1, .dx = x2 - x1, .dy = y2 - y1};

    return dl;
}

static bool PIT_ThingInterceptTrace(const mobj_t* thing, fixed_t* frac) {
    // check a corner to corner crossection for hit
    divline_t dl = PIT_GetThingCrossSection(thing);
    int s1 = P_PointOnDivlineSide(dl.x, dl.y, &trace);
    int s2 = P_PointOnDivlineSide(dl.x + dl.dx, dl.y + dl.dy, &trace);
    if (s1 == s2) {
        // line isn't crossed
        return false;
    }

    // return true if the intersection is ahead of the starting
    // point of the trace (not behind the source)
    *frac = P_InterceptVector(&trace, &dl);
    return *frac >= 0;
}


//
// PIT_AddThingIntercepts
//
static bool PIT_AddThingIntercepts(mobj_t* thing) {
    fixed_t frac;
    if (PIT_ThingInterceptTrace(thing, &frac)) {
        // add new intercept line
        intercept_p->frac = frac;
        intercept_p->isaline = false;
        intercept_p->d.thing = thing;
        InterceptsOverrun(intercept_p - intercepts, intercept_p);
        intercept_p++;
    }
    // keep going
    return true;
}


static fixed_t P_FindMinDistance(intercept_t** in) {
    fixed_t dist = INT_MAX;
    for (intercept_t* scan = intercepts; scan < intercept_p; scan++) {
        if (scan->frac < dist) {
            dist = scan->frac;
            *in = scan;
        }
    }
    return dist;
}

//
// P_TraverseIntercepts
// Returns true if the traverser function returns true for all lines.
//
static bool P_TraverseIntercepts(traverser_t func, fixed_t max_frac) {
    int count = intercept_p - intercepts;
    intercept_t* in = NULL;

    while (count--) {
        fixed_t dist = P_FindMinDistance(&in);
        if (dist > max_frac) {
            // Checked everything in range.
            return true;
        }
        if (!func(in)) {
            // Don't bother going farther.
            return false;
        }
        in->frac = INT_MAX;
    }

    // Everything was traversed.
    return true;
}


//
// Variables used by P_RunBlockIterator.
//
static fixed_t xt1;
static fixed_t yt1;
static fixed_t xt2;
static fixed_t yt2;
static fixed_t xstep;
static fixed_t ystep;
static int mapxstep;
static int mapystep;
static fixed_t xintercept;
static fixed_t yintercept;

static bool P_RunBlockIterator(int flags) {
    // Step through map blocks.
    // Count is present to prevent a round off error from skipping the break.
    int mapx = xt1;
    int mapy = yt1;

    for (int count = 0; count < 64; count++) {
        if (flags & PT_ADDLINES) {
            bool can_move =
                P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts);
            if (!can_move) {
                // early out
                return true;
            }
        }
        if (flags & PT_ADDTHINGS) {
            bool can_move =
                P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts);
            if (!can_move) {
                // early out
                return true;
            }
        }

        if (mapx == xt2 && mapy == yt2) {
            break;
        }

        if ((yintercept >> FRACBITS) == mapy) {
            yintercept += ystep;
            mapx += mapxstep;
        } else if ((xintercept >> FRACBITS) == mapx) {
            xintercept += xstep;
            mapy += mapystep;
        }
    }

    return false;
}

static void P_CalcXIntercept(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
    fixed_t partial;

    if (yt2 > yt1) {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT-1));
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    } else if (yt2 < yt1) {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT-1);
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    } else {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256*FRACUNIT;
    }

    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);
}

static void P_CalcYIntercept(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
    fixed_t partial;

    if (xt2 > xt1) {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    } else if (xt2 < xt1) {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    } else {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }

    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);
}


static void P_SetupGlobalsForIntercept(fixed_t x1, fixed_t y1, fixed_t x2,
                                       fixed_t y2, int flags)
{
    if (((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0) {
        // don't side exactly on a line
        x1 += FRACUNIT;
    }
    if (((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0) {
        // don't side exactly on a line
        y1 += FRACUNIT;
    }

    earlyout = (flags & PT_EARLYOUT) != 0;
    validcount++;
    intercept_p = intercepts;
    trace.x = x1;
    trace.y = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1 >> MAPBLOCKSHIFT;
    yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2 >> MAPBLOCKSHIFT;
    yt2 = y2 >> MAPBLOCKSHIFT;

    P_CalcYIntercept(x1, y1, x2, y2);
    P_CalcXIntercept(x1, y1, x2, y2);
}


//
// P_PathTraverse
// Traces a line from (x1, y1) to (x2, y2), calling the traverser function for each.
// Returns true if the traverser function returns true for all lines.
//
bool P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                    int flags, traverser_t trav)
{
    P_SetupGlobalsForIntercept(x1, y1, x2, y2, flags);
    if (P_RunBlockIterator(flags)) {
        // Blocked.
        return false;
    }
    // Go through the sorted list.
    return P_TraverseIntercepts(trav, FRACUNIT);
}
