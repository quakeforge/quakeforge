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
#include "QF/Vulkan/scrap.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#define s_dynlight (r_refdef.scene->base + scene_dynlight)
#define LUXEL_SIZE 4

static inline void
add_dynamic_lights (const vec4f_t *transform, msurface_t *surf, vec4f_t *block)
{
	qfZoneScoped (true);
	int         sd, td;
	int         smax, tmax;
	int         s, t;
	mtexinfo_t *tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	auto p = surf->plane;
	vec4f_t plane = { VectorExpand (p->normal), -p->dist };
	vec4f_t     entorigin = { 0, 0, 0, 1 };
	if (transform) {
		//FIXME give world entity a transform
		entorigin = transform[3];
	}

	auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;
	for (uint32_t i = 0; i < dlight_pool->count; i++) {
		auto dlight = &dlight_data[i];
		if (!(surf->dlightbits[i / 32] & (1 << (i % 32))))
			continue;					// not lit by this light

		vec4f_t lightorigin = dlight->origin - entorigin;
		lightorigin[3] = 1;
		float   rad = dlight->radius;
		vec4f_t dist = dotf (lightorigin, plane);
		dist[3] = 0;
		rad -= fabs (dist[0]);

		float minlight = dlight->minlight;
		if (rad < minlight) {
			continue;
		}
		vec4f_t impact = dlight->origin - dist * plane;

		vec4f_t local = {
			dotf (impact, tex->vecs[0])[0] - surf->texturemins[0],
			dotf (impact, tex->vecs[1])[0] - surf->texturemins[1],
		};

		vec4f_t color = { VectorExpand (dlight->color), 0 };
		color *= dlight->radius / 4096;

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
				float d;
				if (sd > td) {
					d = sd + (td >> 1);
				} else {
					d = td + (sd >> 1);
				}
				float l = rad - d;
				if (l > minlight) {
					block[t * smax + s] += l * color;
				}
			}
		}
	}
}

static void
vulkan_build_lightmap (const mod_brush_t *brush, msurface_t *surf,
					   vec4f_t *block)
{
	qfZoneScoped (true);
	int         smax = (surf->extents[0] >> 4) + 1;
	int         tmax = (surf->extents[1] >> 4) + 1;
	int         size = smax * tmax;

	if (!brush->lightdata) {
		for (int i = 0; i < size; i++) {
			block[i] = (vec4f_t) {1, 1, 1, 1};
		}
		return;
	}

	for (int i = 0; i < size; i++) {
		block[i] = (vec4f_t) {0, 0, 0, 1};
	}
	if (surf->samples) {
		byte       *lightmap = surf->samples;
		for (int map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255;
			 map++) {
			surf->cached_light[map] = d_lightstylevalue[surf->styles[map]];
			float scale = surf->cached_light[map] / 65536.0;
			auto bl = block;
			for (int i = 0; i < size; i++) {
				vec4f_t val = { VectorExpand (lightmap), 0 };
				val *= scale;
				lightmap += 3;
				*bl++ += val;
			}
		}
	}
}

void
Vulkan_BuildLightMap (const vec4f_t *transform, const mod_brush_t *brush,
					  msurface_t *surf, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	vec4f_t *block = QFV_SubpicBatch (surf->lightpic, bctx->light_stage);
	vulkan_build_lightmap (brush, surf, block);

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount) {
		add_dynamic_lights (transform, surf, block);
	}
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

	QFV_ScrapClear (bctx->light_scrap);

	r_framecount = 1;					// no dlightcache

	for (int j = 1; j < num_models; j++) {
		auto m = models[j];
		if (!m)
			break;
		if (m->path[0] == '*' || m->type != mod_brush) {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		auto brush = &m->brush;
		// non-bsp models don't have surfaces.
		for (uint32_t i = 0; i < brush->numsurfaces; i++) {
			msurface_t *surf = brush->surfaces + i;
			surf->lightpic = 0;     // paranoia
			if (surf->flags & SURF_DRAWTURB) {
				continue;
			}
			if (surf->flags & SURF_DRAWSKY) {
				continue;
			}
			vulkan_create_surf_lightmap (surf, ctx);
			vec4f_t    *block = QFV_SubpicBatch (surf->lightpic, ctx->staging);
			vulkan_build_lightmap (brush, surf, block);
		}
	}

	for (int j = 1; j < num_models; j++) {
		auto m = models[j];
		if (!m) {
			break;
		}
		if (m->path[0] == '*' || m->type != mod_brush) {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		auto brush = &m->brush;
		// non-bsp models don't have surfaces.
		for (uint32_t i = 0; i < brush->numsurfaces; i++) {
			msurface_t *surf = brush->surfaces + i;
			if (surf->lightpic) {
				Vulkan_BuildLightMap (nullptr, brush, surf, ctx);
			}
		}
	}
	QFV_ScrapFlush (bctx->light_scrap);
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
