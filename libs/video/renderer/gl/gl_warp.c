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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/sys.h"

#include "r_cvar.h"
#include "r_shared.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

msurface_t *warpface;



void
BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	float      *v;
	int         i, j;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void
SubdividePolygon (int numverts, float *verts)
{
	float       frac, m, s, t;
	float       dist[64];
	float      *v;
	int         b, f, i, j, k;
	glpoly_t   *poly;
	vec3_t      mins, maxs;
	vec3_t      front[64], back[64];

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size->value * floor (m / gl_subdivide_size->value +
											  0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof (glpoly_t) + (numverts - 4) * VERTEXSIZE *
					   sizeof (float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
	GL_SubdivideSurface

	Breaks a polygon up along axial 64 unit
	boundaries so that turbulent and sky warps
	can be done reasonably.
*/
void
GL_SubdivideSurface (msurface_t *fa)
{
	float      *vec;
	int         lindex, numverts, i;
	vec3_t      verts[64];

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

// speed up sin calculations - Ed
float       turbsin[] = {
#	include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))

/*
	EmitWaterPolys

	Does a water warp on the pre-fragmented glpoly_t chain
*/
void
EmitWaterPolys (msurface_t *fa)
{
	float		os, ot, s, t;
	float      *v;
	int         i;
	glpoly_t   *p;
	vec3_t      nv;

	for (p = fa->polys; p; p = p->next) {
		qfglBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = v[3];
			ot = v[4];
			s = (os + turbsin[(int) ((ot * 0.125 + r_realtime) *
										 TURBSCALE) & 255]) * (1.0 / 64.0);

			t = (ot + turbsin[(int) ((os * 0.125 + r_realtime) *
										 TURBSCALE) & 255]) * (1.0 / 64.0);
			qfglTexCoord2f (s, t);

			VectorCopy (v, nv);
			nv[2] += r_waterripple->value *
				turbsin[(int) ((v[3] * 0.125 + r_realtime) * TURBSCALE) &
						255] *
				turbsin[(int) ((v[4] * 0.125 + r_realtime) * TURBSCALE) &
						255] * (1.0 / 64.0);
			qfglVertex3fv (nv);
		}
		qfglEnd ();
	}
}
