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

#ifndef TXT_DESKTOP_H
#define TXT_DESKTOP_H

/**
 * @file txt_desktop.h
 *
 * Textscreen desktop.
 */

#include "txt_window.h"


void TXT_AddDesktopWindow(txt_window_t *win);
void TXT_RemoveDesktopWindow(txt_window_t *win);
void TXT_DrawDesktop(void);
void TXT_DispatchEvents(void);
void TXT_DrawWindow(txt_window_t *window);
void TXT_SetWindowFocus(txt_window_t *window, int focused);
int TXT_WindowKeyPress(txt_window_t *window, int c);

/**
 * Set the title displayed at the top of the screen.
 *
 * @param title         The title to display (UTF-8 format).
 */

void TXT_SetDesktopTitle(const char *title);

/**
 * Get the top window on the desktop that is currently receiving
 * inputs.
 *
 * @return    The active window, or NULL if no windows are present.
 */

txt_window_t *TXT_GetActiveWindow(void);

/**
 * Lower the z-position of the given window relative to other windows.
 *
 * @param window        The window to make inactive.
 * @return              Non-zero if the window was lowered successfully,
 *                      or zero if the window could not be lowered further.
 */

int TXT_LowerWindow(txt_window_t *window);

#endif /* #ifndef TXT_DESKTOP_H */

