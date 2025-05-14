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
//	BSP traversal, handling of LineSegs for rendering.
//


#include "m_bbox.h"
#include "i_system.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"
#include "doomstat.h"
#include "r_state.h"


seg_t* curline;
side_t* sidedef;
line_t* linedef;
sector_t* frontsector;
sector_t* backsector;
drawseg_t drawsegs[MAXDRAWSEGS];
drawseg_t *ds_p;


void R_RenderWallRange(int start, int stop);


//
// R_ClearDrawSegs
//
void R_ClearDrawSegs(void) {
    ds_p = drawsegs;
}



//
// ClipWallSegment
// Clips the given range of columns and includes it in the new clip list.
//
typedef	struct
{
    int	first;
    int last;
} cliprange_t;

// We must expand MAXSEGS to the theoretical limit of the number of solidsegs
// that can be generated in a scene by the DOOM engine. This was determined by
// Lee Killough during BOOM development to be a function of the screensize.
// The simplest thing we can do, other than fix this bug, is to let the game
// render overage and then bomb out by detecting the overflow after the 
// fact. -haleyjd
//#define MAXSEGS 32
#define MAXSEGS (SCREENWIDTH / 2 + 1)

// newend is one past the last valid seg
static cliprange_t* newend;
static cliprange_t solidsegs[MAXSEGS];


//
// R_ClipSolidWallSegment
// Does handle solid walls,
// e.g. single sided LineDefs (middle texture) that entirely block the view.
// 
static void R_ClipSolidWallSegment(int first, int last) {
    cliprange_t* next;

    // Find the first range that touches the range
    // (adjacent pixels are touching).
    cliprange_t* start = solidsegs;
    while (start->last < first-1) {
        start++;
    }

    if (first < start->first) {
	if (last < start->first-1) {
	    // Post is entirely visible (above start), so insert a new clippost.
            R_RenderWallRange(first, last);
	    next = newend;
	    newend++;
	    
	    while (next != start) {
		*next = *(next-1);
		next--;
	    }
	    next->first = first;
	    next->last = last;
	    return;
	}

	// There is a fragment above *start.
        R_RenderWallRange(first, start->first - 1);
	// Now adjust the clip size.
	start->first = first;	
    }

    // Bottom contained in start?
    if (last <= start->last) {
        return;
    }
		
    next = start;
    while (last >= (next+1)->first - 1) {
	// There is a fragment between two posts.
        R_RenderWallRange(next->last + 1, (next + 1)->first - 1);
	next++;
	if (last <= next->last) {
	    // Bottom is contained in next.
	    // Adjust the clip size.
	    start->last = next->last;	
	    goto crunch;
	}
    }

    // There is a fragment after *next.
    R_RenderWallRange(next->last + 1, last);
    // Adjust the clip size.
    start->last = last;
	
    // Remove start+1 to next from the clip list,
    // because start now covers their area.
  crunch:
    if (next == start) {
	// Post just extended past the bottom of one post.
	return;
    }

    while (next++ != newend) {
	// Remove a post.
	*++start = *next;
    }

    newend = start + 1;
}



//
// R_ClipPassWallSegment
// Clips the given range of columns, but does not include it in the clip list.
// Does handle windows, e.g. LineDefs with upper and lower texture.
//
static void R_ClipPassWallSegment(int first, int last) {
    // Find the first range that touches the range
    // (adjacent pixels are touching).
    const cliprange_t* start = solidsegs;
    while (start->last < first-1) {
        start++;
    }

    if (first < start->first) {
	if (last < start->first - 1) {
	    // Post is entirely visible (above start).
            R_RenderWallRange(first, last);
	    return;
	}
	// There is a fragment above *start.
        R_RenderWallRange(first, start->first - 1);
    }

    // Bottom contained in start?
    if (last <= start->last) {
        return;
    }
		
    while (last >= (start+1)->first-1) {
	// There is a fragment between two posts.
        R_RenderWallRange(start->last + 1, (start + 1)->first - 1);
	start++;
	if (last <= start->last) {
            return;
        }
    }

    // There is a fragment after *next.
    R_RenderWallRange(start->last + 1, last);
}



//
// R_ClearClipSegs
//
void R_ClearClipSegs(void) {
    solidsegs[0].first = -0x7fffffff;
    solidsegs[0].last = -1;
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fffffff;
    newend = solidsegs + 2;
}

//
// Check if line is empty, e.g. has identical floor and
// ceiling on both sides, identical light levels on both sides,
// and no middle texture.
//
static bool R_IsEmptyLine() {
    return backsector->ceilingpic == frontsector->ceilingpic
           && backsector->floorpic == frontsector->floorpic
           && backsector->lightlevel == frontsector->lightlevel
           && curline->sidedef->midtexture == 0;
}

static bool R_IsWindow() {
    return backsector->ceilingheight != frontsector->ceilingheight
           || backsector->floorheight != frontsector->floorheight;
}

static bool R_IsClosedDoor() {
    bool down_door = backsector->ceilingheight <= frontsector->floorheight;
    bool up_door = backsector->floorheight >= frontsector->ceilingheight;
    return down_door || up_door;
}

static bool R_IsWall() {
    return !backsector;
}

//
// R_RenderLine
// Clips the given segment and renders any visible pieces.
//
static void R_RenderLine(seg_t* line) {
    curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle_t angle1 = R_PointToAngle(line->v1->x, line->v1->y);
    angle_t angle2 = R_PointToAngle(line->v2->x, line->v2->y);

    // Clip to view edges.
    // OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
    angle_t span = angle1 - angle2;

    // Back side? I.e. backface culling?
    if (span >= ANG180) {
        return;
    }

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

    // Clip angles to screen space.
    angle_t tspan = angle1 + clipangle;
    if (tspan > 2*clipangle) {
	tspan -= 2*clipangle;
	if (tspan >= span) {
            // Totally off the left edge.
            return;
        }
	angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2*clipangle) {
	tspan -= 2*clipangle;
	if (tspan >= span) {
            // Totally off the left edge.
            return;
        }
	angle2 = -clipangle;
    }

    // The seg is in the view range, but not necessarily visible.
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    int x1 = viewangletox[angle1];
    int x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 == x2) {
        return;
    }

    backsector = line->backsector;

    if (R_IsWall() || R_IsClosedDoor()) {
        R_ClipSolidWallSegment(x1, x2 - 1);
        return;
    }
    // Reject empty lines used for triggers and special events.
    if (R_IsWindow() || !R_IsEmptyLine()) {
        R_ClipPassWallSegment(x1, x2 - 1);
    }
}


//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
static int checkcoord[9][4] = {
//      X1       Y1          X2        Y2
    {BOXRIGHT, BOXTOP,    BOXLEFT,  BOXBOTTOM},
    {BOXRIGHT, BOXTOP,    BOXLEFT,  BOXTOP},
    {BOXRIGHT, BOXBOTTOM, BOXLEFT,  BOXTOP},
    {BOXLEFT,  BOXTOP,    BOXLEFT,  BOXBOTTOM},
    {BOXTOP,   BOXTOP,    BOXTOP,   BOXTOP},    // UNUSED
    {BOXRIGHT, BOXBOTTOM, BOXRIGHT, BOXTOP},
    {BOXLEFT,  BOXTOP,    BOXRIGHT, BOXBOTTOM},
    {BOXLEFT,  BOXBOTTOM, BOXRIGHT, BOXBOTTOM},
    {BOXLEFT,  BOXBOTTOM, BOXRIGHT, BOXTOP}
};

//
// Find the corners of the box that define the edges from current viewpoint.
//
static void R_FindBoxCorners(const fixed_t* bspcoord, int* boxx, int* boxy) {
    if (viewx <= bspcoord[BOXLEFT]) {
        *boxx = 0;
    } else if (viewx < bspcoord[BOXRIGHT]) {
        *boxx = 1;
    } else {
        *boxx = 2;
    }
    if (viewy >= bspcoord[BOXTOP]) {
        *boxy = 0;
    } else if (viewy > bspcoord[BOXBOTTOM]) {
        *boxy = 1;
    } else {
        *boxy = 2;
    }
}

static void R_RenderSubSectorLines(const subsector_t* sub_sector) {
    int first_line = sub_sector->firstline;
    int last_line = first_line + sub_sector->numlines - 1;

    for (int i = first_line; i <= last_line; i++) {
        R_RenderLine(&segs[i]);
    }

    // Check for solidsegs overflow - extremely unsatisfactory!
    if (newend > &solidsegs[32]) {
        I_Error("R_RenderSubSector: solidsegs overflow (vanilla may crash here)\n");
    }
}

static void R_SetPlanes() {
    if (frontsector->floorheight < viewz) {
        fixed_t height = frontsector->floorheight;
        short pic = frontsector->floorpic;
        short light = frontsector->lightlevel;
        floorplane = R_FindPlane(height, pic, light);
    } else {
        floorplane = NULL;
    }

    if (frontsector->ceilingheight > viewz || frontsector->ceilingpic == sky_flat) {
        fixed_t height = frontsector->ceilingheight;
        short pic = frontsector->ceilingpic;
        short light = frontsector->lightlevel;
        ceilingplane = R_FindPlane(height, pic, light);
    } else {
        ceilingplane = NULL;
    }
}

//
// R_RenderSubSector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
static void R_RenderSubSector(int num) {
    if (num >= numsubsectors) {
        I_Error("R_RenderSubSector: ss %i with numss = %i", num, numsubsectors);
    }
    subsector_t* subsector = &subsectors[num];
    frontsector = subsector->sector;
    R_SetPlanes();
    R_AddSprites(frontsector);

    R_RenderSubSectorLines(subsector);
}

static bool R_CheckBBox(const fixed_t* bspcoord) {
    int boxx;
    int boxy;
    R_FindBoxCorners(bspcoord, &boxx, &boxy);
    int boxpos = boxx + (boxy * 3);
    if (boxpos == 4) {
        // If boxpos is 4, the player is inside the bounding box.
        // This indicates the space should be further divided as some
        // part may still be visible.
        return true;
    }

    // Check clip list for an open space.
    fixed_t x1 = bspcoord[checkcoord[boxpos][0]];
    fixed_t y1 = bspcoord[checkcoord[boxpos][1]];
    fixed_t x2 = bspcoord[checkcoord[boxpos][2]];
    fixed_t y2 = bspcoord[checkcoord[boxpos][3]];
    angle_t angle1 = R_PointToAngle(x1, y1) - viewangle;
    angle_t angle2 = R_PointToAngle(x2, y2) - viewangle;
    angle_t span = angle1 - angle2;

    if (span >= ANG180) {
        // Sitting on a line.
        return true;
    }
    angle_t tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span) {
            // Totally off the left edge.
            return false;
        }
        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span) {
            // Totally off the left edge.
            return false;
        }
        angle2 = -clipangle;
    }

    // Find the first clippost that touches the source post (adjacent pixels are touching).
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    int sx1 = viewangletox[angle1];
    int sx2 = viewangletox[angle2];
    if (sx1 == sx2) {
        // Does not cross a pixel.
        return false;
    }
    sx2--;
    const cliprange_t* start = solidsegs;
    while (start->last < sx2) {
        start++;
    }
    return sx1 < start->first || sx2 > start->last;
}


//
// RenderBSPNode
// Renders all subsectors below a given node, traversing subtree recursively.
// Just call with BSP root.
static void R_RenderBSPNode(int bsp_node) {
    if (bsp_node & NF_SUBSECTOR) {
        // Node is a leaf and points to a subsector.
        int sub_sector = (bsp_node == -1) ? 0 : bsp_node & (~NF_SUBSECTOR);
        R_RenderSubSector(sub_sector);
        return;
    }

    const node_t* node = &nodes[bsp_node];

    // Decide which side the view point is on.
    int front_side = R_PointOnSide(viewx, viewy, node);
    int back_side = front_side ^ 1;

    // Recursively divide front space.
    R_RenderBSPNode(node->children[front_side]);

    // Possibly divide back space.
    if (R_CheckBBox(node->bbox[back_side])) {
        R_RenderBSPNode(node->children[back_side]);
    }
}

void R_RenderSectors() {
    // Render the BSP tree.
    // The root node is the last node output.
    R_RenderBSPNode(numnodes - 1);
}
