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
//
// DESCRIPTION:  the automap code
//


#include "am_map.h"

#include <math.h>
#include "deh_str.h"
#include "z_zone.h"
#include "st_stuff.h"
#include "p_local.h"
#include "w_wad.h"

// Needs access to LFB.
#include "v_video.h"

// State.
#include "doomstat.h"


// For use if I do walls with outsides/insides
#define REDS        (256 - 5 * 16)
#define REDRANGE    16
#define BLUES       (256 - 4 * 16 + 8)
#define BLUERANGE   8
#define GREENS      (7 * 16)
#define GREENRANGE  16
#define GRAYS       (6 * 16)
#define GRAYSRANGE  16
#define BROWNS      (4 * 16)
#define BROWNRANGE  16
#define YELLOWS     (256 - 32 + 7)
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       (256 - 47)

// Automap colors
#define BACKGROUND       BLACK
#define YOURCOLORS       WHITE
#define YOURRANGE        0
#define WALLCOLORS       REDS
#define WALLRANGE        REDRANGE
#define TSWALLCOLORS     GRAYS
#define TSWALLRANGE      GRAYSRANGE
#define FDWALLCOLORS     BROWNS
#define FDWALLRANGE      BROWNRANGE
#define CDWALLCOLORS     YELLOWS
#define CDWALLRANGE      YELLOWRANGE
#define THINGCOLORS      GREENS
#define THINGRANGE       GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE  WALLRANGE
#define GRIDCOLORS       (GRAYS + GRAYSRANGE / 2)
#define GRIDRANGE        0
#define XHAIRCOLORS      GRAYS

// drawing stuff

#define AM_NUMMARKPOINTS 10

// scale on entry
#define INITSCALEMTOF (.2 * FRACUNIT)

// how much the automap moves window per tic in frame-buffer
// coordinates moves 140 pixels in 1 second
#define F_PANINC 4


// translates between frame-buffer and map coordinates
#define CXMTOF(x) (f_x + MTOF((x) - m_x))
#define CYMTOF(y) (f_y + (f_h - MTOF((y) - m_y)))


//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
#define R ((8 * PLAYERRADIUS) / 7)
mline_t player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},                  // -----
    {{R, 0}, {R - R / 2, R / 4}},               // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}},     // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}}, // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};
#undef R

#define R ((8 * PLAYERRADIUS) / 7)
mline_t cheat_player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},    // -----
    {{R, 0}, {R - R / 2, R / 6}}, // ----->
    {{R, 0}, {R - R / 2, -R / 6}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 6}}, // >----->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 6}}, // >>----->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6}},
    {{-R / 2, 0}, {-R / 2, -R / 6}}, // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, 0}, {-R / 6, -R / 6}}, // >>-dd-->
    {{-R / 6, -R / 6}, {0, -R / 6}},
    {{0, -R / 6}, {0, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}}, // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}};
#undef R

#define R (FRACUNIT)
mline_t thintriangle_guy[] = {
    {{(fixed_t) (-.5 * R), (fixed_t) (-.7 * R)},
     {(fixed_t) (R), (fixed_t) (0)}},
    {{(fixed_t) (R), (fixed_t) (0)}, {(fixed_t) (-.5 * R), (fixed_t) (.7 * R)}},
    {{(fixed_t) (-.5 * R), (fixed_t) (.7 * R)},
     {(fixed_t) (-.5 * R), (fixed_t) (-.7 * R)}}
};
#undef R


int cheating = 0;
int grid = 0;

bool automapactive = false;
static int finit_width = SCREENWIDTH;
static int finit_height = SCREENHEIGHT - ST_HEIGHT;

// location of window on screen
static int f_x;
static int f_y;

// size of window on screen
int f_w;
static int f_h;

static int lightlev; // used for funky strobing effect
static pixel_t *fb;  // pseudo-frame buffer

mpoint_t m_paninc;    // how far the window pans each tic (map coords)
fixed_t mtof_zoommul; // how far the window zooms in each tic (map coords)
fixed_t ftom_zoommul; // how far the window zooms in each tic (fb coords)

// LL x,y where the window is on the map (map coords)
fixed_t m_x;
fixed_t m_y;

// UR x,y where the window is on the map (map coords)
fixed_t m_x2;
fixed_t m_y2;

// old stuff for recovery later
fixed_t old_m_w;
fixed_t old_m_h;
fixed_t old_m_x;
fixed_t old_m_y;

//
// width/height of window on map (map coords)
//
fixed_t m_w;
fixed_t m_h;

// based on level size
static fixed_t min_x;
static fixed_t min_y;
static fixed_t max_x;
static fixed_t max_y;

static fixed_t max_w; // max_x-min_x,
static fixed_t max_h; // max_y-min_y


//
// used to tell when to stop zooming out
//
static fixed_t min_scale_mtof;

//
// used to tell when to stop zooming in
//
static fixed_t max_scale_mtof;


//
// old location used by the Follower routine
//
mpoint_t f_oldloc;

//
// used by MTOF to scale from map-to-frame-buffer coords
//
fixed_t scale_mtof = (fixed_t) INITSCALEMTOF;

//
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
//
fixed_t scale_ftom;

//
// the player represented by an arrow
//
player_t *plr;

//
// numbers used for marking by the automap
//
static patch_t *marknums[10];

//
// where the points are
//
static mpoint_t markpoints[AM_NUMMARKPOINTS];

//
// next point to be assigned
//
int markpointnum = 0;

//
// specifies whether to follow the player around
//
int followplayer = 1;

cheatseq_t cheat_amap = CHEAT("iddt", 0);

static bool stopped = true;


static void AM_activateNewScale() {
    m_x += m_w / 2;
    m_y += m_h / 2;
    m_w = FTOM(f_w);
    m_h = FTOM(f_h);
    m_x -= m_w / 2;
    m_y -= m_h / 2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
// adds a marker at the current location
//
void AM_addMark() {
    markpoints[markpointnum].x = m_x + m_w/2;
    markpoints[markpointnum].y = m_y + m_h/2;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
static void AM_findMinMaxBoundaries() {
    int i;
    fixed_t a;
    fixed_t b;

    min_x = min_y =  INT_MAX;
    max_x = max_y = -INT_MAX;
  
    for (i=0;i<numvertexes;i++) {
	if (vertexes[i].x < min_x) {
            min_x = vertexes[i].x;
        } else if (vertexes[i].x > max_x) {
            max_x = vertexes[i].x;
        }
    
	if (vertexes[i].y < min_y) {
            min_y = vertexes[i].y;
        } else if (vertexes[i].y > max_y) {
            max_y = vertexes[i].y;
        }
    }
  
    max_w = max_x - min_x;
    max_h = max_y - min_y;

    a = FixedDiv(f_w << FRACBITS, max_w);
    b = FixedDiv(f_h << FRACBITS, max_h);
  
    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h << FRACBITS, 2*PLAYERRADIUS);

}


static void AM_changeWindowLoc() {
    if (m_paninc.x || m_paninc.y) {
	followplayer = 0;
	f_oldloc.x = INT_MAX;
    }

    m_x += m_paninc.x;
    m_y += m_paninc.y;

    if (m_x + m_w/2 > max_x) {
        m_x = max_x - m_w/2;
    } else if (m_x + m_w/2 < min_x) {
        m_x = min_x - m_w/2;
    }
    if (m_y + m_h/2 > max_y) {
        m_y = max_y - m_h/2;
    } else if (m_y + m_h/2 < min_y) {
        m_y = min_y - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
// Find player to center on initially.
//
static void AM_FindPlayer() {
    if (playeringame[consoleplayer]) {
        plr = &players[consoleplayer];
        return;
    }

    plr = &players[0];
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++) {
        if (playeringame[pnum]) {
            plr = &players[pnum];
            break;
        }
    }
}


static void AM_initVariables() {
    static event_t st_notify = { ev_keyup, AM_MSGENTERED, 0, 0 };

    automapactive = true;
    fb = I_VideoBuffer;

    f_oldloc.x = INT_MAX;
    lightlev = 0;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = FRACUNIT;
    mtof_zoommul = FRACUNIT;

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    AM_FindPlayer();

    m_x = plr->mo->x - m_w/2;
    m_y = plr->mo->y - m_h/2;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    // inform the status bar of the change
    ST_Responder(&st_notify);

}

static void AM_loadPics() {
    char namebuf[9];
  
    for (int i = 0; i < 10; i++) {
	DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
	marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
    }
}

static void AM_unloadPics() {
    char namebuf[9];
  
    for (int i = 0; i < 10; i++) {
	DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
	W_ReleaseLumpName(namebuf);
    }
}

void AM_clearMarks() {
    for (int i = 0; i < AM_NUMMARKPOINTS; i++) {
        // means empty
        markpoints[i].x = -1;
    }
    markpointnum = 0;
}


void AM_Stop() {
    static event_t st_notify = {
        0,
        ev_keyup,
        AM_MSGEXITED,
        0
    };

    AM_unloadPics();
    automapactive = false;
    ST_Responder(&st_notify);
    stopped = true;
}

//
// Used for checking if level changed
//
static int lastlevel = -1;
static int lastepisode = -1;

//
// should be called at the start of every level
// right now, i figure it out myself
//
static void AM_LevelInit() {
    f_x = 0;
    f_y = 0;
    f_w = finit_width;
    f_h = finit_height;

    AM_clearMarks();
    AM_findMinMaxBoundaries();
    scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7*FRACUNIT));
    if (scale_mtof > max_scale_mtof) {
        scale_mtof = min_scale_mtof;
    }
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    lastlevel = gamemap;
    lastepisode = gameepisode;
}

static bool AM_LevelChanged() {
    return lastlevel != gamemap || lastepisode != gameepisode;
}


void AM_Start() {
    if (!stopped) {
        AM_Stop();
    }
    stopped = false;
    if (AM_LevelChanged()) {
	AM_LevelInit();
    }
    AM_initVariables();
    AM_loadPics();
}

//
// set the window scale to the maximum size
//
void AM_minOutWindowScale() {
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

//
// set the window scale to the minimum size
//
static void AM_maxOutWindowScale() {
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}


//
// Zooming
//
static void AM_changeWindowScale() {
    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if (scale_mtof < min_scale_mtof) {
        AM_minOutWindowScale();
    } else if (scale_mtof > max_scale_mtof) {
        AM_maxOutWindowScale();
    } else {
        AM_activateNewScale();
    }
}


static void AM_doFollowPlayer() {
    if (f_oldloc.x != plr->mo->x || f_oldloc.y != plr->mo->y) {
	m_x = FTOM(MTOF(plr->mo->x)) - m_w/2;
	m_y = FTOM(MTOF(plr->mo->y)) - m_h/2;
	m_x2 = m_x + m_w;
	m_y2 = m_y + m_h;
	f_oldloc.x = plr->mo->x;
	f_oldloc.y = plr->mo->y;
    }
}

//
// Updates on Game Tick
//
void AM_Ticker() {
    if (!automapactive) {
        return;
    }
    if (followplayer) {
        AM_doFollowPlayer();
    }
    // Change the zoom if necessary
    if (ftom_zoommul != FRACUNIT) {
        AM_changeWindowScale();
    }
    // Change x,y location
    if (m_paninc.x || m_paninc.y) {
        AM_changeWindowLoc();
    }
}


//
// Clear automap frame buffer.
//
static void AM_clearFB(int color) {
    memset(fb, color, f_w * f_h * sizeof(*fb));
}


#define LEFT    1
#define RIGHT   2
#define BOTTOM  4
#define TOP     8

static int AM_ComputeOutCode(int x, int y) {
    int oc = 0;
    if (y < 0) {
        oc |= TOP;
    } else if (y >= f_h) {
        oc |= BOTTOM;
    }
    if (x < 0) {
        oc |= LEFT;
    } else if (x >= f_w) {
        oc |= RIGHT;
    }
    return oc;
}

static fpoint_t AM_ComputeClippedPoint(int outside, const fline_t* fl) {
    int ax = fl->a.x;
    int ay = fl->a.y;
    int bx = fl->b.x;
    int by = fl->b.y;

    int dx = bx - ax;
    int dy = by - ay;
    fpoint_t p = { 0 };

    if (outside & TOP) {
        p.y = 0;
        p.x = ax + (dx * (p.y - ay)) / dy;
    } else if (outside & BOTTOM) {
        p.y = f_h - 1;
        p.x = ax + (dx * (f_h - ay)) / dy;
    } else if (outside & RIGHT) {
        p.x = f_w - 1;
        p.y = ay + (dy * (p.x - ax)) / dx;
    } else if (outside & LEFT) {
        p.x = 0;
        p.y = ay + (dy * (p.x - ax)) / dx;
    }

    return p;
}

static bool AM_CheckTrivialOutside(const mline_t* ml) {
    int outcode1 = 0;
    int outcode2 = 0;

    if (ml->a.y > m_y2) {
        outcode1 = TOP;
    } else if (ml->a.y < m_y) {
        outcode1 = BOTTOM;
    }
    if (ml->b.y > m_y2) {
        outcode2 = TOP;
    } else if (ml->b.y < m_y) {
        outcode2 = BOTTOM;
    }
    if (outcode1 & outcode2) {
        return true;
    }

    if (ml->a.x < m_x) {
        outcode1 |= LEFT;
    } else if (ml->a.x > m_x2) {
        outcode1 |= RIGHT;
    }
    if (ml->b.x < m_x) {
        outcode2 |= LEFT;
    } else if (ml->b.x > m_x2) {
        outcode2 |= RIGHT;
    }
    if (outcode1 & outcode2) {
        return true;
    }

    return false;
}


//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle the common cases.
//
static bool AM_clipMline(mline_t* ml, fline_t* fl) {
    if (AM_CheckTrivialOutside(ml)) {
        return false;
    }

    // transform to frame-buffer coordinates.
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    int outcode1 = AM_ComputeOutCode(fl->a.x, fl->a.y);
    int outcode2 = AM_ComputeOutCode(fl->b.x, fl->b.y);

    while (outcode1 | outcode2) {
        if (outcode1 & outcode2) {
            return false;
        }
        if (outcode1) {
            // fl->a is outside
            fl->a = AM_ComputeClippedPoint(outcode1, fl);
            outcode1 = AM_ComputeOutCode(fl->a.x, fl->a.y);
        } else {
            // fl->b is outside
            fl->b = AM_ComputeClippedPoint(outcode2, fl);
            outcode2 = AM_ComputeOutCode(fl->b.x, fl->b.y);
        }
    }

    return true;
}


static void AM_WritePixel(int x, int y, int color) {
    int spot = x + (y * f_w);
    fb[spot]= (pixel_t) color;
}

static void AM_PlotLineLow(fpoint_t start, fpoint_t end, int color) {
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }
    int d = dy - dx/2;
    int y = start.y;
    for (int x = start.x; x <= end.x; x++) {
        AM_WritePixel(x, y, color);
        if (d >= 0) {
            y += yi;
            d += (2 * (dy - dx));
        } else {
            d += (2 * dy);
        }
    }
}

static void AM_PlotLineHigh(fpoint_t start, fpoint_t end, int color) {
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int xi = 1;
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int d = dx - dy/2;
    int x = start.x;
    for (int y = start.y; y <= end.y; y++) {
        AM_WritePixel(x, y, color);
        if (d >= 0) {
            x += xi;
            d += (2 * (dx - dy));
        } else {
            d += (2 * dx);
        }
    }
}

static bool AM_IsInvalidFline(const fline_t *fl) {
    // For debugging only
    static int fuck = 0;

    bool invalid = fl->a.x < 0 || fl->a.x >= f_w
                      || fl->a.y < 0 || fl->a.y >= f_h
                      || fl->b.x < 0 || fl->b.x >= f_w
                      || fl->b.y < 0 || fl->b.y >= f_h;

    if (invalid) {
        DEH_fprintf(stderr, "fuck %d \r", fuck++);
    }

    return invalid;
}

//
// Classic Bresenham w/ whatever optimizations needed for speed
//
static void AM_drawFline(fline_t* fl, int color) {
    if (AM_IsInvalidFline(fl)) {
        return;
    }

    fpoint_t a = fl->a;
    fpoint_t b = fl->b;
    int dx = b.x - a.x;
    int dy = b.y - a.y;

    if (abs(dy) < abs(dx)) {
        if (dx >= 0) {
            AM_PlotLineLow(a, b, color);
        } else {
            AM_PlotLineLow(b, a, color);
        }
        return;
    }
    if (dy >= 0) {
        AM_PlotLineHigh(a, b, color);
    } else {
        AM_PlotLineHigh(b, a, color);
    }
}


//
// Clip lines, draw visible part sof lines.
//
static void AM_drawMline(mline_t* ml, int color) {
    static fline_t fl;

    if (AM_clipMline(ml, &fl)) {
        // draws it on frame buffer using fb coords
        AM_drawFline(&fl, color);
    }
}



//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
static void AM_drawGrid(int color)
{
    fixed_t x, y;
    fixed_t start, end;
    mline_t ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS))
	start += (MAPBLOCKUNITS<<FRACBITS)
	    - ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS));
    end = m_x + m_w;

    // draw vertical gridlines
    ml.a.y = m_y;
    ml.b.y = m_y+m_h;
    for (x=start; x<end; x+=(MAPBLOCKUNITS<<FRACBITS))
    {
	ml.a.x = x;
	ml.b.x = x;
	AM_drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS))
	start += (MAPBLOCKUNITS<<FRACBITS)
	    - ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS));
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    for (y=start; y<end; y+=(MAPBLOCKUNITS<<FRACBITS))
    {
	ml.a.y = y;
	ml.b.y = y;
	AM_drawMline(&ml, color);
    }

}

static bool AM_IsSecretDoor(const line_t* line) {
    return (line->flags & ML_SECRET) != 0;
}

static bool AM_IsTeleporter(const line_t* line) {
    return line->special == 39;
}

static bool AM_CanBeDraw(const line_t* line) {
    return (line->flags & ML_DONTDRAW) == 0;
}

static bool AM_IsMapped(const line_t* line) {
    return (line->flags & ML_MAPPED) != 0;
}

static int AM_GetComputerMapLineColor() {
    return GRAYS + 3;
}

static int AM_GetSecretDoorColor() {
    if (cheating) {
        return SECRETWALLCOLORS + lightlev;
    }
    return WALLCOLORS + lightlev;
}

static int AM_GetTeleporterColor() {
    return WALLCOLORS + WALLRANGE/2;
}

static int AM_GetTwoSidedLineColor(line_t* line) {
    const sector_t* back_sector = line->backsector;
    const sector_t* front_sector = line->frontsector;

    if (AM_IsTeleporter(line)) {
        return AM_GetTeleporterColor();
    }
    if (AM_IsSecretDoor(line)) {
        return AM_GetSecretDoorColor();
    }
    if (back_sector->floorheight != front_sector->floorheight) {
        // floor level change
        return FDWALLCOLORS + lightlev;
    }
    if (back_sector->ceilingheight != front_sector->ceilingheight) {
        // ceiling level change
        return CDWALLCOLORS + lightlev;
    }
    if (cheating) {
        return TSWALLCOLORS + lightlev;
    }
    return -1;
}

static int AM_GetOneSidedLineColor() {
    return WALLCOLORS + lightlev;
}

static int AM_GetLineColor(line_t* line) {
    if (!AM_CanBeDraw(line) && !cheating) {
        return -1;
    }
    if (AM_IsMapped(line) || cheating) {
        if (line->backsector) {
            return AM_GetTwoSidedLineColor(line);
        }
        return AM_GetOneSidedLineColor();
    }
    if (plr->powers[pw_allmap]) {
        return AM_GetComputerMapLineColor();
    }
    return -1;
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
static void AM_drawWalls() {
    static mline_t l;

    for (int i = 0; i < numlines; i++) {
        line_t* line = &lines[i];
        int line_color = AM_GetLineColor(line);

        if (line_color != -1) {
            l.a.x = line->v1->x;
            l.a.y = line->v1->y;
            l.b.x = line->v2->x;
            l.b.y = line->v2->y;
            AM_drawMline(&l, line_color);
        }
    }
}


//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
static void AM_rotate(fixed_t* x, fixed_t* y, angle_t angle) {
    fixed_t cos = COS(angle);
    fixed_t sin = SIN(angle);
    fixed_t x0 = *x;
    fixed_t y0 = *y;

    *x = FixedMul(x0, cos) - FixedMul(y0, sin);
    *y = FixedMul(x0, sin) + FixedMul(y0, cos);
}

static mpoint_t AM_ComputeCharacterPoint(mpoint_t p, fixed_t scale, const mobj_t* thing) {
    mpoint_t char_point = { .x = p.x, .y = p.y  };

    if (scale) {
        char_point.x = FixedMul(scale, char_point.x);
        char_point.y = FixedMul(scale, char_point.y);
    }
    if (thing->angle) {
        AM_rotate(&char_point.x, &char_point.y, thing->angle);
    }

    char_point.x += thing->x;
    char_point.y += thing->y;

    return char_point;
}

static void AM_drawLineCharacter(const mline_t* lineguy, int lineguylines,
                                 fixed_t scale, int color, mobj_t* thing)
{
    for (int i = 0; i < lineguylines; i++) {
        mpoint_t a = lineguy[i].a;
        mpoint_t b = lineguy[i].b;
        mline_t	line = {
            .a = AM_ComputeCharacterPoint(a, scale, thing),
            .b = AM_ComputeCharacterPoint(b, scale, thing),
        };

	AM_drawMline(&line, color);
    }
}

static void AM_DrawNetPlayers() {
    static int their_colors[] = { GREENS, GRAYS, BROWNS, REDS };

    mline_t* lineguy = player_arrow;
    int lineguylines = arrlen(player_arrow);
    fixed_t scale = 0;

    for (int i = 0; i < MAXPLAYERS; i++) {
        player_t* p = &players[i];

        if (deathmatch && !singledemo && p != plr) {
            continue;
        }
        if (!playeringame[i]) {
            continue;
        }

        // If invisible, get color close to black
        int color = p->powers[pw_invisibility] ? 246 : their_colors[i];

        AM_drawLineCharacter(lineguy, lineguylines, scale, color, p->mo);
    }
}

static void AM_DrawSinglePlayer() {
    fixed_t scale = 0;
    int color = WHITE;
    mline_t* lineguy;
    int lineguylines;

    if (cheating) {
        lineguy = cheat_player_arrow;
        lineguylines = arrlen(cheat_player_arrow);
    } else {
        lineguy = player_arrow;
        lineguylines = arrlen(player_arrow);
    }

    AM_drawLineCharacter(lineguy, lineguylines, scale, color, plr->mo);
}

static void AM_drawPlayers(void) {
    if (netgame) {
        AM_DrawNetPlayers();
        return;
    }
    AM_DrawSinglePlayer();
}

static void AM_drawThings(int colors) {
    for (int i = 0; i < numsectors; i++) {
        mobj_t* t = sectors[i].thinglist;

	while (t) {
            mline_t* lineguy = thintriangle_guy;
            int lineguylines = arrlen(thintriangle_guy);
            fixed_t scale = 16 << FRACBITS;
            int color = colors + lightlev;

            AM_drawLineCharacter(lineguy, lineguylines, scale, color, t);

	    t = t->snext;
	}
    }
}

static void AM_drawMarks(void) {
    for (int i = 0; i < AM_NUMMARKPOINTS; i++) {
        if (markpoints[i].x == -1) {
            continue;
        }
        int w = 5; // because something's wrong with the wad, i guess
        int h = 6; // because something's wrong with the wad, i guess
        int fx = CXMTOF(markpoints[i].x);
        int fy = CYMTOF(markpoints[i].y);
        if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h) {
            V_DrawPatch(fx, fy, marknums[i]);
        }
    }
}

static void AM_drawCrosshair(int color) {
    // single point for now
    fb[(f_w*(f_h+1))/2] = color;
}

void AM_Drawer(void) {
    if (!automapactive) {
        return;
    }

    AM_clearFB(BACKGROUND);
    if (grid) {
        AM_drawGrid(GRIDCOLORS);
    }
    AM_drawWalls();
    AM_drawPlayers();
    if (cheating == 2) {
        AM_drawThings(THINGCOLORS);
    }
    AM_drawCrosshair(XHAIRCOLORS);
    AM_drawMarks();
}
