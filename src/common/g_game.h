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
//   Duh.
//


#ifndef __G_GAME__
#define __G_GAME__


#include "doomdef.h"
#include "d_mode.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "m_fixed.h"


//
// GAME
//
void G_DeathMatchSpawnPlayer(int playernum);

void G_InitNew(skill_t skill, int episode, int map);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew(skill_t skill, int episode, int map);

void G_DeferedPlayDemo(const char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame(char* name);

void G_DoLoadGame(void);

// Called by M_Responder.
void G_SaveGame(int slot, char *description);

// Only called by startup code.
void G_RecordDemo(const char *name);

void G_BeginRecording(void);

void G_TimeDemo(char* name);
bool G_CheckDemoStatus();

void G_ExitLevel();
void G_SecretExitLevel();

void G_WorldDone();

//
// Read current data from inputs and build a player movement command.
//
void G_BuildTicCmd(ticcmd_t* cmd, int maketic);

void G_Ticker();
bool G_Responder(event_t *ev);

void G_ScreenShot();

int G_VanillaVersionCode();

extern int vanilla_savegame_limit;
extern int vanilla_demo_limit;

extern fixed_t forwardmove[2];
extern fixed_t sidemove[2];

extern bool sendpause;


#endif
