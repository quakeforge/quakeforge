/*
	cl_cam.c

	@description@

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

#include "QF/mathlib.h"
#include "client.h"
#include "world.h"

qboolean    SV_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f,
								   vec3_t p1, vec3_t p2, trace_t *trace);

cvar_t     *chase_back;
cvar_t     *chase_up;
cvar_t     *chase_right;
cvar_t     *chase_active;

vec3_t      chase_pos;
vec3_t      chase_angles;

vec3_t      chase_dest;
vec3_t      chase_dest_angles;


void
Chase_Init (void)
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
//  start position 12 units behind head
}

void
TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t     trace;

	memset (&trace, 0, sizeof (trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);
}

void
Chase_Update (void)
{
	int         i;
	float       dist;
	vec3_t      forward, up, right;
	vec3_t      dest, stop;


	// if can't see player, reset
	AngleVectors (cl.viewangles, forward, right, up);

	// calc exact destination
	for (i = 0; i < 3; i++)
		chase_dest[i] = r_refdef.vieworg[i]
			- forward[i] * chase_back->value - right[i] * chase_right->value;
	chase_dest[2] = r_refdef.vieworg[2] + chase_up->value;

	// find the spot the player is looking at
	VectorMA (r_refdef.vieworg, 4096, forward, dest);
	TraceLine (r_refdef.vieworg, dest, stop);

	// calculate pitch to look at the same spot from camera
	VectorSubtract (stop, r_refdef.vieworg, stop);
	dist = DotProduct (stop, forward);
	if (dist < 1)
		dist = 1;
	r_refdef.viewangles[PITCH] = -atan (stop[2] / dist) / M_PI * 180;

	TraceLine (r_refdef.vieworg, chase_dest, stop);
	if (Length (stop) != 0)
		VectorCopy (stop, chase_dest);

	// move towards destination
	VectorCopy (chase_dest, r_refdef.vieworg);
}
