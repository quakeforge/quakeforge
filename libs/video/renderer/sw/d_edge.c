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

#include "QF/cvar.h"
#include "QF/render.h"

#include "QF/scene/entity.h"

#include "d_local.h"
#include "r_internal.h"

static int  miplevel;

float       scale_for_mip;

vec3_t      transformed_modelorg;


void
D_DrawPoly (void)
{
// this driver takes spans, not polygons
}


int
D_MipLevelForScale (float scale)
{
	int         lmiplevel;

	if (scale >= d_scalemip[0])
		lmiplevel = 0;
	else if (scale >= d_scalemip[1])
		lmiplevel = 1;
	else if (scale >= d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	if (lmiplevel < d_minmip)
		lmiplevel = d_minmip;

	return lmiplevel;
}

// FIXME: clean this up

static void
D_DrawSolidSurface (surf_t *surf, int color)
{
	espan_t    *span;
	byte       *pdest;
	int         u, u2, pix;

	pix = (color << 24) | (color << 16) | (color << 8) | color;
	for (span = surf->spans; span; span = span->pnext) {
		pdest = d_viewbuffer + d_rowbytes * span->v;
		u = span->u;
		u2 = span->u + span->count - 1;
		((byte *) pdest)[u] = pix;

		if (u2 - u < 8) {
			for (u++; u <= u2; u++)
				((byte *) pdest)[u] = pix;
		} else {
			for (u++; u & 3; u++)
				((byte *) pdest)[u] = pix;

			u2 -= 4;
			for (; u <= u2; u += 4)
				*(int *) ((byte *) pdest + u) = pix;
			u2 += 4;
			for (; u <= u2; u++)
				((byte *) pdest)[u] = pix;
		}
	}
}


static void
D_CalcGradients (msurface_t *face)
{
	float       mipscale, t;
	vec3_t      p_temp1, p_saxis, p_taxis;

	mipscale = 1.0 / (float) (1 << miplevel);

	TransformVector ((vec_t*)&face->texinfo->vecs[0], p_saxis);//FIXME
	TransformVector ((vec_t*)&face->texinfo->vecs[1], p_taxis);//FIXME

	t = xscaleinv * mipscale;
	d_sdivzstepu = p_saxis[0] * t;
	d_tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	d_sdivzstepv = -p_saxis[1] * t;
	d_tdivzstepv = -p_taxis[1] * t;

	d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
		ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
		ycenter * d_tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = 0x10000 * mipscale;
	sadjust = ((fixed16_t) (DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
		((face->texturemins[0] << 16) >> miplevel)
		 + face->texinfo->vecs[0][3] * t;
	tadjust = ((fixed16_t) (DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
		((face->texturemins[1] << 16) >> miplevel)
		 + face->texinfo->vecs[1][3] * t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	bbextents = ((face->extents[0] << 16) >> miplevel) - 1;
	bbextentt = ((face->extents[1] << 16) >> miplevel) - 1;
}

static void
transform_submodel_poly (surf_t *s)
{
	// FIXME: we don't want to do all this for every polygon!
	// TODO: store once at start of frame
	vec4f_t    *transform = SW_COMP(scene_sw_matrix, s->render_id);
	vec4f_t     local_modelorg = r_refdef.frame.position - transform[3];
	TransformVector ((vec_t*)&local_modelorg, transformed_modelorg);//FIXME

	R_RotateBmodel (transform);		// FIXME: don't mess with the
								// frustum, make entity passed in
}

void
D_DrawSurfaces (void)
{
	surf_t     *s;
	msurface_t *face;
	surfcache_t *pcurrentcache;
	vec3_t      world_transformed_modelorg;

	TransformVector (modelorg, transformed_modelorg);
	VectorCopy (transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	if (r_refdef.drawflat) {
		for (s = &surfaces[1]; s < surface_p; s++) {
			if (!s->spans)
				continue;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			D_DrawSolidSurface (s, ((size_t) s->data & 0xFF));
			D_DrawZSpans (s->spans);
		}
	} else {
		for (s = &surfaces[1]; s < surface_p; s++) {
			if (!s->spans)
				continue;

			r_drawnpolycount++;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			if (s->flags & SURF_DRAWSKY) {
				if (!r_skymade) {
					R_MakeSky ();
				}

				D_DrawSkyScans (s->spans);
				D_DrawZSpans (s->spans);
			} else if (s->flags & SURF_DRAWBACKGROUND) {
				// set up a gradient for the background surface that places
				// it effectively at infinity distance from the viewpoint
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;

				D_DrawSolidSurface (s, r_clearcolor & 0xFF);
				D_DrawZSpans (s->spans);
			} else if (s->flags & SURF_DRAWTURB) {
				face = s->data;
				miplevel = 0;
				cacheblock = ((byte *) face->texinfo->texture +
							  face->texinfo->texture->offsets[0]);
				cachewidth = 64;

				if (s->insubmodel) {
					transform_submodel_poly (s);
				}

				D_CalcGradients (face);

				Turbulent (s->spans);
				D_DrawZSpans (s->spans);

				if (s->insubmodel) {
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up

					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vfwd, vfwd);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					R_TransformFrustum ();
				}
			} else {
				if (s->insubmodel) {
					transform_submodel_poly (s);
				}

				face = s->data;
				miplevel = D_MipLevelForScale (s->nearzi * scale_for_mip
											   * face->texinfo->mipadjust);

				// FIXME: make this passed in to D_CacheSurface
				pcurrentcache = D_CacheSurface (s->render_id, face, miplevel);

				cacheblock = (byte *) pcurrentcache->data;
				cachewidth = pcurrentcache->width;

				D_CalcGradients (face);

				(*d_drawspans) (s->spans);

				D_DrawZSpans (s->spans);

				if (s->insubmodel) {
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up

					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vfwd, vfwd);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					R_TransformFrustum ();
				}
			}
		}
	}
}
