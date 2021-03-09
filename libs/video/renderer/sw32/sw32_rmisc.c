/*
	sw32_rmisc.c

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

#define NH_DEFINE
#include "namehack.h"

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
sw32_R_TimeRefresh_f (void)
{
	int         i;
	float       start, stop, time;
	int         startangle;
	vrect_t     vr;

	startangle = r_refdef.viewangles[1];

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i / 128.0 * 360.0;

		VID_LockBuffer ();

		sw32_R_RenderView ();

		VID_UnlockBuffer ();

		vr.x = r_refdef.vrect.x;
		vr.y = r_refdef.vrect.y;
		vr.width = r_refdef.vrect.width;
		vr.height = r_refdef.vrect.height;
		vr.next = NULL;
		sw32_ctx->update (&vr);
	}
	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);

	r_refdef.viewangles[1] = startangle;
}

void
sw32_R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Sys_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	sw32_R_LoadSkys (Cmd_Argv (1));
}

void
sw32_R_PrintTimes (void)
{
	float       r_time2;
	float       ms;

	r_time2 = Sys_DoubleTime ();

	ms = 1000 * (r_time2 - r_time1);

	Sys_Printf ("%5.1f ms %3i/%3i/%3i poly %3i surf\n",
				ms, sw32_c_faceclip, sw32_r_polycount, sw32_r_drawnpolycount, sw32_c_surf);
	sw32_c_surf = 0;
}

void
sw32_R_PrintAliasStats (void)
{
	Sys_Printf ("%3i polygon model drawn\n", sw32_r_amodels_drawn);
}

void
sw32_R_TransformFrustum (void)
{
	int         i;
	vec3_t      v, v2;

	for (i = 0; i < 4; i++) {
		v[0] = sw32_screenedge[i].normal[2];
		v[1] = -sw32_screenedge[i].normal[0];
		v[2] = sw32_screenedge[i].normal[1];

		v2[0] = v[1] * vright[0] + v[2] * vup[0] + v[0] * vpn[0];
		v2[1] = v[1] * vright[1] + v[2] * vup[1] + v[0] * vpn[1];
		v2[2] = v[1] * vright[2] + v[2] * vup[2] + v[0] * vpn[2];

		VectorCopy (v2, sw32_view_clipplanes[i].normal);

		sw32_view_clipplanes[i].dist = DotProduct (modelorg, v2);
	}
}

void
sw32_TransformVector (const vec3_t in, vec3_t out)
{
	out[0] = DotProduct (in, vright);
	out[1] = DotProduct (in, vup);
	out[2] = DotProduct (in, vpn);
}

void
sw32_R_TransformPlane (plane_t *p, float *normal, float *dist)
{
	float       d;

	d = DotProduct (r_origin, p->normal);
	*dist = p->dist - d;
// TODO: when we have rotating entities, this will need to use the view matrix
	sw32_TransformVector (p->normal, normal);
}

static void
R_SetUpFrustumIndexes (void)
{
	int         i, j, *pindex;

	pindex = sw32_r_frustum_indexes;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 3; j++) {
			if (sw32_view_clipplanes[i].normal[j] < 0) {
				pindex[j] = j;
				pindex[j + 3] = j + 3;
			} else {
				pindex[j] = j + 3;
				pindex[j + 3] = j;
			}
		}

		// FIXME: do just once at start
		sw32_pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}

void
sw32_R_SetupFrame (void)
{
	int         edgecount;
	vrect_t     vrect;
	float       w, h;

	// don't allow cheats in multiplayer
	Cvar_SetValue (r_ambient, 0);
	Cvar_SetValue (r_drawflat, 0);

	if (r_numsurfs->int_val) {
		if ((sw32_surface_p - sw32_surfaces) > sw32_r_maxsurfsseen)
			sw32_r_maxsurfsseen = sw32_surface_p - sw32_surfaces;

		Sys_Printf ("Used %ld of %ld surfs; %d max\n",
					(long) (sw32_surface_p - sw32_surfaces),
					(long) (sw32_surf_max - sw32_surfaces), sw32_r_maxsurfsseen);
	}

	if (r_numedges->int_val) {
		edgecount = sw32_edge_p - sw32_r_edges;

		if (edgecount > sw32_r_maxedgesseen)
			sw32_r_maxedgesseen = edgecount;

		Sys_Printf ("Used %d of %d edges; %d max\n", edgecount,
					sw32_r_numallocatededges, sw32_r_maxedgesseen);
	}

	r_refdef.ambientlight = max (r_ambient->value, 0);

	R_CheckVariables ();

	R_AnimateLight ();
	R_ClearEnts ();
	r_framecount++;

	sw32_numbtofpolys = 0;

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
	VectorCopy (r_refdef.vieworg, modelorg);
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	R_SetFrustum ();

	// current viewleaf
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.renderer.model);

	sw32_r_dowarpold = sw32_r_dowarp;
	sw32_r_dowarp = r_waterwarp->int_val && (r_viewleaf->contents <=
										CONTENTS_WATER);

	if ((sw32_r_dowarp != sw32_r_dowarpold) || sw32_r_viewchanged) {
		if (sw32_r_dowarp) {
			if ((vid.width <= WARP_WIDTH)
				&& (vid.height <= WARP_HEIGHT)) {
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				R_SetVrect (&vrect, &r_refdef.vrect, vr_data.lineadj);
				sw32_R_ViewChanged (vid.aspect);
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
				sw32_R_ViewChanged (vid.aspect * (h / w) * ((float) vid.width /
													   (float) vid.height));
			}
		} else {
			vrect.x = 0;
			vrect.y = 0;
			vrect.width = vid.width;
			vrect.height = vid.height;

			r_refdef.vrect = scr_vrect;
			sw32_R_ViewChanged (vid.aspect);
		}

		sw32_r_viewchanged = false;
	}
	// start off with just the four screen edge clip planes
	sw32_R_TransformFrustum ();

	// save base values
	VectorCopy (vpn, base_vpn);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

	sw32_R_SetSkyFrame ();

	R_SetUpFrustumIndexes ();

	r_cache_thrash = false;

	// clear frame counts
	sw32_c_faceclip = 0;
	sw32_r_polycount = 0;
	sw32_r_drawnpolycount = 0;
	sw32_r_amodels_drawn = 0;
	sw32_r_outofsurfaces = 0;
	sw32_r_outofedges = 0;

	sw32_D_SetupFrame ();
}
