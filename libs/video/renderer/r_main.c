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

bool        r_dowarp, r_dowarpold;
bool        r_inhibit_viewmodel;
bool        r_force_fullscreen;
bool        r_paused;
double      r_realtime;
double      r_frametime;
double      r_time1;
int         r_lineadj;
bool        r_active;
int			r_init;

int         r_framecount = 1;			// so frame counts initialized to 0 don't match

vec3_t      modelorg;			// modelorg is the viewpoint relative to
								// the currently rendering entity
vec3_t      base_modelorg;
vec4f_t     r_entorigin;		// the currently rendering entity in world
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
R_SetFrustum (plane_t *frustum, const refframe_t *frame,
			  float fov_x, float fov_y)
{
	int         i;
	vec4f_t     right = frame->right;
	vec4f_t     fwd = frame->forward;
	vec4f_t     up = frame->up;

	fov_x = 90 - fov_x / 2;
	fov_y = 90 - fov_y / 2;

	// rotate FWD right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, (vec_t*)&up, (vec_t*)&fwd, -fov_x);//FIXME
	// rotate FWD left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, (vec_t*)&up, (vec_t*)&fwd, fov_x);//FIXME
	// rotate FWD up by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[2].normal, (vec_t*)&right, (vec_t*)&fwd, fov_y);//FIXME
	// rotate FWD down by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[3].normal, (vec_t*)&right, (vec_t*)&fwd, -fov_y);//FIXME

	vec4f_t     origin = frame->position;
	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = -DotProduct (origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}
