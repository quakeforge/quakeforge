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

#include "QF/checksum.h"
#include "QF/cmd.h"
#include "compat.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/teamplay.h"

#include "cl_cam.h"
#include "cl_demo.h"
#include "cl_input.h"
#include "cl_parse.h"
#include "client.h"
#include "host.h"
#include "msg_ucmd.h"
#include "view.h"

cvar_t     *cl_nodelta;

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


kbutton_t   in_mlook, in_klook;
kbutton_t   in_left, in_right, in_forward, in_back;
kbutton_t   in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t   in_strafe, in_speed, in_use, in_jump, in_attack;
kbutton_t   in_up, in_down;

int         in_impulse;


void
KeyDown (kbutton_t *b)
{
	int         k;
	char       *c;

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

void
KeyUp (kbutton_t *b)
{
	int         k;
	char       *c;

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


void
IN_KLookDown (void)
{
	KeyDown (&in_klook);
}


void
IN_KLookUp (void)
{
	KeyUp (&in_klook);
}


void
IN_MLookDown (void)
{
	KeyDown (&in_mlook);
}


void
IN_MLookUp (void)
{
	KeyUp (&in_mlook);
	if (!freelook && lookspring->int_val)
		V_StartPitchDrift ();
}


void
IN_UpDown (void)
{
	KeyDown (&in_up);
}


void
IN_UpUp (void)
{
	KeyUp (&in_up);
}


void
IN_DownDown (void)
{
	KeyDown (&in_down);
}


void
IN_DownUp (void)
{
	KeyUp (&in_down);
}


void
IN_LeftDown (void)
{
	KeyDown (&in_left);
}


void
IN_LeftUp (void)
{
	KeyUp (&in_left);
}


void
IN_RightDown (void)
{
	KeyDown (&in_right);
}


void
IN_RightUp (void)
{
	KeyUp (&in_right);
}


void
IN_ForwardDown (void)
{
	KeyDown (&in_forward);
}


void
IN_ForwardUp (void)
{
	KeyUp (&in_forward);
}


void
IN_BackDown (void)
{
	KeyDown (&in_back);
}


void
IN_BackUp (void)
{
	KeyUp (&in_back);
}


void
IN_LookupDown (void)
{
	KeyDown (&in_lookup);
}


void
IN_LookupUp (void)
{
	KeyUp (&in_lookup);
}


void
IN_LookdownDown (void)
{
	KeyDown (&in_lookdown);
}


void
IN_LookdownUp (void)
{
	KeyUp (&in_lookdown);
}


void
IN_MoveleftDown (void)
{
	KeyDown (&in_moveleft);
}


void
IN_MoveleftUp (void)
{
	KeyUp (&in_moveleft);
}


void
IN_MoverightDown (void)
{
	KeyDown (&in_moveright);
}


void
IN_MoverightUp (void)
{
	KeyUp (&in_moveright);
}


void
IN_SpeedDown (void)
{
	KeyDown (&in_speed);
}


void
IN_SpeedUp (void)
{
	KeyUp (&in_speed);
}


void
IN_StrafeDown (void)
{
	KeyDown (&in_strafe);
}


void
IN_StrafeUp (void)
{
	KeyUp (&in_strafe);
}


void
IN_AttackDown (void)
{
	KeyDown (&in_attack);
}


void
IN_AttackUp (void)
{
	KeyUp (&in_attack);
}


void
IN_UseDown (void)
{
	KeyDown (&in_use);
}


void
IN_UseUp (void)
{
	KeyUp (&in_use);
}


void
IN_JumpDown (void)
{
	KeyDown (&in_jump);
}


void
IN_JumpUp (void)
{
	KeyUp (&in_jump);
}


void
IN_Impulse (void)
{
	in_impulse = atoi (Cmd_Argv (1));
	if (Cmd_Argc () <= 2)
		return;

	Team_BestWeaponImpulse ();			// HACK HACK HACK
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


//==========================================================================

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
void
CL_AdjustAngles (void)
{
	float       speed;
	float       up, down;

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
	CL_AdjustAngles ();

	memset (cmd, 0, sizeof (*cmd));

	VectorCopy (cl.viewangles, cmd->angles);
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
//
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


int
MakeChar (int i)
{
	i &= ~3;
	if (i < -127 * 4)
		i = -127 * 4;
	if (i > 127 * 4)
		i = 127 * 4;
	return i;
}


void
CL_FinishMove (usercmd_t *cmd)
{
	int         i;
	int         ms;

// always dump the first two message, because it may contain leftover inputs
// from the last level
	if (++cl.movemessages <= 2)
		return;

// figure button bits
//  
	if (in_attack.state & 3)
		cmd->buttons |= 1;
	in_attack.state &= ~2;

	if (in_jump.state & 3)
		cmd->buttons |= 2;
	in_jump.state &= ~2;

// 1999-10-29 +USE fix by Maddes  start
	if (in_use.state & 3)
		cmd->buttons |= 4;
	in_use.state &= ~2;
// 1999-10-29 +USE fix by Maddes  end

	// send milliseconds of time to apply the move
	ms = host_frametime * 1000;
	if (ms > 250)
		ms = 100;						// time was unreasonable
	cmd->msec = ms;

	VectorCopy (cl.viewangles, cmd->angles);

	cmd->impulse = in_impulse;
	in_impulse = 0;

// chop down so no extra bits are kept that the server wouldn't get
//
	cmd->forwardmove = MakeChar (cmd->forwardmove);
	cmd->sidemove = MakeChar (cmd->sidemove);
	cmd->upmove = MakeChar (cmd->upmove);

	for (i = 0; i < 3; i++)
		cmd->angles[i] =
			((int) (cmd->angles[i] * 65536.0 / 360) & 65535) * (360.0 /
																65536.0);
}


void
CL_SendCmd (void)
{
	sizebuf_t   buf;
	byte        data[128];
	int         i;
	usercmd_t  *cmd, *oldcmd;
	int         checksumIndex;
	int         lost;
	int         seq_hash;

	if (cls.demoplayback)
		return;							// sendcmds come from the demo

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	cl.frames[i].senttime = realtime;
	cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

//	seq_hash = (cls.netchan.outgoing_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
	seq_hash = cls.netchan.outgoing_sequence;

	// get basic movement from keyboard
	CL_BaseMove (cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (); // FIXME: was cmd, should it even exist at all?

	// if we are spectator, try autocam
	if (cl.spectator)
		Cam_Track (cmd);

	CL_FinishMove (cmd);

	Cam_FinishMove (cmd);

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

	i = (cls.netchan.outgoing_sequence - 2) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] =
		COM_BlockSequenceCRCByte (buf.data + checksumIndex + 1,
								  buf.cursize - checksumIndex - 1, seq_hash);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		cl.validsequence = 0;

	if (cl.validsequence && !cl_nodelta->int_val && cls.state == ca_active &&
		!cls.demorecording) {
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence =
			cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
		MSG_WriteByte (&buf, cl.validsequence & 255);
	} else
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence =
			-1;

	if (cls.demorecording)
		CL_WriteDemoCmd (cmd);

// deliver the message
//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);
}


void
CL_Input_Init (void)
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


void
CL_Input_Init_Cvars (void)
{
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", CVAR_NONE, NULL,
						   "Disable player delta compression. Set to 1 if you "
						   "have a poor ISP and get many U_REMOVE warnings.");
}
