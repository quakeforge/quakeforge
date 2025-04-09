/*
	sw_rmisc.c

	(description)

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/ui/view.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

/*
	R_TimeRefresh_f

	For program optimization
*/
void
R_TimeRefresh_f (void)
{
/* FIXME update for simd
	int         i;
	float       start, stop, time;
	int         startangle;
	vrect_t     vr;

	startangle = r_refdef.viewangles[1];

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i / 128.0 * 360.0;

		R_RenderView ();

		vr.x = r_refdef.vrect.x;
		vr.y = r_refdef.vrect.y;
		vr.width = r_refdef.vrect.width;
		vr.height = r_refdef.vrect.height;
		vr.next = NULL;
		sw_ctx->update (&vr);
	}
	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);

	r_refdef.viewangles[1] = startangle;
*/
}

void
R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Sys_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	R_LoadSkys (Cmd_Argv (1));
}

void
R_PrintTimes (void)
{
	float       r_time2;
	float       ms;

	r_time2 = Sys_DoubleTime ();

	ms = 1000 * (r_time2 - r_time1);

	Sys_Printf ("%5.1f ms %3i/%3i/%3i poly %3i surf\n",
				ms, c_faceclip, r_polycount, r_drawnpolycount, c_surf);
	c_surf = 0;
}

void
R_PrintAliasStats (void)
{
	Sys_Printf ("%3i polygon model drawn\n", r_amodels_drawn);
}

void
R_TransformFrustum (void)
{
	int         i;
	vec3_t      v, v2;

	for (i = 0; i < 4; i++) {
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1] * vright[0] + v[2] * vup[0] + v[0] * vfwd[0];
		v2[1] = v[1] * vright[1] + v[2] * vup[1] + v[0] * vfwd[1];
		v2[2] = v[1] * vright[2] + v[2] * vup[2] + v[0] * vfwd[2];

		VectorCopy (v2, view_clipplanes[i].normal);

		view_clipplanes[i].dist = DotProduct (modelorg, v2);
	}
}

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifndef USE_INTEL_ASM
void
TransformVector (const vec3_t in, vec3_t out)
{
	out[0] = DotProduct (in, vright);
	out[1] = DotProduct (in, vup);
	out[2] = DotProduct (in, vfwd);
}
#endif

static void
R_SetUpFrustumIndexes (void)
{
	int         i, j, *pindex;

	pindex = r_frustum_indexes;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 3; j++) {
			if (view_clipplanes[i].normal[j] < 0) {
				pindex[j] = j;
				pindex[j + 3] = j + 3;
			} else {
				pindex[j] = j + 3;
				pindex[j + 3] = j;
			}
		}

		// FIXME: do just once at start
		pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}

void
R_SetupFrame (void)
{
	numbtofpolys = 0;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.frame.position, modelorg);

	VectorCopy (r_refdef.frame.right, vright);
	VectorCopy (r_refdef.frame.forward, vfwd);
	VectorCopy (r_refdef.frame.up, vup);

	// start off with just the four screen edge clip planes
	R_TransformFrustum ();

	// save base values
	VectorCopy (vfwd, base_vfwd);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

	VectorCopy (vright, r_viewmatrix[0]);
	VectorNegate (vup, r_viewmatrix[1]);
	VectorCopy (vfwd, r_viewmatrix[2]);

	R_SetSkyFrame ();

	R_SetUpFrustumIndexes ();

	r_cache_thrash = false;

	// clear frame counts
	c_faceclip = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_amodels_drawn = 0;
	r_outofsurfaces = 0;
	r_outofedges = 0;

	D_SetupFrame ();
}
