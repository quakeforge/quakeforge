/*
	r_billboard.c

	Billboard frame setup

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
# include <string.h>
#endif

#include <math.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "r_internal.h"


int
R_BillboardFrame (transform_t transform, int orientation, vec4f_t cameravec,
				  vec4f_t *bbup, vec4f_t *bbright, vec4f_t *bbfwd)
{
	vec4f_t     tvec;
	float       dot;

	switch (orientation) {
		case SPR_FACING_UPRIGHT:
			// the billboard has its up vector parallel with world up, and
			// its right vector perpendicular to cameravec.
			// Undefined if the camera is too close to the entity.
			tvec = -normalf (cameravec);
			dot = tvec[2];		// DotProduct (tvec, world up)
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree)
				return 0;
			*bbup = (vec4f_t) { 0, 0, 1, 0 };
			//CrossProduct(bbup, -cameravec, bbright)
			*bbright = normalf ((vec4f_t) { tvec[1], -tvec[0], 0, 0 });
			//CrossProduct (bbright, bbup, bbfwd)
			*bbfwd = (vec4f_t) { -(*bbright)[1], (*bbright)[0], 0, 0};
			break;
		case SPR_VP_PARALLEL:
			// the billboard always has the same orientation as the camera
			*bbup = r_refdef.frame.up;
			*bbright = r_refdef.frame.right;
			*bbfwd = r_refdef.frame.forward;
			break;
		case SPR_VP_PARALLEL_UPRIGHT:
			// the billboar has its up vector parallel with world up, and
			// its right vector parallel with the view plane.
			// Undefined if the camera is looking straight up or down.
			dot = r_refdef.frame.forward[2];
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree)
				return 0;
			*bbup = (vec4f_t) { 0, 0, 1, 0 };
			*bbright = normalf ((vec4f_t) {r_refdef.frame.forward[1],
										   -r_refdef.frame.forward[0], 0, 0 });
			*bbfwd = (vec4f_t) { -(*bbright)[1], (*bbright)[0], 0, 0};
			break;
		case SPR_ORIENTED:
			{
				// The billboard's orientation is fully specified by the
				// entity's orientation.
				mat4f_t     mat;
				Transform_GetWorldMatrix (transform, mat);
				*bbfwd = mat[0];
				*bbright = mat[1];
				*bbup = mat[2];
			}
			break;
		case SPR_VP_PARALLEL_ORIENTED:
			{
				// The billboard is rotated relative to the camera using
				// the entity's local rotation.
				mat4f_t     entmat;
				mat4f_t     spmat;
				Transform_GetLocalMatrix (transform, entmat);
				// FIXME needs proper testing (need to find, make, or fake a
				// parallel oriented sprite)
				mmulf (spmat, r_refdef.camera, entmat);
				*bbfwd = spmat[0];
				*bbright = spmat[1];
				*bbup = spmat[2];
			}
			break;
		default:
			Sys_Error ("R_BillboardFrame: Bad orientation %d", orientation);
	}
	return 1;
}
