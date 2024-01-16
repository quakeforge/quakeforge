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

#define LUXEL_SIZE 4

static inline void
add_dynamic_lights (transform_t transform, msurface_t *surf,
				    float *block)
{
#if 0
	unsigned    lnum;
	int         sd, td;
	float       dist, rad, minlight;
	vec3_t      impact, local, lightorigin;
	vec4f_t     entorigin = { 0, 0, 0, 1 };
	int         smax, tmax;
	int         s, t;
	mtexinfo_t *tex;
	plane_t    *plane;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;
	plane = surf->plane;

	if (transform) {
		//FIXME give world entity a transform
		entorigin = Transform_GetWorldPosition (transform);
	}

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		dlight_t   *light = &r_dlights[lnum];

		VectorSubtract (light->origin, entorigin, lightorigin);
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
					float l = (rad - dist);
					VectorMultAdd (out, l, light->color, out);
					out[3] = 1;
					out += LUXEL_SIZE;
				}
			}
		}
	}
#endif
}

void
Vulkan_BuildLightMap (transform_t transform, mod_brush_t *brush,
					  msurface_t *surf, vulkan_ctx_t *ctx)
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
	if (!brush->lightdata) {
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
				out++;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount) {
		add_dynamic_lights (transform, surf, block);
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

static void
vulkan_build_lightmap (mod_brush_t *brush, msurface_t *surf, vulkan_ctx_t *ctx)
{
	int         smax = (surf->extents[0] >> 4) + 1;
	int         tmax = (surf->extents[1] >> 4) + 1;
	int         size = smax * tmax;

	vec4f_t    *blocklights = QFV_SubpicBatch (surf->lightpic, ctx->staging);
	if (!brush->lightdata) {
		for (int i = 0; i < size; i++) {
			blocklights[i] = (vec4f_t) {1, 1, 1, 1};
		}
		return;
	}

	memset (blocklights, 0, sizeof (vec4f_t[size]));
	if (surf->samples) {
		byte       *lightmap = surf->samples;
		for (int map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255;
			 map++) {
			surf->cached_light[map] = d_lightstylevalue[surf->styles[map]];
			float scale = surf->cached_light[map] / 65536.0;
			auto bl = blocklights;
			for (int i = 0; i < size; i++) {
				vec4f_t val = { VectorExpand (lightmap), 0 };
				val *= scale;
				val[3] = 1;
				lightmap += 3;
				*bl++ = val;
			}
		}
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
			vulkan_build_lightmap (brush, surf, ctx);
		}
	}
	QFV_ScrapFlush (bctx->light_scrap);

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
				Vulkan_BuildLightMap (nulltransform, brush, surf, ctx);
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
