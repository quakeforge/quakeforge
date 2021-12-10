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
R_BillboardFrame (entity_t *ent, int orientation, const vec3_t cameravec,
				  vec3_t bbup, vec3_t bbright, vec3_t bbpn)
{
	vec3_t      tvec;
	float       dot;

	switch (orientation) {
		case SPR_FACING_UPRIGHT:
			// generate the sprite's axes, with vup straight up in worldspace,
			// and bbright perpendicular to cameravec.  This will not work if
			// the camera origin is directly above the entity origin
			// (cameravec is straight up or down), because the cross product
			// will be between two nearly parallel vectors and starts to
			// approach an undefined state, so we don't draw if the two
			// vectors are less than 1 degree apart
			VectorNegate (cameravec, tvec);
			VectorNormalize (tvec);
			dot = tvec[2];		// same as DotProduct (tvec, bbup) because
								// bbup is 0, 0, 1
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree)
				return 0;
			VectorSet (0, 0, 1, bbup);
			//CrossProduct(bbup, -cameravec, bbright)
			VectorSet (tvec[1], -tvec[0], 0, bbright);
			VectorNormalize (bbright);
			//CrossProduct (bbright, bbup, bbpn)
			VectorSet (-bbright[1], bbright[0], 0, bbpn);
			break;
		case SPR_VP_PARALLEL:
			// generate the sprite's axes, completely parallel to the
			// viewplane.  There are no problem situations, because the
			// sprite is always in the same orientation relative to the viewer
			VectorCopy (vup, bbup);
			VectorCopy (vright, bbright);
			VectorCopy (vpn, bbpn);
			break;
		case SPR_VP_PARALLEL_UPRIGHT:
			// generate the sprite's axes, with vup straight up in worldspace,
			// and bbright parallel to the viewplane.
			// This will not work if the view direction is very close to
			// straight up or down, because the cross product will be between
			// two nearly parallel vectors and starts to approach an undefined
			// state, so we don't draw if the two vectors are less than 1
			// degree apart
			dot = vpn[2];	// same as DotProduct (vpn, bbup) because
							// bbup is 0, 0, 1
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) =
				return 0;
			VectorSet (0, 0, 1, bbup);
			//CrossProduct(bbup, vpn, bbright)
			VectorSet (vpn[1], -vpn[0], 0, bbright);
			VectorNormalize (bbright);
			//CrossProduct (bbright, bbup, bbpn)
			VectorSet (-bbright[1], bbright[0], 0, bbpn);
			break;
		case SPR_ORIENTED:
			{
				// generate the sprite's axes, according to the sprite's world
				// orientation
				mat4f_t     mat;
				Transform_GetWorldMatrix (ent->transform, mat);
				VectorCopy (mat[0], bbpn);
				VectorNegate (mat[1], bbright);
				VectorCopy (mat[2], bbup);
			}
			break;
		case SPR_VP_PARALLEL_ORIENTED:
			{
				// generate the sprite's axes, parallel to the viewplane, but
				// rotated in that plane around the center according to the
				// sprite entity's roll angle. So vpn stays the same, but
				// vright and vup rotate
				vec4f_t     rot = Transform_GetLocalRotation (ent->transform);
				// FIXME needs proper testing (need to find, make, or fake a
				// parallel oriented sprite)
				rot = qmulf (r_refdef.viewrotation, rot);
				QuatMultVec (&rot[0], vpn, bbpn);
				QuatMultVec (&rot[0], vright, bbright);
				QuatMultVec (&rot[0], vup, bbup);
			}
			break;
		default:
			Sys_Error ("R_BillboardFrame: Bad orientation %d", orientation);
	}
	return 1;
}
