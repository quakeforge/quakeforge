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

#include "compat.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"


static void
R_CheckVariables (void)
{
}

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

		VID_LockBuffer ();

		R_RenderView ();

		VID_UnlockBuffer ();

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

		v2[0] = v[1] * vright[0] + v[2] * vup[0] + v[0] * vpn[0];
		v2[1] = v[1] * vright[1] + v[2] * vup[1] + v[0] * vpn[1];
		v2[2] = v[1] * vright[2] + v[2] * vup[2] + v[0] * vpn[2];

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
	out[2] = DotProduct (in, vpn);
}
#endif

void
R_TransformPlane (plane_t *p, float *normal, float *dist)
{
	float       d;

	d = DotProduct (r_origin, p->normal);
	*dist = p->dist - d;
// TODO: when we have rotating entities, this will need to use the view matrix
	TransformVector (p->normal, normal);
}

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
	int         edgecount;
	vrect_t     vrect;
	float       w, h;

	// don't allow cheats in multiplayer
	Cvar_SetValue (r_ambient, 0);
	Cvar_SetValue (r_drawflat, 0);

	if (r_numsurfs->int_val) {
		if ((surface_p - surfaces) > r_maxsurfsseen)
			r_maxsurfsseen = surface_p - surfaces;

		Sys_Printf ("Used %ld of %ld surfs; %d max\n",
					(long)(surface_p - surfaces),
					(long)(surf_max - surfaces), r_maxsurfsseen);
	}

	if (r_numedges->int_val) {
		edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Sys_Printf ("Used %d of %d edges; %d max\n", edgecount,
					r_numallocatededges, r_maxedgesseen);
	}

	r_refdef.ambientlight = max (r_ambient->value, 0);

	R_CheckVariables ();

	R_AnimateLight ();
	R_ClearEnts ();
	r_framecount++;

	numbtofpolys = 0;

	// debugging
#if 0
	r_refdef.vieworg[0] = 80;
	r_refdef.vieworg[1] = 64;
	r_refdef.vieworg[2] = 40;
	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 46.763641357;
	r_refdef.viewangles[2] = 0;
#endif

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.viewposition, modelorg);
	VectorCopy (r_refdef.viewposition, r_origin);

	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 1, 0, 0, 0 }), vpn);
	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 0, -1, 0, 0 }), vright);
	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 0, 0, 1, 0 }), vup);
	R_SetFrustum ();

	// current viewleaf
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.renderer.model);

	r_dowarpold = r_dowarp;
	r_dowarp = r_waterwarp->int_val && (r_viewleaf->contents <=
										CONTENTS_WATER);

	if ((r_dowarp != r_dowarpold) || r_viewchanged) {
		if (r_dowarp) {
			if ((vid.width <= WARP_WIDTH)
				&& (vid.height <= WARP_HEIGHT)) {
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				R_SetVrect (&vrect, &r_refdef.vrect, vr_data.lineadj);
				R_ViewChanged ();
			} else {
				w = vid.width;
				h = vid.height;

				if (w > WARP_WIDTH) {
					h *= (float) WARP_WIDTH / w;
					w = WARP_WIDTH;
				}

				if (h > WARP_HEIGHT) {
					h = WARP_HEIGHT;
					w *= (float) WARP_HEIGHT / h;
				}

				vrect.x = 0;
				vrect.y = 0;
				vrect.width = (int) w;
				vrect.height = (int) h;

				R_SetVrect (&vrect, &r_refdef.vrect,
							(int) ((float) vr_data.lineadj *
								   (h / (float) vid.height)));
				R_ViewChanged ();
			}
		} else {
			r_refdef.vrect = scr_vrect;
			R_ViewChanged ();
		}

		r_viewchanged = false;
	}
	// start off with just the four screen edge clip planes
	R_TransformFrustum ();

	// save base values
	VectorCopy (vpn, base_vpn);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

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
