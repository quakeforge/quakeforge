/*
	d_edge.c

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

#include "QF/cvar.h"
#include "QF/entity.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

static int  miplevel;

float       sw32_scale_for_mip;

static vec3_t transformed_modelorg;


void
sw32_D_DrawPoly (void)
{
// this driver takes spans, not polygons
}


int
sw32_D_MipLevelForScale (float scale)
{
	int         lmiplevel;

	if (scale >= sw32_d_scalemip[0])
		lmiplevel = 0;
	else if (scale >= sw32_d_scalemip[1])
		lmiplevel = 1;
	else if (scale >= sw32_d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	if (lmiplevel < sw32_d_minmip)
		lmiplevel = sw32_d_minmip;

	return lmiplevel;
}

// FIXME: clean this up

static void
D_DrawSolidSurface (surf_t *surf, int color)
{
	espan_t *span;

	switch(sw32_ctx->pixbytes) {
	case 1:
	{
		byte *pdest, pix;
		int u, u2;

		pix = color;
		for (span = surf->spans; span; span = span->pnext)
		{
			pdest = (byte *) sw32_d_viewbuffer + sw32_screenwidth * span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			for (;u <= u2; u++)
				pdest[u] = pix;
		}
	}
	break;
	case 2:
	{
		short *pdest, pix;
		int u, u2;

		pix = sw32_8to16table[color];
		for (span = surf->spans; span; span = span->pnext)
		{
			pdest = (short *) sw32_d_viewbuffer + sw32_screenwidth * span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			for (;u <= u2; u++)
				pdest[u] = pix;
		}
	}
	break;
	case 4:
	{
		int *pdest, pix;
		int u, u2;

		pix = d_8to24table[color];
		for (span = surf->spans; span; span = span->pnext)
		{
			pdest = (int *) sw32_d_viewbuffer + sw32_screenwidth * span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			for (;u <= u2; u++)
				pdest[u] = pix;
		}
	}
	break;
	default:
		Sys_Error("D_DrawSolidSurface: unsupported r_pixbytes %i",
				  sw32_ctx->pixbytes);
	}
}


static void
D_CalcGradients (msurface_t *pface)
{
	float       mipscale, t;
	vec3_t      p_temp1, p_saxis, p_taxis;

	mipscale = 1.0 / (float) (1 << miplevel);

	sw32_TransformVector (pface->texinfo->vecs[0], p_saxis);
	sw32_TransformVector (pface->texinfo->vecs[1], p_taxis);

	t = sw32_xscaleinv * mipscale;
	sw32_d_sdivzstepu = p_saxis[0] * t;
	sw32_d_tdivzstepu = p_taxis[0] * t;

	t = sw32_yscaleinv * mipscale;
	sw32_d_sdivzstepv = -p_saxis[1] * t;
	sw32_d_tdivzstepv = -p_taxis[1] * t;

	sw32_d_sdivzorigin = p_saxis[2] * mipscale -
		sw32_xcenter * sw32_d_sdivzstepu -
		sw32_ycenter * sw32_d_sdivzstepv;
	sw32_d_tdivzorigin = p_taxis[2] * mipscale -
		sw32_xcenter * sw32_d_tdivzstepu -
		sw32_ycenter * sw32_d_tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = 0x10000 * mipscale;
	sw32_sadjust = ((fixed16_t) (DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
		((pface->texturemins[0] << 16) >> miplevel)
		 + pface->texinfo->vecs[0][3] * t;
	sw32_tadjust = ((fixed16_t) (DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
		((pface->texturemins[1] << 16) >> miplevel)
		 + pface->texinfo->vecs[1][3] * t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	sw32_bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	sw32_bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}


void
sw32_D_DrawSurfaces (void)
{
	surf_t     *s;
	msurface_t *pface;
	surfcache_t *pcurrentcache;
	vec3_t      world_transformed_modelorg;
	vec3_t      local_modelorg;

	currententity = &r_worldentity;
	sw32_TransformVector (modelorg, transformed_modelorg);
	VectorCopy (transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	if (r_drawflat->int_val) {
		for (s = &sw32_surfaces[1]; s < sw32_surface_p; s++) {
			if (!s->spans)
				continue;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			D_DrawSolidSurface (s, ((intptr_t) s->data & 0xFF));
			sw32_D_DrawZSpans (s->spans);
		}
	} else {
		for (s = &sw32_surfaces[1]; s < sw32_surface_p; s++) {
			if (!s->spans)
				continue;

			sw32_r_drawnpolycount++;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			if (s->flags & SURF_DRAWSKY) {
				if (!sw32_r_skymade) {
					sw32_R_MakeSky ();
				}

				sw32_D_DrawSkyScans (s->spans);
				sw32_D_DrawZSpans (s->spans);
			} else if (s->flags & SURF_DRAWBACKGROUND) {
				// set up a gradient for the background surface that places
				// it effectively at infinity distance from the viewpoint
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;

				D_DrawSolidSurface (s, r_clearcolor->int_val & 0xFF);
				sw32_D_DrawZSpans (s->spans);
			} else if (s->flags & SURF_DRAWTURB) {
				pface = s->data;
				miplevel = 0;
				sw32_cacheblock = ((byte *) pface->texinfo->texture +
							  pface->texinfo->texture->offsets[0]);
				sw32_cachewidth = 64;

				if (s->insubmodel) {
					// FIXME: we don't want to do all this for every polygon!
					// TODO: store once at start of frame
					currententity = s->entity;	// FIXME: make this passed in
												// to sw32_R_RotateBmodel ()
					VectorSubtract (r_origin,
						Transform_GetWorldPosition (currententity->transform),
									local_modelorg);
					sw32_TransformVector (local_modelorg, transformed_modelorg);

					sw32_R_RotateBmodel ();	// FIXME: don't mess with the
										// frustum, make entity passed in
				}

				D_CalcGradients (pface);

				sw32_Turbulent (s->spans);
				sw32_D_DrawZSpans (s->spans);

				if (s->insubmodel) {
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up

					currententity = &r_worldentity;
					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					sw32_R_TransformFrustum ();
				}
			} else {
				if (s->insubmodel) {
					// FIXME: we don't want to do all this for every polygon!
					// TODO: store once at start of frame
					currententity = s->entity;	// FIXME: make this passed in
												// to sw32_R_RotateBmodel ()
					VectorSubtract (r_origin,
						Transform_GetWorldPosition (currententity->transform),
									local_modelorg);
					sw32_TransformVector (local_modelorg, transformed_modelorg);

					sw32_R_RotateBmodel ();	// FIXME: don't mess with the
										// frustum, make entity passed in
				}

				pface = s->data;
				miplevel = sw32_D_MipLevelForScale (s->nearzi * sw32_scale_for_mip
											   * pface->texinfo->mipadjust);

				// FIXME: make this passed in to D_CacheSurface
				pcurrentcache = sw32_D_CacheSurface (pface, miplevel);

				sw32_cacheblock = (byte *) pcurrentcache->data;
				sw32_cachewidth = pcurrentcache->width;

				D_CalcGradients (pface);

				sw32_D_DrawSpans (s->spans);

				sw32_D_DrawZSpans (s->spans);

				if (s->insubmodel) {
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up

					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					sw32_R_TransformFrustum ();
					currententity = &r_worldentity;
				}
			}
		}
	}
}
