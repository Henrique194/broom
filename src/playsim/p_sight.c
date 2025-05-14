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
//	LineOfSight/Visibility checks, uses REJECT Lookup Table.
//


#include "p_local.h"

#include "doomstat.h"
#include "i_system.h"

//
// P_CheckSight
//

// eye z of looker
static fixed_t sightzstart;

// This is the highest point of t2 that can be seen from t1.
fixed_t topslope;

// This is the lowest point of t2 that can be seen from t1.
fixed_t bottomslope;

static divline_t strace; // from t1 to t2
static fixed_t t2x;
static fixed_t t2y;


// PTR_SightTraverse() for Doom 1.2 sight calculations taken
// from prboom-plus/src/p_sight.c:69-102
static bool PTR_SightTraverse(intercept_t *in) {
    fixed_t slope;

    //
    // crosses a two sided line
    //
    line_t* li = in->d.line;
    P_LineOpening(li);

    // quick test for totally closed doors
    if (openbottom >= opentop) {
        // stop
        return false;
    }

    if (li->frontsector->floorheight != li->backsector->floorheight) {
        slope = FixedDiv(openbottom - sightzstart, in->frac);
        if (slope > bottomslope) {
            bottomslope = slope;
        }
    }
    if (li->frontsector->ceilingheight != li->backsector->ceilingheight) {
        slope = FixedDiv(opentop - sightzstart, in->frac);
        if (slope < topslope) {
            topslope = slope;
        }
    }

    if (topslope <= bottomslope) {
        // stop
        return false;
    }
    return true; // keep going
}

static bool P_SightUnobstructedOld(const mobj_t* t1, const mobj_t* t2) {
    validcount++;

    sightzstart = t1->z + t1->height - (t1->height >> 2);
    bottomslope = t2->z - sightzstart;
    topslope = bottomslope + t2->height;

    return P_PathTraverse(t1->x, t1->y, t2->x, t2->y,
                          PT_EARLYOUT | PT_ADDLINES, PTR_SightTraverse);
}


//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
static int P_DivlineSide(fixed_t x, fixed_t y, const divline_t* node) {
    if (node->dx == 0) {
        if (x == node->x) {
            return 2;
        }
        if (x <= node->x) {
            return node->dy > 0;
        }
        return node->dy < 0;
    }
    if (node->dy == 0) {
        // Vanilla Bug: this code compares the x coordinate with the y
        // coordinate, which can cause the sidedness decision to be incorrect,
        // either indicating that the enemy is on the same side of the node line
        // as the player or a different side than the player, when the opposite
        // is actually true.
        // More info on this bug here:
        // https://doomwiki.org/wiki/Sleeping_shotgun_guy_in_MAP02_(Doom_II)
        if (x == node->y) {
            return 2;
        }
        if (y <= node->y) {
            return node->dx < 0;
        }
        return node->dx > 0;
    }

    fixed_t dx = (x - node->x);
    fixed_t dy = (y - node->y);
    fixed_t left = (node->dy >> FRACBITS) * (dx >> FRACBITS);
    fixed_t right = (dy >> FRACBITS) * (node->dx >> FRACBITS);
    if (right < left) {
        // front side
        return 0;
    }
    if (left == right) {
        return 2;
    }
    // back side
    return 1;
}

//
// P_InterceptVector2
//
// Returns the first degree Bézier parameter of line v2.
//
// - If the returned parameter is within the range [0, FRACUNIT], it indicates
//   that v1 intersects v2 at a point along v2. You can use the Bézier parameter
//   to calculate the exact intersection point on v2.
// - If the returned parameter is less than 0, the intersection occurs before
//   the start of v2 (i.e., before the point (v2->x, v2->y)).
// - If the returned parameter is greater than 1, the intersection occurs after
//   the end of v2 (i.e., beyond the point (v2->x + v2->dx, v2->y + v2->dy)).
//
// This is only called by the addthings and addlines traversers.
//
static fixed_t P_InterceptVector2(const divline_t* v2, const divline_t* v1) {
    // The mathematical formula to calculate the Bézier parameter for the
    // intersection of v1 with v2 is:
    //
    //   Bézier parameter = num / den,
    //
    // Where:
    //   num = (v2->y - v1->y) * v1->dx - (v2->x - v1->x) * v1->dy
    //   den = (v1->dy * v2->dx) - (v1->dx * v2->dy)
    //
    // If den equals zero, it indicates that v1 and v2 are either parallel or
    // the same line, meaning no valid intersection exists or the lines coincide.
    //
    // Note: In the "FixedMul" function calls, we perform bit shifts (>> 8) to
    //       discard the extra bits that result from multiplying fixed-point
    //       integers. This ensures that we are working with the correct
    //       precision for fixed-point math.
    //

    fixed_t den = FixedMul(v1->dy >> 8, v2->dx) - FixedMul(v1->dx >> 8, v2->dy);
    if (den == 0) {
        return 0;
    }

    fixed_t num = FixedMul((v1->x - v2->x) >> 8, v1->dy)
                  + FixedMul((v2->y - v1->y) >> 8, v1->dx);

    return FixedDiv(num, den);
}

static bool P_IsTwoSided(const seg_t* seg) {
    return (seg->linedef->flags & ML_TWOSIDED) != 0;
}

//
// Checks if the given segment crosses the strace (a line of sight or trace line).
// A segment crosses the strace if both of the following conditions are met:
// 1. The endpoints of the segment are on different sides of the strace.
// 2. The endpoints of the strace are on different sides of the segment.
//
static bool P_CrossesStrace(const seg_t* seg) {
    const line_t* line = seg->linedef;
    divline_t divl = {
        .x = line->v1->x,
        .y = line->v1->y,
        .dx = line->v2->x - line->v1->x,
        .dy = line->v2->y - line->v1->y
    };

    const vertex_t* v1 = line->v1;
    const vertex_t* v2 = line->v2;
    int s1 = P_DivlineSide(v1->x, v1->y, &strace);
    int s2 = P_DivlineSide(v2->x, v2->y, &strace);
    if (s1 == s2) {
        // endpoints of segment are on the same side of strace
        return false;
    }
    s1 = P_DivlineSide(strace.x, strace.y, &divl);
    s2 = P_DivlineSide(t2x, t2y, &divl);
    return s1 != s2;
}

static void P_CalculateOpeningSpace(const seg_t* seg, fixed_t* top,
                                    fixed_t* bottom)
{
    const sector_t* front = seg->frontsector;
    const sector_t* back = seg->backsector;

    *top = (front->ceilingheight < back->ceilingheight) ? front->ceilingheight
                                                        : back->ceilingheight;

    *bottom = (front->floorheight > back->floorheight) ? front->floorheight
                                                       : back->floorheight;
}

//
// Checks if the line of sight is vertically blocked by any obstruction along
// the segment.
// Returns true if the line of sight is vertically blocked (i.e., there's an
// obstruction in the vertical direction), and false if it is clear.
//
static bool P_CheckVerticalObstruction(const seg_t* seg) {
    const sector_t* front = seg->frontsector;
    const sector_t* back = seg->backsector;

    const line_t* line = seg->linedef;
    divline_t divl = {
        .x = line->v1->x,
        .y = line->v1->y,
        .dx = line->v2->x - line->v1->x,
        .dy = line->v2->y - line->v1->y
    };
    // Vanilla Bug: in some rare cases the "frac" will overflow and incorrectly
    // indicate that the line of sight is blocked. This in turn can lead to
    // barrel explosions doing no damage.
    // More info on this bug here:
    // https://doomwiki.org/wiki/Barrel_explosions_which_do_no_damage
    // https://www.doomworld.com/forum/topic/72743-theory-about-barrel-explosions-which-do-no-damage-bug/
    fixed_t frac = P_InterceptVector2(&strace, &divl);

    fixed_t open_top;
    fixed_t open_bottom;
    P_CalculateOpeningSpace(seg, &open_top, &open_bottom);

    if (front->floorheight != back->floorheight) {
        fixed_t slope = FixedDiv(open_bottom - sightzstart, frac);
        if (slope > bottomslope) {
            bottomslope = slope;
        }
    }
    if (front->ceilingheight != back->ceilingheight) {
        fixed_t slope = FixedDiv(open_top - sightzstart, frac);
        if (slope < topslope) {
            topslope = slope;
        }
    }

    return topslope <= bottomslope;
}

static bool P_IsClosedDoor(const seg_t* seg) {
    fixed_t open_top;
    fixed_t open_bottom;
    P_CalculateOpeningSpace(seg, &open_top, &open_bottom);

    return open_bottom >= open_top;
}

static bool P_HasSightBlockingWall(const seg_t *seg) {
    const sector_t* front = seg->frontsector;
    const sector_t* back = seg->backsector;

    return front->floorheight != back->floorheight
           || front->ceilingheight != back->ceilingheight;
}

static bool P_TwoSidedBlocksStrace(const seg_t *seg) {
    if (!P_HasSightBlockingWall(seg)) {
        // no wall to block sight with
        return false;
    }
    if (P_IsClosedDoor(seg)) {
        return true;
    }
    return P_CheckVerticalObstruction(seg);
}

static bool P_BlocksStrace(const seg_t* seg) {
    if (!P_CrossesStrace(seg)) {
        // Does not cross strace, so segment can't block it.
        return false;
    }
    if (seg->linedef->backsector == NULL) {
        // Backsector may be NULL if this is an "impassible glass" hack line.
        return true;
    }
    if (P_IsTwoSided(seg)) {
        return P_TwoSidedBlocksStrace(seg);
    }
    // All solid walls block strace.
    return true;
}

//
// P_CrossSubsector
// Checks if a strace successfully crosses the specified subsector.
// Returns false if strace is blocked by any segment in the subsector,
// otherwise returns true.
//
// Parameters:
//   - num: The index of the subsector to check.
//
// Returns:
//   - bool: True if the strace crosses the subsector, false if blocked.
//
static bool P_CrossSubsector(int num) {
    if (num >= numsubsectors) {
        I_Error("P_CrossSubsector: ss %i with numss = %i", num, numsubsectors);
    }
    const subsector_t* sub = &subsectors[num];
    for (int i = 0; i < sub->numlines; i++) {
        seg_t* seg = &segs[sub->firstline + i];
        line_t* line = seg->linedef;

        if (line->validcount == validcount) {
            // already checked other side
            continue;
        }
        line->validcount = validcount;

        if (P_BlocksStrace(seg)) {
            return false;
        }
    }

    // passed the subsector ok
    return true;
}

//
// P_CrossBSPNode
// Returns true if strace crosses the given node successfully.
//
static bool P_CrossBSPNode(int bspnum) {
    if (bspnum & NF_SUBSECTOR) {
        if (bspnum == -1) {
            return P_CrossSubsector(0);
        }
        return P_CrossSubsector(bspnum & (~NF_SUBSECTOR));
    }

    node_t* bsp = &nodes[bspnum];

    // decide which side the start point is on
    int side = P_DivlineSide(strace.x, strace.y, (divline_t *) bsp);
    if (side == 2) {
        // an "on" should cross both sides
        side = 0;
    }

    // cross the starting side
    if (!P_CrossBSPNode(bsp->children[side])) {
        return false;
    }

    // the partition plane is crossed here
    if (side == P_DivlineSide(t2x, t2y, (divline_t *) bsp)) {
        // the line doesn't touch the other side
        return true;
    }

    // cross the ending side
    return P_CrossBSPNode(bsp->children[side ^ 1]);
}

static bool P_SightUnobstructed(const mobj_t* t1, const mobj_t* t2) {
    validcount++;

    sightzstart = t1->z + t1->height - (t1->height >> 2);
    bottomslope = t2->z - sightzstart;
    topslope = bottomslope + t2->height;

    strace.x = t1->x;
    strace.y = t1->y;
    t2x = t2->x;
    t2y = t2->y;
    strace.dx = t2->x - t1->x;
    strace.dy = t2->y - t1->y;

    // the head node is the last node output
    return P_CrossBSPNode(numnodes - 1);
}


static bool P_CheckRejectTable(const mobj_t* t1, const mobj_t* t2) {
    // Determine subsector entries in REJECT table.
    int s1 = (t1->subsector->sector - sectors);
    int s2 = (t2->subsector->sector - sectors);
    int pnum = (s1 * numsectors) + s2;
    int bytenum = pnum >> 3;
    int bitnum = 1 << (pnum & 7);

    return (rejectmatrix[bytenum] & bitnum) != 0;
}

//
// P_CheckSight
// Returns true if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
bool P_CheckSight(const mobj_t* t1, const mobj_t* t2) {
    if (P_CheckRejectTable(t1, t2)) {
        // can't possibly be connected
        return false;
    }
    // An unobstructed line of sight is possible.
    // Now look from eyes of t1 to any part of t2.
    if (gameversion <= exe_doom_1_2) {
        return P_SightUnobstructedOld(t1, t2);
    }
    return P_SightUnobstructed(t1, t2);
}
