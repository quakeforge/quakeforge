/*
	glsl_lightmap.c

	GLSL lightmaps

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Joseph Carter <knghtbrd@debian.org>
	Copyright (C) 2012       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/6

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
#include <stdlib.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

#define BLOCK_SIZE (BLOCK_WIDTH * BLOCK_HEIGHT)

static scrap_t *light_scrap;
static unsigned *blocklights;
static int      bl_extents[2];

void (*glsl_R_BuildLightMap) (const vec4f_t *transform, mod_brush_t *brush,
							  msurface_t *surf);

static void
R_AddDynamicLights_1 (const vec4f_t *transform, msurface_t *surf)
{
	unsigned    lnum;
	int         sd, td;
	float       dist, rad, minlight;
	vec3_t      impact, local, lightorigin;
	vec4f_t     entorigin = { 0, 0, 0, 1};
	int         s, t;
	int         smax, tmax;
	mtexinfo_t *tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	if (transform) {
		//FIXME give world entity a transform
		entorigin = transform[3];
	}

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, entorigin, lightorigin);
		rad = r_dlights[lnum].radius;
		dist = DotProduct (lightorigin, surf->plane->normal)
				- surf->plane->dist;
		rad -= fabs (dist);
		minlight = r_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		VectorMultSub (lightorigin, dist, surf->plane->normal, impact);

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++) {
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++) {
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
					blocklights[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

static void
R_BuildLightMap_1 (const vec4f_t *transform, mod_brush_t *brush,
				   msurface_t *surf)
{
	int         smax, tmax, size;
	unsigned    scale;
	int         i, t;
	byte       *out;

	// If we add dlights this frame, make sure they get removed next frame
	// if the dlights disappear suddenly
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;

	// clear to no light
	memset (blocklights, 0, size * sizeof (blocklights[0]));
	if (!brush->lightdata) {
		// because we by-pass the inversion, "no light" = "full bright"
		GLSL_SubpicUpdate (surf->lightpic, (byte *) blocklights, 1);
		return;
	}

	// add all the lightmaps
	if (surf->samples) {
		int         lmap;
		byte       *lightmap = surf->samples;

		for (lmap = 0; lmap < MAXLIGHTMAPS && surf->styles[lmap] != 255;
			 lmap++) {
			unsigned int *bl;

			scale = d_lightstylevalue[surf->styles[lmap]];
			surf->cached_light[lmap] = scale;
			bl = blocklights;
			for (i = 0; i < size; i++)
				*bl++ += *lightmap++ * scale;
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights_1 (transform, surf);

	// bound, invert, and shift
	out = (byte *) blocklights;
	for (i = 0; i < size; i++) {
		t = (255 * 256 - (int) blocklights[i]);
		t = max (t, 1 << (14 - VID_CBITS));
		t = ((unsigned) t) >> (16 - VID_CBITS);
		*out++ = t;
	}

	GLSL_SubpicUpdate (surf->lightpic, (byte *) blocklights, 1);
}

static void
create_surf_lightmap (msurface_t *surf)
{
	int        smax, tmax;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	surf->lightpic = GLSL_ScrapSubpic (light_scrap, smax, tmax);
	if (!surf->lightpic)
		Sys_Error ("FIXME taniwha is being lazy");
	if (smax > bl_extents[0])
		bl_extents[0] = smax;
	if (tmax > bl_extents[1])
		bl_extents[1] = tmax;
}

void
glsl_R_BuildLightmaps (model_t **models, int num_models)
{
	int         size;
	model_t    *m;
	mod_brush_t *brush;

	//FIXME RGB support
	if (!light_scrap) {
		light_scrap = GLSL_CreateScrap (4096, GL_LUMINANCE, 1);
	} else {
		GLSL_ScrapClear (light_scrap);
	}
	glsl_R_BuildLightMap = R_BuildLightMap_1;

	bl_extents[1] = bl_extents[0] = 0;
	for (int j = 1; j < num_models; j++) {
		m = models[j];
		if (!m)
			break;
		if (m->path[0] == '*' || m->type != mod_brush) {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		brush = &m->brush;
		// non-bsp models don't have surfaces.
		for (unsigned i = 0; i < brush->numsurfaces; i++) {
			msurface_t *surf = brush->surfaces + i;
			surf->lightpic = 0;		// paranoia
			if (surf->flags & SURF_DRAWTURB)
				continue;
			if (surf->flags & SURF_DRAWSKY)
				continue;
			create_surf_lightmap (surf);
		}
	}
	size = bl_extents[0] * bl_extents[1] * 3;	// * 3 for rgb support
	blocklights = realloc (blocklights, size * sizeof (blocklights[0]));
	for (int j = 1; j < num_models; j++) {
		m = models[j];
		if (!m)
			break;
		if (m->path[0] == '*' || m->type != mod_brush) {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		brush = &m->brush;
		// non-bsp models don't have surfaces.
		for (unsigned i = 0; i < brush->numsurfaces; i++) {
			msurface_t *surf = brush->surfaces + i;
			if (surf->lightpic)
				glsl_R_BuildLightMap (0, brush, surf);
		}
	}
}

int
glsl_R_LightmapTexture (void)
{
	return GLSL_ScrapTexture (light_scrap);
}

void
glsl_R_FlushLightmaps (void)
{
	GLSL_ScrapFlush (light_scrap);
}
