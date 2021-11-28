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

#include "nq/include/chase.h"
#include "nq/include/client.h"
#include "nq/include/host.h"

int         cl_game_context;
int         cl_demo_context;
static int  cl_event_id;

in_axis_t viewdelta_position_forward = {
	.mode = ina_set,
	.name = "move.forward",
	.description = "Move forward (negative) or backward (positive)",
};
in_axis_t viewdelta_position_side = {
	.mode = ina_set,
	.name = "move.side",
	.description = "Move right (positive) or left (negative)",
};
in_axis_t viewdelta_position_up = {
	.mode = ina_set,
	.name = "move.up",
	.description = "Move up (positive) or down (negative)",
};

in_axis_t viewdelta_angles_pitch = {
	.mode = ina_set,
	.name = "move.pitch",
	.description = "Pitch axis",
};
in_axis_t viewdelta_angles_yaw = {
	.mode = ina_set,
	.name = "move.yaw",
	.description = "Yaw axis",
};
in_axis_t viewdelta_angles_roll = {
	.mode = ina_set,
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
	&viewdelta_position_side,
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

void (*write_angles) (sizebuf_t *sb, const vec3_t angles);

static void
IN_Impulse (void *data)
{
	in_impulse = atoi (Cmd_Argv (1));
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

	cmd->forwardmove -= IN_UpdateAxis (&viewdelta_position_forward) * m_forward->value;
	cmd->sidemove += IN_UpdateAxis (&viewdelta_position_side) * m_side->value;
	cmd->upmove -= IN_UpdateAxis (&viewdelta_position_up);
	cl.viewstate.angles[PITCH] -= IN_UpdateAxis (&viewdelta_angles_pitch) * m_pitch->value;
	cl.viewstate.angles[YAW] -= IN_UpdateAxis (&viewdelta_angles_yaw) * m_yaw->value;
	cl.viewstate.angles[ROLL] -= IN_UpdateAxis (&viewdelta_angles_roll) * m_pitch->value;

	if (freelook && !(in_strafe.state & inb_down)) {
		cl.viewstate.angles[PITCH]
			= bound (-70, cl.viewstate.angles[PITCH], 80);
	}
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

	write_angles (&buf, cl.viewstate.angles);

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

static int
cl_event_handler (const IE_event_t *ie_event, void *unused)
{
	if (ie_event->type == ie_key) {
		if (ie_event->key.code == QFK_ESCAPE) {
			Con_SetState (con_menu);
			return 1;
		}
	}
	return IN_Binding_HandleEvent (ie_event);
}

void
CL_Input_Init (cbuf_t *cbuf)
{
	cl_event_id = IE_Add_Handler (cl_event_handler, 0);

	for (int i = 0; cl_in_axes[i]; i++) {
		IN_RegisterAxis (cl_in_axes[i]);
	}
	for (int i = 0; cl_in_buttons[i]; i++) {
		IN_RegisterButton (cl_in_buttons[i]);
	}
	cl_game_context = IMT_CreateContext ("key_game");
	IMT_SetContextCbuf (cl_game_context, cbuf);
	cl_demo_context = IMT_CreateContext ("key_demo");
	IMT_SetContextCbuf (cl_demo_context, cbuf);
	Cmd_AddDataCommand ("impulse", IN_Impulse, 0,
						"Call a game function or QuakeC function.");
	CL_Legacy_Init ();
}

void
CL_Input_Activate (void)
{
	host_in_game = !cls.demoplayback;
	IMT_SetContext (cls.demoplayback ? cl_demo_context : cl_game_context);
	IE_Set_Focus (cl_event_id);
}
