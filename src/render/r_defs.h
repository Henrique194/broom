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
//      Refresh/rendering module, shared data struct definitions.
//


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"
// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#include "i_video.h"

#include "v_patch.h"


// Silhouette, needed for clipping Segs (mainly) and sprites representing things.
#define SIL_NONE   0
#define SIL_BOTTOM 1
#define SIL_TOP    2
#define SIL_BOTH   3

#define MAXDRAWSEGS 256


//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally, like some DOOM-alikes ("wt", "WebView") did.
//
typedef struct
{
    fixed_t x;
    fixed_t y;
} vertex_t;


// Forward of LineDefs, for Sectors.
struct line_s;


// Each sector has a degenmobj_t in its center for sound origin purposes.
// I suppose this does not handle sound from moving objects (doppler),
// because position is prolly just buffered, not updated.
typedef struct
{
    // not used for anything
    thinker_t thinker;
    fixed_t x;
    fixed_t y;
    fixed_t z;
} degenmobj_t;


//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef struct
{
    fixed_t floorheight;
    fixed_t ceilingheight;
    short floorpic;
    short ceilingpic;
    short lightlevel;
    short special;
    short tag;

    // 0 = Untraversed.
    // 1 = Sector was flooded through a path with no sound-blocking line.
    // 2 = Sector was flooded through a path with a sound-blocking line.
    int soundtraversed;

    // thing that made a sound (or null)
    mobj_t* soundtarget;

    // mapblock bounding box for height changes
    int blockbox[4];

    // origin for any sounds played by the sector
    degenmobj_t soundorg;

    // if == validcount, already checked
    int validcount;

    // List of map objects in sector.
    // Used when rendering map objects.
    mobj_t* thinglist;

    // thinker_t for reversable actions
    void* specialdata;

    int linecount;

    // [linecount] size
    struct line_s** lines;
} sector_t;


//
// The SideDef.
//
typedef struct
{
    // add this to the calculated texture column
    fixed_t textureoffset;

    // add this to the calculated texture top
    fixed_t rowoffset;

    // Texture indices.
    // We do not maintain names here.
    short toptexture;
    short bottomtexture;
    short midtexture;

    // Sector the SideDef is facing.
    sector_t *sector;
} side_t;


//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;


typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t *v1;
    vertex_t *v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t dx;
    fixed_t dy;

    // Animation related.
    short flags;
    short special;
    short tag;

    // Visual appearance: SideDefs.
    // sidenum[1] will be -1 if one sided
    short sidenum[2];

    // Neat. Another bounding box, for the extent of the LineDef.
    fixed_t bbox[4];

    // To aid move clipping.
    slopetype_t slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t *frontsector;
    sector_t *backsector;

    // if == validcount, already checked
    int validcount;

    // thinker_t for reversable actions
    void *specialdata;
} line_t;


//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs, indicating the visible
// walls that define (all or some) sides of a convex BSP leaf.
//
typedef struct subsector_s
{
    sector_t *sector;
    short numlines;
    short firstline;
} subsector_t;


//
// The LineSeg.
//
typedef struct
{
    // Start vertex.
    vertex_t *v1;
    // End vertex.
    vertex_t *v2;

    // Distance along linedef to start of seg.
    // The offset is in the same direction as the seg. If the segment is
    // in the same direction as the linedef, then the distance is from the
    // "start" vertex of the linedef to the "start" vertex of the seg.
    // Otherwise, the offset is from the "end" vertex of the linedef to
    // the "start" vertex of the seg. So if the seg begins at one of the
    // two endpoints of the linedef, this offset will be 0.
    fixed_t offset;

    angle_t angle;

    side_t *sidedef;
    // Line that this segment belongs to.
    line_t *linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one-sided lines.
    sector_t *frontsector;
    sector_t *backsector;
} seg_t;

//
// BSP node.
//
typedef struct
{
    // Partition line.
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;

    // Bounding box for each child.
    fixed_t bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
} node_t;


//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is possible precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef pixel_t lighttable_t;

//
// The sprite clipping information built while rendering walls.
// It effectively serves as a log of occluders that were rendered to the screen,
// allowing proper sprite clipping.
//
// - For each wall that generated pixels in the framebuffer, a drawseg_t is added.
// - For each portal, up to two drawseg_ts are added (one for the upper part
//   and one for the lower part if the portal had no middle texture).
// - For each masked segment that was skipped, one drawseg_t entry is added to
//   record what should have been drawn.
//
// The log entries are ordered by distance from the player (since each entry
// was added while rendering the walls). The distance is not a z value but a
// scale value. The idea is to replay a portion of the log for each thing
// (everything in front of the thing) in order to build an accurate occlusion
// rectangle. Once the occlusion rectangle is obtained, the Thing is
// clipped against it. Each drawseg_t features the screen-space horizontal
// boundaries (x1 and x2) as well as their respective scales (scale1 and scale2).
// Since a scale value is generated for each Thing (based on its distance from
// the player), it is used to know what portion of the drawsegs array to replay.
//
// The screen-space horizontal top and bottom edge of each wall are obtained
// from pointers sprtopclip and sprbottomclip which point to an array shared by
// all drawseg_ts.
//
typedef struct drawseg_s
{
    seg_t *curline;
    int x1;
    int x2;

    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int silhouette;

    // do not clip sprites above this
    fixed_t bsilheight;

    // do not clip sprites below this
    fixed_t tsilheight;

    // Pointers to lists for sprite clipping,
    // all three adjusted so [x1] is first value.
    short *sprtopclip;
    short *sprbottomclip;
    short *maskedtexturecol;
} drawseg_t;


// A vissprite_t is a thing that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s* prev;
    struct vissprite_s* next;

    // Screen coordinates.
    int x1;
    int x2;

    // Map coordinates.
    // For line side calculation.
    fixed_t gx;
    fixed_t gy;

    // Map bottom/top coordinates for silhouette clipping.
    fixed_t gz;
    fixed_t gzt;

    // Horizontal position of x1.
    fixed_t startfrac;

    // From world space to screen space.
    fixed_t scale;

    // From screen space to world space.
    // Negative if flipped.
    fixed_t xiscale;

    fixed_t texturemid;
    int patch;

    // For color translation and shadow draw, maxbright frames as well.
    lighttable_t* colormap;

    int mobjflags;
} vissprite_t;


//	
// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites. The base name is NNNNFx or NNNNFxFx, with x
// indicating the rotation, x = 0, 1-7. The sprite and frame specified by a
// thing_t is range checked at run time. A sprite is a patch_t that is assumed
// to represent a three dimensional object and may have multiple rotations pre
// drawn. Horizontal flipping is used to save space, thus NNNNF2F5 defines a
// mirrored patch. Some sprites will only have one picture used for all views:
// NNNNF0
//
typedef struct
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    int rotate;

    // Lump to use for view angles 0-7.
    short lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte flip[8];
} spriteframe_t;


//
// A sprite definition: a number of animation frames.
//
typedef struct
{
    int numframes;
    spriteframe_t *spriteframes;
} spritedef_t;


//
// Now what is a visplane, anyway?
// A visplane represents a horizontal runs of texture, given a floor
// or ceiling at a particular height, light level and texture (if two
// adjacent sectors have the exact same floor, these can be merged into
// one visplane). Each X position in the visplane has a particular vertical
// line of texture which is to be drawn.
// 
typedef struct
{
    fixed_t height;
    int picnum;
    int lightlevel;
    int minx;
    int maxx;

    // leave pads for [minx-1]/[maxx+1]
    byte pad1;

    // Here lies the rub for all dynamic resize/change of resolution.
    byte top[SCREENWIDTH];
    byte pad2;
    byte pad3;

    // See above.
    byte bottom[SCREENWIDTH];
    byte pad4;
} visplane_t;

#endif
