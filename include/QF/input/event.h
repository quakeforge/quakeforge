/*
	in_event.h

	input event handling

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/8/9

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_in_event_h
#define __QF_in_event_h

#include "QF/qtypes.h"

typedef enum {
	ies_shift = 1,
	ies_capslock = 2,
	ies_control = 4,
	ies_alt = 8,
} IE_shift;

typedef enum {
	ie_mousedown,
	ie_mouseup,
	ie_mouseclick,
	ie_mousemove,
	ie_mouseauto,
} IE_mouse_type;

typedef struct {
	IE_mouse_type type;
	unsigned    shift;	///< ored bit pattern of IE_shift
	int         x, y;
	unsigned    buttons;
	int         click;
} IE_mouse_event_t;

typedef struct {
	int         code;
	int         unicode;
	unsigned    shift;	///< ored bit pattern of IE_shift
} IE_key_event_t;

typedef struct {
	int         devid;
	int         axis;
	int         value;
} IE_axis_event_t;

typedef struct {
	int         devid;
	int         button;
	int         state;
} IE_button_event_t;

typedef struct {
	int         devid;
} IE_device_event_t;

typedef enum {
	ie_none,
	ie_gain_focus,
	ie_lose_focus,
	ie_add_device,
	ie_remove_device,
	ie_mouse,
	ie_key,
	ie_axis,
	ie_button,
} IE_event_type;

#define IE_broadcast_events (0 \
		| (1 << ie_add_device) \
		| (1 << ie_remove_device) \
	)

typedef struct {
	IE_event_type type;
	uint64_t    when;
	union {
		IE_mouse_event_t mouse;
		IE_key_event_t key;
		IE_axis_event_t axis;
		IE_button_event_t button;
		IE_device_event_t device;
	};
} IE_event_t;

typedef int ie_handler_t (const IE_event_t *, void *data);

int IE_Send_Event (const IE_event_t *event);
int IE_Add_Handler (ie_handler_t *event_handler, void *data);
void IE_Remove_Handler (int handle);
void IE_Set_Focus (int handle);

#endif//__QF_in_event_h
