/*
	joy_null.c

	Joystick device driver template

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

#include "QF/console.h"
#include "QF/cvar.h"
#include "protocol.h"
#include "QF/qtypes.h"
#include "client.h"

// Joystick variables and structures
cvar_t     *joy_device;					// Joystick device name
cvar_t     *joy_enable;					// Joystick enabling flag
cvar_t     *joy_sensitivity;			// Joystick sensitivity

qboolean    joy_found = false;
qboolean    joy_active = false;

void
JOY_Command (void)
{
}

void
JOY_Move (usercmd_t *cmd)
{
}

void
JOY_Init (void)
{
	Con_DPrintf ("This system does not have joystick support.\n");
}

void
JOY_Init_Cvars (void)
{
	joy_device =
		Cvar_Get ("joy_device", "none", CVAR_NONE | CVAR_ROM,
				  "Joystick device");
	joy_enable =
		Cvar_Get ("joy_enable", "1", CVAR_NONE | CVAR_ARCHIVE,
				  "Joystick enable flag");
	joy_sensitivity =
		Cvar_Get ("joy_sensitivity", "1", CVAR_NONE | CVAR_ARCHIVE,
				  "Joystick sensitivity");
}

void
JOY_Shutdown (void)
{
	joy_active = false;
	joy_found = false;
}
