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
#include "QF/joystick.h"	// needed for JOY_MAX_AXES

typedef struct {
	float			x, y;
	unsigned int	buttons;
} IE_mouse_event_t;

typedef struct {
	int				key_code;
	qboolean		pressed;
} IE_key_event_t;

typedef struct {
	float			axis[JOY_MAX_AXES];
	unsigned int	buttons;
} IE_joystick_event_t;

typedef enum {
	ie_none,
	ie_gain_focus,
	ie_lose_focus,
	ie_mouse,
	ie_key,
	ie_joystick,
} IE_event_type;

typedef struct {
	IE_event_type	type;
	union {
		IE_mouse_event_t	mouse;
		IE_key_event_t		key;
		IE_joystick_event_t	joystick;
	} e;
} IE_event_t;

void IE_Init (void);
void IE_Init_Cvars (void);
void IE_Shutdown (void);
int IE_Send_Event (const IE_event_t *event);
int IE_Add_Handler (int (*event_handler)(const IE_event_t*));
void IE_Remove_Handler (int handle);
void IE_Set_Focus (int handle);

#endif//__QF_in_event_h
