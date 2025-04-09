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

#include "QF/checksum.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/listener.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "compat.h"

#include "client/input.h"
#include "client/view.h"

#include "qw/msg_ucmd.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_demo.h"
#include "qw/include/cl_input.h"
#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "qw/include/host.h"

int cl_nodelta;
static cvar_t cl_nodelta_cvar = {
	.name = "cl_nodelta",
	.description =
		"Disable player delta compression. Set to 1 if you have a poor ISP and"
		" get many U_REMOVE warnings.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_nodelta },
};
int cl_maxnetfps;
static cvar_t cl_maxnetfps_cvar = {
	.name = "cl_maxnetfps",
	.description =
		"Controls number of command packets sent per second. Default 0 is "
		"unlimited.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_maxnetfps },
};
int cl_spamimpulse;
static cvar_t cl_spamimpulse_cvar = {
	.name = "cl_spamimpulse",
	.description =
		"Controls number of duplicate packets sent if an impulse is being "
		"sent. Default (id behavior) is 3.",
	.default_value = "3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_spamimpulse },
};

int         in_impulse;

static void
IN_Impulse (void *data)
{
	in_impulse = atoi (Cmd_Argv (1));
	if (Cmd_Argc () <= 2)
		return;

	Team_BestWeaponImpulse ();			// HACK HACK HACK
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
	VectorCopy (cl.movestate.angles, cmd->angles);
}

static int
MakeChar (int i)
{
	i &= ~3;
	if (i < -127 * 4)
		i = -127 * 4;
	if (i > 127 * 4)
		i = 127 * 4;
	return i;
}

static void
CL_FinishMove (usercmd_t *cmd)
{
	int				ms, i;
	static double	accum = 0.0;

	// always dump the first two message, because it may contain leftover
	// inputs from the last level
	if (++cl.movemessages <= 2)
		return;

	// figure button bits
	cmd->buttons |= IN_ButtonPressed (&in_attack) << 0;
	cmd->buttons |= IN_ButtonPressed (&in_jump)   << 1;
	cmd->buttons |= IN_ButtonPressed (&in_use)    << 2;

	// send milliseconds of time to apply the move
	accum += (host_frametime * 1000.0);
	ms = accum + 0.5;
	accum -= ms;

	if (ms > 250) {
		ms = 100;						// time was unreasonable
		accum = 0.0;
	}
	cmd->msec = ms;

	VectorCopy (cl.viewstate.player_angles, cmd->angles);

	cmd->impulse = in_impulse;
	in_impulse = 0;

	// chop down so no extra bits are kept that the server wouldn't get
	cmd->forwardmove = MakeChar (cmd->forwardmove);
	cmd->sidemove = MakeChar (cmd->sidemove);
	cmd->upmove = MakeChar (cmd->upmove);

	for (i = 0; i < 3; i++)
		cmd->angles[i] = ((int) (cmd->angles[i] * (65536.0 / 360.0)) & 65535) *
			(360.0 / 65536.0);
}

static inline int
pps_check (int dontdrop)
{
	static float	pps_balance = 0.0;
	static int		dropcount = 0;

	pps_balance += host_frametime;
	// never drop more than 2 messages in a row -- that'll cause PL
	// and don't drop if one of the last two movemessages have an impulse
	if (pps_balance > 0.0 || dropcount >= 2 || dontdrop) {
		float   pps;

		if (!(pps = cl_maxnetfps))
			pps = rate / 80.0;

		pps = bound (1, pps, 72);

		pps_balance -= 1.0 / pps;
		pps_balance = bound (-0.1, pps_balance, 0.1);
		dropcount = 0;
		return 1;
	} else {
		int i;
		// don't count this message when calculating PL
		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		cl.frames[i].receivedtime = -3;
		// drop this message
		cls.netchan.outgoing_sequence++;
		dropcount++;
		return 0;
	}
}

static inline void
build_cmd (usercmd_t *cmd)
{
	// get basic movement from keyboard, mouse, etc
	CL_BaseMove (cmd);

	// if we are spectator, try autocam
	if (cl.spectator)
		Cam_Track (cmd);

	CL_FinishMove (cmd);

	Cam_FinishMove (cmd);
}

void
CL_SendCmd (void)
{
	byte			data[128];
	int				checksumIndex, lost, seq_hash, frame;
	bool			dontdrop; // FIXME: needed without cl_c2sImpulseBackup?
	sizebuf_t		buf;
	usercmd_t	   *cmd, *oldcmd;

	if (cls.demoplayback && !cls.demoplayback2)
		return;							// sendcmds come from the demo

	// save this command off for prediction
	frame = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	cl.frames[frame].senttime = realtime;
	cl.frames[frame].receivedtime = -1;	// we haven't gotten a reply yet

//	seq_hash = (cls.netchan.outgoing_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
	seq_hash = cls.netchan.outgoing_sequence;

	build_cmd (cmd);

	if (cls.demoplayback2) {
		cls.netchan.outgoing_sequence++;
		return;
	}

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	buf.maxsize = 128;
	buf.cursize = 0;
	buf.data = data;

	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// write our lossage percentage
	lost = CL_CalcNet ();
	MSG_WriteByte (&buf, (byte) lost);

	dontdrop = false;

	frame = (cls.netchan.outgoing_sequence - 2) & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	if (cl_spamimpulse >= 2)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	frame = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	if (cl_spamimpulse >= 3)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	frame = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	if (cl_spamimpulse >= 1)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] =
		COM_BlockSequenceCRCByte (buf.data + checksumIndex + 1,
								  buf.cursize - checksumIndex - 1, seq_hash);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		cl.validsequence = 0;

	if (cl.validsequence && !cl_nodelta && cls.state == ca_active
		&& !cls.demorecording) {
		cl.frames[frame].delta_sequence = cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
		MSG_WriteByte (&buf, cl.validsequence & 255);
	} else {
		cl.frames[frame].delta_sequence = -1;
	}

	if (cls.demorecording)
		CL_WriteDemoCmd (cmd);

	cl.viewstate.movecmd[FORWARD] = cmd->forwardmove;

	// deliver the message
	if (pps_check (dontdrop))
		Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);
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
	Cvar_Register (&cl_nodelta_cvar, 0, 0);
	Cvar_Register (&cl_maxnetfps_cvar, 0, 0);
	Cvar_Register (&cl_spamimpulse_cvar, 0, 0);
}
