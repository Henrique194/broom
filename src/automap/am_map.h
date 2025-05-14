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
//  AutoMap module.
//

#ifndef __AMMAP_H__
#define __AMMAP_H__


#include "m_cheat.h"
#include "m_fixed.h"
#include "am_responder.h"

// Used by ST StatusBar stuff.
#define AM_MSGHEADER  (('a'<<24) + ('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e' << 8))
#define AM_MSGEXITED  (AM_MSGHEADER | ('x' << 8))

// how much zoom-out per tic pulls out to 0.5x in 1 second
#define M_ZOOMOUT  ((int) (FRACUNIT/1.02))

// how much zoom-in per tic goes to 2x in 1 second
#define M_ZOOMIN  ((int) (1.02*FRACUNIT))

// translates from frame-buffer coordinates to map coordinates
#define FTOM(x) FixedMul(((x) << FRACBITS), scale_ftom)

// translates from map coordinates to frame-buffer coordinates
#define MTOF(x) (FixedMul((x), scale_mtof) >> FRACBITS)

// how much the automap moves window per tic in frame-buffer
// coordinates moves 140 pixels in 1 second
#define F_PANINC  4

typedef struct
{
    int x;
    int y;
} fpoint_t;

typedef struct
{
    fpoint_t a;
    fpoint_t b;
} fline_t;

typedef struct
{
    fixed_t x;
    fixed_t y;
} mpoint_t;

typedef struct
{
    mpoint_t a;
    mpoint_t b;
} mline_t;

// Called by main loop.
void AM_Ticker();

// Called by main loop, called instead of view drawer if automap active.
void AM_Drawer();

// Called to force the automap to quit if the level is completed while it is up.
void AM_Stop();


extern cheatseq_t cheat_amap;
extern fixed_t scale_mtof;
extern fixed_t scale_ftom;

#endif
