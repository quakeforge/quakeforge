/*
	cl_legacy.c

	Client legacy commands

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/11/24

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
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/plist.h"
#include "QF/sys.h"

#include "QF/input/event.h"

#include "old_keys.h"

#include "client/input.h"
#include "client/view.h"

int         cl_game_context;
int         cl_demo_context;
static int  cl_event_id;

static struct LISTENER_SET_TYPE(int) cl_on_focus_change
	= LISTENER_SET_STATIC_INIT(4);

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

cvar_t     *cl_anglespeedkey;
cvar_t     *cl_backspeed;
cvar_t     *cl_forwardspeed;
cvar_t     *cl_movespeedkey;
cvar_t     *cl_pitchspeed;
cvar_t     *cl_sidespeed;
cvar_t     *cl_upspeed;
cvar_t     *cl_yawspeed;

cvar_t     *lookspring;
cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

static void
CL_AdjustAngles (float frametime, movestate_t *ms, viewstate_t *vs)
{
	float       down, up;
	float       pitchspeed, yawspeed;
	vec4f_t     delta = {};

	pitchspeed = cl_pitchspeed->value;
	yawspeed = cl_yawspeed->value;

	if (in_speed.state & inb_down) {
		pitchspeed *= cl_anglespeedkey->value;
		yawspeed *= cl_anglespeedkey->value;
	}

	pitchspeed *= frametime;
	yawspeed *= frametime;

	if (!(in_strafe.state & inb_down)) {
		delta[YAW] -= yawspeed * IN_ButtonState (&in_right);
		delta[YAW] += yawspeed * IN_ButtonState (&in_left);
	}
	if (in_klook.state & inb_down) {
		V_StopPitchDrift (vs);
		delta[PITCH] -= pitchspeed * IN_ButtonState (&in_forward);
		delta[PITCH] += pitchspeed * IN_ButtonState (&in_back);
	}

	up = IN_ButtonState (&in_lookup);
	down = IN_ButtonState (&in_lookdown);

	delta[PITCH] -= pitchspeed * up;
	delta[PITCH] += pitchspeed * down;

	delta[PITCH] -= IN_UpdateAxis (&viewdelta_angles_pitch) * m_pitch->value;
	delta[YAW] -= IN_UpdateAxis (&viewdelta_angles_yaw) * m_yaw->value;
	delta[ROLL] -= IN_UpdateAxis (&viewdelta_angles_roll) * m_pitch->value;

	ms->angles += delta;

	if (delta[PITCH]) {
		V_StopPitchDrift (vs);
		ms->angles[PITCH] = bound (-70, ms->angles[PITCH], 80);
	}
	if (delta[ROLL]) {
		ms->angles[ROLL] = bound (-50, ms->angles[ROLL], 50);
	}
	if (delta[YAW]) {
		ms->angles[YAW] = anglemod (ms->angles[YAW]);
	}
}

static const char default_input_config[] = {
#include "libs/client/default_input.plc"
};

static void
cl_bind_f (void)
{
	int         c, i;
	const char *key;
	static dstring_t *cmd_buf;

	if (!cmd_buf) {
		cmd_buf = dstring_newstr ();
	}

	c = Cmd_Argc ();

	if (c < 2) {
		Sys_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	if (strcasecmp (Cmd_Argv (1), "ESCAPE") == 0) {
		return;
	}

	if (!(key = OK_TranslateKeyName (Cmd_Argv (1)))) {
		return;
	}

	dsprintf (cmd_buf, "in_bind imt_mod %s", key);
	if (c >= 3) {
		for (i = 2; i < c; i++) {
			dasprintf (cmd_buf, " \"%s\"", Cmd_Argv (i));
		}
	}

	Cmd_ExecuteString (cmd_buf->str, src_command);
}

static void
cl_unbind_f (void)
{
	int         c;
	const char *key;
	static dstring_t *cmd_buf;

	if (!cmd_buf) {
		cmd_buf = dstring_newstr ();
	}

	c = Cmd_Argc ();

	if (c < 2) {
		Sys_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	if (!(key = OK_TranslateKeyName (Cmd_Argv (1)))) {
		return;
	}
	dsprintf (cmd_buf, "in_unbind imt_mod %s", key);
	Cmd_ExecuteString (cmd_buf->str, src_command);
}

static void
CL_Legacy_Init (void)
{
	OK_Init ();
	Cmd_AddCommand ("bind", cl_bind_f, "compatibility wrapper for in_bind");
	Cmd_AddCommand ("unbind", cl_unbind_f, "compatibility wrapper for in_bind");
	// FIXME hashlinks
	IN_LoadConfig (PL_GetPropertyList (default_input_config, 0));
}

void
CL_Input_BuildMove (float frametime, movestate_t *state, viewstate_t *vs)
{
	if (IN_ButtonReleased (&in_mlook) && !freelook && lookspring->int_val) {
		V_StartPitchDrift (vs);
	}

	CL_AdjustAngles (frametime, state, vs);

	vec4f_t     move = {};

	if (in_strafe.state & inb_down) {
		move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_right);
		move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_left);
	}

	move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_moveright);
	move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_moveleft);

	move[UP] += cl_upspeed->value * IN_ButtonState (&in_up);
	move[UP] -= cl_upspeed->value * IN_ButtonState (&in_down);

	if (!(in_klook.state & inb_down)) {
		move[FORWARD] += cl_forwardspeed->value * IN_ButtonState (&in_forward);
		move[FORWARD] -= cl_backspeed->value * IN_ButtonState (&in_back);
	}

	// adjust for speed key
	if (in_speed.state & inb_down) {
		move *= cl_movespeedkey->value;
	}

	move[FORWARD] -= IN_UpdateAxis (&viewdelta_position_forward) * m_forward->value;
	move[SIDE] += IN_UpdateAxis (&viewdelta_position_side) * m_side->value;
	move[UP] -= IN_UpdateAxis (&viewdelta_position_up);

	if (freelook)
		V_StopPitchDrift (vs);

	//if (cl.chase
	//	&& (chase_active->int_val == 2 || chase_active->int_val == 3)) {
	//	/* adjust for chase camera angles
	//	 * makes the player move relative to the chase camera frame rather
	//	 * than the player's frame
	//	 */
	//	vec3_t      forward, right, up, f, r;
	//	vec3_t      dir = {0, 0, 0};
	//	// need separate camera and player angles
	//	//FIXME dir[1] = r_data->refdef->viewangles[1] - cl.viewstate.angles[1];
	//	AngleVectors (dir, forward, right, up);
	//	VectorScale (forward, move[FORWARD], f);
	//	VectorScale (right, move[SIDE], r);
	//	move[FORWARD] = f[0] + r[0];
	//	move[SIDE] = f[1] + r[1];
	//}
	state->move = move;
}

static void
cl_on_focus_change_redirect (void *_func, const int *game)
{
	void (*func) (int game) = _func;
	func (*game);
}

void
CL_OnFocusChange (void (*func) (int game))
{
	LISTENER_ADD (&cl_on_focus_change, cl_on_focus_change_redirect, func);
}

static int
cl_focus_event (const IE_event_t *ie_event)
{
	int         game = ie_event->type == ie_gain_focus;
	LISTENER_INVOKE (&cl_on_focus_change, &game);
	return 1;
}

static int
cl_key_event (const IE_event_t *ie_event)
{
	if (ie_event->key.code == QFK_ESCAPE) {
		Con_SetState (con_menu);
		return 1;
	}
	return 0;
}

static int
cl_event_handler (const IE_event_t *ie_event, void *unused)
{
	static int (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_key] = cl_key_event,
		[ie_gain_focus] = cl_focus_event,
		[ie_lose_focus] = cl_focus_event,
	};
	if (ie_event->type < 0 || ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return IN_Binding_HandleEvent (ie_event);
	}
	return handlers[ie_event->type] (ie_event);
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
	CL_Legacy_Init ();
}

void
CL_Input_Init_Cvars (void)
{
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL, "Snap view "
						   "to center when moving and no mlook/klook");
	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL,
						"mouse pitch (up/down) multipier");
	m_yaw =	Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE, NULL,
					  "mouse yaw (left/right) multipiler");
	m_forward = Cvar_Get ("m_forward", "1", CVAR_ARCHIVE, NULL,
						  "mouse forward/back speed");
	m_side = Cvar_Get ("m_side", "0.8", CVAR_ARCHIVE, NULL,
					   "mouse strafe speed");
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL,
								 "turn `run' speed multiplier");
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL,
							 "backward speed");
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, NULL,
								"forward speed");
	cl_movespeedkey = Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, NULL,
								"move `run' speed multiplier");
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, NULL,
							  "look up/down speed");
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, NULL,
							 "strafe speed");
	cl_upspeed = Cvar_Get ("cl_upspeed", "200", CVAR_NONE, NULL,
						   "swim/fly up/down speed");
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, NULL,
							"turning speed");
}

void
CL_Input_Activate (int in_game)
{
	IMT_SetContext (!in_game ? cl_demo_context : cl_game_context);
	IE_Set_Focus (cl_event_id);
}
