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
// DESCRIPTION:  none
//

#ifndef __HULIB__
#define __HULIB__

// We are referring to patches.
#include "v_patch.h"

#define HU_MAXLINES      4
#define HU_MAXLINELENGTH 80

//
// Typedefs of widgets
//

// Text Line widget (parent of Scrolling Text and Input Text widgets)
typedef struct
{
    // left-justified position of scrolling text window
    int x;
    int y;

    patch_t **f;                  // font
    int sc;                       // start character
    char l[HU_MAXLINELENGTH + 1]; // line of text
    int len;                      // current line length

    // whether this line needs to be udpated
    int needsupdate;

} hu_textline_t;


// Scrolling Text window widget (child of Text Line widget)
typedef struct
{
    hu_textline_t l[HU_MAXLINES]; // text lines to draw
    int h;                        // height in lines
    int cl;                       // current line number

    // pointer to bool stating whether to update window
    bool *on;
    bool laston; // last value of *->on.
} hu_stext_t;


// Input Text Line widget (child of Text Line widget)
typedef struct
{
    hu_textline_t l; // text line to input on

    // left margin past which I am not to delete characters
    int lm;

    // pointer to bool stating whether to update window
    bool *on;
    bool laston; // last value of *->on;
} hu_itext_t;


//
// Widget creation, access, and update routines
//

//
// textline code
//

// clear a line of text
void HUlib_clearTextLine(hu_textline_t *t);

void HUlib_initTextLine(hu_textline_t *t, int x, int y, patch_t **f, int sc);

// returns success
bool HUlib_addCharToTextLine(hu_textline_t *t, char ch);

// returns success
bool HUlib_delCharFromTextLine(hu_textline_t *t);

// draws tline
void HUlib_drawTextLine(hu_textline_t *l, bool drawcursor);

// erases text line
void HUlib_eraseTextLine(hu_textline_t *l);


//
// Scrolling Text window widget routines
//

// ?
void HUlib_initSText(hu_stext_t *s, int x, int y, int h, patch_t **font,
                     int startchar, bool *on);

// add a new line
void HUlib_addLineToSText(hu_stext_t *s);

// ?
void HUlib_addMessageToSText(hu_stext_t *s, const char *prefix,
                             const char *msg);

// draws stext
void HUlib_drawSText(hu_stext_t *s);

// erases all stext lines
void HUlib_eraseSText(hu_stext_t *s);

// Input Text Line widget routines
void HUlib_initIText(hu_itext_t *it, int x, int y, patch_t **font,
                     int startchar, bool *on);

// enforces left margin
void HUlib_delCharFromIText(hu_itext_t *it);

// resets line and left margin
void HUlib_resetIText(hu_itext_t *it);

// whether eaten
bool HUlib_keyInIText(hu_itext_t *it, unsigned char ch);

void HUlib_drawIText(hu_itext_t *it);

// erases all itext lines
void HUlib_eraseIText(hu_itext_t *it);

#endif
