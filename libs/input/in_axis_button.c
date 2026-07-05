/*
	in_axis_button.c

	Multi-stage analog axis to button conversion

	Copyright (C) 2000 David Jeffery
	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2001 Ragnvald `Despair` Maartmann-Moe IV
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>
#include <stdlib.h>

#include "QF/sys.h"

#include "QF/input/axis_button.h"
#include "QF/input/event.h"

static void
ab_send_event (uint64_t when, int devid, in_ab_state_t *state)
{
	IE_event_t event = {
		.when = when,
		.button = {
			.data = nullptr,
			.devid = devid,
			.button = state->button,
			.state = state->state,
		},
	};
	IE_Send_Event (&event);
}

int
IN_AxisButton_Check (in_axis_button_t *axis_button, int value)
{
	int         pressed = -1;

	// the axis button list is sorted in decending order of absolute threshold
	for (int i = 0; i < axis_button->num_buttons; i++) {
		auto ab = &axis_button->buttons[i];
		if ((value < 0) == (ab->threshold < 0)
			&& abs (value) >= abs (ab->threshold)) {
			pressed = i;
			break;

		}
	}
	return pressed;
}

void
IN_AxisButton_Event (in_axis_button_t *axis_button, int value)
{
	int pressed = IN_AxisButton_Check (axis_button, value);
	// make sure any buttons that are no longer active are "released"
	for (int i = 0; i < axis_button->num_buttons; i++) {
		if (i == pressed)
			continue;
		auto ab = &axis_button->buttons[i];
		if (ab->state) {
			ab->state = 0;
			ab_send_event (Sys_LongTime (), axis_button->devid, ab);
		}
	}
	// press the active button if there is one
	if (pressed >= 0) {
		// FIXME support repeat?
		auto ab = &axis_button->buttons[pressed];
		if (!ab->state) {
			ab->state = 1;
			ab_send_event (Sys_LongTime (), axis_button->devid, ab);
		}
	}
}

int
IN_AxisButton_Test (in_axis_button_t *axis_button, int value, int button)
{
	for (int i = 0; i < axis_button->num_buttons; i++) {
		auto ab = &axis_button->buttons[i];
		if (ab->button == button) {
			return ab->state;
		}
	}
	return 0;
}

in_axis_button_t *
IN_AxisButton_Create (int devid, int num_buttons, in_ab_state_t *buttons)
{
	size_t size = sizeof (in_axis_button_t)
				+ sizeof (in_ab_state_t[num_buttons]);
	in_axis_button_t *axis_button = malloc (size);
	*axis_button = (in_axis_button_t) {
		.devid = devid,
		.num_buttons = num_buttons,
		.buttons = (in_ab_state_t *) &axis_button[1],
	};
	for (int i = 0; i < num_buttons; i++) {
		axis_button->buttons[i] = buttons[i];
	}
	auto ab = axis_button->buttons;
	for (int i = 0; i < num_buttons - 1; i++) {
		for (int j = i + 1; j < num_buttons; j++) {
			if (abs (ab[i].threshold) < abs (ab[j].threshold)) {
				auto t = ab[i];
				ab[i] = ab[j];
				ab[j] = t;
			}
		}
	}
	return axis_button;
}
