/*
	joy.c

	Joystick input interface

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"

#include "compat.h"

#define JOY_MAX_AXES	6
#define JOY_MAX_BUTTONS 16

cvar_t     *joy_device;					// Joystick device name
cvar_t     *joy_enable;					// Joystick enabling flag
cvar_t     *joy_amp;					// Joystick amplification
cvar_t     *joy_pre_amp;				// Joystick pre-amplification


qboolean    joy_found = false;
qboolean    joy_active = false;

typedef struct {
	char       *name;
	char       *string;
} ocvar_t;

ocvar_t joy_axes_cvar_init[JOY_MAX_AXES] = {
	{"joyaxis1", "1"},
	{"joyaxis2", "2"},
	{"joyaxis3", "3"},
	{"joyaxis4", "0"},
	{"joyaxis5", "0"},
	{"joyaxis6", "0"}
};

struct joy_axis joy_axes[JOY_MAX_AXES];
struct joy_button joy_buttons[JOY_MAX_BUTTONS];


void
JOY_Command (void)
{
	JOY_Read ();
}

void
JOY_Move (void)
{
	float       mult_joy;
	int         i;

	if (!joy_active || !joy_enable->int_val)
		return;

	Cvar_SetValue (joy_amp, max (0.0001, joy_amp->value));
	Cvar_SetValue (joy_pre_amp, max (0.0001, joy_pre_amp->value));

	mult_joy = 4 * joy_amp->value * joy_pre_amp->value *
		in_amp->value * in_pre_amp->value;
	// Yes, mult_joy looks like a mess, but use of pre_amp values is useful in
	// scripts, and *_pre_amp will matter once joystick filtering/acceleration
	// is implemented

	for (i = 0; i < JOY_MAX_AXES; i++) {
		switch (joy_axes[i].axis->int_val) {
			case 1:
				viewdelta.angles[YAW] -=
					(float) (joy_axes[i].current /
						 (201 - mult_joy));
				break;
			case 2:
				viewdelta.position[2] -=
					(float) (joy_axes[i].current /
						  (201 - mult_joy));
				break;
			case 3:
				viewdelta.position[0] +=
					(float) (joy_axes[i].current /
						 (201 - mult_joy));
				break;
			case 4:
				if (joy_axes[i].current) {
					viewdelta.angles[PITCH] -=
						(float) (joy_axes[i].current /
							 (201 - mult_joy));
				}
				break;
		}
	}
}

void
JOY_Init (void)
{
	int     i;

	if (JOY_Open () == -1) {
		Con_Printf ("JOY: Joystick not found.\n");
		joy_found = false;
		joy_active = false;
		return;
	}

	joy_found = true;

	if (!joy_enable->int_val) {
		Con_Printf ("JOY: Joystick found, but not enabled.\n");
		joy_active = false;
		JOY_Close ();
	}

	Con_Printf ("JOY: Joystick found and activated.\n");

	// Initialize joystick if found and enabled
	for (i = 0; i < JOY_MAX_BUTTONS; i++) {
		joy_buttons[i].old = 0;
		joy_buttons[i].current = 0;
	}   
	joy_active = true;
}

void
JOY_Init_Cvars (void)
{
	int         i;

	joy_device = Cvar_Get ("joy_device", "/dev/js0", CVAR_NONE | CVAR_ROM, 0,
						   "Joystick device");
	joy_enable = Cvar_Get ("joy_enable", "1", CVAR_NONE | CVAR_ARCHIVE, 0,
						   "Joystick enable flag");
	joy_amp = Cvar_Get ("joy_amp", "1", CVAR_NONE | CVAR_ARCHIVE, 0,
						"Joystick amplification");
	joy_pre_amp = Cvar_Get ("joy_pre_amp", "1", CVAR_NONE | CVAR_ARCHIVE, 0,
							"Joystick pre-amplification");

	for (i = 0; i < JOY_MAX_AXES; i++) {
		joy_axes[i].axis = Cvar_Get (joy_axes_cvar_init[i].name,
									 joy_axes_cvar_init[i].string,
									 CVAR_ARCHIVE, 0, "Set joystick axes");
	}
}

void
JOY_Shutdown (void)
{
	if (!joy_active)
		return;

	JOY_Close ();

	joy_active = false;
	joy_found = false;
}
