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
//
//


#ifndef __M_RANDOM__
#define __M_RANDOM__

//
// Returns a number from 0 to 255, from a lookup table.
// This is used only by the play simulation, such as calculating hit damage.
//
int P_Random();

//
// Defined version of P_Random() - P_Random()
//
int P_SubRandom();

//
// As P_Random, but used to maintain multiplayer synchronisation. For example,
// M_Random is used to apply a random pitch variation to sounds. As two players
// may not hear the same sound effect (they may be in different parts of the
// level), using a single index would cause the game to become desynchronised.
//
int M_Random();

//
// Fix randoms for demos.
//
void M_ClearRandom();

#endif
