/*
	in_common.c

	general input driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define _BSD
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/in_event.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"

cvar_t     *_windowed_mouse;
cvar_t     *in_mouse_filter;
cvar_t     *in_mouse_pre_accel;
cvar_t     *in_mouse_accel;
cvar_t     *lookstrafe;
cvar_t     *in_freelook;

kbutton_t   in_mlook, in_klook;
kbutton_t   in_strafe;
kbutton_t   in_speed;

qboolean    in_mouse_avail;
float       in_mouse_x, in_mouse_y;
static float in_old_mouse_x, in_old_mouse_y;

void
IN_Commands (void)
{
	JOY_Command ();

	IN_LL_Commands ();
}


void
IN_SendKeyEvents (void)
{
	/* Get events from X server. */
	IN_LL_SendKeyEvents ();
}


void
IN_Move (void)
{
	JOY_Move ();

	if (!in_mouse_avail)
		return;

	in_mouse_x *= in_mouse_pre_accel->value;
	in_mouse_y *= in_mouse_pre_accel->value;

	if (in_mouse_filter->int_val) {
		in_mouse_x = (in_mouse_x + in_old_mouse_x) * 0.5;
		in_mouse_y = (in_mouse_y + in_old_mouse_y) * 0.5;

		in_old_mouse_x = in_mouse_x;
		in_old_mouse_y = in_mouse_y;
	}

	in_mouse_x *= in_mouse_accel->value;
	in_mouse_y *= in_mouse_accel->value;

	if ((in_strafe.state & 1) || (lookstrafe->int_val && freelook))
		viewdelta.position[0] += in_mouse_x;
	else
		viewdelta.angles[YAW] -= in_mouse_x;

	if (freelook && !(in_strafe.state & 1)) {
		viewdelta.angles[PITCH] += in_mouse_y;
	} else {
		viewdelta.position[2] -= in_mouse_y;
	}
	in_mouse_x = in_mouse_y = 0.0;
}

/*
  Called at shutdown
*/
void
IN_Shutdown (void)
{
	JOY_Shutdown ();

	Con_Printf ("IN_Shutdown\n");
	IN_LL_Shutdown ();

	IE_Shutdown ();
}

void
IN_Init (void)
{
	IE_Init ();
	IN_LL_Init ();

	JOY_Init ();

	in_mouse_x = in_mouse_y = 0.0;
}

void
IN_Init_Cvars (void)
{
	IE_Init_Cvars ();
	JOY_Init_Cvars ();
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, NULL,
			"With this set to 1, quake will grab the mouse from X");
	in_freelook = Cvar_Get ("freelook", "0", CVAR_ARCHIVE, NULL,
			                        "force +mlook");
	
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, NULL,
						   "when mlook/klook on player will strafe");
	in_mouse_filter = Cvar_Get ("in_mouse_filter", "0", CVAR_ARCHIVE, NULL,
			"Toggle mouse input filtering.");
	in_mouse_pre_accel = Cvar_Get ("in_mouse_pre_accel", "1", CVAR_ARCHIVE,
						NULL, "mouse in_mouse_pre_accel multiplier");
	in_mouse_accel = Cvar_Get ("in_mouse_accel", "3", CVAR_ARCHIVE, NULL,
							"mouse in_mouse_accel multiplier");
	IN_LL_Init_Cvars ();
}

void
IN_HandlePause (qboolean paused)
{
}

void
IN_ClearStates (void)
{
	IN_LL_ClearStates ();
	Key_ClearStates ();
}
