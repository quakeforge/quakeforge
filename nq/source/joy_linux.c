
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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>

#include "console.h"
#include "client.h"
#include "cvar.h"
#include "keys.h"
#include "protocol.h"
#include "view.h"

#define JOY_MAX_AXES	6
#define JOY_MAX_BUTTONS 16

cvar_t     *joy_device;					// Joystick device name
cvar_t     *joy_enable;					// Joystick enabling flag
cvar_t     *joy_sensitivity;			// Joystick sensitivity

qboolean    joy_found = false;
qboolean    joy_active = false;

// Variables and structures for this driver
int         joy_handle;

typedef struct {
	char       *name;
	char       *string;
} ocvar_t;

struct joy_axis {
	cvar_t     *axis;
	ocvar_t     var;
	int         current;
};

struct joy_button {
	int         old;
	int         current;
};

struct joy_axis joy_axes[JOY_MAX_AXES] = {
	{NULL, {"joyaxis1", "1"}, 0},
	{NULL, {"joyaxis2", "2"}, 0},
	{NULL, {"joyaxis3", "3"}, 0},
	{NULL, {"joyaxis4", "0"}, 0},
	{NULL, {"joyaxis5", "0"}, 0},
	{NULL, {"joyaxis6", "0"}, 0}
};

struct joy_button joy_buttons[JOY_MAX_BUTTONS];

void
JOY_Command (void)
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
				Key_Event (K_AUX1 + event.number, 0, true);
			} else {
				if (joy_buttons[event.number].current <
					joy_buttons[event.number].old) {
					Key_Event (K_AUX1 + event.number, 0, false);
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

void
JOY_Move (usercmd_t *cmd)
{
	int         i;

	if (!joy_active || !joy_enable->int_val)
		return;

	Cvar_SetValue (joy_sensitivity, bound (1, joy_sensitivity->value, 25));
	for (i = 0; i < JOY_MAX_AXES; i++) {
		switch (joy_axes[i].axis->int_val) {
			case 1:
			cl.viewangles[YAW] -=
				m_yaw->value * (float) (joy_axes[i].current /
										(201 - (joy_sensitivity->value * 4)));
			break;
			case 2:
			cmd->forwardmove -=
				m_forward->value * (float) (joy_axes[i].current /
											(201 -
											 (joy_sensitivity->value * 4)));
			break;
			case 3:
			cmd->sidemove +=
				m_side->value * (float) (joy_axes[i].current /
										 (201 - (joy_sensitivity->value * 4)));
			break;
			case 4:
			if (joy_axes[i].current) {
				V_StopPitchDrift ();
				cl.viewangles[PITCH] -=
					m_pitch->value * (float) (joy_axes[i].current /
											  (201 -
											   (joy_sensitivity->value * 4)));
				cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
			}
			break;
		}
	}
}

void
JOY_Init (void)
{
	// Open joystick device
	joy_handle = open (joy_device->string, O_RDONLY | O_NONBLOCK);
	if (joy_handle < 0) {
		Con_Printf ("JOY: Joystick not found.\n");
	} else {
		int         i;

		joy_found = true;

		if (!joy_enable->int_val) {
			Con_Printf ("JOY: Joystick found, but not enabled.\n");
			i = close (joy_handle);
			if (i) {
				Con_Printf ("JOY: Failed to close joystick device!\n");
			}
		} else {
			// Initialize joystick if found and enabled
			for (i = 0; i < JOY_MAX_BUTTONS; i++) {
				joy_buttons[i].old = 0;
				joy_buttons[i].current = 0;
			}
			joy_active = true;
			Con_Printf ("JOY: Joystick found and activated.\n");
		}
	}
}

void
JOY_Init_Cvars (void)
{
	int         i;

	joy_device =
		Cvar_Get ("joy_device", "/dev/js0", CVAR_NONE | CVAR_ROM,
				  "Joystick device");
	joy_enable =
		Cvar_Get ("joy_enable", "1", CVAR_NONE | CVAR_ARCHIVE,
				  "Joystick enable flag");
	joy_sensitivity =
		Cvar_Get ("joy_sensitivity", "1", CVAR_NONE | CVAR_ARCHIVE,
				  "Joystick sensitivity");

	for (i = 0; i < JOY_MAX_AXES; i++) {
		joy_axes[i].axis = Cvar_Get (joy_axes[i].var.name,
									 joy_axes[i].var.string,
									 CVAR_ARCHIVE, "Set joystick axes");
	}
}

void
JOY_Shutdown (void)
{
	int         i;

	if (!joy_active)
		return;

	i = close (joy_handle);
	if (i) {
		Con_Printf ("JOY: Failed to close joystick device!\n");
	} else {
		Con_Printf ("JOY_Shutdown\n");
	}
	joy_active = false;
	joy_found = false;
}
