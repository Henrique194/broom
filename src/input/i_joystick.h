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
//      System-specific joystick interface.
//


#ifndef __I_JOYSTICK__
#define __I_JOYSTICK__

#include "SDL_gamecontroller.h"

// Number of "virtual" joystick buttons defined in configuration files.
// This needs to be at least as large as the number of different key
// bindings supported by the higher-level game code (joyb* variables).
#define NUM_VIRTUAL_BUTTONS 11

// Max allowed number of virtual mappings. Chosen to be less than joybspeed
// autorun value.
#define MAX_VIRTUAL_BUTTONS 20

// If this bit is set in a configuration file axis value, the axis is
// not actually a joystick axis, but instead is a "button axis". This
// means that instead of reading an SDL joystick axis, we read the
// state of two buttons to get the axis value. This is needed for eg.
// the PS3 SIXAXIS controller, where the D-pad buttons register as
// buttons, not as two axes.
#define BUTTON_AXIS 0x10000

// Query whether a given axis value describes a button axis.
#define IS_BUTTON_AXIS(axis) ((axis) >= 0 && ((axis) & BUTTON_AXIS) != 0)

// Get the individual buttons from a button axis value.
#define BUTTON_AXIS_NEG(axis)  ((axis) & 0xff)
#define BUTTON_AXIS_POS(axis)  (((axis) >> 8) & 0xff)

// If this bit is set in an axis value, the axis is not actually a
// joystick axis, but is a "hat" axis. This means that we read (one of)
// the hats on the joystick.
#define HAT_AXIS    0x20000

#define IS_HAT_AXIS(axis) ((axis) >= 0 && ((axis) & HAT_AXIS) != 0)

// Get the hat number from a hat axis value.
#define HAT_AXIS_HAT(axis)         ((axis) & 0xff)
// Which axis of the hat? (horizonal or vertical)
#define HAT_AXIS_DIRECTION(axis)   (((axis) >> 8) & 0xff)

#define HAT_AXIS_HORIZONTAL 1
#define HAT_AXIS_VERTICAL   2

// When a trigger reads greater than this, consider it to be pressed.  30 comes
// from XINPUT_GAMEPAD_TRIGGER_THRESHOLD in xinput.h, and is scaled here for
// the SDL_GameController trigger max value.
#define TRIGGER_THRESHOLD (30 * 32767 / 255)

// To be used with SDL_JoystickGetGUIDString; see SDL_joystick.h
#define GUID_STRING_BUF_SIZE 33

// Extend the SDL_GameControllerButton enum to include the triggers.
enum
{
    GAMEPAD_BUTTON_TRIGGERLEFT = SDL_CONTROLLER_BUTTON_MAX,
    GAMEPAD_BUTTON_TRIGGERRIGHT,
    GAMEPAD_BUTTON_MAX
};

void I_InitJoystick(void);
void I_ShutdownJoystick(void);
void I_UpdateJoystick(void);

void I_BindJoystickVariables(void);

#endif /* #ifndef __I_JOYSTICK__ */

