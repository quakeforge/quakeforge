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

#include "world.h"

#include "nq/include/chase.h"
#include "nq/include/client.h"

vec3_t camera_origin = {0,0,0};
vec3_t camera_angles = {0,0,0};
vec3_t player_origin = {0,0,0};
vec3_t player_angles = {0,0,0};

vec3_t      chase_angles;
vec3_t      chase_dest;
vec3_t      chase_dest_angles;
vec3_t      chase_pos;

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

static inline void
TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t     trace;

	memset (&trace, 0, sizeof (trace));
	trace.fraction = 1;
	MOD_TraceLine (cl.worldmodel->hulls, 0, start, end, &trace);

	VectorCopy (trace.endpos, impact);
}

void
Chase_Update (void)
{
	float		pitch, yaw, fwd;
	int			i;
	usercmd_t	cmd;	// movement direction
	vec3_t		forward, up, right, stop, dir;

	// lazy camera, look toward player entity

	if (chase_active->int_val == 2 || chase_active->int_val == 3) {
		// control camera angles with key/mouse/joy-look

		camera_angles[PITCH] += cl.viewangles[PITCH] - player_angles[PITCH];
		camera_angles[YAW] += cl.viewangles[YAW] - player_angles[YAW];
		camera_angles[ROLL] += cl.viewangles[ROLL] - player_angles[ROLL];

		if (chase_active->int_val == 2) {
			if (camera_angles[PITCH] < -60)
				camera_angles[PITCH] = -60;
			if (camera_angles[PITCH] >  60)
				camera_angles[PITCH] =  60;
		}

		// move camera, it's not enough to just change the angles because
		// the angles are automatically changed to look toward the player

		if (chase_active->int_val == 3)
			VectorCopy (r_data->refdef->vieworg, player_origin);

		AngleVectors (camera_angles, forward, right, up);
		VectorScale (forward, chase_back->value, forward);
		VectorSubtract (player_origin, forward, camera_origin);

		if (chase_active->int_val == 2) {
			VectorCopy (r_data->refdef->vieworg, player_origin);

			// don't let camera get too low
			if (camera_origin[2] < player_origin[2] + chase_up->value)
				camera_origin[2] = player_origin[2] + chase_up->value;
		}

		// don't let camera get too far from player

		VectorSubtract  (camera_origin, player_origin, dir);
		VectorCopy      (dir, forward);
		VectorNormalize (forward);

		if (VectorLength (dir) > chase_back->value) {
			VectorScale (forward, chase_back->value, dir);
			VectorAdd (player_origin, dir, camera_origin);
		}

		// check for walls between player and camera

		VectorScale (forward, 8, forward);
		VectorAdd (camera_origin, forward, camera_origin);
		TraceLine (player_origin, camera_origin, stop);
		if (VectorLength (stop) != 0)
			VectorSubtract (stop, forward, camera_origin);

		VectorSubtract (camera_origin, r_data->refdef->vieworg, dir);
		VectorCopy (dir, forward);
		VectorNormalize (forward);

		if (chase_active->int_val == 2) {
			if (dir[1] == 0 && dir[0] == 0) {
				// look straight up or down
//				camera_angles[YAW] = r_data->refdef->viewangles[YAW];
				if (dir[2] > 0)
					camera_angles[PITCH] = 90;
				else
					camera_angles[PITCH] = 270;
			} else {
				yaw = (atan2 (dir[1], dir[0]) * 180 / M_PI);
				if (yaw < 0)
					yaw += 360;
				if (yaw < 180)
					yaw += 180;
				else
					yaw -= 180;
				camera_angles[YAW] = yaw;

				fwd = sqrt (dir[0] * dir[0] + dir[1] * dir[1]);
				pitch = (atan2 (dir[2], fwd) * 180 / M_PI);
				if (pitch < 0)
					pitch += 360;
				camera_angles[PITCH] = pitch;
			}
		}

		VectorCopy (camera_angles, r_data->refdef->viewangles);// rotate camera
		VectorCopy (camera_origin, r_data->refdef->vieworg);    // move camera

		// get basic movement from keyboard

		memset (&cmd, 0, sizeof (cmd));
//		VectorCopy (cl.viewangles, cmd.angles);

		if (in_strafe.state & 1) {
			cmd.sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
			cmd.sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
		}
		cmd.sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
		cmd.sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

		if (!(in_klook.state & 1)) {
			cmd.forwardmove += cl_forwardspeed->value
				* CL_KeyState (&in_forward);
			cmd.forwardmove -= cl_backspeed->value * CL_KeyState (&in_back);
		}
		if (in_speed.state & 1) {
			cmd.forwardmove *= cl_movespeedkey->value;
			cmd.sidemove    *= cl_movespeedkey->value;
		}

		// mouse and joystick controllers add to movement
		VectorSet (0, cl.viewangles[1] - camera_angles[1], 0, dir);
		AngleVectors (dir, forward, right, up);
		VectorScale  (forward, viewdelta.position[2] * m_forward->value,
					  forward);
		VectorScale  (right, viewdelta.position[0] * m_side->value, right);
		VectorAdd    (forward, right, dir);
		cmd.forwardmove += dir[0];
		cmd.sidemove    -= dir[1];

		VectorSet (0, camera_angles[1], 0, dir);
		AngleVectors (dir, forward, right, up);

		VectorScale (forward, cmd.forwardmove, forward);
		VectorScale (right,   cmd.sidemove,    right);
		VectorAdd   (forward, right, dir);

		if (dir[1] || dir[0]) {
			cl.viewangles[YAW] = (atan2 (dir[1], dir[0]) * 180 / M_PI);
			if (cl.viewangles[YAW] <   0) cl.viewangles[YAW] += 360;
//			if (cl.viewangles[YAW] < 180)
//				cl.viewangles[YAW] += 180;
//			else
//				cl.viewangles[YAW] -= 180;
		}

		cl.viewangles[PITCH] = 0;

		// remember the new angle to calculate the difference next frame
		VectorCopy (cl.viewangles, player_angles);

		return;
	}

	// regular camera, faces same direction as player

	AngleVectors (cl.viewangles, forward, right, up);

	// calc exact destination
	for (i = 0; i < 3; i++)
		camera_origin[i] = r_data->refdef->vieworg[i]
			- forward[i] * chase_back->value - right[i] * chase_right->value;
	camera_origin[2] += chase_up->value;

	// check for walls between player and camera
	TraceLine (r_data->refdef->vieworg, camera_origin, stop);
	if (VectorLength (stop) != 0)
		for (i = 0; i < 3; i++)
			camera_origin[i] = stop[i] + forward[i] * 8;

	VectorCopy (camera_origin, r_data->refdef->vieworg);
}
