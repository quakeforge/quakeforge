/*
	gl_rsurf.c

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
static const char rcsid[] = 
	"$Id$";

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
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_local.h"
#include "r_shared.h"

void EmitWaterPolys (msurface_t *fa);

int          active_lightmaps;
int          dlightdivtable[8192];
int			 gl_internalformat;
int          lightmap_bytes;				// 1, 3, or 4
int          lightmap_textures;
int			 skytexturenum;

// LordHavoc: since lightmaps are now allocated only as needed, allow a ridiculous number :)
#define	MAX_LIGHTMAPS	1024
#define	BLOCK_WIDTH		128 // 256
#define	BLOCK_HEIGHT	128 // 256

// keep lightmap texture data in main memory so texsubimage can update properly
// LordHavoc: changed to be allocated at runtime (typically lower memory usage)
byte        *lightmaps[MAX_LIGHTMAPS];

// unsigned int blocklights[BLOCK_WIDTH * BLOCK_HEIGHT * 3];
unsigned int blocklights[18 * 18 * 3];
int          allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

typedef struct glRect_s {
	unsigned short l, t, w, h;
} glRect_t;

glpoly_t    *fullbright_polys[MAX_GLTEXTURES];
qboolean     lightmap_modified[MAX_LIGHTMAPS];
glpoly_t    *lightmap_polys[MAX_LIGHTMAPS];
glRect_t     lightmap_rectchange[MAX_LIGHTMAPS];

msurface_t  *waterchain = NULL;
msurface_t  *sky_chain;


// LordHavoc: place for gl_rsurf setup code
void
glrsurf_init (void)
{
	int         s;

	memset (&lightmaps, 0, sizeof (lightmaps));
		dlightdivtable[0] = 1048576 >> 7;
		for (s = 1; s < 8192; s++)
			dlightdivtable[s] = 1048576 / (s << 7);
}

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
			 c--, surf++) surf->cached_dlight = true;
}

void
R_AddDynamicLights (msurface_t *surf)
{
	float		  dist;
	int			  lnum, maxdist, maxdist2, maxdist3, red, green, blue, smax,
				  tmax, td, i, j, s, t;
	int           sdtable[18];
	unsigned int *bl;
	vec3_t		  impact, local;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits & (1 << lnum)))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, currententity->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;
		for (i = 0; i < 3; i++)
			impact[i] =
				r_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;

		i = DotProduct (impact,	surf->texinfo->vecs[0]) +
			surf->texinfo->vecs[0][3] - surf->texturemins[0];

		// reduce calculations
		t = dist * dist;
		for (s = 0; s < smax; s++, i -= 16)
			sdtable[s] = i * i + t;

		i = DotProduct (impact,	surf->texinfo->vecs[1]) +
			surf->texinfo->vecs[1][3] - surf->texturemins[1];

		// for comparisons to minimum acceptable light
		maxdist = (int) ((r_dlights[lnum].radius * r_dlights[lnum].radius) *
						 0.75);

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
			if (td < maxdist3) {	// ensure part is visible on this line
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++) {
					if (sdtable[s] < maxdist2) {
						j = dlightdivtable[(sdtable[s] + td) >> 7];
						bl[0] += (red * j) >> 7;
						bl[1] += (green * j) >> 7;
						bl[2] += (blue * j) >> 7;
					}
					bl += 3;
				}
			} else
				bl += smax * 3;			// skip line
		}
	}
}

/*
  R_BuildLightMap

  Combine and scale multiple lightmaps.
  After talking it over with LordHavoc, I've decided to switch to using
  GL_RGB for colored lights and averaging them out for plain white
  lighting if needed.  Much cleaner that way.  --KB
*/
void
R_BuildLightMap (msurface_t *surf, byte * dest, int stride)
{
	byte		   *lightmap;
	int				maps, shift, size, smax, tmax, t2, i, j;
	unsigned int	scale;
	unsigned int   *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (!r_worldentity.model->lightdata) {
		memset (&blocklights[0], 0xff, 3 * size * sizeof(int));
		goto store;
	}

	// clear to no light
	memset (&blocklights[0], 0, 3 * size * sizeof(int));

	// add all the lightmaps
	if (lightmap) {
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
			 maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < size; i++) {
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
		}
	}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

  store:
	// bound and shift
	stride -= smax * lightmap_bytes;
	bl = blocklights;

	if (gl_mtex_active) {
		shift = 7;		// 0-1 lightmap range.
	} else {
		shift = 8;		// 0-2 lightmap range.
	}

	switch (lightmap_bytes) {
	case 4:
		for (i = 0; i < tmax; i++, dest += stride) {
			for (j = 0; j < smax; j++) {
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
				*dest++ = 255;
			}
		}
		break;
	case 3:
		for (i = 0; i < tmax; i++, dest += stride) {
			for (j = 0; j < smax; j++) {
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
				*dest++ = bound (0, *bl >> shift, 255);
				bl++;
			}
		}
		break;
	case 1:
		for (i = 0; i < tmax; i++, dest += stride) {
			for (j = 0; j < smax; j++) {
				t2 = *bl++;
				t2 += *bl++;
				t2 += *bl++;
				t2 /= 3;
				*dest++ = bound (0, t2 >> shift, 255);
			}
		}
		break;
	}
}

/*
  R_TextureAnimation

  Returns the proper texture for a given time and base texture
*/
texture_t  *
R_TextureAnimation (texture_t *base)
{
	int         count, relative;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (r_realtime * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/* BRUSH MODELS */

void
GL_UploadLightmap (int i, int x, int y, int w, int h)
{
/*	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, y, BLOCK_WIDTH, h,
	gl_lightmap_format, GL_UNSIGNED_BYTE,
	lightmaps[i] + (y * BLOCK_WIDTH) * lightmap_bytes);
*/
	switch (gl_lightmap_subimage->int_val) {
	case 2:
		qfglTexSubImage2D (GL_TEXTURE_2D, 0, x, y, w, h,
						   gl_lightmap_format, GL_UNSIGNED_BYTE,
						   lightmaps[i] + (y * BLOCK_WIDTH) * lightmap_bytes);
		break;
	case 1:
		qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, y, BLOCK_WIDTH, h,
						   gl_lightmap_format, GL_UNSIGNED_BYTE,
						   lightmaps[i] + (y * BLOCK_WIDTH) * lightmap_bytes);
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
R_DrawMultitexturePoly (msurface_t *s)
{
	float      *v;
	int         maps, i;
	texture_t  *texture = R_TextureAnimation (s->texinfo->texture);

	c_brush_polys++;

	i = s->lightmaptexturenum;

	// Binds world to texture env 0
	qglActiveTexture (gl_mtex_enum + 0);
	qfglBindTexture (GL_TEXTURE_2D, texture->gl_texturenum);
	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qfglEnable (GL_TEXTURE_2D);
	// Binds lightmap to texenv 1
	qglActiveTexture (gl_mtex_enum + 1);
	qfglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qfglEnable (GL_TEXTURE_2D);

	// check for lightmap modification
	if (r_dynamic->int_val) {
		for (maps = 0; maps < MAXLIGHTMAPS && s->styles[maps] != 255; maps++)
			if (d_lightstylevalue[s->styles[maps]] != s->cached_light[maps])
				goto dynamic;

		if ((s->dlightframe == r_framecount) || s->cached_dlight)	{
		  dynamic:
			R_BuildLightMap (s,
							 lightmaps[s->lightmaptexturenum] +
							 (s->light_t * BLOCK_WIDTH +
							  s->light_s) * lightmap_bytes,
							 BLOCK_WIDTH * lightmap_bytes);
			GL_UploadLightmap (i, s->light_s, s->light_t,
							   (s->extents[0] >> 4) + 1,
							   (s->extents[1] >> 4) + 1);
		}
	}

	qfglBegin (GL_POLYGON);
	v = s->polys->verts[0];
	for (i = 0; i < s->polys->numverts; i++, v += VERTEXSIZE) {
		qglMultiTexCoord2f (gl_mtex_enum + 0, v[3], v[4]);
		qglMultiTexCoord2f (gl_mtex_enum + 1, v[5], v[6]);
		qfglVertex3fv (v);
	}
	qfglEnd ();
	qfglDisable (GL_TEXTURE_2D);
	qglActiveTexture (gl_mtex_enum + 0);
	qfglEnable (GL_TEXTURE_2D);

	if (texture->gl_fb_texturenum > 0) {
		s->polys->fb_chain = fullbright_polys[texture->gl_fb_texturenum];
		fullbright_polys[texture->gl_fb_texturenum] = s->polys;
	}
	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void
R_BlendLightmaps (void)
{
	float      *v;
	int         i, j;
	glpoly_t   *p;

	qfglDepthMask (GL_FALSE);					// don't bother writing Z

	qfglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		qfglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		if (lightmap_modified[i]) {
			GL_UploadLightmap (i, lightmap_rectchange[i].l,
							   lightmap_rectchange[i].t,
							   lightmap_rectchange[i].w,
							   lightmap_rectchange[i].h);
			lightmap_modified[i] = false;
		}
		for (; p; p = p->chain) {
			qfglBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qfglTexCoord2fv (&v[5]);
				qfglVertex3fv (v);
			}
			qfglEnd ();
		}
	}

	// Return to normal blending  --KB
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qfglDepthMask (GL_TRUE);					// back to normal Z buffering
}

void
R_RenderFullbrights (void)
{
	float      *v;
	int         i, j;
	glpoly_t   *p;

	qfglBlendFunc (GL_ONE, GL_ONE);
	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;
		qfglBindTexture (GL_TEXTURE_2D, i);
		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			qfglBegin (GL_POLYGON);
			for (j = 0, v = p->verts[0]; j < p->numverts; j++, v += VERTEXSIZE)
			{
				qfglTexCoord2fv (&v[3]);
				qfglVertex3fv (v);
			}
			qfglEnd ();
		}
	}
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
R_RenderBrushPoly (msurface_t *fa)
{
	byte       *base;
	float      *v;
	int         maps, smax, tmax, i;
	glRect_t   *theRect;
	texture_t  *texture = R_TextureAnimation (fa->texinfo->texture);

	c_brush_polys++;

	qfglBindTexture (GL_TEXTURE_2D, texture->gl_texturenum);

	qfglBegin (GL_POLYGON);
	v = fa->polys->verts[0];
	for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
		qfglTexCoord2fv (&v[3]);
		qfglVertex3fv (v);
	}
	qfglEnd ();

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	if (texture->gl_fb_texturenum > 0) {
		fa->polys->fb_chain = fullbright_polys[texture->gl_fb_texturenum];
		fullbright_polys[texture->gl_fb_texturenum] = fa->polys;
	}
	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if ((fa->dlightframe == r_framecount) || fa->cached_dlight) {
	  dynamic:
		if (r_dynamic->int_val) {
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0] >> 4) + 1;
			tmax = (fa->extents[1] >> 4) + 1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s - theRect->l) + smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t - theRect->t) + tmax;
			base =
				lightmaps[fa->lightmaptexturenum] +
				(fa->light_t * BLOCK_WIDTH + fa->light_s) * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}

void
GL_WaterSurface (msurface_t *s)
{
	int         i;

	i = s->texinfo->texture->gl_texturenum;
	qfglBindTexture (GL_TEXTURE_2D, i);
	if (r_wateralpha->value < 1.0) {
		qfglDepthMask (GL_FALSE);
		color_white[3] = r_wateralpha->value * 255;
		qfglColor4ubv (color_white);
		EmitWaterPolys (s);
		qfglColor3ubv (color_white);
		qfglDepthMask (GL_TRUE);
	} else
		EmitWaterPolys (s);
}

void
R_DrawWaterSurfaces (void)
{
	int         i;
	msurface_t *s;

	if (!waterchain)
		return;

	// go back to the world matrix
	qfglLoadMatrixf (r_world_matrix);

	if (r_wateralpha->value < 1.0) {
		qfglDepthMask (GL_FALSE);
		color_white[3] = r_wateralpha->value * 255;
		qfglColor4ubv (color_white);
	}

	i = -1;
	for (s = waterchain; s; s = s->texturechain) {
		if (i != s->texinfo->texture->gl_texturenum) {
			i = s->texinfo->texture->gl_texturenum;
			qfglBindTexture (GL_TEXTURE_2D, i);
		}
		EmitWaterPolys (s);
	}

	waterchain = NULL;

	if (r_wateralpha->value < 1.0) {
		qfglDepthMask (GL_TRUE);
		qfglColor3ubv (color_white);
	}
}

void
DrawTextureChains (void)
{
	int         i;
	msurface_t *s;

	qfglDisable (GL_BLEND);

	for (i = 0; i < r_worldentity.model->numtextures; i++) {
		if (!r_worldentity.model->textures[i])
			continue;
		for (s = r_worldentity.model->textures[i]->texturechain; s;
			 s = s->texturechain) R_RenderBrushPoly (s);

		r_worldentity.model->textures[i]->texturechain = NULL;
	}

	qfglEnable (GL_BLEND);
}

// FIXME: add modelalpha support?
void
R_DrawBrushModel (entity_t *e)
{
	float       dot;
	int         i, k;
	model_t    *clmodel;
	mplane_t   *pplane;
	msurface_t *psurf;
	qboolean    rotated;
	vec3_t      mins, maxs;

	currententity = e;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	} else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));
	memset (fullbright_polys, 0, sizeof (fullbright_polys));

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t      temp;
		vec3_t      forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 && r_dlight_lightmap->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < r_maxdlights; k++) {
			if ((r_dlights[k].die < r_realtime) || (!r_dlights[k].radius))
				continue;

			VectorSubtract (r_dlights[k].origin, e->origin, lightorigin);
			R_RecursiveMarkLights (lightorigin, &r_dlights[k], 1 << k,
						  clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	qfglPushMatrix ();
	e->angles[0] = -e->angles[0];		// stupid quake bug
	R_RotateForEntity (e);
	e->angles[0] = -e->angles[0];		// stupid quake bug

	// draw texture
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (psurf->flags & SURF_DRAWTURB) {
				GL_WaterSurface (psurf);
			} else if (psurf->flags & SURF_DRAWSKY) {
				psurf->texturechain = sky_chain;
				sky_chain = psurf;
				return;
			} else if (gl_mtex_active) {
				R_DrawMultitexturePoly (psurf);
			} else {
				R_RenderBrushPoly (psurf);
			}
		}
	}

	if (!gl_mtex_active)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val)
		R_RenderFullbrights ();

	//if (gl_sky_clip->int_val)
	//	R_DrawSkyChain (sky_chain);

	qfglPopMatrix ();
}

/* WORLD MODEL */

void
R_RecursiveWorldNode (mnode_t *node)
{
	double      dot;
	int         c, side;
	mleaf_t    *pleaf;
	mplane_t   *plane;
	msurface_t *surf;

	if (node->contents == CONTENTS_SOLID)
		return;
	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;
		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}
	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;
	if (plane->type < 3)
		dot = modelorg[plane->type] - plane->dist;
	else
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
	side = dot < 0;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
 	side = (~side + 1) & SURF_PLANEBACK;
	// draw stuff
	if ((c = node->numsurfaces)) {
		surf = r_worldentity.model->surfaces + node->firstsurface;
		for (; c; c--, surf++) {
			if (surf->visframe != r_visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;				// wrong side

			if (surf->flags & SURF_DRAWTURB) {
				surf->texturechain = waterchain;
				waterchain = surf;
			} else if (surf->flags & SURF_DRAWSKY) {
				surf->texturechain = sky_chain;
				sky_chain = surf;
			} else if (gl_mtex_active) {
				R_DrawMultitexturePoly (surf);
			} else {
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}
		}
	}
	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}

void
R_DrawWorld (void)
{
	entity_t    ent;

	memset (&ent, 0, sizeof (ent));
	ent.model = r_worldentity.model;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));
	memset (fullbright_polys, 0, sizeof (fullbright_polys));

	sky_chain = 0;
	if (!gl_sky_clip->int_val) {
		R_DrawSky ();
	}

	R_RecursiveWorldNode (r_worldentity.model->nodes);

	R_DrawSkyChain (sky_chain);

	DrawTextureChains ();

	if (!gl_mtex_active)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val)
		R_RenderFullbrights ();
}

void
R_MarkLeaves (void)
{
	byte         solid[4096];
	byte        *vis;
	int          c, i;
	mleaf_t     *leaf;
	mnode_t     *node;
	msurface_t **mark;

	if (r_oldviewleaf == r_viewleaf && !r_novis->int_val)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis->int_val) {
		vis = solid;
		memset (solid, 0xff, (r_worldentity.model->numleafs + 7) >> 3);
	} else
		vis = Mod_LeafPVS (r_viewleaf, r_worldentity.model);

	for (i = 0; i < r_worldentity.model->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			leaf = &r_worldentity.model->leafs[i + 1];
			if ((c = leaf->nummarksurfaces)) {
				mark = leaf->firstmarksurface;
				do {
					(*mark)->visframe = r_visframecount;
					mark++;
				} while (--c);
			}
			node = (mnode_t *) leaf;
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/* LIGHTMAP ALLOCATION */

// returns a texture number and the position inside it
int
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

int         nColinElim;
model_t    *currentmodel;
mvertex_t  *r_pcurrentvertbase;

void
BuildSurfaceDisplayList (msurface_t *fa)
{
	float       s, t;
	float      *vec;
	int         lindex, lnumverts, vertpage, i;
	glpoly_t   *poly;
	medge_t    *pedges, *r_pedge;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = Hunk_Alloc (sizeof (glpoly_t) + (lnumverts - 4) *
					   VERTEXSIZE * sizeof (float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16;			// fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16;			// fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	// remove co-linear points - Ed
	if (!gl_keeptjunctions->int_val && !(fa->flags & SURF_UNDERWATER)) {
		for (i = 0; i < lnumverts; ++i) {
			vec3_t      v1, v2;
			float      *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract (this, prev, v1);
			VectorNormalize (v1);
			VectorSubtract (next, prev, v2);
			VectorNormalize (v2);

			// skip co-linear points
#			define COLINEAR_EPSILON 0.001
			if ((fabs (v1[0] - v2[0]) <= COLINEAR_EPSILON) &&
				(fabs (v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
				(fabs (v1[2] - v2[2]) <= COLINEAR_EPSILON)) {
				int         j;

				for (j = i + 1; j < lnumverts; ++j) {
					int         k;

					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

void
GL_CreateSurfaceLightmap (msurface_t *surf)
{
	byte       *base;
	int         smax, tmax;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum =
		AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps[surf->lightmaptexturenum] +
		(surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH * lightmap_bytes);
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

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	switch (r_lightmap_components->int_val) {
	case 1:
		gl_internalformat = 1;
		gl_lightmap_format = GL_LUMINANCE;
		lightmap_bytes = 1;
		break;
	case 3:
		gl_internalformat = 3;
		gl_lightmap_format = GL_RGB;
		lightmap_bytes = 3;
		break;
	case 4:
	default:
		gl_internalformat = 3;
		gl_lightmap_format = GL_RGBA;
		lightmap_bytes = 4;
		break;
	}

	for (j = 1; j < num_models; j++) {
		m = models[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & SURF_DRAWTURB)
				continue;
			if (gl_sky_divide->int_val && (m->surfaces[i].flags &
										   SURF_DRAWSKY))
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	if (gl_mtex_active)
		qglActiveTexture (gl_mtex_enum + 1);

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;						// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		qfglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qfglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH,
					  BLOCK_HEIGHT, 0, gl_lightmap_format,
					  GL_UNSIGNED_BYTE, lightmaps[i]);
	}

	if (gl_mtex_active)
		qglActiveTexture (gl_mtex_enum + 0);
}
