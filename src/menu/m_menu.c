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


#include <stdlib.h>
#include <ctype.h>


#include "doomdef.h"
#include "d_loop.h"
#include "dstrings.h"

#include "d_main.h"
#include "deh_str.h"

#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_controls.h"
#include "p_saveg.h"
#include "p_setup.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"


//
// defaulted values
//
int mouseSensitivity = 5;

// Show messages has default, 0 = off, 1 = on
int showMessages = 1;


// Blocky mode, has default, 0 = high, 1 = normal
int detailLevel = 0;
int screenblocks = 9;

// temp for screenblocks (0-9)
int screenSize;

// -1 = no quicksave slot picked!
int quickSaveSlot;

// 1 = message to be printed
int messageToPrint;
// ...and here is the message string!
const char *messageString;

// message x & y
bool messageLastMenuActive;

// timed message = no input from user
bool messageNeedsInput;

void (*messageRoutine)(int response);

// we are going to be entering a savegame string
int saveStringEnter;
int saveSlot;               // which slot to save in
int saveCharIndex;          // which char we're editing
bool joypadSave = false; // was the save action initiated by joypad?
// old save description before edit
char saveOldString[SAVESTRINGSIZE];

bool inhelpscreens;
bool menuactive;

#define SKULLXOFF  -32
#define LINEHEIGHT 16

char savegamestrings[10][SAVESTRINGSIZE];

char endstring[160];

static bool opldev;

short itemOn;           // menu item skull is on
short skullAnimCounter; // skull animation counter
short whichSkull;       // which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
const char *skullName[2] = {"M_SKULL1", "M_SKULL2"};

// current menudef
menu_t *currentMenu;


//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

menuitem_t MainMenu[] = {
    {1, "M_NGAME", M_NewGame, 'n'},
    {1, "M_OPTION", M_Options, 'o'},
    {1, "M_LOADG", M_LoadGame, 'l'},
    {1, "M_SAVEG", M_SaveGame, 's'},
    // Another hickup with Special edition.
    {1, "M_RDTHIS", M_ReadThis, 'r'},
    {1, "M_QUITG", M_QuitDOOM, 'q'}
};

menu_t MainDef = {main_end, NULL, MainMenu, M_DrawMainMenu, 97, 64, 0};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

menuitem_t EpisodeMenu[] = {
    {1, "M_EPI1", M_Episode, 'k'},
    {1, "M_EPI2", M_Episode, 't'},
    {1, "M_EPI3", M_Episode, 'i'},
    {1, "M_EPI4", M_Episode, 't'}
};

menu_t EpiDef = {
    ep_end,        // # of menu items
    &MainDef,      // previous menu
    EpisodeMenu,   // menuitem_t ->
    M_DrawEpisode, // drawing routine ->
    48,
    63, // x,y
    ep1 // lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[] = {
    {1, "M_JKILL", M_ChooseSkill, 'i'},
    {1, "M_ROUGH", M_ChooseSkill, 'h'},
    {1, "M_HURT", M_ChooseSkill, 'h'},
    {1, "M_ULTRA", M_ChooseSkill, 'u'},
    {1, "M_NMARE", M_ChooseSkill, 'n'}
};

menu_t NewDef = {
    newg_end,      // # of menu items
    &EpiDef,       // previous menu
    NewGameMenu,   // menuitem_t ->
    M_DrawNewGame, // drawing routine ->
    48,
    63,    // x,y
    hurtme // lastOn
};


//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    detail,
    scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_end
} options_e;

menuitem_t OptionsMenu[] = {
    {1, "M_ENDGAM", M_EndGame, 'e'},
    {1, "M_MESSG", M_ChangeMessages, 'm'},
    {1, "M_DETAIL", M_ChangeDetail, 'g'},
    {2, "M_SCRNSZ", M_SizeDisplay, 's'},
    {-1, "", 0, '\0'},
    {2, "M_MSENS", M_ChangeSensitivity, 'm'},
    {-1, "", 0, '\0'},
    {1, "M_SVOL", M_Sound, 's'}
};

menu_t OptionsDef = {opt_end, &MainDef, OptionsMenu, M_DrawOptions, 60, 37, 0};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] = {{1, "", M_ReadThis2, 0}};

menu_t ReadDef1 = {read1_end, &MainDef, ReadMenu1, M_DrawReadThis1,
                   280,       185,      0};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[] = {{1, "", M_FinishReadThis, 0}};

menu_t ReadDef2 = {read2_end, &ReadDef1, ReadMenu2, M_DrawReadThis2,
                   330,       175,       0};


menuitem_t SoundMenu[] = {
    {2, "M_SFXVOL", M_SfxVol, 's'},
    {-1, "", 0, '\0'},
    {2, "M_MUSVOL", M_MusicVol, 'm'},
    {-1, "", 0, '\0'}
};

menu_t SoundDef = {sound_end, &OptionsDef, SoundMenu, M_DrawSound, 80, 64, 0};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[] = {
    {1, "", M_LoadSelect, '1'}, {1, "", M_LoadSelect, '2'},
    {1, "", M_LoadSelect, '3'}, {1, "", M_LoadSelect, '4'},
    {1, "", M_LoadSelect, '5'}, {1, "", M_LoadSelect, '6'}
};

menu_t LoadDef = {load_end, &MainDef, LoadMenu, M_DrawLoad, 80, 54, 0};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[] = {
    {1, "", M_SaveSelect, '1'}, {1, "", M_SaveSelect, '2'},
    {1, "", M_SaveSelect, '3'}, {1, "", M_SaveSelect, '4'},
    {1, "", M_SaveSelect, '5'}, {1, "", M_SaveSelect, '6'}
};

menu_t SaveDef = {load_end, &MainDef, SaveMenu, M_DrawSave, 80, 54, 0};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    FILE   *handle;
    int     i;
    char    name[256];

    for (i = 0;i < load_end;i++)
    {
        int retval;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = M_fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad() {
    int x = 72;
    int y = 28;
    patch_t* patch = W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE);
    V_DrawPatch(x, y, patch);

    x = LoadDef.x;
    for (int i = 0; i < load_end; i++) {
        y = LoadDef.y + LINEHEIGHT * i;
        M_DrawSaveLoadBorder(x, y);
        M_WriteText(x, y, savegamestrings[i]);
    }
}


//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x, int y) {
    patch_t* patch = W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE);
    V_DrawPatch(x - 8, y + 7, patch);

    patch = W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE);
    for (int i = 0; i < 24; i++) {
        V_DrawPatch(x, y + 7, patch);
        x += 8;
    }

    patch = W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE);
    V_DrawPatch(x, y + 7, patch);
}


//
// User wants to load this game
//
void M_LoadSelect(int choice) {
    char name[256];
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));

    G_LoadGame(name);
    M_ClearMenus();
}

//
// Selected from DOOM menu
//
void M_LoadGame(int choice) {
    if (netgame) {
        M_StartMessage(DEH_String(LOADNET), NULL, false);
        return;
    }
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave() {
    int x = 72;
    int y = 28;
    patch_t* patch = W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE);
    V_DrawPatch(x, y, patch);

    x = LoadDef.x;
    for (int i = 0; i < load_end; i++) {
        y = LoadDef.y + LINEHEIGHT * i;
        M_DrawSaveLoadBorder(x, y);
        M_WriteText(x, y, savegamestrings[i]);
    }

    if (saveStringEnter) {
        int i = M_StringWidth(savegamestrings[saveSlot]);
        x = LoadDef.x + i;
        y = LoadDef.y + LINEHEIGHT * saveSlot;
        M_WriteText(x, y, "_");
    }
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // map from IWAD or PWAD?
    if (W_IsIWADLump(maplumpinfo) && strcmp(savegamedir, ""))
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s", maplumpinfo->name);
    }
    else
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpinfo));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s (%s)", maplumpinfo->name,
                   wadname);
        free(wadname);
    }
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave)
        {
            SetDefaultSaveName(choice);
        }
    }
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame(int choice) {
    if (!usergame) {
        M_StartMessage(DEH_String(SAVEDEAD), NULL, false);
        return;
    }
    if (gamestate == GS_LEVEL) {
        M_SetupNextMenu(&SaveDef);
        M_ReadSaveStrings();
    }
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1() {
    inhelpscreens = true;
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
}


//
// Read This Menus - optional second page.
//
void M_DrawReadThis2() {
    inhelpscreens = true;
    // We only ever draw the second page if this is
    // gameversion == exe_doom_1_9 and gamemode == registered
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
}

void M_DrawReadThisCommercial() {
    inhelpscreens = true;
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP"), PU_CACHE));
}


//
// Change Sfx & Music volumes
//
void M_DrawSound() {
    int x = 60;
    int y = 38;
    patch_t* patch = W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE);
    V_DrawPatch(x, y, patch);

    x = SoundDef.x;
    y = SoundDef.y + LINEHEIGHT * (sfx_vol + 1);
    int therm_width = 16;
    M_DrawThermo(x, y, therm_width, sfxVolume);

    y = SoundDef.y + LINEHEIGHT * (music_vol + 1);
    M_DrawThermo(x, y, therm_width, musicVolume);
}

void M_Sound(int choice) {
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice) {
    switch (choice) {
        case 0:
            if (sfxVolume) {
                sfxVolume--;
            }
            break;
        case 1:
            if (sfxVolume < 15) {
                sfxVolume++;
            }
            break;
        default:
            break;
    }
    S_SetSfxVolume(sfxVolume * 8);
}

void M_MusicVol(int choice) {
    switch (choice) {
        case 0:
            if (musicVolume) {
                musicVolume--;
            }
            break;
        case 1:
            if (musicVolume < 15) {
                musicVolume++;
            }
            break;
        default:
            break;
    }
    S_SetMusicVolume(musicVolume * 8);
}


//
// M_DrawMainMenu
//
void M_DrawMainMenu() {
    int x = 94;
    int y = 2;
    patch_t* patch = W_CacheLumpName(DEH_String("M_DOOM"), PU_CACHE);
    V_DrawPatch(x, y, patch);
}


//
// M_NewGame
//
void M_DrawNewGame() {
    int x = 96;
    int y = 14;
    patch_t* patch = W_CacheLumpName(DEH_String("M_NEWG"), PU_CACHE);
    V_DrawPatch(x, y, patch);

    x = 54;
    y = 38;
    patch = W_CacheLumpName(DEH_String("M_SKILL"), PU_CACHE);
    V_DrawPatch(x, y, patch);
}

void M_NewGame(int choice) {
    if (netgame && !demoplayback) {
        M_StartMessage(DEH_String(NEWGAME), NULL, false);
        return;
    }
    // Chex Quest disabled the episode select screen, as did Doom II.
    if (gamemode == commercial || gameversion == exe_chex) {
        M_SetupNextMenu(&NewDef);
    } else {
        M_SetupNextMenu(&EpiDef);
    }
}


//
//      M_Episode
//
static int epi;

void M_DrawEpisode() {
    int x = 54;
    int y = 38;
    patch_t* patch = W_CacheLumpName(DEH_String("M_EPISOD"), PU_CACHE);
    V_DrawPatch(x, y, patch);
}

void M_VerifyNightmare(int key) {
    if (key != key_menu_confirm) {
        return;
    }
    G_DeferedInitNew(nightmare, epi + 1, 1);
    M_ClearMenus();
}

void M_ChooseSkill(int choice) {
    if (choice == nightmare) {
        M_StartMessage(DEH_String(NIGHTMARE), &M_VerifyNightmare, true);
        return;
    }
    G_DeferedInitNew(choice, epi + 1, 1);
    M_ClearMenus();
}

void M_Episode(int choice) {
    if (gamemode == shareware && choice) {
        M_StartMessage(DEH_String(SWSTRING), NULL, false);
        M_SetupNextMenu(&ReadDef1);
        return;
    }
    epi = choice;
    M_SetupNextMenu(&NewDef);
}


//
// M_Options
//
static const char* detailNames[2] = {"M_GDHIGH","M_GDLOW"};
static const char* msgNames[2] = {"M_MSGOFF","M_MSGON"};

void M_DrawOptions(void)
{
    V_DrawPatch(108, 15, W_CacheLumpName(DEH_String("M_OPTTTL"),
                                               PU_CACHE));
	
    V_DrawPatch(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		      W_CacheLumpName(DEH_String(detailNames[detailLevel]),
			              PU_CACHE));

    V_DrawPatch(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                      W_CacheLumpName(DEH_String(msgNames[showMessages]),
                                      PU_CACHE));

    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,screenSize);
}

void M_Options(int choice) {
    M_SetupNextMenu(&OptionsDef);
}


//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;
	
    if (!showMessages)
	players[consoleplayer].message = DEH_String(MSGOFF);
    else
	players[consoleplayer].message = DEH_String(MSGON);

    message_dontfuckwithme = true;
}


//
// M_EndGame
//
void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(DEH_String(NETEND),NULL,false);
	return;
    }
	
    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
int quitsounds[8] = {sfx_pldeth, sfx_dmpain, sfx_popain, sfx_slop,
                     sfx_telept, sfx_posit1, sfx_posit3, sfx_sgtatk};

int quitsounds2[8] = {sfx_vilact, sfx_getpow, sfx_boscub, sfx_slop,
                      sfx_skeswg, sfx_kntdth, sfx_bspact, sfx_sgtatk};

static void M_DoExitSound() {
    int quit_sound;
    if (gamemode == commercial) {
        quit_sound = quitsounds2[(gametic >> 2) & 7];
    } else {
        quit_sound = quitsounds[(gametic >> 2) & 7];
    }
    S_StartSound(NULL, quit_sound);
    I_WaitVBL(105);
}

void M_QuitResponse(int key) {
    if (key != key_menu_confirm) {
        return;
    }
    if (!netgame) {
        M_DoExitSound();
    }
    I_Quit();
}


static const char *M_SelectEndMessage(void)
{
    const char **endmsg;

    if (logical_gamemission == doom)
    {
        // Doom 1

        endmsg = doom1_endmsg;
    }
    else
    {
        // Doom 2
        
        endmsg = doom2_endmsg;
    }

    return endmsg[gametic % NUM_QUITMESSAGES];
}


void M_QuitDOOM(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY,
                 DEH_String(M_SelectEndMessage()));

    M_StartMessage(endstring,M_QuitResponse,true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}


void M_ChangeDetail(int choice){
    detailLevel = detailLevel ^ 1;
    R_SetViewSize(screenblocks, detailLevel);
    if (detailLevel) {
        players[consoleplayer].message = DEH_String(DETAILLO);
    } else {
        players[consoleplayer].message = DEH_String(DETAILHI);
    }
}


void M_SizeDisplay(int choice) {
    switch (choice) {
        case 0:
            if (screenSize > 0) {
                screenblocks--;
                screenSize--;
            }
            break;
        case 1:
            if (screenSize < 8) {
                screenblocks++;
                screenSize++;
            }
            break;
        default:
            break;
    }
    R_SetViewSize(screenblocks, detailLevel);
}


//
//      Menu Functions
//
void M_DrawThermo(int x, int y, int thermWidth, int thermDot) {
    int xx = x;
    patch_t* patch = W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE);
    V_DrawPatch(xx, y, patch);

    xx += 8;
    patch = W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE);
    for (int i = 0; i < thermWidth; i++) {
        V_DrawPatch(xx, y, patch);
        xx += 8;
    }

    patch = W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE);
    V_DrawPatch(xx, y, patch);

    xx = (x + 8) + thermDot * 8;
    patch = W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE);
    V_DrawPatch(xx, y, patch);
}


void M_StartMessage(const char* string, void* routine, bool input) {
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
}


//
// Find string width from hu_font chars
//
int M_StringWidth(const char* string) {
    int w = 0;
    for (size_t i = 0; i < strlen(string); i++) {
        int c = toupper(string[i]) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE) {
            w += 4;
        } else {
            w += SHORT(hu_font[c]->width);
        }
    }
    return w;
}


//
// Find string height from hu_font chars.
//
int M_StringHeight(const char* string) {
    int height = SHORT(hu_font[0]->height);
    int h = height;
    for (size_t i = 0; i < strlen(string); i++) {
        if (string[i] == '\n') {
            h += height;
        }
    }
    return h;
}


//
// Write a string using the hu_font.
//
void M_WriteText(int x, int y, const char* string) {
    const char* ch = string;
    int cx = x;
    int cy = y;

    for (int i = 0;; i++) {
        int c = (int) ch[i];
        if (!c) {
            break;
        }
        if (c == '\n') {
            cx = x;
            cy += 12;
            continue;
        }
        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE) {
            cx += 4;
            continue;
        }

        int w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH) {
            break;
        }
        V_DrawPatch(cx, cy, hu_font[c]);
        cx += w;
    }
}

//
// CONTROL PANEL
//


//
// M_StartControlPanel
//
void M_StartControlPanel() {
    if (menuactive) {
        // Intro might call this repeatedly.
        return;
    }
    menuactive = true;
    currentMenu = &MainDef;
    itemOn = currentMenu->lastOn;
}

// Display OPL debug messages - hack for GENMIDI development.

static void M_DrawOPLDev(void)
{
    char debug[1024];
    char *curr, *p;
    int line;

    I_OPL_DevMessages(debug, sizeof(debug));
    curr = debug;
    line = 0;

    for (;;)
    {
        p = strchr(curr, '\n');

        if (p != NULL)
        {
            *p = '\0';
        }

        M_WriteText(0, line * 8, curr);
        ++line;

        if (p == NULL)
        {
            break;
        }

        curr = p + 1;
    }
}

static void M_DrawSkull(int x) {
    int patch_x = x + SKULLXOFF;
    int patch_y = currentMenu->y - 5 + itemOn * LINEHEIGHT;
    const char* patch_name = DEH_String(skullName[whichSkull]);
    patch_t* patch = W_CacheLumpName(patch_name, PU_CACHE);

    V_DrawPatch(patch_x, patch_y, patch);
}

//
// M_Drawer
// Called after the view has been rendered, but before it has been blitted.
//
void M_Drawer() {
    static short x;
    static short y;

    unsigned int i;
    unsigned int max;
    char string[80];
    const char *name;
    int start;

    inhelpscreens = false;

    // Horiz. & Vertically center string and print it.
    if (messageToPrint) {
        start = 0;
        y = SCREENHEIGHT / 2 - M_StringHeight(messageString) / 2;

        while (messageString[start] != '\0') {
            bool foundnewline = false;

            for (i = 0; messageString[start + i] != '\0'; i++) {
                if (messageString[start + i] == '\n') {
                    M_StringCopy(string, messageString + start, sizeof(string));
                    if (i < sizeof(string)) {
                        string[i] = '\0';
                    }
                    foundnewline = true;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline) {
                M_StringCopy(string, messageString + start, sizeof(string));
                start += strlen(string);
            }

            x = SCREENWIDTH / 2 - M_StringWidth(string) / 2;
            M_WriteText(x, y, string);
            y += SHORT(hu_font[0]->height);
        }

        return;
    }

    if (opldev) {
        M_DrawOPLDev();
    }

    if (!menuactive) {
        return;
    }

    if (currentMenu->routine) {
        // Call draw routine.
        currentMenu->routine();
    }

    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i = 0; i < max; i++) {
        name = DEH_String(currentMenu->menuitems[i].name);
        if (name[0] && W_CheckNumForName(name) > 0) {
            V_DrawPatch(x, y, W_CacheLumpName(name, PU_CACHE));
        }
        y += LINEHEIGHT;
    }

    M_DrawSkull(x);
}


//
// M_ClearMenus
//
void M_ClearMenus() {
    menuactive = false;
}


//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t* menudef) {
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker() {
    skullAnimCounter--;
    if (skullAnimCounter <= 0) {
        whichSkull ^= 1;
        skullAnimCounter = 8;
    }
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = false;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    // The same hacks were used in the original Doom EXEs.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
    }

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate)
    {
        EpiDef.numitems--;
    }
    // chex.exe shows only one episode.
    else if (gameversion == exe_chex)
    {
        EpiDef.numitems = 1;
    }

    opldev = M_CheckParm("-opldev") > 0;
}

