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
static const char rcsid[] = 
	"$Id$";

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

#include "chase.h"
#include "client.h"
#include "compat.h"
#include "host.h"

/*
	KEY BUTTONS

	Continuous button event tracking is complicated by the fact that two
	different input sources (say, mouse button 1 and the control key) can
	both press the same button, but the button should only be released when
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


static void
KeyDown (kbutton_t *b)
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
		Con_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;							// still down
	b->state |= 1 + 2;					// down + impulse down
}

static void
KeyUp (kbutton_t *b)
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
IN_KLookDown (void)
{
	KeyDown (&in_klook);
}

static void
IN_KLookUp (void)
{
	KeyUp (&in_klook);
}

static void
IN_MLookDown (void)
{
	KeyDown (&in_mlook);
}

static void
IN_MLookUp (void)
{
	KeyUp (&in_mlook);
	if (!(in_mlook.state & 1) && lookspring->int_val)
		V_StartPitchDrift ();
}

static void
IN_UpDown (void)
{
	KeyDown (&in_up);
}

static void
IN_UpUp (void)
{
	KeyUp (&in_up);
}

static void
IN_DownDown (void)
{
	KeyDown (&in_down);
}

static void
IN_DownUp (void)
{
	KeyUp (&in_down);
}

static void
IN_LeftDown (void)
{
	KeyDown (&in_left);
}

static void
IN_LeftUp (void)
{
	KeyUp (&in_left);
}

static void
IN_RightDown (void)
{
	KeyDown (&in_right);
}

static void
IN_RightUp (void)
{
	KeyUp (&in_right);
}

static void
IN_ForwardDown (void)
{
	KeyDown (&in_forward);
}

static void
IN_ForwardUp (void)
{
	KeyUp (&in_forward);
}

static void
IN_BackDown (void)
{
	KeyDown (&in_back);
}

static void
IN_BackUp (void)
{
	KeyUp (&in_back);
}

static void
IN_LookupDown (void)
{
	KeyDown (&in_lookup);
}

static void
IN_LookupUp (void)
{
	KeyUp (&in_lookup);
}

static void
IN_LookdownDown (void)
{
	KeyDown (&in_lookdown);
}

static void
IN_LookdownUp (void)
{
	KeyUp (&in_lookdown);
}

static void
IN_MoveleftDown (void)
{
	KeyDown (&in_moveleft);
}

static void
IN_MoveleftUp (void)
{
	KeyUp (&in_moveleft);
}

static void
IN_MoverightDown (void)
{
	KeyDown (&in_moveright);
}

static void
IN_MoverightUp (void)
{
	KeyUp (&in_moveright);
}

static void
IN_SpeedDown (void)
{
	KeyDown (&in_speed);
}

static void
IN_SpeedUp (void)
{
	KeyUp (&in_speed);
}

static void
IN_StrafeDown (void)
{
	KeyDown (&in_strafe);
}

static void
IN_StrafeUp (void)
{
	KeyUp (&in_strafe);
}

static void
IN_AttackDown (void)
{
	KeyDown (&in_attack);
}

static void
IN_AttackUp (void)
{
	KeyUp (&in_attack);
}

static void
IN_UseDown (void)
{
	KeyDown (&in_use);
}

static void
IN_UseUp (void)
{
	KeyUp (&in_use);
}

static void
IN_JumpDown (void)
{
	KeyDown (&in_jump);
}

static void
IN_JumpUp (void)
{
	KeyUp (&in_jump);
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
	float       speed, up, down;

	if (in_speed.state & 1)
		speed = host_frametime * cl_anglespeedkey->value;
	else
		speed = host_frametime;

	if (!(in_strafe.state & 1)) {
		cl.viewangles[YAW] -=
			speed * cl_yawspeed->value * CL_KeyState (&in_right);
		cl.viewangles[YAW] +=
			speed * cl_yawspeed->value * CL_KeyState (&in_left);
		cl.viewangles[YAW] = anglemod (cl.viewangles[YAW]);
	}
	if (in_klook.state & 1) {
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -=
			speed * cl_pitchspeed->value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] +=
			speed * cl_pitchspeed->value * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState (&in_lookdown);

	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * up;
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * down;

	if (up || down)
		V_StopPitchDrift ();

	if (cl.viewangles[PITCH] > 80)
		cl.viewangles[PITCH] = 80;
	if (cl.viewangles[PITCH] < -70)
		cl.viewangles[PITCH] = -70;

	if (cl.viewangles[ROLL] > 50)
		cl.viewangles[ROLL] = 50;
	if (cl.viewangles[ROLL] < -50)
		cl.viewangles[ROLL] = -50;

}


/*
	CL_BaseMove

	Send the intended movement message to the server
*/
void
CL_BaseMove (usercmd_t *cmd)
{
	if (cls.signon != SIGNONS)
		return;

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
	if (chase_active->int_val == 2 || chase_active->int_val == 3)
	{
		vec3_t dir = {0,0,0}, forward, right, up, f, r;
		dir[1] = r_refdef.viewangles[1] - cl.viewangles[1];
		AngleVectors (dir, forward, right, up);
		VectorScale (forward, cmd->forwardmove, f);
		VectorScale (right,   cmd->sidemove,    r);
		cmd->forwardmove = f[0] + r[0];
		cmd->sidemove    = f[1] + r[1];
		VectorScale (forward, viewdelta.position[2], f);
		VectorScale (right,   viewdelta.position[0], r);
		viewdelta.position[2] =  f[0] + r[0];
		viewdelta.position[0] = (f[1] + r[1]) * -1;
	}

	cmd->forwardmove += viewdelta.position[2] * m_forward->value;
	cmd->sidemove += viewdelta.position[0] * m_side->value;
	cmd->upmove += viewdelta.position[1];
	cl.viewangles[PITCH] += viewdelta.angles[PITCH] * m_pitch->value;
	cl.viewangles[YAW] += viewdelta.angles[YAW] * m_yaw->value;
	cl.viewangles[ROLL] += viewdelta.angles[ROLL];

	if (freelook && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
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

	MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times

	MSG_WriteAngleV (&buf, cl.viewangles);

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
		Con_Printf ("CL_SendMove: lost server connection\n");
		CL_Disconnect ();
	}
}

void
CL_InitInput (void)
{
	Cmd_AddCommand ("+moveup", IN_UpDown, "When active the player is swimming "
					"up in a liquid");
	Cmd_AddCommand ("-moveup", IN_UpUp, "When active the player is not "
					"swimming up in a liquid");
	Cmd_AddCommand ("+movedown", IN_DownDown, "When active the player is "
					"swimming down in a liquid");
	Cmd_AddCommand ("-movedown", IN_DownUp, "When active the player is not "
					"swimming down in a liquid");
	Cmd_AddCommand ("+left", IN_LeftDown, "When active the player is turning "
					"left");
	Cmd_AddCommand ("-left", IN_LeftUp, "When active the player is not turning"
					" left");
	Cmd_AddCommand ("+right", IN_RightDown, "When active the player is "
					"turning right");
	Cmd_AddCommand ("-right", IN_RightUp, "When active the player is not "
					"turning right");
	Cmd_AddCommand ("+forward", IN_ForwardDown, "When active the player is "
					"moving forward");
	Cmd_AddCommand ("-forward", IN_ForwardUp, "When active the player is not "
					"moving forward");
	Cmd_AddCommand ("+back", IN_BackDown, "When active the player is moving "
					"backwards");
	Cmd_AddCommand ("-back", IN_BackUp, "When active the player is not "
					"moving backwards");
	Cmd_AddCommand ("+lookup", IN_LookupDown, "When active the player's view "
					"is looking up");
	Cmd_AddCommand ("-lookup", IN_LookupUp, "When active the player's view is "
					"not looking up");
	Cmd_AddCommand ("+lookdown", IN_LookdownDown, "When active the player's "
					"view is looking down");
	Cmd_AddCommand ("-lookdown", IN_LookdownUp, "When active the player's "
					"view is not looking up");
	Cmd_AddCommand ("+strafe", IN_StrafeDown, "When active, +left and +right "
					"function like +moveleft and +moveright");
	Cmd_AddCommand ("-strafe", IN_StrafeUp, "When active, +left and +right "
					"stop functioning like +moveleft and +moveright");
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown, "When active the player is "
					"strafing left");
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp, "When active the player is "
					"not strafing left");
	Cmd_AddCommand ("+moveright", IN_MoverightDown, "When active the player "
					"is strafing right");
	Cmd_AddCommand ("-moveright", IN_MoverightUp, "When active the player is "
					"not strafing right");
	Cmd_AddCommand ("+speed", IN_SpeedDown, "When active the player is "
					"running");
	Cmd_AddCommand ("-speed", IN_SpeedUp, "When active the player is not "
					"running");
	Cmd_AddCommand ("+attack", IN_AttackDown, "When active player is "
					"firing/using current weapon");
	Cmd_AddCommand ("-attack", IN_AttackUp, "When active player is not "
					"firing/using current weapon");
	Cmd_AddCommand ("+use", IN_UseDown, "Non-functional. Left over command "
					"for opening doors and triggering switches");
	Cmd_AddCommand ("-use", IN_UseUp, "Non-functional. Left over command for "
					"opening doors and triggering switches");
	Cmd_AddCommand ("+jump", IN_JumpDown, "When active the player is jumping");
	Cmd_AddCommand ("-jump", IN_JumpUp, "When active the player is not "
					"jumping");
	Cmd_AddCommand ("impulse", IN_Impulse, "Call a game function or QuakeC "
					"function.");
	Cmd_AddCommand ("+klook", IN_KLookDown, "When active, +forward and +back "
					"perform +lookup and +lookdown");
	Cmd_AddCommand ("-klook", IN_KLookUp, "When active, +forward and +back "
					"don't perform +lookup and +lookdown");
	Cmd_AddCommand ("+mlook", IN_MLookDown, "When active moving the mouse or "
					"joystick forwards and backwards performs +lookup and "
					"+lookdown");
	Cmd_AddCommand ("-mlook", IN_MLookUp, "When active moving the mouse or "
					"joystick forwards and backwards doesn't perform +lookup "
					"and +lookdown");
}
