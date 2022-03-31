/*
	cl_input.c

	builds an intended movement command to send to the server

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "QF/input/event.h"
#include "QF/plugin/vid_render.h"

#include "compat.h"

#include "client/input.h"

#include "nq/include/client.h"
#include "nq/include/host.h"

int         in_impulse;

void (*write_angles) (sizebuf_t *sb, const vec3_t angles);

static void
IN_Impulse (void *data)
{
	in_impulse = atoi (Cmd_Argv (1));
}

/*
	CL_BaseMove

	Send the intended movement message to the server
*/
void
CL_BaseMove (usercmd_t *cmd)
{
	if (cls.state != ca_active) {
		return;
	}
	VectorCopy (cl.viewstate.player_angles, cl.movestate.angles);//FIXME
	CL_Input_BuildMove (host_frametime, &cl.movestate, &cl.viewstate);
	VectorCopy (cl.movestate.angles, cl.viewstate.player_angles);//FIXME

	memset (cmd, 0, sizeof (*cmd));
	cmd->forwardmove = cl.movestate.move[FORWARD];
	cmd->sidemove = cl.movestate.move[SIDE];
	cmd->upmove = cl.movestate.move[UP];
	cl.viewstate.movecmd = cl.movestate.move;
}


void
CL_SendMove (usercmd_t *cmd)
{
	byte        data[128];
	int         bits;
	sizebuf_t   buf;

	buf.maxsize = 128;
	buf.cursize = 0;
	buf.data = data;

	cl.cmd = *cmd;

	// send the movement message
	MSG_WriteByte (&buf, clc_move);

	MSG_WriteFloat (&buf, cl.mtime[0]);		// so server can get ping times

	write_angles (&buf, cl.viewstate.player_angles);

	MSG_WriteShort (&buf, cmd->forwardmove);
	MSG_WriteShort (&buf, cmd->sidemove);
	MSG_WriteShort (&buf, cmd->upmove);

	// send button bits
	bits = 0;

	bits |= IN_ButtonPressed (&in_attack) << 0;
	bits |= IN_ButtonPressed (&in_jump)   << 1;
	bits |= IN_ButtonPressed (&in_use)    << 2;

	MSG_WriteByte (&buf, bits);

	MSG_WriteByte (&buf, in_impulse);
	in_impulse = 0;

	// deliver the message
	if (cls.demoplayback)
		return;

	// always dump the first two message, because it may contain leftover
	// inputs from the last level
	if (++cl.movemessages <= 2)
		return;

	if (NET_SendUnreliableMessage (cls.netcon, &buf) == -1) {
		Sys_Printf ("CL_SendMove: lost server connection\n");
		CL_Disconnect ();
	}
}

void
CL_Init_Input (cbuf_t *cbuf)
{
	CL_Input_Init (cbuf);
	Cmd_AddDataCommand ("impulse", IN_Impulse, 0,
						"Call a game function or QuakeC function.");
}

void
CL_Init_Input_Cvars (void)
{
	CL_Input_Init_Cvars ();
}
