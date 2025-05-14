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
//    Exit text-mode ENDOOM screen.
//


#include "i_endoom.h"

#include "deh_str.h"
#include "txt_main.h"
#include "w_wad.h"
#include "z_zone.h"

#define ENDOOM_W 80
#define ENDOOM_H 25


static void I_ShutDownTextScreen() {
    TXT_Shutdown();
}

static void I_WaitKeyPress() {
    while (true) {
        TXT_UpdateScreen();
        if (TXT_GetChar() > 0) {
            break;
        }
        TXT_Sleep(0);
    }
}

//
// Write the data to the screen memory
//
static void I_CopyEndoomToScreen() {
    const char* lump_name = DEH_String("ENDOOM");
    const byte* endoom_data = W_CacheLumpName(lump_name, PU_STATIC);

    byte* screendata = TXT_GetScreenData();
    int indent = (ENDOOM_W - TXT_SCREEN_W) / 2;
    size_t size = TXT_SCREEN_W * 2;

    for (int y = 0; y < TXT_SCREEN_H; y++) {
        byte* dest = screendata + (y * TXT_SCREEN_W * 2);
        const byte* src = endoom_data + (y * ENDOOM_W + indent) * 2;
        memcpy(dest, src, size);
    }
}

//
// Set up text mode screen.
//
static void I_InitTextScreen() {
    TXT_Init();
    TXT_SetWindowTitle(PACKAGE_STRING);
}

// 
// Displays the text mode ending screen after the game quits
//
void I_Endoom() {
    I_InitTextScreen();
    I_CopyEndoomToScreen();
    I_WaitKeyPress();
    I_ShutDownTextScreen();
}

