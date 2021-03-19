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
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"

#include "nq/include/chase.h"
#include "nq/include/client.h"
#include "nq/include/host.h"

/*
	KEY BUTTONS

	Continuous button event tracking is complicated by the fact that two
	different input sources (say, mouse button 1 and the control key) can
	both press the same button, but the button should be released only when
	both of the pressing key have been released.

	When a key event issues a button command (+forward, +attack, etc), it
	appends its key number as a parameter to the command so it can be
	matched up with the release.

	state bit 0 is the current state of the key
	state bit 1 is edge triggered on the up to down transition
	state bit 2 is edge triggered on the down to up transition
*/

kbutton_t   in_left, in_right, in_forward, in_back;
kbutton_t   in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t   in_use, in_jump, in_attack;
kbutton_t   in_up, in_down;

int         in_impulse;

void (*write_angles) (sizebuf_t *sb, const vec3_t angles);

static void
KeyPress (kbutton_t *b)
{
	const char *c;
	int         k;

	c = Cmd_Argv (1);
	if (c[0])
		k = atoi (c);
	else
		k = -1;							// typed manually at the console for
										// continuous down

	if (k == b->down[0] || k == b->down[1])
		return;							// repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Sys_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;							// still down
	b->state |= 1 + 2;					// down + impulse down
}

static void
KeyRelease (kbutton_t *b)
{
	const char *c;
	int         k;

	c = Cmd_Argv (1);
	if (c[0])
		k = atoi (c);
	else {								// typed manually at the console,
										// assume for unsticking, so clear
										// all
		b->down[0] = b->down[1] = 0;
		b->state = 4;					// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;							// key up without coresponding down
										// (menu pass through)
	if (b->down[0] || b->down[1])
		return;							// some other key is still holding it
										// down

	if (!(b->state & 1))
		return;							// still up (this should not happen)
	b->state &= ~1;						// now up
	b->state |= 4;						// impulse up
}

static void
IN_KLookPress (void)
{
	KeyPress (&in_klook);
}

static void
IN_KLookRelease (void)
{
	KeyRelease (&in_klook);
}

static void
IN_MLookPress (void)
{
	KeyPress (&in_mlook);
}

static void
IN_MLookRelease (void)
{
	KeyRelease (&in_mlook);
	if (!freelook && lookspring->int_val)
		V_StartPitchDrift ();
}

static void
IN_UpPress (void)
{
	KeyPress (&in_up);
}

static void
IN_UpRelease (void)
{
	KeyRelease (&in_up);
}

static void
IN_DownPress (void)
{
	KeyPress (&in_down);
}

static void
IN_DownRelease (void)
{
	KeyRelease (&in_down);
}

static void
IN_LeftPress (void)
{
	KeyPress (&in_left);
}

static void
IN_LeftRelease (void)
{
	KeyRelease (&in_left);
}

static void
IN_RightPress (void)
{
	KeyPress (&in_right);
}

static void
IN_RightRelease (void)
{
	KeyRelease (&in_right);
}

static void
IN_ForwardPress (void)
{
	KeyPress (&in_forward);
}

static void
IN_ForwardRelease (void)
{
	KeyRelease (&in_forward);
}

static void
IN_BackPress (void)
{
	KeyPress (&in_back);
}

static void
IN_BackRelease (void)
{
	KeyRelease (&in_back);
}

static void
IN_LookupPress (void)
{
	KeyPress (&in_lookup);
}

static void
IN_LookupRelease (void)
{
	KeyRelease (&in_lookup);
}

static void
IN_LookdownPress (void)
{
	KeyPress (&in_lookdown);
}

static void
IN_LookdownRelease (void)
{
	KeyRelease (&in_lookdown);
}

static void
IN_MoveleftPress (void)
{
	KeyPress (&in_moveleft);
}

static void
IN_MoveleftRelease (void)
{
	KeyRelease (&in_moveleft);
}

static void
IN_MoverightPress (void)
{
	KeyPress (&in_moveright);
}

static void
IN_MoverightRelease (void)
{
	KeyRelease (&in_moveright);
}

static void
IN_SpeedPress (void)
{
	KeyPress (&in_speed);
}

static void
IN_SpeedRelease (void)
{
	KeyRelease (&in_speed);
}

static void
IN_StrafePress (void)
{
	KeyPress (&in_strafe);
}

static void
IN_StrafeRelease (void)
{
	KeyRelease (&in_strafe);
}

static void
IN_AttackPress (void)
{
	KeyPress (&in_attack);
}

static void
IN_AttackRelease (void)
{
	KeyRelease (&in_attack);
}

static void
IN_UsePress (void)
{
	KeyPress (&in_use);
}

static void
IN_UseRelease (void)
{
	KeyRelease (&in_use);
}

static void
IN_JumpPress (void)
{
	KeyPress (&in_jump);
}

static void
IN_JumpRelease (void)
{
	KeyRelease (&in_jump);
}

static void
IN_Impulse (void)
{
	in_impulse = atoi (Cmd_Argv (1));
}

/*
	CL_KeyState

	Returns 0.25 if a key was pressed and released during the frame,
	0.5 if it was pressed and held
	0 if held then released, and
	1.0 if held for the entire time
*/
float
CL_KeyState (kbutton_t *key)
{
	float       val;
	qboolean    impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;

	if (impulsedown && !impulseup) {
		if (down)
			val = 0.5;					// pressed and held this frame
		else
			val = 0;					// I_Error ();
	}
	if (impulseup && !impulsedown) {
		if (down)
			val = 0;					// I_Error ();
		else
			val = 0;					// released this frame
	}
	if (!impulsedown && !impulseup) {
		if (down)
			val = 1.0;					// held the entire frame
		else
			val = 0;					// up the entire frame
	}
	if (impulsedown && impulseup) {
		if (down)
			val = 0.75;					// released and re-pressed this frame
		else
			val = 0.25;					// pressed and released this frame
	}

	key->state &= 1;					// clear impulses

	return val;
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

	if (in_speed.state & 1) {
		pitchspeed *= cl_anglespeedkey->value;
		yawspeed *= cl_anglespeedkey->value;
	}

	if ((cl.fpd & FPD_LIMIT_PITCH) && pitchspeed > FPD_MAXPITCH)
		pitchspeed = FPD_MAXPITCH;
	if ((cl.fpd & FPD_LIMIT_YAW) && yawspeed > FPD_MAXYAW)
		yawspeed = FPD_MAXYAW;

	pitchspeed *= host_frametime;
	yawspeed *= host_frametime;

	if (!(in_strafe.state & 1)) {
		cl.viewstate.angles[YAW] -= yawspeed * CL_KeyState (&in_right);
		cl.viewstate.angles[YAW] += yawspeed * CL_KeyState (&in_left);
		cl.viewstate.angles[YAW] = anglemod (cl.viewstate.angles[YAW]);
	}
	if (in_klook.state & 1) {
		V_StopPitchDrift ();
		cl.viewstate.angles[PITCH] -= pitchspeed * CL_KeyState (&in_forward);
		cl.viewstate.angles[PITCH] += pitchspeed * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState (&in_lookdown);

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
	if (cls.state != ca_active) {
		return;
	}

	CL_AdjustAngles ();

	memset (cmd, 0, sizeof (*cmd));

	if (in_strafe.state & 1) {
		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

	if (!(in_klook.state & 1)) {
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed->value * CL_KeyState (&in_back);
	}

	// adjust for speed key
	if (in_speed.state & 1) {
		cmd->forwardmove *= cl_movespeedkey->value;
		cmd->sidemove *= cl_movespeedkey->value;
		cmd->upmove *= cl_movespeedkey->value;
	}

	if (freelook)
		V_StopPitchDrift ();

	viewdelta.angles[0] = viewdelta.angles[1] = viewdelta.angles[2] = 0;
	viewdelta.position[0] = viewdelta.position[1] = viewdelta.position[2] = 0;

	IN_Move ();

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

	cmd->forwardmove += viewdelta.position[2] * m_forward->value;
	cmd->sidemove += viewdelta.position[0] * m_side->value;
	cmd->upmove += viewdelta.position[1];
	cl.viewstate.angles[PITCH] += viewdelta.angles[PITCH] * m_pitch->value;
	cl.viewstate.angles[YAW] += viewdelta.angles[YAW] * m_yaw->value;
	cl.viewstate.angles[ROLL] += viewdelta.angles[ROLL];

	if (freelook && !(in_strafe.state & 1)) {
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

	if (in_attack.state & 3)
		bits |= 1;
	in_attack.state &= ~2;

	if (in_jump.state & 3)
		bits |= 2;
	in_jump.state &= ~2;

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
CL_Input_Init (void)
{
	Cmd_AddCommand ("+moveup", IN_UpPress, "When active the player is "
					"swimming up in a liquid");
	Cmd_AddCommand ("-moveup", IN_UpRelease, "When active the player is not "
					"swimming up in a liquid");
	Cmd_AddCommand ("+movedown", IN_DownPress, "When active the player is "
					"swimming down in a liquid");
	Cmd_AddCommand ("-movedown", IN_DownRelease, "When active the player is "
					"not swimming down in a liquid");
	Cmd_AddCommand ("+left", IN_LeftPress, "When active the player is turning "
					"left");
	Cmd_AddCommand ("-left", IN_LeftRelease, "When active the player is not "
					"turning left");
	Cmd_AddCommand ("+right", IN_RightPress, "When active the player is "
					"turning right");
	Cmd_AddCommand ("-right", IN_RightRelease, "When active the player is not "
					"turning right");
	Cmd_AddCommand ("+forward", IN_ForwardPress, "When active the player is "
					"moving forward");
	Cmd_AddCommand ("-forward", IN_ForwardRelease, "When active the player is "
					"not moving forward");
	Cmd_AddCommand ("+back", IN_BackPress, "When active the player is moving "
					"backwards");
	Cmd_AddCommand ("-back", IN_BackRelease, "When active the player is not "
					"moving backwards");
	Cmd_AddCommand ("+lookup", IN_LookupPress, "When active the player's view "
					"is looking up");
	Cmd_AddCommand ("-lookup", IN_LookupRelease, "When active the player's "
					"view is not looking up");
	Cmd_AddCommand ("+lookdown", IN_LookdownPress, "When active the player's "
					"view is looking down");
	Cmd_AddCommand ("-lookdown", IN_LookdownRelease, "When active the "
					"player's view is not looking up");
	Cmd_AddCommand ("+strafe", IN_StrafePress, "When active, +left and +right "
					"function like +moveleft and +moveright");
	Cmd_AddCommand ("-strafe", IN_StrafeRelease, "When active, +left and "
					"+right stop functioning like +moveleft and +moveright");
	Cmd_AddCommand ("+moveleft", IN_MoveleftPress, "When active the player is "
					"strafing left");
	Cmd_AddCommand ("-moveleft", IN_MoveleftRelease, "When active the player "
					"is not strafing left");
	Cmd_AddCommand ("+moveright", IN_MoverightPress, "When active the player "
					"is strafing right");
	Cmd_AddCommand ("-moveright", IN_MoverightRelease, "When active the "
					"player is not strafing right");
	Cmd_AddCommand ("+speed", IN_SpeedPress, "When active the player is "
					"running");
	Cmd_AddCommand ("-speed", IN_SpeedRelease, "When active the player is not "
					"running");
	Cmd_AddCommand ("+attack", IN_AttackPress, "When active player is "
					"firing/using current weapon");
	Cmd_AddCommand ("-attack", IN_AttackRelease, "When active player is not "
					"firing/using current weapon");
	Cmd_AddCommand ("+use", IN_UsePress, "Non-functional. Left over command "
					"for opening doors and triggering switches");
	Cmd_AddCommand ("-use", IN_UseRelease, "Non-functional. Left over command "
					"for opening doors and triggering switches");
	Cmd_AddCommand ("+jump", IN_JumpPress, "When active the player is "
					"jumping");
	Cmd_AddCommand ("-jump", IN_JumpRelease, "When active the player is not "
					"jumping");
	Cmd_AddCommand ("impulse", IN_Impulse, "Call a game function or QuakeC "
					"function.");
	Cmd_AddCommand ("+klook", IN_KLookPress, "When active, +forward and +back "
					"perform +lookup and +lookdown");
	Cmd_AddCommand ("-klook", IN_KLookRelease, "When active, +forward and "
					"+back don't perform +lookup and +lookdown");
	Cmd_AddCommand ("+mlook", IN_MLookPress, "When active moving the mouse or "
					"joystick forwards and backwards performs +lookup and "
					"+lookdown");
	Cmd_AddCommand ("-mlook", IN_MLookRelease, "When active moving the mouse "
					"or joystick forwards and backwards doesn't perform "
					"+lookup and +lookdown");
}
