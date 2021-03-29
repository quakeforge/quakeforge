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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/in_event.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/vid.h"

VISIBLE viewdelta_t viewdelta;

cvar_t     *in_grab;
VISIBLE cvar_t     *in_amp;
VISIBLE cvar_t     *in_pre_amp;
cvar_t     *in_freelook;
cvar_t     *in_mouse_filter;
cvar_t     *in_mouse_amp;
cvar_t     *in_mouse_pre_amp;
cvar_t     *lookstrafe;

kbutton_t   in_mlook, in_klook;
kbutton_t   in_strafe;
kbutton_t   in_speed;

qboolean    in_mouse_avail;
float       in_mouse_x, in_mouse_y;
static float in_old_mouse_x, in_old_mouse_y;

void
IN_UpdateGrab (cvar_t *var)		// called from context_*.c
{
	if (var) {
		IN_LL_Grab_Input (var->int_val);
	}
}

void
IN_ProcessEvents (void)
{
	/* Get events from environment. */
	JOY_Command ();
	IN_LL_ProcessEvents ();
}

void
IN_Move (void)
{
	JOY_Move ();

	if (!in_mouse_avail)
		return;

	in_mouse_x *= in_mouse_pre_amp->value * in_pre_amp->value;
	in_mouse_y *= in_mouse_pre_amp->value * in_pre_amp->value;

	if (in_mouse_filter->int_val) {
		in_mouse_x = (in_mouse_x + in_old_mouse_x) * 0.5;
		in_mouse_y = (in_mouse_y + in_old_mouse_y) * 0.5;

		in_old_mouse_x = in_mouse_x;
		in_old_mouse_y = in_mouse_y;
	}

	in_mouse_x *= in_mouse_amp->value * in_amp->value;
	in_mouse_y *= in_mouse_amp->value * in_amp->value;

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

/* Called at shutdown */
static void
IN_shutdown (void *data)
{
	JOY_Shutdown ();

	Sys_MaskPrintf (SYS_vid, "IN_Shutdown\n");
	IN_LL_Shutdown ();

	IE_Shutdown ();
}

void
IN_Init (cbuf_t *cbuf)
{
	Sys_RegisterShutdown (IN_shutdown, 0);

	IE_Init ();
	IN_LL_Init ();
	Key_Init (cbuf);
	JOY_Init ();

	in_mouse_x = in_mouse_y = 0.0;
}

void
IN_Init_Cvars (void)
{
	IE_Init_Cvars ();
	Key_Init_Cvars ();
	JOY_Init_Cvars ();
	in_grab = Cvar_Get ("in_grab", "0", CVAR_ARCHIVE, IN_UpdateGrab,
						"With this set to 1, quake will grab the mouse, "
						"preventing loss of input focus.");
	in_amp = Cvar_Get ("in_amp", "1", CVAR_ARCHIVE, NULL,
					   "global in_amp multiplier");
	in_pre_amp = Cvar_Get ("in_pre_amp", "1", CVAR_ARCHIVE, NULL,
						   "global in_pre_amp multiplier");
	in_freelook = Cvar_Get ("freelook", "0", CVAR_ARCHIVE, NULL,
							"force +mlook");
	in_mouse_filter = Cvar_Get ("in_mouse_filter", "0", CVAR_ARCHIVE, NULL,
								"Toggle mouse input filtering.");
	in_mouse_amp = Cvar_Get ("in_mouse_amp", "15", CVAR_ARCHIVE, NULL,
							 "mouse in_mouse_amp multiplier");
	in_mouse_pre_amp = Cvar_Get ("in_mouse_pre_amp", "1", CVAR_ARCHIVE, NULL,
								 "mouse in_mouse_pre_amp multiplier");
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, NULL,
						   "when mlook/klook on player will strafe");
	IN_LL_Init_Cvars ();
}

void
IN_ClearStates (void)
{
	IN_LL_ClearStates ();
	Key_ClearStates ();
}
