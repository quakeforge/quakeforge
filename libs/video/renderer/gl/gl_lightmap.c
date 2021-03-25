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
#include "QF/entity.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_lightmap.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"

static int          dlightdivtable[8192];
static int			 gl_internalformat;				// 1 or 3
static int          lightmap_bytes;				// 1, 3, or 4
int          gl_lightmap_textures;

// keep lightmap texture data in main memory so texsubimage can update properly
// LordHavoc: changed to be allocated at runtime (typically lower memory usage)
static byte        *lightmaps[MAX_LIGHTMAPS];

static unsigned int blocklights[34 * 34 * 3];	//FIXME make dynamic
static int          allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

qboolean	 gl_lightmap_modified[MAX_LIGHTMAPS];
instsurf_t	*gl_lightmap_polys[MAX_LIGHTMAPS];
glRect_t	 gl_lightmap_rectchange[MAX_LIGHTMAPS];

static int	 lmshift = 7;

void (*gl_R_BuildLightMap) (mod_brush_t *brush, msurface_t *surf);

extern void gl_multitexture_f (cvar_t *var);


void
gl_lightmap_init (void)
{
	int         s;

	memset (&lightmaps, 0, sizeof (lightmaps));
	dlightdivtable[0] = 1048576 >> 7;
	for (s = 1; s < 8192; s++)
		dlightdivtable[s] = 1048576 / (s << 7);
}
/*
static void
R_RecursiveLightUpdate (mnode_t *node)
{
	int         c;
	msurface_t *surf;

	if (node->children[0]->contents >= 0)
		R_RecursiveLightUpdate (node->children[0]);
	if (node->children[1]->contents >= 0)
		R_RecursiveLightUpdate (node->children[1]);
	if ((c = node->numsurfaces))
		for (surf = r_worldentity.model->surfaces + node->firstsurface; c;
			 c--, surf++)
			surf->cached_dlight = true;
}
*/
static inline void
R_AddDynamicLights_1 (msurface_t *surf)
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

	if (currententity->transform) {
		//FIXME give world entity a transform
		entorigin = Transform_GetWorldPosition (currententity->transform);
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
R_AddDynamicLights_3 (msurface_t *surf)
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

	if (currententity->transform) {
		entorigin = Transform_GetWorldPosition (currententity->transform);
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
R_BuildLightMap_1 (mod_brush_t *brush, msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, stride, smax, tmax, i, j;
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
		R_AddDynamicLights_1 (surf);

  store:
	// bound and shift
	// Also, invert because we're using a diff blendfunc now

	stride = (BLOCK_WIDTH - smax) * lightmap_bytes;
	bl = blocklights;

	dest = lightmaps[surf->lightmaptexturenum]
			+ (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = smax; j; j--) {
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
		}
	}
}

static void
R_BuildLightMap_3 (mod_brush_t *brush, msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, stride, smax, tmax, i, j;
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
		R_AddDynamicLights_3 (surf);

  store:
	// bound and shift
	// and invert too
	stride = (BLOCK_WIDTH - smax) * lightmap_bytes;
	bl = blocklights;

	dest = lightmaps[surf->lightmaptexturenum]
			+ (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
		}
	}
}

static void
R_BuildLightMap_4 (mod_brush_t *brush, msurface_t *surf)
{
	byte		   *dest;
	int				maps, size, smax, tmax, i, j, stride;
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
		R_AddDynamicLights_3 (surf);

  store:
	// bound and shift
	// and invert too
	stride = (BLOCK_WIDTH - smax) * lightmap_bytes;
	bl = blocklights;

	dest = lightmaps[surf->lightmaptexturenum]
			+ (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++) {
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
			*dest++ = min (*bl >> lmshift, 255);
			bl++;
			*dest++ = 255;
		}
	}
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
	b = block = Hunk_TempAlloc (rect->h * width);
	lm = lightmaps[i] + (rect->t * BLOCK_WIDTH + rect->l) * lightmap_bytes;
	for (i = rect->h; i > 0; i--) {
		memcpy (b, lm, width);
		b += width;
		lm += stride;
	}
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, rect->l, rect->t, rect->w, rect->h,
					   gl_lightmap_format, GL_UNSIGNED_BYTE, block);
}

static void
GL_UploadLightmap (int i)
{
	switch (gl_lightmap_subimage->int_val) {
	case 2:
		do_subimage_2 (i);
		break;
	case 1:
		qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, gl_lightmap_rectchange[i].t,
						   BLOCK_WIDTH, gl_lightmap_rectchange[i].h,
						   gl_lightmap_format, GL_UNSIGNED_BYTE,
						   lightmaps[i] + (gl_lightmap_rectchange[i].t *
										   BLOCK_WIDTH) * lightmap_bytes);
		break;
	default:
	case 0:
		qfglTexImage2D (GL_TEXTURE_2D, 0, gl_internalformat, BLOCK_WIDTH,
						BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmaps[i]);
		break;
	}
}

void
gl_R_CalcLightmaps (void)
{
	int         i;

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!gl_lightmap_polys[i])
			continue;
		if (gl_lightmap_modified[i]) {
			qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures + i);
			GL_UploadLightmap (i);
			gl_lightmap_modified[i] = false;
		}
	}
}

void
gl_R_BlendLightmaps (void)
{
	float      *v;
	int         i, j;
	instsurf_t *sc;
	glpoly_t   *p;

	qfglDepthMask (GL_FALSE);					// don't bother writing Z
	qfglBlendFunc (lm_src_blend, lm_dest_blend);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		for (sc = gl_lightmap_polys[i]; sc; sc = sc->lm_chain) {
			qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures + i);
			if (sc->transform) {
				qfglPushMatrix ();
				qfglLoadMatrixf (sc->transform);
			}
			for (p = sc->surface->polys; p; p = p->next) {
				qfglBegin (GL_POLYGON);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					qfglTexCoord2fv (&v[5]);
					qfglVertex3fv (v);
				}
				qfglEnd ();
			}
			if (sc->transform)
				qfglPopMatrix ();
		}
	}

	// Return to normal blending  --KB
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qfglDepthMask (GL_TRUE);					// back to normal Z buffering
}

void
gl_overbright_f (cvar_t *var)
{
	int			 num, i, j;
	model_t		*m;
	msurface_t  *fa;
	entity_t    *ent;
	mod_brush_t *brush;

	if (!var)
		return;

	if (var->int_val) {
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

			switch (var->int_val) {
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

	if (gl_multitexture)
		gl_multitexture_f (gl_multitexture);

	if (!gl_R_BuildLightMap)
		return;

	for (ent = r_ent_queue; ent; ent = ent->next) {
		m = ent->renderer.model;

		if (m->type != mod_brush)
			continue;
		if (m->path[0] == '*')
			continue;

		brush = &m->brush;
		for (j = 0, fa = brush->surfaces; j < brush->numsurfaces; j++, fa++) {
			if (fa->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;

			num = fa->lightmaptexturenum;
			gl_lightmap_modified[num] = true;
			gl_lightmap_rectchange[num].l = 0;
			gl_lightmap_rectchange[num].t = 0;
			gl_lightmap_rectchange[num].w = BLOCK_WIDTH;
			gl_lightmap_rectchange[num].h = BLOCK_HEIGHT;

			gl_R_BuildLightMap (brush, fa);
		}
	}

	brush = &r_worldentity.renderer.model->brush;

	for (i = 0, fa = brush->surfaces; i < brush->numsurfaces; i++, fa++) {
		if (fa->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		num = fa->lightmaptexturenum;
		gl_lightmap_modified[num] = true;
		gl_lightmap_rectchange[num].l = 0;
		gl_lightmap_rectchange[num].t = 0;
		gl_lightmap_rectchange[num].w = BLOCK_WIDTH;
		gl_lightmap_rectchange[num].h = BLOCK_HEIGHT;

		gl_R_BuildLightMap (brush, fa);
	}
}

// LIGHTMAP ALLOCATION ========================================================

// returns a texture number and the position inside it
static int
AllocBlock (int w, int h, int *x, int *y)
{
	int         best, best2, texnum, i, j;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++) {
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w) {
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		// LordHavoc: allocate lightmaps only as needed
		if (!lightmaps[texnum])
			lightmaps[texnum] = calloc (BLOCK_WIDTH * BLOCK_HEIGHT,
										lightmap_bytes);
		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

static void
GL_CreateSurfaceLightmap (mod_brush_t *brush, msurface_t *surf)
{
	int         smax, tmax;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum =
		AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	gl_R_BuildLightMap (brush, surf);
}

/*
  GL_BuildLightmaps

  Builds the lightmap texture with all the surfaces from all brush models
*/
void
GL_BuildLightmaps (model_t **models, int num_models)
{
	int         i, j;
	model_t    *m;
	mod_brush_t *brush;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	if (!gl_lightmap_textures) {
		gl_lightmap_textures = gl_texture_number;
		gl_texture_number += MAX_LIGHTMAPS;
	}

	switch (r_lightmap_components->int_val) {
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

	for (j = 1; j < num_models; j++) {
		m = models[j];
		if (!m)
			break;
		if (m->path[0] == '*' || m->type != mod_brush) {
			// sub model surfaces are processed as part of the main model
			continue;
		}
		brush = &m->brush;
		r_pcurrentvertbase = brush->vertexes;
		gl_currentmodel = m;
		// non-bsp models don't have surfaces.
		for (i = 0; i < brush->numsurfaces; i++) {
			if (brush->surfaces[i].flags & SURF_DRAWTURB)
				continue;
			if (gl_sky_divide->int_val && (brush->surfaces[i].flags &
										   SURF_DRAWSKY))
				continue;
			GL_CreateSurfaceLightmap (brush, brush->surfaces + i);
			GL_BuildSurfaceDisplayList (brush->surfaces + i);
		}
	}

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;						// no more used
		gl_lightmap_modified[i] = false;
		gl_lightmap_rectchange[i].l = BLOCK_WIDTH;
		gl_lightmap_rectchange[i].t = BLOCK_HEIGHT;
		gl_lightmap_rectchange[i].w = 0;
		gl_lightmap_rectchange[i].h = 0;
		qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures + i);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (gl_Anisotropy)
			qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
							   gl_aniso);
		qfglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH,
						BLOCK_HEIGHT, 0, gl_lightmap_format,
						GL_UNSIGNED_BYTE, lightmaps[i]);
	}
}
