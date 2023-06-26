/*
	joy.c

	Joystick input interface

	Copyright (C) 2000 David Jeffery
	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2001 Ragnvald `Despair` Maartmann-Moe IV

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

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/cmd.h"

#include "compat.h"
#include <string.h>

char *joy_device;
static cvar_t joy_device_cvar = {
	.name = "joy_device",
	.description =
		"Joystick device",
	.default_value = "none",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &joy_device },
};
int joy_enable;
static cvar_t joy_enable_cvar = {
	.name = "joy_enable",
	.description =
		"Joystick enable flag",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &joy_enable },
};
float joy_amp;
static cvar_t joy_amp_cvar = {
	.name = "joy_amp",
	.description =
		"Joystick amplification",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &joy_amp },
};
float joy_pre_amp;
static cvar_t joy_pre_amp_cvar = {
	.name = "joy_pre_amp",
	.description =
		"Joystick pre-amplification",
	.default_value = "0.01",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &joy_pre_amp },
};

bool        joy_found = false;
bool        joy_active = false;

struct joy_axis joy_axes[JOY_MAX_AXES];
struct joy_button joy_buttons[JOY_MAX_BUTTONS];

void
joy_clear_axis (int i)
{
	joy_axes[i].dest = js_none;
	joy_axes[i].amp = 1;
	joy_axes[i].pre_amp = 1;
	joy_axes[i].deadzone = 12500;

	joy_axes[i].num_buttons = 0;
	if (joy_axes[i].axis_buttons) {
		free (joy_axes[i].axis_buttons);
		joy_axes[i].axis_buttons = NULL;
	}
}

static void
joy_check_axis_buttons (struct joy_axis *ja, float value)
{
	struct joy_axis_button *ab;
	int         pressed = -1;
	int         i;

	// the axis button list is sorted in decending order of absolute threshold
	for (i = 0; i < ja->num_buttons; i++) {
		ab = &ja->axis_buttons[i];
		if ((value < 0) == (ab->threshold < 0)
			&& fabsf (value) >= fabsf (ab->threshold)) {
			pressed = i;
			break;

		}
	}
	// make sure any buttons that are no longer active are "released"
	for (i = 0; i < ja->num_buttons; i++) {
		if (i == pressed)
			continue;
		ab = &ja->axis_buttons[i];
		if (ab->state) {
			Key_Event (ab->key, 0, 0);
			ab->state = 0;
		}
	}
	// press the active button if there is one
	if (pressed >= 0) {
		// FIXME support repeat?
		ab = &ja->axis_buttons[pressed];
		if (!ab->state) {
			Key_Event (ab->key, 0, 1);
		}
		ab->state = 1;
	}
}

VISIBLE void
JOY_Command (void)
{
	JOY_Read ();
}

VISIBLE void
JOY_Move (void)
{
	struct joy_axis *ja;
	float       value;
	float       amp = joy_amp * in_amp;
	float       pre = joy_pre_amp * in_pre_amp;
	int         i;

	if (!joy_active || !joy_enable)
		return;

	for (i = 0; i < JOY_MAX_AXES; i++) {
		ja = &joy_axes[i];
		value = ja->current * pre * ja->pre_amp;
		if (fabs (value) < ja->deadzone)
			value = -ja->offset;
		value += ja->offset;
		value *= amp * ja->amp;
		switch (ja->dest) {
			case js_none:
				// ignore axis
				break;
			case js_position:
				if (ja->current)
					viewdelta.position[(ja->axis) ? 2 : 0] += value;
				break;
			case js_angles:
				if (ja->current)
					viewdelta.angles[(ja->axis) ? 1 : 0] -= value;
				break;
			case js_button:
				joy_check_axis_buttons (ja, value);
				break;
		}
	}
}

VISIBLE void
JOY_Init (void)
{
	int         i;

	if (JOY_Open () == -1) {
		Sys_MaskPrintf (SYS_vid, "JOY: Joystick not found.\n");
		joy_found = false;
		joy_active = false;
		return;
	}

	joy_found = true;

	if (!joy_enable) {
		Sys_MaskPrintf (SYS_vid, "JOY: Joystick found, but not enabled.\n");
		joy_active = false;
		JOY_Close ();
	}

	Sys_MaskPrintf (SYS_vid, "JOY: Joystick found and activated.\n");

	// Initialize joystick if found and enabled
	for (i = 0; i < JOY_MAX_BUTTONS; i++) {
		joy_buttons[i].old = 0;
		joy_buttons[i].current = 0;
	}
	joy_active = true;
}

static void
joyamp_f (void *data, const cvar_t *cvar)
{
	Cvar_SetVar (cvar, va (0, "%g", max (0.0001, *(float *)data)));
}

typedef struct {
	const char *name;
	js_dest_t   destnum;
} js_dests_t;

typedef struct {
	const char *name;
	js_dest_t   optnum;
} js_opts_t;

typedef struct {
	const char *name;
	int         axis;
} js_axis_t;

js_dests_t  js_dests[] = {
	{"none",		js_none},				// ignore axis
	{"movement",	js_position},			// linear delta
	{"aim",			js_angles},				// linear delta
	{"button",		js_button},				// axis button
	{0, 0}
};

js_opts_t   js_opts[] = {
	{"clear",		js_clear},
	{"amp",			js_amp},
	{"pre_amp",		js_pre_amp},
	{"deadzone",	js_deadzone},
	{"offset",		js_offset},
	{"type",		js_type},
	{"button",		js_axis_button},
	{0, 0}
};

js_axis_t   js_position_names[] = {
	{"x",	0},
	{"y",	1},
	{"z",   2},
	{0, 0}
};

js_axis_t   js_angles_names[] = {
	{"pitch",	PITCH},
	{"yaw",		YAW},
	{"roll",	ROLL},
	{"p",		PITCH},
	{"y",		YAW},
	{"r",		ROLL},
	{0, 0}
};

js_axis_t  *js_axis_names[] = {
	0,				// js_none
	js_position_names,
	js_angles_names,
	0,				// js_button
};

const char *
JOY_GetOption_c (int i)
{
	js_opts_t  *opt;

	for (opt = &js_opts[0]; opt->name; opt++) {
		if ((int) opt->optnum == i)
			return opt->name;
	}

	return NULL;
}

int
JOY_GetOption_i (const char *c)
{
	js_opts_t  *opt;

	for (opt = &js_opts[0]; opt->name; opt++) {
		if (!strcmp (opt->name, c))
			return opt->optnum;
	}

	return -1;							// Failure code;
}

const char *
JOY_GetDest_c (int i)
{
	js_dests_t *dest;

	for (dest = &js_dests[0]; dest->name; dest++) {
		if ((int) dest->destnum == i)
			return dest->name;
	}

	return NULL;
}

int
JOY_GetDest_i (const char *c)
{
	js_dests_t *dest;

	for (dest = &js_dests[0]; dest->name; dest++) {
		if (!strcmp (dest->name, c))
			return dest->destnum;
	}

	return -1;							// Failure code;
}

int
JOY_GetAxis_i (int dest, const char *c)
{
	char       *end;
	int         axis;
	js_axis_t  *axis_names;

	axis = strtol (c, &end, 10);
	if (*end || axis < 0 || axis > 2) {
		axis = -1;
		for (axis_names = js_axis_names[dest]; axis_names && axis_names->name;
			 axis_names++) {
			if (!strcasecmp (axis_names->name, c)) {
				axis = axis_names->axis;
				break;
			}
		}
	}

	return axis;
}

static void
in_joy_button_add_f (int ax, int index)
{
	int         n;
	size_t      size;
	const char *key = Cmd_Argv (index);
	int         keynum;
	const char *thrsh = Cmd_Argv (index + 1);
	float       threshold;
	char       *end = 0;

	keynum = strtol (key, &end, 10) + QFJ_AXIS1;
	if (*end || keynum < QFJ_AXIS1 || keynum > QFJ_AXIS32) {
		// if the key is not valid, try a key name
		 keynum = Key_StringToKeynum (key);
	}
	if (keynum == -1) {
		Sys_Printf ("\"%s\" isn't a valid key\n", key);
	}
	threshold = strtof (thrsh, &end);
	if (*end) {
		Sys_Printf ("invalid threshold: %s\n", thrsh);
		keynum = -1;
	}
	if (keynum == -1)
		return;

	n = joy_axes[ax].num_buttons++;
	size = joy_axes[ax].num_buttons * sizeof (struct joy_axis_button);
	joy_axes[ax].axis_buttons = realloc (joy_axes[ax].axis_buttons, size);
	joy_axes[ax].axis_buttons[n].key = keynum;
	joy_axes[ax].axis_buttons[n].threshold = threshold;
	joy_axes[ax].axis_buttons[n].state = 0;
}

static void
in_joy_f (void)
{
	const char *arg;
	int         i, ax, c = Cmd_Argc ();

	if (c == 2) {
		int         var = JOY_GetOption_i (Cmd_Argv (1));
		switch (var) {
			case js_clear:
				Sys_Printf ("Clearing all joystick settings...\n");
				for (i = 0; i < JOY_MAX_AXES; i++) {
					joy_clear_axis (i);
				}
				break;
			case js_amp:
				Sys_Printf ("[...]<amp> [<#amp>]: Axis sensitivity\n");
				break;
			case js_pre_amp:
				Sys_Printf ("[...]<pre_amp> [<#pre_amp>]: Axis sensitivity.\n");
				break;
			case js_deadzone:
				Sys_Printf ("[...]<deadzone> [<#dz>]: Axis deadzone.\n");
				break;
			case js_offset:
				Sys_Printf ("[...]<offset> [<#off>]: Axis initial position.\n");
				break;
			case js_type:
				Sys_Printf ("[...]<type> [<act> <#act>].\n");
				Sys_Printf ("Values for <act>:\n");
				Sys_Printf ("none:     #0\n");
				Sys_Printf ("aim:      #1..0\n");
				Sys_Printf ("movement: #1..0\n");

				break;
			case js_axis_button:
				/* TODO */
				break;
			default:
				ax = strtol (Cmd_Argv (1), NULL, 0);

				Sys_Printf ("<=====> AXIS %i <=====>\n", ax);
				Sys_Printf ("amp:       %.9g\n", joy_axes[ax].amp);
				Sys_Printf ("pre_amp:   %.9g\n", joy_axes[ax].pre_amp);
				Sys_Printf ("deadzone:  %i\n", joy_axes[ax].deadzone);
				Sys_Printf ("offset:    %.9g\n", joy_axes[ax].offset);
				Sys_Printf ("type:      %s\n",
							JOY_GetDest_c (joy_axes[ax].dest));
				Sys_Printf ("<====================>\n");
				break;
		}
		return;
	} else if (c < 4) {
		if (c == 3 && JOY_GetOption_i (Cmd_Argv (2)) == js_clear) {
			ax = strtol (Cmd_Argv (1), NULL, 0);

			joy_clear_axis (ax);
			return;
		} else {
			Sys_Printf ("in_joy <axis#> [<var> <value>]*\n"
						"    Configures the joystick behaviour\n");
			return;
		}
	}

	ax = strtol (Cmd_Argv (1), NULL, 0);

	i = 2;
	while (i < c) {
		int         var = JOY_GetOption_i (Cmd_Argv (i++));

		switch (var) {
			case js_amp:
				joy_axes[ax].amp = strtof (Cmd_Argv (i++), NULL);
				break;
			case js_pre_amp:
				joy_axes[ax].pre_amp = strtof (Cmd_Argv (i++), NULL);
				break;
			case js_deadzone:
				joy_axes[ax].deadzone = strtol (Cmd_Argv (i++), NULL, 10);
				break;
			case js_offset:
				joy_axes[ax].offset = strtol (Cmd_Argv (i++), NULL, 10);
				break;
			case js_type:
				joy_axes[ax].dest = JOY_GetDest_i (Cmd_Argv (i++));
				joy_axes[ax].axis = JOY_GetAxis_i (joy_axes[ax].dest,
												   Cmd_Argv (i++));
				if (joy_axes[ax].axis > 2 || joy_axes[ax].axis < 0) {
					joy_axes[ax].axis = 0;
					Sys_Printf ("Invalid axis value.");
				}
				break;
			case js_axis_button:
				arg = Cmd_Argv (i++);
				if (!strcmp ("add", arg)) {
					in_joy_button_add_f (ax, i);
					i += 2;
				}
				break;
			default:
				Sys_Printf ("Unknown option %s.\n", Cmd_Argv (i - 1));
				break;
		}
	}
}

VISIBLE void
JOY_Init_Cvars (void)
{
	int         i;

	Cvar_Register (&joy_device_cvar, 0, 0);
	Cvar_Register (&joy_enable_cvar, 0, 0);
	Cvar_Register (&joy_amp_cvar, joyamp_f, &joy_amp);
	Cvar_Register (&joy_pre_amp_cvar, joyamp_f, &joy_pre_amp);

	Cmd_AddCommand ("in_joy", in_joy_f, "Configures the joystick behaviour");

	for (i = 0; i < JOY_MAX_AXES; i++) {
		joy_axes[i].dest = js_none;
		joy_axes[i].amp = 1;
		joy_axes[i].pre_amp = 1;
		joy_axes[i].deadzone = 500;
	}
}


void
Joy_WriteBindings (QFile * f)
{
	int         i;

	for (i = 0; i < JOY_MAX_AXES; i++) {
		if (!js_axis_names[joy_axes[i].dest]) {
			Qprintf (f, "in_joy %i amp %.9g pre_amp %.9g deadzone %i "
					 "offset %.9g type %s %i\n",
					 i, joy_axes[i].amp, joy_axes[i].pre_amp,
					 joy_axes[i].deadzone,
					 joy_axes[i].offset, JOY_GetDest_c (joy_axes[i].dest),
					 joy_axes[i].axis);
		} else {
			Qprintf (f, "in_joy %i amp %.9g pre_amp %.9g deadzone %i "
					 "offset %.9g type %s %s\n",
					 i, joy_axes[i].amp, joy_axes[i].pre_amp,
					 joy_axes[i].deadzone,
					 joy_axes[i].offset, JOY_GetDest_c (joy_axes[i].dest),
					 js_axis_names[joy_axes[i].dest][joy_axes[i].axis].name);
		}

		if (joy_axes[i].num_buttons > 0) {
			int         n;

			for (n = 0; n < joy_axes[i].num_buttons; n++) {
				Qprintf (f, "in_joy %i button add %s %.9g\n", i,
						 Key_KeynumToString (joy_axes[i].axis_buttons[n].key),
						 joy_axes[i].axis_buttons[n].threshold);
			}
		}
	}
}

VISIBLE void
JOY_Shutdown (void)
{
	if (!joy_active)
		return;

	JOY_Close ();

	joy_active = false;
	joy_found = false;
}
