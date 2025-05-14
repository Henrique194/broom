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


#ifndef V_PATCH_H
#define V_PATCH_H

#include "doomtype.h"


// Represents a patch, which is a collection of one or more columns.
// Patches are used for sprites and all masked pictures, and textures are
// composed by assembling patches from the TEXTURE1/2 lists.
typedef PACKED_STRUCT({
    // Width of graphic, in pixels.
    short width;

    // Height of graphic, in pixels.
    short height;

    // Offset in pixels to the left of the origin.
    // A positive value means the patch is shifted to the left of the origin,
    // while a negative value means it is shifted to the right of the origin.
    short leftoffset;

    // Offset in pixels above the origin.
    // A positive value means the patch is displaced above the origin,
    // while a negative value means it is displaced below the origin.
    short topoffset;

    // Array of column offsets relative to the beginning of the patch header.
    int columnofs[8];
}) patch_t;

// posts are runs of non masked source pixels.
typedef PACKED_STRUCT({
    // The y offset of this post in this patch.
    // If 0xFF, then end-of-column (not a valid post)
    byte topdelta;

    // Length of data in this post
    byte length;

    // Unused padding byte;
    // prevents error on column underflow due to loss of precision.
    byte pad1;

    // Array of pixels in this post;
    // each data pixel is an index into the Doom palette.
    byte data[0];
}) post_t;

// Each column is an array of post_t, of indeterminate length. The column is
// terminated by a special byte value, which is defined by the macro END_COLUMN.
typedef post_t column_t;

// Macro to retrieve a specific column from a patch based on the column number.
// The column number (col_num) must be less than the patch's width.
#define GET_COLUMN(patch, col_num)                                             \
    ((column_t *) ((byte *) patch + LONG(patch->columnofs[col_num])))

// Marker that indicates the end of a column when iterating over posts.
#define END_COLUMN (0xff)

// Used for iterating over posts of a column.
#define NEXT_COLUMN(col) ((column_t *) ((byte *) col + col->length + 4))

#endif 

