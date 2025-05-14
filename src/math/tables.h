//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	Lookup tables.
//	Do not try to look them up :-).
//	In the order of appearance:
//
//	int finetangent[4096]	- Tangens LUT.
//	 Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//	int finesine[10240]		- Sine lookup.
//	 Guess what, serves as cosine, too.
//	 Remarkable thing is, how to use BAMs with this?
//
//	int tantoangle[2049]	- ArcTan LUT,
//	  maps tan(angle) to angle fast. Gotta search.
//


#ifndef __TABLES__
#define __TABLES__


#include "doomtype.h"
#include "m_fixed.h"

//
// Binary Angle Measument, BAM.
//

#define ANG45  0x20000000
#define ANG90  0x40000000
#define ANG180 0x80000000
#define ANG270 0xc0000000
#define ANG5   (ANG90 / 18)

#define SLOPERANGE 2048

#define FINEANGLES 8192

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT 19

#define SIN(angle) (finesine[(angle) >> ANGLETOFINESHIFT])
#define COS(angle) SIN((angle) + ANG90)
#define TAN(angle) (finetangent[((angle) + ANG90) >> ANGLETOFINESHIFT])
#define ATAN(num, den) (tantoangle[SlopeDiv(num, den)])
#define ATAN2(x, y) (Atan2(x, y))


typedef unsigned int angle_t;


// Effective size is 10240.
extern const fixed_t finesine[5 * FINEANGLES / 4];

// Re-use data, is just PI/2 phase shift.
extern const fixed_t* finecosine;

// Effective size is 4096.
extern const fixed_t finetangent[FINEANGLES / 2];

// Gamma correction tables.
extern const byte gammatable[5][256];

// This table converts a given fraction/slope into its corresponding angle.
// Note that it can only convert slopes whose angles are between 0° and 45°,
// i.e. the effective size is 2049.
// The +1 size is to handle the case when x==y without additional checking.
extern const angle_t tantoangle[SLOPERANGE + 1];


int SlopeDiv(unsigned int num, unsigned int den);
angle_t Atan2(fixed_t x, fixed_t y);

#endif
