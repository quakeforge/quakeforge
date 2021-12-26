/*
	joy_linux.c

	Joystick driver for Linux

	Copyright (C) 2000 David Jeffery
	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>

#include "QF/cvar.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

// Variables and structures for this driver
int         joy_handle;


void
JOY_Read (void)
{
	struct js_event event;

	if (!joy_active || !joy_enable->int_val)
		return;

	while (read (joy_handle, &event, sizeof (struct js_event)) > -1) {
		if (event.type & JS_EVENT_BUTTON) {
			if (event.number >= JOY_MAX_BUTTONS)
				continue;

			joy_buttons[event.number].current = event.value;

			if (joy_buttons[event.number].current >
				joy_buttons[event.number].old) {
				Key_Event (QFJ_BUTTON1 + event.number, 0, true);
			} else {
				if (joy_buttons[event.number].current <
					joy_buttons[event.number].old) {
					Key_Event (QFJ_BUTTON1 + event.number, 0, false);
				}
			}
			joy_buttons[event.number].old = joy_buttons[event.number].current;
		} else {
			if (event.type & JS_EVENT_AXIS) {
				if (event.number >= JOY_MAX_AXES)
					continue;
				joy_axes[event.number].current = event.value;
			}
		}
	}
}

int
JOY_Open (void)
{
	// Open joystick device
	joy_handle = open (joy_device->string, O_RDONLY | O_NONBLOCK);
	if (joy_handle < 0) {
		return -1;
	}
	return 0;
}

void
JOY_Close (void)
{
	int         i;

	i = close (joy_handle);
	if (i) {
		Sys_MaskPrintf (SYS_vid, "JOY: Failed to close joystick device!\n");
	} else {
		Sys_MaskPrintf (SYS_vid, "JOY_Shutdown\n");
	}
}
