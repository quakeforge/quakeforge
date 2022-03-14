/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
#include "QF/qtypes.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "r_internal.h"

qboolean    r_inhibit_viewmodel;
qboolean    r_force_fullscreen;
qboolean    r_paused;
double      r_realtime;
double      r_frametime;
entity_t   *r_view_model;
entity_t   *r_player_entity;
float       r_time1;
int         r_lineadj;
qboolean    r_active;
int			r_init;

int         r_visframecount;			// bumped when going to a new PVS
int         r_framecount = 1;			// so frame counts initialized to 0 don't match

vec3_t      modelorg;			// modelorg is the viewpoint relative to
								// the currently rendering entity
vec3_t      base_modelorg;
vec3_t      r_entorigin;		// the currently rendering entity in world
								// coordinates
// screen size info
refdef_t    r_refdef;

int         d_lightstylevalue[256];     // 8.8 fraction of base light value

byte        color_white[4] = { 255, 255, 255, 0 };	// alpha will be explicitly set
byte        color_black[4] = { 0, 0, 0, 0 };		// alpha will be explicitly set

static inline int
SignbitsForPlane (plane_t *out)
{
	int		bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

void
R_SetFrustum (void)
{
	int         i;
	vec4f_t     vright = r_refdef.frame.right;
	vec4f_t     vfwd = r_refdef.frame.forward;
	vec4f_t     vup = r_refdef.frame.up;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, &vup[0], &vfwd[0],
							 -(90 - r_refdef.fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, &vup[0], &vfwd[0],
							 90 - r_refdef.fov_x / 2);
	// rotate VPN up by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[2].normal, &vright[0], &vfwd[0],
							 90 - r_refdef.fov_y / 2);
	// rotate VPN down by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[3].normal, &vright[0], &vfwd[0],
							 -(90 - r_refdef.fov_y / 2));

	vec4f_t     origin = r_refdef.frame.position;
	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}
