/*
	joystick.h

	QuakeForge joystick DPI (driver programming interface)

	Copyright (C) 1996-1997 Jeff Teunissen <deek@dusknet.dhs.org>

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

#ifndef __QF_joystick_h
#define __QF_joystick_h

#include <QF/qtypes.h>
#include "QF/quakeio.h"

#define JOY_MAX_AXES    32
#define JOY_MAX_BUTTONS 64

extern char *joy_device;
extern int joy_enable;

struct joy_axis_button {
	float       threshold;
	int         key;
	int         state;
};

typedef enum {
	js_none,								// ignore axis
	js_position,							// linear delta
	js_angles,								// linear delta
	js_button,								// axis button
} js_dest_t;

typedef enum {
	js_clear,
	js_amp,
	js_pre_amp,
	js_deadzone,
	js_offset,
	js_type,
	js_axis_button,
} js_opt_t;

struct joy_axis {
	int         current;
	float       amp;
	float       pre_amp;
	int         deadzone;
	float       offset;
	js_dest_t   dest;
	int         axis;						// if linear delta
	int         num_buttons;				// if axis button
	struct joy_axis_button *axis_buttons;	// if axis button
};

extern bool joy_found;					// Joystick present?
extern bool joy_active; 				// Joystick in use?

struct joy_button {
	int         old;
	int         current;
};

extern struct joy_axis joy_axes[JOY_MAX_AXES];

extern struct joy_button joy_buttons[JOY_MAX_BUTTONS];

/*
	JOY_Command ()

	Use this function to process joystick button presses and generate key
	events. It is called inside the IN_Commands () input function, once each
	frame.

	You should exit this function immediately if either joy_active or
	joy_enable are zero.
*/
void JOY_Command (void);
void joy_clear_axis (int i);

/*
	JOY_Move (usercmd_t *) // FIXME: Not anymore!

	Use this function to process joystick movements to move the player around.

	You should exit this function immediately if either joy_active or
	joy_enable are zero.
*/
void JOY_Move (void);

/*
	JOY_Init ()

	Use this function to initialize the joystick Cvars, open your joystick
	device, and get it ready for use. You MUST obey the value of the
	joy_enable Cvar. Set joy_found if there is a device, and joy_active if
	you have successfully enabled it.
*/
void JOY_Init (void);
void JOY_Init_Cvars (void);

/*
	JOY_Shutdown ()

	Use this function to close the joystick device and tell QuakeForge that it
	is no longer available. It is called from IN_Init (), but may be called
	elsewhere to disable the device.
*/
void JOY_Shutdown (void);

/*
	JOY_Open ()
	JOY_Close ()

	OS-specific joystick init/deinit
*/
int JOY_Open (void);
void JOY_Close (void);

/*
	JOY_Read ()

	OS-specific joystick reading
*/
void JOY_Read (void);


const char *JOY_GetOption_c (int i) __attribute__((pure));
int JOY_GetOption_i (const char *c) __attribute__((pure));

const char *JOY_GetDest_c (int i) __attribute__((pure));
int JOY_GetDest_i (const char *c) __attribute__((pure));
int JOY_GetAxis_i (int dest, const char *c);


void Joy_WriteBindings (QFile *f);

#endif//__QF_joystick_h
