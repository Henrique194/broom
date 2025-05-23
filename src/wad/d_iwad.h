//
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
//     Find IWAD and initialize according to IWAD type.
//


#ifndef __D_IWAD__
#define __D_IWAD__

#include "d_mode.h"

#define IWAD_MASK_DOOM    ((1 << doom)           \
                         | (1 << doom2)          \
                         | (1 << pack_tnt)       \
                         | (1 << pack_plut)      \
                         | (1 << pack_chex)      \
                         | (1 << pack_hacx))

typedef struct
{
    const char *name;
    GameMission_t mission;
    GameMode_t mode;
    const char *description;
} iwad_t;

bool D_IsIWADName(const char *name);
char *D_FindWADByName(const char *filename);
char *D_TryFindWADByName(const char *filename);
char *D_FindIWAD(int mask, GameMission_t *mission);
const char *D_SaveGameIWADName(GameMission_t gamemission, GameVariant_t gamevariant);
const char *D_SuggestGameName(GameMission_t mission, GameMode_t mode);

#endif

