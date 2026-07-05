/*
	in_axis_button.h

	Multi-stage analog axis to button conversion

	Copyright (C) 1996-1997 Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2012-2026 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_input_axis_button_h
#define __QF_input_axis_button_h

#include <QF/qtypes.h>

typedef struct in_ab_state_s {
	int         threshold;
	int         button;
	int         state;
} in_ab_state_t;

typedef struct in_axis_button_s {
	int         devid;
	int         num_buttons;
	in_ab_state_t *buttons;
} in_axis_button_t;

int IN_AxisButton_Check (in_axis_button_t *axis_button, int value)
	__attribute__((pure));
int IN_AxisButton_Test (in_axis_button_t *axis_button, int value, int button)
	__attribute__((pure));
void IN_AxisButton_Event (in_axis_button_t *axis_button, int value);
in_axis_button_t *IN_AxisButton_Create (int devid, int num_buttons,
										in_ab_state_t *buttons);

#endif//__QF_input_axis_button_h
