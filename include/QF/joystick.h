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

	$Id$
*/

#ifndef __QF_joystick_h_
#define __QF_joystick_h_

#include <QF/qtypes.h>

#define JOY_MAX_AXES    8
#define JOY_MAX_BUTTONS 18

extern struct cvar_s	*joy_device;		// Joystick device name
extern struct cvar_s	*joy_enable;		// Joystick enabling flag

extern qboolean 	joy_found;			// Joystick present?
extern qboolean 	joy_active; 		// Joystick in use?

struct joy_axis {
	struct cvar_s	*axis;
	int 			current;
};

struct joy_button {
	int 	old;
	int 	current;
};

extern struct joy_axis joy_axes[JOY_MAX_AXES];

extern struct joy_button joy_buttons[JOY_MAX_BUTTONS];

/*
	JOY_Command ()

	Use this function to process joystick button presses and generate key
	events. It is called inside the IN_Commands () input function, once each
	frame.

	You should exit this function immediately if either joy_active or
	joy_enable->int_val are zero.
*/
void JOY_Command (void);

/*
	JOY_Move (usercmd_t *) // FIXME: Not anymore!

	Use this function to process joystick movements to move the player around.

	You should exit this function immediately if either joy_active or
	joy_enable->int_val are zero.
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

#endif	// __QF_joystick_h_
