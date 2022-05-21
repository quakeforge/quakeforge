/*
	gl_lightmap.c

	surface-related refresh code

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Joseph Carter <knghtbrd@debian.org>

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

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_lightmap.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"

static scrap_t *light_scrap;
static int          dlightdivtable[8192];
static int			 gl_internalformat;				// 1 or 3
static int          lightmap_bytes;				// 1, 3, or 4
GLuint gl_lightmap_textures[MAX_LIGHTMAPS];

// keep lightmap texture data in main memory so texsubimage can update properly
// LordHavoc: changed to be allocated at runtime (typically lower memory usage)
static byte        *lightmaps[MAX_LIGHTMAPS];

static unsigned *blocklights;
static int bl_extents[2];
static int          allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

qboolean	 gl_lightmap_modified[MAX_LIGHTMAPS];
instsurf_t	*gl_lightmap_polys;
glRect_t	 gl_lightmap_rectchange[MAX_LIGHTMAPS];

static int	 lmshift = 7;

void (*gl_R_BuildLightMap) (const transform_t *transform, mod_brush_t *brush,
							msurface_t *surf);

void
gl_lightmap_init (void)
{
	int         s;

	memset (&lightmaps, 0, sizeof (lightmaps));
	dlightdivtable[0] = 1048576 >> 7;
	for (s = 1; s < 8192; s++)
		dlightdivtable[s] = 1048576 / (s << 7);
}

static inline void
R_AddDynamicLights_1 (const transform_t *transform, msurface_t *surf)
{
	float			dist;
	unsigned int	maxdist, maxdist2, maxdist3;
	int             smax, smax_bytes, tmax,
					grey, s, t;
	unsigned int	lnum, td, i, j;
	unsigned int    sdtable[18];
	unsigned int   *bl;
	vec3_t			impact, local;
	vec4f_t         entorigin = { 0, 0, 0, 1 };

	smax = (surf->extents[0] >> 4) + 1;
	smax_bytes = smax * gl_internalformat;
	tmax = (surf->extents[1] >> 4) + 1;

	if (transform) {
		//FIXME give world entity a transform
		entorigin = Transform_GetWorldPosition (transform);
	}

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, entorigin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;
		VectorMultSub (r_dlights[lnum].origin, dist, surf->plane->normal,
					   impact);

		i = DotProduct (impact,	surf->texinfo->vecs[0]) +
			surf->texinfo->vecs[0][3] - surf->texturemins[0];

		// reduce calculations
		t = dist * dist;
		for (s = 0; s < smax; s++, i -= 16)
			sdtable[s] = i * i + t;

		i = DotProduct (impact,	surf->texinfo->vecs[1]) +
			surf->texinfo->vecs[1][3] - surf->texturemins[1];

		// for comparisons to minimum acceptable light
		maxdist = (int) (r_dlights[lnum].radius * r_dlights[lnum].radius);

		// clamp radius to avoid exceeding 8192 entry division table
		if (maxdist > 1048576)
			maxdist = 1048576;
		maxdist3 = maxdist - t;

		// convert to 8.8 blocklights format
		grey = (r_dlights[lnum].color[0] + r_dlights[lnum].color[1] +
				r_dlights[lnum].color[2]) * maxdist / 3.0;
		bl = blocklights;
		for (t = 0; t < tmax; t++, i -= 16) {
			td = i * i;
			if (td < maxdist3) {		// ensure part is visible on this line
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++) {
					if (sdtable[s] < maxdist2) {
						j = dlightdivtable[(sdtable[s] + td) >> 7];
						*bl++ += (grey * j) >> 7;
					} else
						bl++;
				}
			} else
				bl += smax_bytes;		// skip line
		}
	}
}

static inline void
R_AddDynamicLights_3 (const transform_t *transform, msurface_t *surf)
{
	float			dist;
	unsigned int	maxdist, maxdist2, maxdist3;
	int             smax, smax_bytes, tmax,
					red, green, blue, s, t;
	unsigned int	lnum, td, i, j;
	unsigned int    sdtable[18];
	unsigned int   *bl;
	vec3_t			impact, local;
	vec4f_t         entorigin = { 0, 0, 0, 1 };

	smax = (surf->extents[0] >> 4) + 1;
	smax_bytes = smax * gl_internalformat;
	tmax = (surf->extents[1] >> 4) + 1;

	if (transform) {
		entorigin = Transform_GetWorldPosition (transform);
	}

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, entorigin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;
		VectorMultSub (r_dlights[lnum].origin, dist, surf->plane->normal,
					   impact);

		i = DotProduct (impact,	surf->texinfo->vecs[0]) +
			surf->texinfo->vecs[0][3] - surf->texturemins[0];

		// reduce calculations
		t = dist * dist;
		for (s = 0; s < smax; s++, i -= 16)
			sdtable[s] = i * i + t;

		i = DotProduct (impact,	surf->texinfo->vecs[1]) +
			surf->texinfo->vecs[1][3] - surf->texturemins[1];

		// for comparisons to minimum acceptable light
		maxdist = (int) (r_dlights[lnum].radius * r_dlights[lnum].radius);

		// clamp radius to avoid exceeding 8192 entry division table
		if (maxdist > 1048576)
			maxdist = 1048576;
		maxdist3 = maxdist - t;

		// convert to 8.8 blocklights format
		red = r_dlights[lnum].color[0] * maxdist;
		green = r_dlights[lnum].color[1] * maxdist;
		blue = r_dlights[lnum].color[2] * maxdist;
		bl = blocklights;
		for (t = 0; t < tmax; t++, i -= 16) {
			td = i * i;
			if (td < maxdist3) {		// ensure part is visible on this line
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++) {
					if (sdtable[s] < maxdist2) {
						j = dlightdivtable[(sdtable[s] + td) >> 7];
						*bl++ += (red * j) >> 7;
						*bl++ += (green * j) >> 7;
						*bl++ += (blue * j) >> 7;
					} else
						bl += 3;
				}
			} else
				bl += smax_bytes;		// skip line
		}
	}
}

static void
R_BuildLightMap_1 (const transform_t *transform, mod_brush_t *brush,
				   msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, smax, tmax, i;
	unsigned int	scale;
	unsigned int   *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax * gl_internalformat;

	// set to full bright if no light data
	if (!brush->lightdata) {
		memset (&blocklights[0], 0xff, size * sizeof(int));
		goto store;
	}

	// clear to no light
	memset (&blocklights[0], 0, size * sizeof(int));

	// add all the lightmaps
	if (surf->samples) {
		byte		   *lightmap;

		lightmap = surf->samples;
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;					// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < size; i++) {
				*bl++ += *lightmap++ * scale;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights_1 (transform, surf);

  store:
	// bound and shift
	// Also, invert because we're using a diff blendfunc now
	bl = blocklights;
	dest = (byte *) blocklights;
	for (i = 0; i < tmax * smax; i++) {
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
	}

	GL_SubpicUpdate (surf->lightpic, (byte *) blocklights, 1);
}

static void
R_BuildLightMap_3 (const transform_t *transform, mod_brush_t *brush,
				   msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, smax, tmax, i;
	unsigned int	scale;
	unsigned int   *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax * gl_internalformat;

	// set to full bright if no light data
	if (!brush->lightdata) {
		memset (&blocklights[0], 0xff, size * sizeof(int));
		goto store;
	}

	// clear to no light
	memset (&blocklights[0], 0, size * sizeof(int));

	// add all the lightmaps
	if (surf->samples) {
		byte		   *lightmap;

		lightmap = surf->samples;
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;					// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < smax * tmax; i++) {
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights_3 (transform, surf);

  store:
	// bound and shift
	// and invert too
	bl = blocklights;
	dest = (byte *) blocklights;
	for (i = 0; i < tmax * smax; i++) {
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
	}

	GL_SubpicUpdate (surf->lightpic, (byte *) blocklights, 1);
}

static void
R_BuildLightMap_4 (const transform_t *transform, mod_brush_t *brush,
				   msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, smax, tmax, i;
	unsigned int	scale;
	unsigned int   *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax * gl_internalformat;

	// set to full bright if no light data
	if (!brush->lightdata) {
		memset (&blocklights[0], 0xff, size * sizeof(int));
		goto store;
	}

	// clear to no light
	memset (&blocklights[0], 0, size * sizeof(int));

	// add all the lightmaps
	if (surf->samples) {
		byte		   *lightmap;

		lightmap = surf->samples;
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;					// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < smax * tmax; i++) {
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights_3 (transform, surf);

  store:
	// bound and shift
	// and invert too
	bl = blocklights;
	dest = (byte *) blocklights;
	for (i = 0; i < tmax * smax; i++) {
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
		*dest++ = min (*bl >> lmshift, 255);
		bl++;
		*dest++ = 255;
	}

	GL_SubpicUpdate (surf->lightpic, (byte *) blocklights, 1);
}

// BRUSH MODELS ===============================================================

static inline void
do_subimage_2 (int i)
{
	byte       *block, *lm, *b;
	int         stride, width;
	glRect_t   *rect = &gl_lightmap_rectchange[i];

	width = rect->w * lightmap_bytes;
	stride = BLOCK_WIDTH * lightmap_bytes;
	b = block = Hunk_TempAlloc (0, rect->h * width);
	lm = lightmaps[i] + (rect->t * BLOCK_WIDTH + rect->l) * lightmap_bytes;
	for (i = rect->h; i > 0; i--) {
		memcpy (b, lm, width);
		b += width;
		lm += stride;
	}
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, rect->l, rect->t, rect->w, rect->h,
					   gl_lightmap_format, GL_UNSIGNED_BYTE, block);
}

void
gl_R_BlendLightmaps (void)
{
	float      *v;
	instsurf_t *sc;
	glpoly_t   *p;

	qfglDepthMask (GL_FALSE);					// don't bother writing Z
	qfglBlendFunc (lm_src_blend, lm_dest_blend);

	qfglBindTexture (GL_TEXTURE_2D, gl_R_LightmapTexture ());
	for (sc = gl_lightmap_polys; sc; sc = sc->lm_chain) {
		if (sc->transform) {
			qfglPushMatrix ();
			qfglLoadMatrixf (sc->transform);
		}
		for (p = sc->surface->polys; p; p = p->next) {
			qfglBegin (GL_POLYGON);
			v = p->verts[0];
			for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qfglTexCoord2fv (&v[5]);
				qfglVertex3fv (v);
			}
			qfglEnd ();
		}
		if (sc->transform)
			qfglPopMatrix ();
	}

	// Return to normal blending  --KB
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qfglDepthMask (GL_TRUE);					// back to normal Z buffering
}

void
gl_overbright_f (void *data, const cvar_t *cvar)
{
	mod_brush_t *brush;

	if (!cvar)
		return;

	if (gl_overbright) {
		if (!gl_combine_capable && gl_mtex_capable) {
			Sys_Printf ("Warning: gl_overbright has no effect with "
						"gl_multitexture enabled if you don't have "
						"GL_COMBINE support in your driver.\n");
			lm_src_blend = GL_ZERO;
			lm_dest_blend = GL_SRC_COLOR;
			lmshift = 7;
			gl_rgb_scale = 1.0;
		} else {
			lm_src_blend = GL_DST_COLOR;
			lm_dest_blend = GL_SRC_COLOR;

			switch (gl_overbright) {
			case 2:
				lmshift = 9;
				gl_rgb_scale = 4.0;
				break;
			case 1:
				lmshift = 8;
				gl_rgb_scale = 2.0;
				break;
			default:
				lmshift = 7;
				gl_rgb_scale = 1.0;
				break;
			}
		}
	} else {
		lm_src_blend = GL_ZERO;
		lm_dest_blend = GL_SRC_COLOR;
		lmshift = 7;
		gl_rgb_scale = 1.0;
	}

	if (!gl_R_BuildLightMap)
		return;

	brush = &r_refdef.worldmodel->brush;

	for (unsigned i = 0; i < brush->numsurfaces; i++) {
		msurface_t *surf = brush->surfaces + i;
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		gl_R_BuildLightMap (0, brush, surf);
	}
}

// LIGHTMAP ALLOCATION ========================================================

static void
GL_CreateSurfaceLightmap (mod_brush_t *brush, msurface_t *surf)
{
	int         smax, tmax;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightpic = GL_ScrapSubpic (light_scrap, smax, tmax);
	if (!surf->lightpic) {
		Sys_Error ("FIXME taniwha is being lazy");
	}
	if (smax > bl_extents[0]) {
		bl_extents[0] = smax;
	}
	if (tmax > bl_extents[1]) {
		bl_extents[1] = tmax;
	}
}

/*
  GL_BuildLightmaps

  Builds the lightmap texture with all the surfaces from all brush models
*/
void
GL_BuildLightmaps (model_t **models, int num_models)
{
	model_t    *m;
	mod_brush_t *brush;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	switch (r_lightmap_components) {
	case 1:
		gl_internalformat = 1;
		gl_lightmap_format = GL_LUMINANCE;
		lightmap_bytes = 1;
		gl_R_BuildLightMap = R_BuildLightMap_1;
		break;
	case 3:
		gl_internalformat = 3;
		if (gl_use_bgra)
			gl_lightmap_format = GL_BGR;
		else
			gl_lightmap_format = GL_RGB;
		lightmap_bytes = 3;
		gl_R_BuildLightMap = R_BuildLightMap_3;
		break;
	case 4:
	default:
		gl_internalformat = 3;
		if (gl_use_bgra)
			gl_lightmap_format = GL_BGRA;
		else
			gl_lightmap_format = GL_RGBA;
		lightmap_bytes = 4;
		gl_R_BuildLightMap = R_BuildLightMap_4;
		break;
	}

	if (!light_scrap) {
		light_scrap = GL_CreateScrap (4096, gl_lightmap_format, 1);
	} else {
		GL_ScrapClear (light_scrap);
	}

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
		gl_currentmodel = m;
		// non-bsp models don't have surfaces.
		for (unsigned i = 0; i < brush->numsurfaces; i++) {
			if (brush->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			GL_CreateSurfaceLightmap (brush, brush->surfaces + i);
			GL_BuildSurfaceDisplayList (brush, brush->surfaces + i);
		}
	}

	int size = bl_extents[0] * bl_extents[1] * 3;	// * 3 for rgb support
	blocklights = realloc (blocklights, size * sizeof (blocklights[0]));

	// upload all lightmaps that were filled
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
			if (surf->lightpic) {
				gl_R_BuildLightMap (0, brush, surf);
			}
		}
	}
}

int
gl_R_LightmapTexture (void)
{
	return GL_ScrapTexture (light_scrap);
}

void
gl_R_FlushLightmaps (void)
{
	GL_ScrapFlush (light_scrap);
}
