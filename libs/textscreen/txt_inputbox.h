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

#ifndef TXT_INPUTBOX_H
#define TXT_INPUTBOX_H

/**
 * @file txt_inputbox.h
 *
 * Input box widget.
 */

/**
 * Input box widget.
 *
 * An input box is a widget that displays a value, which can be
 * selected to enter a new value.
 *
 * Input box widgets can be of an integer or string type.
 */

typedef struct txt_inputbox_s txt_inputbox_t;

#include "txt_widget.h"

struct txt_inputbox_s
{
    txt_widget_t widget;
    char *buffer;
    size_t buffer_len;
    unsigned int size;
    int editing;
    void *value;
};


#endif /* #ifndef TXT_INPUTBOX_H */


