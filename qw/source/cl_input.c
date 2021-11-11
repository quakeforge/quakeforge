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
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/va.h"

#include "compat.h"

#include "client/view.h"

#include "qw/msg_ucmd.h"

#include "qw/include/chase.h"
#include "qw/include/cl_cam.h"
#include "qw/include/cl_demo.h"
#include "qw/include/cl_input.h"
#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "qw/include/host.h"

int         cl_game_context;
int         cl_demo_context;

cvar_t     *cl_nodelta;
cvar_t     *cl_maxnetfps;
cvar_t     *cl_spamimpulse;

in_axis_t viewdelta_position_forward = {
	.mode = ina_accumulate,
	.name = "move.forward",
	.description = "Move forward (positive) or backward (negative)",
};
in_axis_t viewdelta_position_left = {
	.mode = ina_accumulate,
	.name = "move.left",
	.description = "Move left (positive) or right (negative)",
};
in_axis_t viewdelta_position_up = {
	.mode = ina_accumulate,
	.name = "move.up",
	.description = "Move up (positive) or down (negative)",
};

in_axis_t viewdelta_angles_pitch = {
	.mode = ina_accumulate,
	.name = "move.pitch",
	.description = "Pitch axis",
};
in_axis_t viewdelta_angles_yaw = {
	.mode = ina_accumulate,
	.name = "move.yaw",
	.description = "Yaw axis",
};
in_axis_t viewdelta_angles_roll = {
	.mode = ina_accumulate,
	.name = "move.roll",
	.description = "Roll axis",
};

in_button_t in_left = {
	.name = "left",
	.description = "When active the player is turning left"
};
in_button_t in_right = {
	.name = "right",
	.description = "When active the player is turning right"
};
in_button_t in_forward = {
	.name = "forward",
	.description = "When active the player is moving forward"
};
in_button_t in_back = {
	.name = "back",
	.description = "When active the player is moving backwards"
};
in_button_t in_lookup = {
	.name = "lookup",
	.description = "When active the player's view is looking up"
};
in_button_t in_lookdown = {
	.name = "lookdown",
	.description = "When active the player's view is looking down"
};
in_button_t in_moveleft = {
	.name = "moveleft",
	.description = "When active the player is strafing left"
};
in_button_t in_moveright = {
	.name = "moveright",
	.description = "When active the player is strafing right"
};
in_button_t in_use = {
	.name = "use",
	.description = "Left over command for opening doors and triggering"
				   " switches"
};
in_button_t in_jump = {
	.name = "jump",
	.description = "When active the player is jumping"
};
in_button_t in_attack = {
	.name = "attack",
	.description = "When active player is firing/using current weapon"
};
in_button_t in_up = {
	.name = "moveup",
	.description = "When active the player is swimming up in a liquid"
};
in_button_t in_down = {
	.name = "movedown",
	.description = "When active the player is swimming down in a liquid"
};
in_button_t in_strafe = {
	.name = "strafe",
	.description = "When active, +left and +right function like +moveleft and"
				   " +moveright"
};
in_button_t in_klook = {
	.name = "klook",
	.description = "When active, +forward and +back perform +lookup and"
				   " +lookdown"
};
in_button_t in_speed = {
	.name = "speed",
	.description = "When active the player is running"
};
in_button_t in_mlook = {
	.name = "mlook",
	.description = "When active moving the mouse or joystick forwards "
				   "and backwards performs +lookup and "
				   "+lookdown"
};

static in_axis_t *cl_in_axes[] = {
	&viewdelta_position_forward,
	&viewdelta_position_left,
	&viewdelta_position_up,
	&viewdelta_angles_pitch,
	&viewdelta_angles_yaw,
	&viewdelta_angles_roll,
	0,
};

static in_button_t *cl_in_buttons[] = {
	&in_left,
	&in_right,
	&in_forward,
	&in_back,
	&in_lookup,
	&in_lookdown,
	&in_moveleft,
	&in_moveright,
	&in_use,
	&in_jump,
	&in_attack,
	&in_up,
	&in_down,
	&in_strafe,
	&in_klook,
	&in_speed,
	&in_mlook,
	0
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

cvar_t     *cl_anglespeedkey;
cvar_t     *cl_backspeed;
cvar_t     *cl_forwardspeed;
cvar_t     *cl_movespeedkey;
cvar_t     *cl_pitchspeed;
cvar_t     *cl_sidespeed;
cvar_t     *cl_upspeed;
cvar_t     *cl_yawspeed;

/*
	CL_AdjustAngles

	Moves the local angle positions
*/
static void
CL_AdjustAngles (void)
{
	float       down, up;
	float       pitchspeed, yawspeed;

	pitchspeed = cl_pitchspeed->value;
	yawspeed = cl_yawspeed->value;

	if (in_speed.state & inb_down) {
		pitchspeed *= cl_anglespeedkey->value;
		yawspeed *= cl_anglespeedkey->value;
	}

	if ((cl.fpd & FPD_LIMIT_PITCH) && pitchspeed > FPD_MAXPITCH)
		pitchspeed = FPD_MAXPITCH;
	if ((cl.fpd & FPD_LIMIT_YAW) && yawspeed > FPD_MAXYAW)
		yawspeed = FPD_MAXYAW;

	pitchspeed *= host_frametime;
	yawspeed *= host_frametime;

	if (!(in_strafe.state & inb_down)) {
		cl.viewstate.angles[YAW] -= yawspeed * IN_ButtonState (&in_right);
		cl.viewstate.angles[YAW] += yawspeed * IN_ButtonState (&in_left);
		cl.viewstate.angles[YAW] = anglemod (cl.viewstate.angles[YAW]);
	}
	if (in_klook.state & inb_down) {
		V_StopPitchDrift ();
		cl.viewstate.angles[PITCH] -= pitchspeed * IN_ButtonState (&in_forward);
		cl.viewstate.angles[PITCH] += pitchspeed * IN_ButtonState (&in_back);
	}

	up = IN_ButtonState (&in_lookup);
	down = IN_ButtonState (&in_lookdown);

	cl.viewstate.angles[PITCH] -= pitchspeed * up;
	cl.viewstate.angles[PITCH] += pitchspeed * down;

	if (up || down)
		V_StopPitchDrift ();

	// FIXME: Need to clean up view angle limits
	if (cl.viewstate.angles[PITCH] > 80)
		cl.viewstate.angles[PITCH] = 80;
	if (cl.viewstate.angles[PITCH] < -70)
		cl.viewstate.angles[PITCH] = -70;

	if (cl.viewstate.angles[ROLL] > 50)
		cl.viewstate.angles[ROLL] = 50;
	if (cl.viewstate.angles[ROLL] < -50)
		cl.viewstate.angles[ROLL] = -50;
}

/*
	CL_BaseMove

	Send the intended movement message to the server
*/
void
CL_BaseMove (usercmd_t *cmd)
{
	if (IN_ButtonReleased (&in_mlook) && !freelook && lookspring->int_val) {
		V_StartPitchDrift ();
	}

	if (cls.state != ca_active) {
		return;
	}

	CL_AdjustAngles ();

	memset (cmd, 0, sizeof (*cmd));

	VectorCopy (cl.viewstate.angles, cmd->angles);
	if (in_strafe.state & inb_down) {
		cmd->sidemove += cl_sidespeed->value * IN_ButtonState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * IN_ButtonState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * IN_ButtonState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * IN_ButtonState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * IN_ButtonState (&in_up);
	cmd->upmove -= cl_upspeed->value * IN_ButtonState (&in_down);

	if (!(in_klook.state & inb_down)) {
		cmd->forwardmove += cl_forwardspeed->value * IN_ButtonState (&in_forward);
		cmd->forwardmove -= cl_backspeed->value * IN_ButtonState (&in_back);
	}

	// adjust for speed key
	if (in_speed.state & inb_down) {
		cmd->forwardmove *= cl_movespeedkey->value;
		cmd->sidemove *= cl_movespeedkey->value;
		cmd->upmove *= cl_movespeedkey->value;
	}

	if (freelook)
		V_StopPitchDrift ();

	// adjust for chase camera angles
	/*FIXME:chase figure out just what this does and get it working
	if (cl.chase
		&& (chase_active->int_val == 2 || chase_active->int_val == 3)) {
		vec3_t      forward, right, up, f, r;
		vec3_t      dir = {0, 0, 0};

		dir[1] = r_data->refdef->viewangles[1] - cl.viewstate.angles[1];
		AngleVectors (dir, forward, right, up);
		VectorScale (forward, cmd->forwardmove, f);
		VectorScale (right, cmd->sidemove, r);
		cmd->forwardmove = f[0] + r[0];
		cmd->sidemove = f[1] + r[1];
		VectorScale (forward, viewdelta.position[2], f);
		VectorScale (right, viewdelta.position[0], r);
		viewdelta.position[2] = f[0] + r[0];
		viewdelta.position[0] = (f[1] + r[1]) * -1;
	}
	*/

	cmd->forwardmove += viewdelta_position_forward.value * m_forward->value;
	cmd->sidemove += viewdelta_position_left.value * m_side->value;
	cmd->upmove += viewdelta_position_up.value;
	cl.viewstate.angles[PITCH] += viewdelta_angles_pitch.value * m_pitch->value;
	cl.viewstate.angles[YAW] += viewdelta_angles_yaw.value * m_yaw->value;
	cl.viewstate.angles[ROLL] += viewdelta_angles_roll.value;

	viewdelta_angles_pitch.value = 0;
	viewdelta_angles_yaw.value = 0;
	viewdelta_angles_roll.value = 0;
	viewdelta_position_forward.value = 0;
	viewdelta_position_left.value = 0;
	viewdelta_position_up.value = 0;

	if (freelook && !(in_strafe.state & inb_down)) {
		cl.viewstate.angles[PITCH]
			= bound (-70, cl.viewstate.angles[PITCH], 80);
	}
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

	VectorCopy (cl.viewstate.angles, cmd->angles);

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

		if (!(pps = cl_maxnetfps->int_val))
			pps = rate->value / 80.0;

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
	qboolean		dontdrop; // FIXME: needed without cl_c2sImpulseBackup?
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
	if (cl_spamimpulse->int_val >= 2)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	frame = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	if (cl_spamimpulse->int_val >= 3)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	frame = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[frame].cmd;
	if (cl_spamimpulse->int_val >= 1)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] =
		COM_BlockSequenceCRCByte (buf.data + checksumIndex + 1,
								  buf.cursize - checksumIndex - 1, seq_hash);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		cl.validsequence = 0;

	if (cl.validsequence && !cl_nodelta->int_val && cls.state == ca_active
		&& !cls.demorecording) {
		cl.frames[frame].delta_sequence = cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
		MSG_WriteByte (&buf, cl.validsequence & 255);
	} else {
		cl.frames[frame].delta_sequence = -1;
	}

	if (cls.demorecording)
		CL_WriteDemoCmd (cmd);

	// deliver the message
	if (pps_check (dontdrop))
		Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);
}

void
CL_Input_Init (void)
{
	for (int i = 0; cl_in_axes[i]; i++) {
		IN_RegisterAxis (cl_in_axes[i]);
	}
	for (int i = 0; cl_in_buttons[i]; i++) {
		IN_RegisterButton (cl_in_buttons[i]);
	}
	cl_game_context = IMT_CreateContext ("key_game");
	IMT_SetContextCbuf (cl_game_context, cl_cbuf);
	cl_demo_context = IMT_CreateContext ("key_demo");
	IMT_SetContextCbuf (cl_demo_context, cl_cbuf);
	Cmd_AddDataCommand ("impulse", IN_Impulse, 0,
						"Call a game function or QuakeC function.");
}

void
CL_Input_Activate (void)
{
	IMT_SetContext (cl_game_context);
	IN_Binding_Activate ();
}

void
CL_Input_Init_Cvars (void)
{
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", CVAR_NONE, NULL,
						   "Disable player delta compression. Set to 1 if you "
						   "have a poor ISP and get many U_REMOVE warnings.");
	cl_maxnetfps = Cvar_Get ("cl_maxnetfps", "0", CVAR_ARCHIVE, NULL,
							 "Controls number of command packets sent per "
							 "second. Default 0 is unlimited.");
	cl_spamimpulse = Cvar_Get ("cl_spamimpulse", "3", CVAR_NONE, NULL,
							   "Controls number of duplicate packets sent if "
							   "an impulse is being sent. Default (id "
							   "behavior) is 3.");
}
