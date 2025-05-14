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


#ifndef __M_MENU_RESPONDER__
#define __M_MENU_RESPONDER__

#include "d_event.h"


// Called by main loop, saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed, this can resize the view and change
// game parameters. Does all the real work of the menu interaction.
bool M_Responder(const event_t* ev);

#endif
