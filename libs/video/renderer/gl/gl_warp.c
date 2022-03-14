/*
	gl_warp.c

	water polygons

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

#include "QF/cvar.h"
#include "QF/sys.h"

#include "r_internal.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rsurf.h"

// speed up sin calculations - Ed
static float turbsin[] = {
#	include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))
#define TURBFRAC (32.0 / (2 * M_PI))		// an 8th of TURBSCALE

/*
	GL_EmitWaterPolys

	Does a water warp on the pre-fragmented glpoly_t chain
*/
void
GL_EmitWaterPolys (msurface_t *surf)
{
	float		os, ot, s, t, timetemp;
	float      *v;
	int         i;
	glpoly_t   *p;

	timetemp = vr_data.realtime * TURBSCALE;

	for (p = surf->polys; p; p = p->next) {
		qfglBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = turbsin[(int) (v[3] * TURBFRAC + timetemp) & 255];
			ot = turbsin[(int) (v[4] * TURBFRAC + timetemp) & 255];
			s = (v[3] + ot) * (1.0 / 64.0);
			t = (v[4] + os) * (1.0 / 64.0);
			qfglTexCoord2f (s, t);

			if (r_waterripple->value != 0) {
				vec3_t		nv;

				VectorCopy (v, nv);
				nv[2] += r_waterripple->value * os * ot * (1.0 / 64.0);
				qfglVertex3fv (nv);
			} else
				qfglVertex3fv (v);
		}
		qfglEnd ();
	}
}
