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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "protocol.h"

extern cvar_t	*joy_device;		// Joystick device name
extern cvar_t	*joy_enable;		// Joystick enabling flag
extern cvar_t	*joy_sensitivity;	// Joystick sensitivity

extern qboolean joy_found;			// Joystick present?
extern qboolean joy_active; 		// Joystick in use?

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
	JOY_Move (usercmd_t *)

	Use this function to process joystick movements to move the player around.

	You should exit this function immediately if either joy_active or
	joy_enable->int_val are zero.
*/
void JOY_Move (usercmd_t *);

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
