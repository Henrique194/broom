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
//      Timer functions.
//

#include "SDL.h"
#include "i_timer.h"

//
// Returns time in milliseconds corresponding to “now”.
//
static Uint32 I_NowMS() {
    static Uint32 start_time = 0;
    Uint32 millis = SDL_GetTicks();
    if (start_time == 0) {
        start_time = millis;
    }
    return millis - start_time;
}

//
// Returns time in 1/35th second tics.
//
int I_GetTime() {
    Uint32 now = I_NowMS();
    Uint32 ticks = (now * TICRATE) / 1000;
    return (int) ticks;
}

//
// Same as I_GetTime, but returns time in milliseconds
//
int I_GetTimeMS() {
    return (int) I_NowMS();
}

//
// Sleep for a specified number of ms
//
void I_Sleep(int ms) {
    SDL_Delay(ms);
}

void I_WaitVBL(int count) {
    I_Sleep((count * 1000) / 70);
}

void I_InitTimer(void) {
    // initialize timer
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
    SDL_Init(SDL_INIT_TIMER);
}
