/*
	vulkan_lightmap.c

	surface-related refresh code

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Joseph Carter <knghtbrd@debian.org>
	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <stdio.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/scrap.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#define LUXEL_SIZE 4

static inline void
add_dynamic_lights (msurface_t *surf, float *block)
{
	unsigned    lnum;
	int         sd, td;
	float       dist, rad, minlight;
	vec3_t      impact, local, lightorigin;
	int         smax, tmax;
	int         s, t;
	mtexinfo_t *tex;
	plane_t    *plane;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;
	plane = surf->plane;

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		dlight_t   *light = &r_dlights[lnum];

		VectorSubtract (light->origin, currententity->origin, lightorigin);
		rad = light->radius;
		dist = DotProduct (lightorigin, plane->normal) - plane->dist;
		rad -= fabs (dist);

		minlight = light->minlight;
		if (rad < minlight) {
			continue;
		}
		VectorMultSub (light->origin, dist, plane->normal, impact);

		local[0] = DotProduct (impact,	tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact,	tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++) {
			td = local[1] - t * 16;
			if (td < 0) {
				td = -td;
			}
			for (s = 0; s < smax; s++) {
				sd = local[0] - s * 16;
				if (sd < 0) {
					sd = -sd;
				}
				if (sd > td) {
					dist = sd + (td >> 1);
				} else {
					dist = td + (sd >> 1);
				}
				if (dist < minlight) {
					float *out = block + (t * smax + s) * LUXEL_SIZE;
					float l = (rad - dist) * 256;
					VectorMultAdd (out, l, light->color, out);
					out[3] = 1;
					out += LUXEL_SIZE;
				}
			}
		}
	}
}

void
Vulkan_BuildLightMap (msurface_t *surf, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	int         smax, tmax, size;
	unsigned    scale;
	int         i;
	float      *out, *block;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax * LUXEL_SIZE;

	block = QFV_SubpicBatch (surf->lightpic, bctx->light_stage);

	// set to full bright if no light data
	if (!r_worldentity.model->lightdata) {
		out = block;
		while (size-- > 0) {
			*out++ = 1;
		}
		return;
	}

	// clear to no light
	memset (block, 0, size * sizeof(float));

	// add all the lightmaps
	if (surf->samples) {
		byte		   *lightmap;

		lightmap = surf->samples;
		for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;				// 8.8 fraction
			out = block;
			for (i = 0; i < smax * tmax; i++) {
				*out++ += *lightmap++ * scale / 65536.0;
				*out++ += *lightmap++ * scale / 65536.0;
				*out++ += *lightmap++ * scale / 65536.0;
				*out++ = 1;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount) {
		add_dynamic_lights (surf, block);
	}
}

void
Vulkan_CalcLightmaps (vulkan_ctx_t *ctx)
{
/*	int         i;

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!gl_lightmap_polys[i])
			continue;
		if (gl_lightmap_modified[i]) {
			qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures + i);
			GL_UploadLightmap (i);
			gl_lightmap_modified[i] = false;
		}
	}*/
}

static void
vulkan_create_surf_lightmap (msurface_t *surf, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	int         smax, tmax;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightpic = QFV_ScrapSubpic (bctx->light_scrap, smax, tmax);
	if (!surf->lightpic) {
		Sys_Error ("FIXME taniwha is being lazy");
	}
}

/*
  GL_BuildLightmaps

  Builds the lightmap texture with all the surfaces from all brush models
*/
void
Vulkan_BuildLightmaps (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	int         i, j;
	model_t    *m;

	QFV_ScrapClear (bctx->light_scrap);

	r_framecount = 1;					// no dlightcache

	for (j = 1; j < num_models; j++) {
		m = models[j];
		if (!m)
			break;
		if (m->name[0] == '*') {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		// non-bsp models don't have surfaces.
		for (i = 0; i < m->numsurfaces; i++) {
			msurface_t *surf = m->surfaces + i;
			surf->lightpic = 0;     // paranoia
			if (surf->flags & SURF_DRAWTURB) {
				continue;
			}
			if (surf->flags & SURF_DRAWSKY) {
				continue;
			}
			vulkan_create_surf_lightmap (surf, ctx);
		}
	}

	for (j = 1; j < num_models; j++) {
		m = models[j];
		if (!m) {
			break;
		}
		if (m->name[0] == '*') {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		// non-bsp models don't have surfaces.
		for (i = 0; i < m->numsurfaces; i++) {
			msurface_t *surf = m->surfaces + i;
			if (surf->lightpic) {
				Vulkan_BuildLightMap (surf, ctx);
			}
		}
	}
}

VkImageView
Vulkan_LightmapImageView (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	return QFV_ScrapImageView (bctx->light_scrap);
}

void
Vulkan_FlushLightmaps (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	QFV_ScrapFlush (bctx->light_scrap);
}
