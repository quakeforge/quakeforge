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

	$Id$
*/

#ifndef __QF_in_event_h
#define __QF_in_event_h

#include "QF/qtypes.h"
#include "QF/joystick.h"	// needed for JOY_MAX_AXES

struct ie_event_s;

typedef void (*ie_handler) (struct ie_event_s *event, float value);

typedef struct ie_event_s {
	union {
		void *p;
		int i;
		float f;
	} data;
	ie_handler handler;
} ie_event_t;

typedef struct ie_threshold_data_s {
	float threshold;
	float time;
	struct ie_timevaluepair_s *history;
	int history_count;
	ie_event_t *nextevent;
	ie_handler handler;
} ie_threshold_data_t;

typedef struct ie_timevaluepair_s {
	float time;
	float value;
} ie_timevaluepair_t;

typedef struct ie_translation_table_s {
	int maxevents;
	ie_event_t *events;
} ie_translation_table_t;

typedef struct ie_translation_index_s {
	// FIXME: I don't like this organization
	struct ie_translation_index_s *next;
	ie_translation_table_t *table;
} ie_translation_index_t;

typedef struct ie_translation_data_s {
	int offset;
	ie_translation_index_t *index;
} ie_translation_data_t;

typedef struct ie_multiply_data_s {
	float multiplier;
	ie_event_t *nextevent;
	ie_handler handler;
} ie_multiplier_data_t;


void IE_Threshold_Event (ie_event_t *event, float value);
void IE_Translation_Event (ie_event_t *event, float value);
void IE_Multiplier_Event (ie_event_t *event, float value);

void IE_CallHandler (ie_handler handler, ie_event_t *event, float value);


/*
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
*/
void IE_Init (void);
void IE_Init_Cvars (void);
void IE_Shutdown (void);
/*
int IE_Send_Event (const IE_event_t *event);
int IE_Add_Handler (int (*event_handler)(const IE_event_t*));
void IE_Remove_Handler (int handle);
void IE_Set_Focus (int handle);
*/

#endif//__QF_in_event_h
