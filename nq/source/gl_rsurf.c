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

	$Id$
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

#include <stdio.h>
#include <math.h>

#include "QF/compat.h"
#include "r_local.h"
#include "QF/qargs.h"
#include "QF/vid.h"
#include "QF/sys.h"
#include "QF/mathlib.h"
#include "QF/wad.h"
#include "draw.h"
#include "QF/cvar.h"
#include "protocol.h"
#include "QF/cmd.h"
#include "sbar.h"
#include "render.h"
#include "client.h"
#include "QF/model.h"
#include "QF/console.h"
#include "glquake.h"

extern double realtime;
int         skytexturenum;

extern vec3_t shadecolor;				// Ender (Extend) Colormod
int         lightmap_bytes;				// 1 or 3

int         lightmap_textures;

unsigned    blocklights[18 * 18 * 3];

cvar_t     *gl_colorlights;
cvar_t     *gl_lightmap_components;

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

// LordHavoc: since lightmaps are now allocated only as needed, allow a ridiculous number :)
#define	MAX_LIGHTMAPS	1024
int         active_lightmaps;

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

glpoly_t   *lightmap_polys[MAX_LIGHTMAPS];
glpoly_t   *fullbright_polys[MAX_GLTEXTURES];
qboolean    lightmap_modified[MAX_LIGHTMAPS];
glRect_t    lightmap_rectchange[MAX_LIGHTMAPS];

int         allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
// LordHavoc: changed to be allocated at runtime (typically lower memory usage)
byte       *lightmaps[MAX_LIGHTMAPS];

msurface_t *waterchain = NULL;

extern qboolean lighthalf;


// LordHavoc: place for gl_rsurf setup code
void
glrsurf_init ()
{
	memset (&lightmaps, 0, sizeof (lightmaps));
}


void
recursivelightupdate (mnode_t *node)
{
	int         c;
	msurface_t *surf;

	if (node->children[0]->contents >= 0)
		recursivelightupdate (node->children[0]);
	if (node->children[1]->contents >= 0)
		recursivelightupdate (node->children[1]);
	if ((c = node->numsurfaces))
		for (surf = cl.worldmodel->surfaces + node->firstsurface; c;
			 c--, surf++) surf->cached_dlight = true;
}


// LordHavoc: function to force all lightmaps to be updated
void
R_ForceLightUpdate ()
{
	if (cl.worldmodel && cl.worldmodel->nodes
		&& cl.worldmodel->nodes->contents >= 0)
		recursivelightupdate (cl.worldmodel->nodes);
}


/*
	R_AddDynamicLights

	LordHavoc: completely rewrote this, relies on 64bit integer math...
*/
int         dlightdivtable[8192];
int         dlightdivtableinitialized = 0;

void
R_AddDynamicLights (msurface_t *surf)
{
	int         sdtable[18], lnum, td, maxdist, maxdist2, maxdist3, i, s, t,
		smax, tmax, red, green, blue, j;
	unsigned   *bl;
	float       dist, f;
	vec3_t      impact, local;

	// use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64     k;
#else
	long long   k;
#endif

	if (!dlightdivtableinitialized) {
		dlightdivtable[0] = 1048576 >> 7;
		for (s = 1; s < 8192; s++)
			dlightdivtable[s] = 1048576 / (s << 7);
		dlightdivtableinitialized = 1;
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (!(surf->dlightbits & (1 << lnum)))
			continue;					// not lit by this light

		VectorSubtract (cl_dlights[lnum].origin, currententity->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;
		for (i = 0; i < 3; i++)
			impact[i] =
				cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;

		f =
			DotProduct (impact,
						surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] -
			surf->texturemins[0];
		i = f;

		// reduce calculations
		t = dist * dist;
		for (s = 0; s < smax; s++, i -= 16)
			sdtable[s] = i * i + t;

		f =
			DotProduct (impact,
						surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] -
			surf->texturemins[1];
		i = f;

		maxdist = (int) (cl_dlights[lnum].radius * cl_dlights[lnum].radius);	// for 
																				// 
		// 
		// comparisons 
		// to 
		// minimum 
		// acceptable 
		// light
		// clamp radius to avoid exceeding 8192 entry division table
		if (maxdist > 1048576)
			maxdist = 1048576;
		maxdist3 = maxdist - (int) (dist * dist);
		// convert to 8.8 blocklights format
//      if (!cl_dlights[lnum].dark)
//      {
		f = cl_dlights[lnum].color[0] * maxdist;
		red = f;
		f = cl_dlights[lnum].color[1] * maxdist;
		green = f;
		f = cl_dlights[lnum].color[2] * maxdist;
		blue = f;
		/* 
		   } else // negate for darklight { f = cl_dlights[lnum].color[0] *
		   -maxdist;red = f; f = cl_dlights[lnum].color[1] * -maxdist;green = 
		   f; f = cl_dlights[lnum].color[2] * -maxdist;blue = f; } */
		bl = blocklights;
		for (t = 0; t < tmax; t++, i -= 16) {
			td = i * i;
			if (td < maxdist3)			// make sure some part of it is
				// visible on this line
			{
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++) {
					if (sdtable[s] < maxdist2) {
						j = dlightdivtable[(sdtable[s] + td) >> 7];
						k = (red * j) >> 7;
						bl[0] += k;
						k = (green * j) >> 7;
						bl[1] += k;
						k = (blue * j) >> 7;
						bl[2] += k;
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

	Combine and scale multiple lightmaps
	After talking it over with LordHavoc, I've decided to switch to using
	 GL_RGB for colored lights and averaging them out for plain white
	 lighting if needed.  Much cleaner that way.  --KB
*/
void
R_BuildLightMap (msurface_t *surf, byte * dest, int stride)
{
	int         smax, tmax;
	int         t;
	int         i, j, size;
	byte       *lightmap;
	unsigned    scale;
	int         maps;
	float       t2;
	unsigned   *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if ( /* r_fullbright->int_val || */ !cl.worldmodel->lightdata) {
		bl = blocklights;
		for (i = 0; i < size; i++) {
			*bl++ = 255 * 256;
			*bl++ = 255 * 256;
			*bl++ = 255 * 256;
		}
		goto store;
	}
	// clear to no light
	bl = blocklights;
	for (i = 0; i < size; i++) {
		*bl++ = 0;
		*bl++ = 0;
		*bl++ = 0;
	}
	bl = blocklights;

	// add all the lightmaps
	if (lightmap)
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < size; i++) {
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
		}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

  store:
	// bound and shift
	if (gl_colorlights->int_val) {
		stride -= smax * 3;
		bl = blocklights;
		if (lighthalf) {
			for (i = 0; i < tmax; i++, dest += stride)
				for (j = 0; j < smax; j++) {
					t = (int) *bl++ >> 8;
					*dest++ = bound (0, t, 255);
					t = (int) *bl++ >> 8;
					*dest++ = bound (0, t, 255);
					t = (int) *bl++ >> 8;
					*dest++ = bound (0, t, 255);
				}
		} else {
			for (i = 0; i < tmax; i++, dest += stride)
				for (j = 0; j < smax; j++) {
					t = (int) *bl++ >> 7;
					*dest++ = bound (0, t, 255);
					t = (int) *bl++ >> 7;
					*dest++ = bound (0, t, 255);
					t = (int) *bl++ >> 7;
					*dest++ = bound (0, t, 255);
				}
		}
	} else {
		stride -= smax;
		bl = blocklights;
		if (lighthalf) {
			for (i = 0; i < tmax; i++, dest += stride)
				for (j = 0; j < smax; j++) {
					t = (int) *bl++ >> 8;
					t2 = bound (0, t, 255);
					t = (int) *bl++ >> 8;
					t2 += bound (0, t, 255);
					t = (int) *bl++ >> 8;
					t2 += bound (0, t, 255);
					t2 *= (1.0 / 3.0);
					*dest++ = t2;
				}
		} else {
			for (i = 0; i < tmax; i++, dest += stride)
				for (j = 0; j < smax; j++) {
					t = (int) *bl++ >> 7;
					t2 = bound (0, t, 255);
					t = (int) *bl++ >> 7;
					t2 += bound (0, t, 255);
					t = (int) *bl++ >> 7;
					t2 += bound (0, t, 255);
					t2 *= (1.0 / 3.0);
					*dest++ = t2;
				}
		}
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t  *
R_TextureAnimation (texture_t *base)
{
	int         relative;
	int         count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (cl.time * 10) % base->anim_total;

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


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern int  solidskytexture;
extern int  alphaskytexture;
extern float speedscale;				// for top sky and bottom sky

lpMTexFUNC  qglMTexCoord2f = NULL;
lpSelTexFUNC qglSelectTexture = NULL;

void
GL_UploadLightmap (int i, int x, int y, int w, int h)
{
	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, y, BLOCK_WIDTH, h, gl_lightmap_format,
					 GL_UNSIGNED_BYTE,
					 lightmaps[i] + (y * BLOCK_WIDTH) * lightmap_bytes);
}


/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void
R_DrawMultitexturePoly (msurface_t *s)
{
	int         maps;
	float      *v;
	int         i;
	texture_t  *texture = R_TextureAnimation (s->texinfo->texture);

	c_brush_polys++;

	i = s->lightmaptexturenum;

	glColor3f (1, 1, 1);
	// Binds world to texture env 0
	qglSelectTexture (gl_mtex_enum + 0);
	glBindTexture (GL_TEXTURE_2D, texture->gl_texturenum);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable (GL_TEXTURE_2D);
	// Binds lightmap to texenv 1
	qglSelectTexture (gl_mtex_enum + 1);
	glBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable (GL_TEXTURE_2D);

	// check for lightmap modification
	if (r_dynamic->int_val) {
		for (maps = 0; maps < MAXLIGHTMAPS && s->styles[maps] != 255; maps++)
			if (d_lightstylevalue[s->styles[maps]] != s->cached_light[maps])
				goto dynamic;

		if (s->dlightframe == r_framecount	// dynamic this frame
			|| s->cached_dlight)		// dynamic previously
		{
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

	glBegin (GL_POLYGON);
	v = s->polys->verts[0];
	for (i = 0; i < s->polys->numverts; i++, v += VERTEXSIZE) {
		qglMTexCoord2f (gl_mtex_enum + 0, v[3], v[4]);
		qglMTexCoord2f (gl_mtex_enum + 1, v[5], v[6]);
		glVertex3fv (v);
	}
	glEnd ();
	glDisable (GL_TEXTURE_2D);
	qglSelectTexture (gl_mtex_enum + 0);
	glEnable (GL_TEXTURE_2D);

	if (texture->gl_fb_texturenum > 0) {
		s->polys->fb_chain = fullbright_polys[texture->gl_fb_texturenum];
		fullbright_polys[texture->gl_fb_texturenum] = s->polys;
	}
}


void
R_BlendLightmaps (void)
{
	int         i, j;
	glpoly_t   *p;
	float      *v;

	glDepthMask (0);					// don't bother writing Z

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc (GL_ZERO, GL_SRC_COLOR);
	glEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		glBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		if (lightmap_modified[i]) {
			GL_UploadLightmap (i, lightmap_rectchange[i].l,
							   lightmap_rectchange[i].t,
							   lightmap_rectchange[i].w,
							   lightmap_rectchange[i].h);
			lightmap_modified[i] = false;
		}
		for (; p; p = p->chain) {
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				glTexCoord2fv (&v[5]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}

	// Return to normal blending  --KB
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask (1);					// back to normal Z buffering
}


void
R_RenderFullbrights (void)
{
	int         i, j;
	glpoly_t   *p;
	float      *v;

	// glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc (GL_ONE, GL_ONE);
	glEnable (GL_BLEND);
	glColor3f (1, 1, 1);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;
		glBindTexture (GL_TEXTURE_2D, i);
		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			glBegin (GL_POLYGON);
			for (j = 0, v = p->verts[0]; j < p->numverts; j++, v += VERTEXSIZE) {
				glTexCoord2fv (&v[3]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}

	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void
R_RenderBrushPoly (msurface_t *fa)
{
	byte       *base;
	int         maps;
	glRect_t   *theRect;
	int         i;
	float      *v;
	int         smax, tmax;
	texture_t  *texture = R_TextureAnimation (fa->texinfo->texture);

	c_brush_polys++;

	glBindTexture (GL_TEXTURE_2D, texture->gl_texturenum);

	glBegin (GL_POLYGON);
	v = fa->polys->verts[0];
	for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
		glTexCoord2fv (&v[3]);
		glVertex3fv (v);
	}
	glEnd ();

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

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
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
				lightmaps[fa->lightmaptexturenum] + (fa->light_t * BLOCK_WIDTH +
													 fa->light_s) *
				lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}


void
GL_WaterSurface (msurface_t *s)
{
	int         i;

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	if (lighthalf)
		glColor4f (0.5, 0.5, 0.5, r_wateralpha->value);
	else
		glColor4f (1, 1, 1, r_wateralpha->value);
	i = s->texinfo->texture->gl_texturenum;
	glBindTexture (GL_TEXTURE_2D, i);
	if (r_wateralpha->value < 1.0) {
		glDepthMask (0);
		EmitWaterPolys (s);
		glDepthMask (1);
	} else
		EmitWaterPolys (s);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


void
R_DrawWaterSurfaces (void)
{
	int         i;
	msurface_t *s;

	if (!waterchain)
		return;

	// go back to the world matrix

	glLoadMatrixf (r_world_matrix);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	if (lighthalf)
		glColor4f (0.5, 0.5, 0.5, r_wateralpha->value);
	else
		glColor4f (1, 1, 1, r_wateralpha->value);
	if (r_wateralpha->value < 1.0)
		glDepthMask (0);

	i = -1;
	for (s = waterchain; s; s = s->texturechain) {
		if (i != s->texinfo->texture->gl_texturenum) {
			i = s->texinfo->texture->gl_texturenum;
			glBindTexture (GL_TEXTURE_2D, i);
		}
		EmitWaterPolys (s);
	}

	waterchain = NULL;

	glColor3f (1, 1, 1);
	if (r_wateralpha->value < 1.0)
		glDepthMask (1);
}


void
DrawTextureChains (void)
{
	int         i;
	msurface_t *s;

	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		for (s = cl.worldmodel->textures[i]->texturechain; s;
			 s = s->texturechain) R_RenderBrushPoly (s);

		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}


void
R_DrawBrushModel (entity_t *e)
{
	int         i;
	int         k;
	vec3_t      mins, maxs;
	msurface_t *psurf;
	float       dot;
	mplane_t   *pplane;
	model_t    *clmodel;
	qboolean    rotated;

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

	glColor3f (1, 1, 1);

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

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < MAX_DLIGHTS; k++) {
			if ((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
				continue;

			VectorSubtract (cl_dlights[k].origin, e->origin, lightorigin);
			R_MarkLights (lightorigin, &cl_dlights[k], 1 << k,
						  clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix ();
	e->angles[0] = -e->angles[0];		// stupid quake bug
	R_RotateForEntity (e);
	e->angles[0] = -e->angles[0];		// stupid quake bug

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// 
	// draw texture
	// 
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		if (psurf->flags & SURF_DRAWSKY)
			return;

		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (psurf->flags & SURF_DRAWTURB)
				GL_WaterSurface (psurf);
			else if (gl_texsort->int_val)
				R_RenderBrushPoly (psurf);
			else
				R_DrawMultitexturePoly (psurf);
		}
	}

	if (gl_texsort->int_val)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val)
		R_RenderFullbrights ();

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);

	glPopMatrix ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/


void
R_RecursiveWorldNode (mnode_t *node)
{
	int         c, side;
	mplane_t   *plane;
	msurface_t *surf, **mark;
	mleaf_t    *pleaf;
	double      dot;

	if (node->contents == CONTENTS_SOLID)
		return;							// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs + 3))
		return;

// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;

		if ((c = pleaf->nummarksurfaces)) {
			mark = pleaf->firstmarksurface;
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}
		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}
// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type) {
		case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
		case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
		case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
		default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	side = dot < 0;

// recurse down the children, front side first
	// LordHavoc: save a stack frame by avoiding a call
	if (node->children[side]->contents != CONTENTS_SOLID
		&& node->children[side]->visframe == r_visframecount
		&& !R_CullBox (node->children[side]->minmaxs,
					   node->children[side]->minmaxs + 3))
		R_RecursiveWorldNode (node->children[side]);

// draw stuff
	if ((c = node->numsurfaces)) {
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for (; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
				continue;				// wrong side

			if (surf->flags & SURF_DRAWSKY)
				continue;

			if (surf->flags & SURF_DRAWTURB) {
				surf->texturechain = waterchain;
				waterchain = surf;
			} else if (gl_texsort->int_val) {
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			} else
				R_DrawMultitexturePoly (surf);
		}
	}
// recurse down the back side
	// LordHavoc: save a stack frame by avoiding a call
	side = !side;
	if (node->children[side]->contents != CONTENTS_SOLID
		&& node->children[side]->visframe == r_visframecount
		&& !R_CullBox (node->children[side]->minmaxs,
					   node->children[side]->minmaxs + 3))
		R_RecursiveWorldNode (node->children[side]);
}


void
R_DrawWorld (void)
{
	entity_t    ent;

	memset (&ent, 0, sizeof (ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;

	glColor3f (1.0, 1.0, 1.0);
	memset (lightmap_polys, 0, sizeof (lightmap_polys));
	memset (fullbright_polys, 0, sizeof (fullbright_polys));
	// Be sure to clear the skybox --KB
	R_DrawSky ();

	glDisable (GL_BLEND);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	R_RecursiveWorldNode (cl.worldmodel->nodes);

	DrawTextureChains ();

	if (gl_texsort->int_val)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val)
		R_RenderFullbrights ();

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
}


void
R_MarkLeaves (void)
{
	byte       *vis;
	mnode_t    *node;
	int         i;
	byte        solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis->int_val)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis->int_val) {
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int
AllocBlock (int w, int h, int *x, int *y)
{
	int         i, j;
	int         best, best2;
	int         texnum;

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
			lightmaps[texnum] = calloc (BLOCK_WIDTH * BLOCK_HEIGHT, 3);

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


mvertex_t  *r_pcurrentvertbase;
model_t    *currentmodel;

int         nColinElim;

void
BuildSurfaceDisplayList (msurface_t *fa)
{
	int         i, lindex, lnumverts;
	medge_t    *pedges, *r_pedge;
	int         vertpage;
	float      *vec;
	float       s, t;
	glpoly_t   *poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// 
	// draw texture
	// 
	poly =

		Hunk_Alloc (sizeof (glpoly_t) +
					(lnumverts - 4) * VERTEXSIZE * sizeof (float));
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

		// 
		// lightmap texture coordinates
		// 
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

	// 
	// remove co-linear points - Ed
	// 
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
	int         smax, tmax;
	byte       *base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum =
		AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base =
		lightmaps[surf->lightmaptexturenum] + (surf->light_t * BLOCK_WIDTH +
											   surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH * lightmap_bytes);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void
GL_BuildLightmaps (void)
{
	int         i, j;
	model_t    *m;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_ROM, NULL,
							   "Whether to use RGB lightmaps or not");

	if (gl_colorlights->int_val) {
		gl_lightmap_format = GL_RGB;
		lightmap_bytes = 3;
	} else {
		gl_lightmap_format = GL_LUMINANCE;
		lightmap_bytes = 1;
	}

	for (j = 1; j < MAX_MODELS; j++) {
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & SURF_DRAWTURB)
				continue;
			if (m->surfaces[i].flags & SURF_DRAWSKY)
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	if (!gl_texsort->int_val)
		qglSelectTexture (gl_mtex_enum + 1);

	// 
	// upload all lightmaps that were filled
	// 
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;						// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		glBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH,
					  BLOCK_HEIGHT, 0, gl_lightmap_format,
					  GL_UNSIGNED_BYTE, lightmaps[i]);
	}

	if (!gl_texsort->int_val)
		qglSelectTexture (gl_mtex_enum + 0);
}
