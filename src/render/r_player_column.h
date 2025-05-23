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
//	System specific interface stuff.
//


#ifndef __R_PLAYER_COLUMN__
#define __R_PLAYER_COLUMN__

extern byte* translationtables;
extern byte* dc_translation;

// Draw with color translation tables,
// for player sprite rendering, Green/Red/Blue/Indigo shirts.
void R_DrawTranslatedColumn();
void R_DrawTranslatedColumnLow();

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables();

#endif
