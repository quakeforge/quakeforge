/*
	cl_chase.c

	chase camera support

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

#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/input.h"
#include "QF/mathlib.h"

#include "QF/plugin/vid_render.h"
#include "QF/scene/transform.h"

#include "world.h"

#include "client/chase.h"
#include "client/input.h"
#include "client/view.h"


cvar_t     *chase_back;
cvar_t     *chase_up;
cvar_t     *chase_right;
cvar_t     *chase_active;


void
Chase_Init_Cvars (void)
{
	chase_back = Cvar_Get ("chase_back", "100", CVAR_NONE, NULL, "None");
	chase_up = Cvar_Get ("chase_up", "16", CVAR_NONE, NULL, "None");
	chase_right = Cvar_Get ("chase_right", "0", CVAR_NONE, NULL, "None");
	chase_active = Cvar_Get ("chase_active", "0", CVAR_NONE, NULL, "None");
}

void
Chase_Reset (void)
{
	// for respawning and teleporting
	// start position 12 units behind head
}

static inline vec4f_t
TraceLine (chasestate_t *cs, vec4f_t start, vec4f_t end)
{
	trace_t     trace;

	memset (&trace, 0, sizeof (trace));
	trace.fraction = 1;
	MOD_TraceLine (cs->worldmodel->brush.hulls, 0, &start[0], &end[0], &trace);

	return (vec4f_t) {trace.endpos[0], trace.endpos[1], trace.endpos[2], 1};
}

static void
check_for_walls (chasestate_t *cs, vec4f_t forward)
{
	// check for walls between player and camera
	cs->camera_origin += 8 * forward;
	vec4f_t     stop = TraceLine (cs, cs->player_origin, cs->camera_origin);
	if (magnitude3f (stop)[0] != 0) {
		cs->camera_origin = stop - forward;
	}
}


static void
limit_distance (chasestate_t *cs)
{
	// don't let camera get too far from player

	vec4f_t     dir = cs->camera_origin - cs->player_origin;
	vec4f_t     forward = normalf (dir);

	if (magnitudef (dir)[0] > chase_back->value) {
		cs->camera_origin = cs->player_origin + forward * chase_back->value;
	}
}

static void
set_camera (chasestate_t *cs, viewstate_t *vs)
{
	vec4f_t     rotation;
	AngleQuat (cs->camera_angles, &rotation[0]);//FIXME
	Transform_SetWorldRotation (vs->camera_transform, rotation);
	Transform_SetWorldPosition (vs->camera_transform, cs->camera_origin);
}

static void
cam_controls (chasestate_t *cs, viewstate_t *vs)
{
	// get basic movement from keyboard
	vec4f_t     move = { };
	vec4f_t     forward = { };
	vec4f_t     right = { };
	vec4f_t     up = { };
	vec4f_t     dir = { };

	if (in_strafe.state & 1) {
		move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_right);
		move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_left);
	}
	move[SIDE] += cl_sidespeed->value * IN_ButtonState (&in_moveright);
	move[SIDE] -= cl_sidespeed->value * IN_ButtonState (&in_moveleft);

	if (!(in_klook.state & 1)) {
		move[FORWARD] += cl_forwardspeed->value
			* IN_ButtonState (&in_forward);
		move[FORWARD] -= cl_backspeed->value * IN_ButtonState (&in_back);
	}
	if (in_speed.state & 1) {
		move *= cl_movespeedkey->value;
	}

	// mouse and joystick controllers add to movement
	VectorSet (0, vs->player_angles[1] - cs->camera_angles[1], 0, dir);
	AngleVectors (&dir[0], &forward[0], &right[0], &up[0]); //FIXME
	forward *= IN_UpdateAxis (&in_cam_forward) * m_forward->value;
	right *= IN_UpdateAxis (&in_cam_side) * m_side->value;
	dir = forward + right;
	move[FORWARD] += dir[0];
	move[SIDE]    -= dir[1];

	VectorSet (0, cs->camera_angles[1], 0, dir);
	AngleVectors (&dir[0], &forward[0], &right[0], &up[0]); //FIXME

	dir = forward * move[FORWARD] + right * move[SIDE];

	if (dir[1] || dir[0]) {
		vs->player_angles[YAW] = (atan2 (dir[1], dir[0]) * 180 / M_PI);
	}

	vs->player_angles[PITCH] = 0;
}

static void
update_cam_frame (chasestate_t *cs, viewstate_t *vs)
{
	vec3_t      d;
	VectorSubtract (vs->player_angles, cs->player_angles, d);
	VectorAdd (cs->camera_angles, d, cs->camera_angles);
	// remember the new angle to calculate the difference next frame
	VectorCopy (vs->player_angles, cs->player_angles);
}

static void
chase_mode_1 (chasestate_t *cs)
{
	// regular camera, faces same direction as player
	viewstate_t *vs = cs->viewstate;
	vec4f_t	    forward = {}, up = {}, right = {}, stop = {};
	//FIXME
	AngleVectors (vs->player_angles, &forward[0], &right[0], &up[0]);

	// calc exact destination
	cs->camera_origin = vs->player_origin
		- forward * chase_back->value - right * chase_right->value;
	// chase_up is world up
	cs->camera_origin[2] += chase_up->value;

	// check for walls between player and camera
	stop = TraceLine (cs, vs->player_origin, cs->camera_origin);
	if (magnitude3f (stop)[0] != 0) {
		cs->camera_origin = stop + forward * 8;
	}

	Transform_SetWorldPosition (vs->camera_transform, cs->camera_origin);
}

static void
chase_mode_2 (chasestate_t *cs)
{
	viewstate_t *vs = cs->viewstate;
	vec4f_t	    forward = {}, up = {}, right = {}, dir = {};

	// lazy camera, look toward player entity

	update_cam_frame (cs, vs);

	cs->camera_angles[PITCH] = bound (-60, cs->camera_angles[PITCH], 60);

	// move camera, it's not enough to just change the angles because
	// the angles are automatically changed to look toward the player
	AngleVectors (cs->camera_angles, &forward[0], &right[0], &up[0]);
	cs->camera_origin = cs->player_origin - chase_back->value * forward;

	cs->player_origin = vs->player_origin;

	// don't let camera get too low
	if (cs->camera_origin[2] < cs->player_origin[2] + chase_up->value) {
		cs->camera_origin[2] = cs->player_origin[2] + chase_up->value;
	}

	limit_distance (cs);
	check_for_walls (cs, forward);

	dir = vs->player_origin - cs->camera_origin;

	if (dir[1] == 0 && dir[0] == 0) {
		// look straight up or down
		cs->camera_angles[YAW] = vs->player_angles[YAW];
		if (dir[2] > 0)
			cs->camera_angles[PITCH] = 90;
		else
			cs->camera_angles[PITCH] = -90;
	} else {
		float       pitch, yaw, fwd;
		yaw = (atan2 (dir[1], dir[0]) * 180 / M_PI);
		cs->camera_angles[YAW] = yaw;

		fwd = sqrt (dir[0] * dir[0] + dir[1] * dir[1]);
		pitch = -(atan2 (dir[2], fwd) * 180 / M_PI);
		cs->camera_angles[PITCH] = pitch;
	}
	set_camera (cs, vs);
	cam_controls (cs, vs);
}

static void
chase_mode_3 (chasestate_t *cs)
{
	viewstate_t *vs = cs->viewstate;
	vec4f_t	    forward = {}, up = {}, right = {};

	// lazy camera, look toward player entity
	update_cam_frame (cs, vs);

	// move camera, it's not enough to just change the angles because
	// the angles are automatically changed to look toward the player

	cs->player_origin = vs->player_origin;
	AngleVectors (cs->camera_angles, &forward[0], &right[0], &up[0]);
	cs->camera_origin = cs->player_origin - chase_back->value * forward;
	limit_distance (cs);
	check_for_walls (cs, forward);
	set_camera (cs, vs);
	cam_controls (cs, vs);
}

void
Chase_Update (chasestate_t *cs)
{
	switch (chase_active->int_val) {
		case 1:
			chase_mode_1 (cs);
			return;
		case 2:
			chase_mode_2 (cs);
			return;
		case 3:
			chase_mode_3 (cs);
			return;
	}
}
