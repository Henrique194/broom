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
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <ctype.h>


#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"

#include "deh_str.h"

#include "i_input.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_controls.h"
#include "p_saveg.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"


char gammamsg[5][26] = {GAMMALVL0, GAMMALVL1, GAMMALVL2, GAMMALVL3, GAMMALVL4};

//
// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.
//
static bool IsNullKey(int key) {
    return key == KEY_PAUSE
           || key == KEY_CAPSLOCK
           || key == KEY_SCRLCK
           || key == KEY_NUMLOCK;
}

//
// M_QuickLoad
//
void M_QuickLoadResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_LoadSelect(quickSaveSlot);
        S_StartSound(NULL,sfx_swtchx);
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
        quickSaveSlot = slot;
}

void M_QuickSaveResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_DoSave(quickSaveSlot);
        S_StartSound(NULL,sfx_swtchx);
    }
}

//
//      M_QuickSave
//
static char tempstring[90];


void M_QuickLoad(void)
{
    if (netgame)
    {
        M_StartMessage(DEH_String(QLOADNET),NULL,false);
        return;
    }

    if (quickSaveSlot < 0)
    {
        M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
        return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QLPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickLoadResponse, true);
}

void M_QuickSave(void)
{
    if (!usergame)
    {
        S_StartSound(NULL,sfx_oof);
        return;
    }

    if (gamestate != GS_LEVEL)
        return;

    if (quickSaveSlot < 0)
    {
        M_StartControlPanel();
        M_ReadSaveStrings();
        M_SetupNextMenu(&SaveDef);
        quickSaveSlot = -2;	// means to pick a slot now
        return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QSPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickSaveResponse, true);
}

static bool M_ResponderInactive(int key) {
    // Screen size down
    if (key == key_menu_decscreen) {
        if (automapactive || chat_on) {
            return false;
        }
        M_SizeDisplay(0);
        S_StartSound(NULL, sfx_stnmov);
        return true;
    }
    // Screen size up
    if (key == key_menu_incscreen) {
        if (automapactive || chat_on) {
            return false;
        }
        M_SizeDisplay(1);
        S_StartSound(NULL, sfx_stnmov);
        return true;
    }
    // Help key
    if (key == key_menu_help) {
        M_StartControlPanel();
        if (gameversion >= exe_ultimate) {
            currentMenu = &ReadDef2;
        } else {
            currentMenu = &ReadDef1;
        }
        itemOn = 0;
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }
    // Save
    if (key == key_menu_save) {
        M_StartControlPanel();
        S_StartSound(NULL, sfx_swtchn);
        M_SaveGame(0);
        return true;
    }
    // Load
    if (key == key_menu_load) {
        M_StartControlPanel();
        S_StartSound(NULL, sfx_swtchn);
        M_LoadGame(0);
        return true;
    }
    // Sound Volume
    if (key == key_menu_volume) {
        M_StartControlPanel();
        currentMenu = &SoundDef;
        itemOn = sfx_vol;
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }
    // Detail toggle
    if (key == key_menu_detail) {
        M_ChangeDetail(0);
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }
    // Quicksave
    if (key == key_menu_qsave) {
        S_StartSound(NULL, sfx_swtchn);
        M_QuickSave();
        return true;
    }
    // End game
    if (key == key_menu_endgame) {
        S_StartSound(NULL, sfx_swtchn);
        M_EndGame(0);
        return true;
    }
    // Toggle messages
    if (key == key_menu_messages) {
        M_ChangeMessages(0);
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }
    // Quickload
    if (key == key_menu_qload) {
        S_StartSound(NULL, sfx_swtchn);
        M_QuickLoad();
        return true;
    }
    // Quit DOOM
    if (key == key_menu_quit) {
        S_StartSound(NULL, sfx_swtchn);
        M_QuitDOOM(0);
        return true;
    }
    // gamma toggle
    if (key == key_menu_gamma) {
        usegamma++;
        if (usegamma > 4) {
            usegamma = 0;
        }
        players[consoleplayer].message = DEH_String(gammamsg[usegamma]);
        I_SetPalette(W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE));
        return true;
    }
    // Pop-up menu?
    if (key == key_menu_activate) {
        M_StartControlPanel();
        S_StartSound(NULL, sfx_swtchn);
        return true;
    }
    return false;
}

//
// Keys usable within menu;
//
static bool M_ResponderActive(const event_t* ev, int key) {
    if (key == key_menu_down) {
        // Move down to next item
        do
        {
            if (itemOn + 1 > currentMenu->numitems - 1)
                itemOn = 0;
            else
                itemOn++;
            S_StartSound(NULL, sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }
    if (key == key_menu_up) {
        // Move back up to previous item
        do
        {
            if (!itemOn) {
                itemOn = currentMenu->numitems - 1;
            } else {
                itemOn--;
            }
            S_StartSound(NULL, sfx_pstop);
        } while (currentMenu->menuitems[itemOn].status == -1);
        return true;
    }
    if (key == key_menu_left) {
        // Slide slider left
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(0);
        }
        return true;
    }
    if (key == key_menu_right) {
        // Slide slider right
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(1);
        }
        return true;
    }
    if (key == key_menu_forward) {
        // Activate menu item

        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;
            if (currentMenu->menuitems[itemOn].status == 2) {
                currentMenu->menuitems[itemOn].routine(1); // right arrow
                S_StartSound(NULL, sfx_stnmov);
            } else {
                currentMenu->menuitems[itemOn].routine(itemOn);
                S_StartSound(NULL, sfx_pistol);
            }
        }
        return true;
    }
    if (key == key_menu_activate) {
        // Deactivate menu
        currentMenu->lastOn = itemOn;
        M_ClearMenus();
        S_StartSound(NULL, sfx_swtchx);
        return true;
    }
    if (key == key_menu_back) {
        // Go back to previous menu
        currentMenu->lastOn = itemOn;
        if (currentMenu->prevMenu) {
            currentMenu = currentMenu->prevMenu;
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
        }
        return true;
    }

    // Keyboard shortcut?
    // Vanilla Doom has a weird behavior where it jumps to the scroll bars
    // when the certain keys are pressed, so emulate this.
    if (ev->type == ev_keydown || IsNullKey(key)) {
        for (int i = itemOn + 1; i < currentMenu->numitems; i++) {
            if (currentMenu->menuitems[i].alphaKey == ev->data2) {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }
        for (int i = 0; i <= itemOn; i++) {
            if (currentMenu->menuitems[i].alphaKey == ev->data2) {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }
    }

    return false;
}

static bool M_ScreenshotPressed(int key) {
    return (devparm && key == key_menu_help)
           || (key != 0 && key == key_menu_screenshot);
}

static bool M_PrintMessage(int key) {
    if (messageNeedsInput) {
        if (key != ' ' && key != KEY_ESCAPE && key != key_menu_confirm &&
            key != key_menu_abort)
        {
            return false;
        }
    }

    menuactive = messageLastMenuActive;
    messageToPrint = 0;
    if (messageRoutine) {
        messageRoutine(key);
    }

    menuactive = false;
    S_StartSound(NULL, sfx_swtchx);
    return true;
}

static bool M_FontHasChar(int ch) {
    if (ch == ' ') {
        return true;
    }
    if (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE) {
        return false;
    }
    return ch >= 32 && ch <= 127;
}

//
// Savegame name entry. This is complicated.
// Vanilla has a bug where the shift key is ignored when entering a savegame
// name. If vanilla_keyboard_mapping is on, we want to emulate this bug by
// using ev->data1. But if it's turned off, it implies the user doesn't care
// about Vanilla emulation: instead, use ev->data3 which gives the
// fully-translated and modified key input.
//
static void M_SaveGameChar(const event_t* ev) {
    int ch = vanilla_keyboard_mapping ? ev->data1 : ev->data3;
    ch = toupper(ch);

    if (!M_FontHasChar(ch)) {
        return;
    }

    if (saveCharIndex < SAVESTRINGSIZE - 1 &&
        M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE - 2) * 8)
    {
        savegamestrings[saveSlot][saveCharIndex++] = ch;
        savegamestrings[saveSlot][saveCharIndex] = 0;
    }
}

static void M_DeleteGameChar() {
    if (saveCharIndex > 0) {
        saveCharIndex--;
        savegamestrings[saveSlot][saveCharIndex] = 0;
    }
}

static void M_SaveGameStringInput(const event_t* ev, int key) {
    switch (key) {
        case KEY_BACKSPACE:
            M_DeleteGameChar();
            break;
        case KEY_ESCAPE:
            saveStringEnter = 0;
            I_StopTextInput();
            M_StringCopy(savegamestrings[saveSlot], saveOldString, SAVESTRINGSIZE);
            break;
        case KEY_ENTER:
            saveStringEnter = 0;
            I_StopTextInput();
            if (savegamestrings[saveSlot][0]) {
                M_DoSave(saveSlot);
            }
            break;
        default:
            M_SaveGameChar(ev);
            break;
    }
}

static int M_GetMouseKey(const event_t* ev) {
    static int mousewait = 0;
    static int mousey = 0;
    static int lasty = 0;
    static int mousex = 0;
    static int lastx = 0;

    if (mousewait >= I_GetTime() || !menuactive) {
        return -1;
    }

    mousey += ev->data3;
    if (mousey < lasty - 30) {
        mousewait = I_GetTime() + 5;
        mousey = lasty -= 30;
        return key_menu_down;
    }
    if (mousey > lasty + 30) {
        mousewait = I_GetTime() + 5;
        mousey = lasty += 30;
        return key_menu_up;
    }

    mousex += ev->data2;
    if (mousex < lastx - 30) {
        mousewait = I_GetTime() + 5;
        mousex = lastx -= 30;
        return key_menu_left;
    }
    if (mousex > lastx + 30) {
        mousewait = I_GetTime() + 5;
        mousex = lastx += 30;
        return key_menu_right;
    }

    if (ev->data1 & 1) {
        mousewait = I_GetTime() + 15;
        return key_menu_forward;
    }
    if (ev->data1 & 2) {
        mousewait = I_GetTime() + 15;
        return key_menu_back;
    }

    return -1;
}

static bool M_IsJoyButtonPressed(const event_t* ev, int button) {
    if (button < 0) {
        return false;
    }
    return (ev->data1 & (1 << button)) != 0;
}

static int M_GetJoyStickKey(const event_t* ev) {
    if (!menuactive) {
        if (M_IsJoyButtonPressed(ev, joybmenu)) {
            joywait = I_GetTime() + 5;
            return key_menu_activate;
        }
        return -1;
    }
    if (ev->data3 < 0) {
        joywait = I_GetTime() + 5;
        return key_menu_up;
    }
    if (ev->data3 > 0) {
        joywait = I_GetTime() + 5;
        return key_menu_down;
    }
    if (ev->data2 < 0) {
        joywait = I_GetTime() + 2;
        return key_menu_left;
    }
    if (ev->data2 > 0) {
        joywait = I_GetTime() + 2;
        return key_menu_right;
    }
    if (M_IsJoyButtonPressed(ev, joybfire)) {
        joywait = I_GetTime() + 5;

        // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
        if (messageToPrint && messageNeedsInput) {
            return key_menu_confirm;
        }
        // Simulate pressing "Enter" when we are supplying a save slot name
        if (saveStringEnter) {
            return KEY_ENTER;
        }
        // if selecting a save slot via joypad, set a flag
        if (currentMenu == &SaveDef) {
            joypadSave = true;
        }
        return key_menu_forward;
    }
    if (M_IsJoyButtonPressed(ev, joybuse)) {
        joywait = I_GetTime() + 5;

        // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
        if (messageToPrint && messageNeedsInput) {
            return key_menu_abort;
        }
        // If user was entering a save name, back out
        if (saveStringEnter) {
            return KEY_ESCAPE;
        }
        return key_menu_back;
    }

    return -1;
}

static int M_GetKey(const event_t* ev) {
    switch (ev->type) {
        case ev_joystick:
            return M_GetJoyStickKey(ev);
        case ev_mouse:
            return M_GetMouseKey(ev);
        case ev_keydown:
            return ev->data1;
        default:
            return -1;
    }
}

//
// First click on close button = bring up quit confirm message.
// Second click on close button = confirm quit.
//
static bool M_DoQuit() {
    if (menuactive && messageToPrint && messageRoutine == M_QuitResponse) {
        M_QuitResponse(key_menu_confirm);
    } else {
        S_StartSound(NULL, sfx_swtchn);
        M_QuitDOOM(0);
    }
    return true;
}

//
// In testcontrols mode, none of the function keys should do anything
// - the only key is escape to quit.
//
static bool M_DoTestControls(const event_t* ev) {
    switch (ev->type) {
        case ev_quit:
            I_Quit();
            return true;
        case ev_keydown:
            int key = ev->data1;
            if (key == key_menu_activate || key == key_menu_quit) {
                I_Quit();
                return true;
            }
            return false;
        default:
            return false;
    }
}


//
// M_Responder
//
bool M_Responder(const event_t* ev) {
    if (testcontrols) {
        return M_DoTestControls(ev);
    }
    // "close" button pressed on window?
    if (ev->type == ev_quit) {
        M_DoQuit();
        return true;
    }

    // key is the key pressed, ch is the actual character typed
    int key = M_GetKey(ev);
    if (key == -1) {
        return false;
    }
    // Save Game string input
    if (saveStringEnter) {
        M_SaveGameStringInput(ev, key);
        return true;
    }
    // Take care of any messages that need input
    if (messageToPrint) {
        return M_PrintMessage(key);
    }
    if (M_ScreenshotPressed(key)) {
        G_ScreenShot();
        return true;
    }
    if (menuactive) {
        // Keys usable within menu
        return M_ResponderActive(ev, key);
    }
    // F-Keys
    return M_ResponderInactive(key);
}
