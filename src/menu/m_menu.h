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
//   Menu widget stuff, episode selection and such.
//    


#ifndef __M_MENU__
#define __M_MENU__


#include "d_event.h"
#include "p_saveg.h"
#include "m_menu_responder.h"


//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short status;

    char name[10];

    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void (*routine)(int choice);

    // hotkey in menu
    char alphaKey;
} menuitem_t;


typedef struct menu_s
{
    short numitems;          // # of menu items
    struct menu_s *prevMenu; // previous menu
    menuitem_t *menuitems;   // menu items
    void (*routine)();       // draw routine
    short x;
    short y;      // x,y of menu
    short lastOn; // last item user was on in menu
} menu_t;

//
// SOUND VOLUME MENU
//
typedef enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;


//
// MENUS
//

// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer (void);

// Called by D_DoomMain,
// loads the config file.
void M_Init (void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel (void);

//
// PROTOTYPES
//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_ChangeMessages(int choice);
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_MusicVol(int choice);
void M_ChangeDetail(int choice);
void M_SizeDisplay(int choice);
void M_Sound(int choice);

void M_FinishReadThis(int choice);
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_DrawSaveLoadBorder(int x, int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawThermo(int x, int y, int thermWidth, int thermDot);
void M_WriteText(int x, int y, const char *string);
int M_StringWidth(const char *string);
int M_StringHeight(const char *string);
void M_StartMessage(const char *string, void *routine, bool input);
void M_ClearMenus(void);

void M_QuitResponse(int key);


extern int detailLevel;
extern int screenblocks;

extern bool inhelpscreens;
extern int showMessages;

extern int messageToPrint;

// message x & y
extern bool messageLastMenuActive;

// timed message = no input from user
extern bool messageNeedsInput;

extern void (*messageRoutine)(int response);

// we are going to be entering a savegame string
extern int saveStringEnter;
extern int saveSlot;      // which slot to save in
extern int saveCharIndex; // which char we're editing
// old save description before edit
extern char saveOldString[SAVESTRINGSIZE];

extern char savegamestrings[10][SAVESTRINGSIZE];

extern short itemOn;           // menu item skull is on
extern short skullAnimCounter; // skull animation counter
extern short whichSkull;       // which skull to draw

// current menudef
extern menu_t *currentMenu;

extern bool joypadSave;

extern menu_t SoundDef;
extern menu_t SaveDef;
extern menu_t ReadDef1;
extern menu_t ReadDef2;
extern int quickSaveSlot;


#endif
