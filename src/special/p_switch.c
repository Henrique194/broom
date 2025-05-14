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
// DESCRIPTION:
//	Switches, buttons. Two-state animation. Exits.
//


#include "p_switch.h"

#include "i_system.h"
#include "deh_str.h"
#include "p_local.h"
#include "p_plats.h"
#include "p_floor.h"
#include "p_doors.h"
#include "p_lights.h"

#include "g_game.h"

#include "s_sound.h"

#include "p_ceiling.h"

// Data.
#include "sounds.h"

// State.
#include "doomstat.h"
#include "r_state.h"


//
// CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//
switchlist_t alphSwitchList[] = {
    // Doom shareware episode 1 switches
    {"SW1BRCOM", "SW2BRCOM", 1},
    {"SW1BRN1", "SW2BRN1", 1},
    {"SW1BRN2", "SW2BRN2", 1},
    {"SW1BRNGN", "SW2BRNGN", 1},
    {"SW1BROWN", "SW2BROWN", 1},
    {"SW1COMM", "SW2COMM", 1},
    {"SW1COMP", "SW2COMP", 1},
    {"SW1DIRT", "SW2DIRT", 1},
    {"SW1EXIT", "SW2EXIT", 1},
    {"SW1GRAY", "SW2GRAY", 1},
    {"SW1GRAY1", "SW2GRAY1", 1},
    {"SW1METAL", "SW2METAL", 1},
    {"SW1PIPE", "SW2PIPE", 1},
    {"SW1SLAD", "SW2SLAD", 1},
    {"SW1STARG", "SW2STARG", 1},
    {"SW1STON1", "SW2STON1", 1},
    {"SW1STON2", "SW2STON2", 1},
    {"SW1STONE", "SW2STONE", 1},
    {"SW1STRTN", "SW2STRTN", 1},

    // Doom registered episodes 2&3 switches
    {"SW1BLUE", "SW2BLUE", 2},
    {"SW1CMT", "SW2CMT", 2},
    {"SW1GARG", "SW2GARG", 2},
    {"SW1GSTON", "SW2GSTON", 2},
    {"SW1HOT", "SW2HOT", 2},
    {"SW1LION", "SW2LION", 2},
    {"SW1SATYR", "SW2SATYR", 2},
    {"SW1SKIN", "SW2SKIN", 2},
    {"SW1VINE", "SW2VINE", 2},
    {"SW1WOOD", "SW2WOOD", 2},

    // Doom II switches
    {"SW1PANEL", "SW2PANEL", 3},
    {"SW1ROCK", "SW2ROCK", 3},
    {"SW1MET2", "SW2MET2", 3},
    {"SW1WDMET", "SW2WDMET", 3},
    {"SW1BRIK", "SW2BRIK", 3},
    {"SW1MOD1", "SW2MOD1", 3},
    {"SW1ZIM", "SW2ZIM", 3},
    {"SW1STON6", "SW2STON6", 3},
    {"SW1TEK", "SW2TEK", 3},
    {"SW1MARB", "SW2MARB", 3},
    {"SW1SKULL", "SW2SKULL", 3},
};

int switchlist[MAXSWITCHES * 2];
int numswitches;
button_t buttonlist[MAXBUTTONS];

//
// P_InitSwitchList
// Only called at game initialization.
//
void P_InitSwitchList(void)
{
    int i, slindex, episode;

    // Note that this is called "episode" here but it's actually something
    // quite different. As we progress from Shareware->Registered->Doom II
    // we support more switch textures.
    switch (gamemode)
    {
        case registered:
        case retail:
            episode = 2;
            break;
        case commercial:
            episode = 3;
            break;
        default:
            episode = 1;
            break;
    }

    slindex = 0;

    for (i = 0; i < arrlen(alphSwitchList); i++)
    {
        if (alphSwitchList[i].episode <= episode)
        {
            switchlist[slindex++] =
                R_TextureNumForName(DEH_String(alphSwitchList[i].name1));
            switchlist[slindex++] =
                R_TextureNumForName(DEH_String(alphSwitchList[i].name2));
        }
    }

    numswitches = slindex / 2;
    switchlist[slindex] = -1;
}

//
// Start a button counting down till it turns off.
//
static void P_StartButton(line_t *line, bwhere_e w, int texture, int time)
{
    // See if button is already pressed
    for (int i = 0; i < MAXBUTTONS; i++)
    {
        const button_t *button = &buttonlist[i];
        if (button->btimer != 0 && button->line == line)
        {
            return;
        }
    }

    for (int i = 0; i < MAXBUTTONS; i++)
    {
        button_t *button = &buttonlist[i];
        if (button->btimer == 0)
        {
            button->line = line;
            button->where = w;
            button->btexture = texture;
            button->btimer = time;
            button->soundorg = &line->frontsector->soundorg;
            return;
        }
    }

    I_Error("P_StartButton: no button slots left!");
}

static int P_GetSwitchSound(const line_t *line)
{
    if (line->special == 11)
    {
        // Exit switch
        return sfx_swtchx;
    }
    return sfx_swtchn;
}

static void P_ChangeTexture(int switch_num, line_t *line, bwhere_e w,
                            bool useAgain)
{
    short new_texture = (short) switchlist[switch_num ^ 1];
    side_t *side = &sides[line->sidenum[0]];
    switch (w)
    {
        case top:
            side->toptexture = new_texture;
            break;
        case middle:
            side->midtexture = new_texture;
            break;
        case bottom:
            side->bottomtexture = new_texture;
            break;
    }

    int sound = P_GetSwitchSound(line);
    S_StartSound(buttonlist[0].soundorg, sound);

    if (useAgain)
    {
        P_StartButton(line, w, switchlist[switch_num], BUTTONTIME);
    }
}


//
// Function that changes wall texture.
// Tell it if switch is ok to use again (1=yes, it's a button).
//
void P_ChangeSwitchTexture(line_t *line, int useAgain)
{
    if (useAgain == 0)
    {
        line->special = 0;
    }

    const side_t *side = &sides[line->sidenum[0]];

    for (int i = 0; i < numswitches * 2; i++)
    {
        if (switchlist[i] == side->toptexture)
        {
            P_ChangeTexture(i, line, top, useAgain);
            return;
        }
        if (switchlist[i] == side->midtexture)
        {
            P_ChangeTexture(i, line, middle, useAgain);
            return;
        }
        if (switchlist[i] == side->bottomtexture)
        {
            P_ChangeTexture(i, line, bottom, useAgain);
            return;
        }
    }
}

static bool P_TryUseButton(mobj_t *thing, line_t *line)
{
    switch (line->special)
    {
        case 42:
            // Close Door
            if (EV_DoDoor(line, vld_close))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 43:
            // Lower Ceiling to Floor
            if (EV_DoCeiling(line, lowerToFloor))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 45:
            // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 60:
            // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 61:
            // Open Door
            if (EV_DoDoor(line, vld_open))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 62:
            // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay, 1))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 63:
            // Raise Door
            if (EV_DoDoor(line, vld_normal))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 64:
            // Raise Floor to ceiling
            if (EV_DoFloor(line, raiseFloor))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 66:
            // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 67:
            // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 65:
            // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 68:
            // Raise Plat to next highest floor and change texture
            if (EV_DoPlat(line, raiseToNearestAndChange, 0))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 69:
            // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 70:
            // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 114:
            // Blazing Door Raise (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeRaise))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 115:
            // Blazing Door Open (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeOpen))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 116:
            // Blazing Door Close (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeClose))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 123:
            // Blazing PlatDownWaitUpStay
            if (EV_DoPlat(line, blazeDWUS, 0))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 132:
            // Raise Floor Turbo
            if (EV_DoFloor(line, raiseFloorTurbo))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 99:  // BlzOpenDoor BLUE
        case 134: // BlzOpenDoor RED
        case 136: // BlzOpenDoor YELLOW
            if (EV_DoLockedDoor(line, vld_blazeOpen, thing))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;
        case 138:
            // Light Turn On
            EV_LightTurnOn(line, 255);
            P_ChangeSwitchTexture(line, 1);
            return true;
        case 139:
            // Light Turn Off
            EV_LightTurnOn(line, 35);
            P_ChangeSwitchTexture(line, 1);
            return true;
        default:
            return false;
    }
}

static bool P_TryUseSwitch(mobj_t *thing, line_t *line)
{
    switch (line->special)
    {
        case 7:
            // Build Stairs
            if (EV_BuildStairs(line, build8))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 9:
            // Change Donut
            if (EV_DoDonut(line))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 11:
            // Exit level
            P_ChangeSwitchTexture(line, 0);
            G_ExitLevel();
            return true;
        case 14:
            // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 15:
            // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 18:
            // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 20:
            // Raise Plat next highest floor and change texture
            if (EV_DoPlat(line, raiseToNearestAndChange, 0))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 21:
            // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay, 0))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 23:
            // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 29:
            // Raise Door
            if (EV_DoDoor(line, vld_normal))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 41:
            // Lower Ceiling to Floor
            if (EV_DoCeiling(line, lowerToFloor))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 71:
            // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 49:
            // Ceiling Crush And Raise
            if (EV_DoCeiling(line, crushAndRaise))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 50:
            // Close Door
            if (EV_DoDoor(line, vld_close))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 51:
            // Secret EXIT
            P_ChangeSwitchTexture(line, 0);
            G_SecretExitLevel();
            return true;
        case 55:
            // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 101:
            // Raise Floor
            if (EV_DoFloor(line, raiseFloor))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 102:
            // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 103:
            // Open Door
            if (EV_DoDoor(line, vld_open))
                P_ChangeSwitchTexture(line, 0);
            return true;
        case 111:
            // Blazing Door Raise (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeRaise))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 112:
            // Blazing Door Open (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeOpen))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 113:
            // Blazing Door Close (faster than TURBO!)
            if (EV_DoDoor(line, vld_blazeClose))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 122:
            // Blazing PlatDownWaitUpStay
            if (EV_DoPlat(line, blazeDWUS, 0))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 127:
            // Build Stairs Turbo 16
            if (EV_BuildStairs(line, turbo16))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 131:
            // Raise Floor Turbo
            if (EV_DoFloor(line, raiseFloorTurbo))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 133: // BlzOpenDoor BLUE
        case 135: // BlzOpenDoor RED
        case 137: // BlzOpenDoor YELLOW
            if (EV_DoLockedDoor(line, vld_blazeOpen, thing))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        case 140:
            // Raise Floor 512
            if (EV_DoFloor(line, raiseFloor512))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;
        default:
            return false;
    }
}

static bool P_TryUseManualDoor(mobj_t *thing, line_t *line)
{
    switch (line->special)
    {
        case 1:  // Vertical Door
        case 26: // Blue Door/Locked
        case 27: // Yellow Door /Locked
        case 28: // Red Door /Locked

        case 31: // Manual door open
        case 32: // Blue locked door open
        case 33: // Red locked door open
        case 34: // Yellow locked door open

        case 117: // Blazing door raise
        case 118: // Blazing door open
            EV_VerticalDoor(line, thing);
            return true;
        default:
            return false;
    }
}

//
// Switches that other things can activate.
//
static bool P_CanMonsterUseSpecialLine(const line_t *line)
{
    if (line->flags & ML_SECRET)
    {
        // never open secret doors
        return false;
    }
    switch (line->special)
    {
        case 1:  // MANUAL DOOR RAISE
        case 32: // MANUAL BLUE
        case 33: // MANUAL RED
        case 34: // MANUAL YELLOW
            return true;
        default:
            return false;
    }
}

static bool P_CanUseSpecialLine(const mobj_t *thing, const line_t *line,
                                int side)
{
    // Err...
    // Use the back sides of VERY SPECIAL lines...
    if (side && line->special != 124)
    {
        return false;
    }
    if (thing->player)
    {
        return true;
    }
    return P_CanMonsterUseSpecialLine(line);
}

//
// P_UseSpecialLine
// Called when a thing uses a special line.
// Only the front sides of lines are usable.
//
bool P_UseSpecialLine(mobj_t *thing, line_t *line, int side)
{
    if (!P_CanUseSpecialLine(thing, line, side))
    {
        return false;
    }
    if (P_TryUseManualDoor(thing, line))
    {
        // Door ate the event.
        return true;
    }
    if (P_TryUseSwitch(thing, line))
    {
        // Switch ate the event.
        return true;
    }
    P_TryUseButton(thing, line);
    // Returns true even if no button eats the event.
    return true;
}
