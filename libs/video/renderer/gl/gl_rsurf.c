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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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
#include "QF/GL/qf_lightmap.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_local.h"
#include "r_shared.h"

int			 skytexturenum;

glpoly_t    *fullbright_polys[MAX_GLTEXTURES];

msurface_t  *waterchain = NULL;
msurface_t **waterchain_tail = &waterchain;
msurface_t  *sky_chain;
msurface_t **sky_chain_tail;

#define CHAIN_SURF_F2B(surf,chain)				\
	do { 										\
		*(chain##_tail) = (surf);				\
		(chain##_tail) = &(surf)->texturechain;	\
		*(chain##_tail) = 0;					\
	} while (0)

#define CHAIN_SURF_B2F(surf,chain) 				\
	do { 										\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	} while (0)

extern int			lightmap_textures;
extern qboolean		lightmap_modified[MAX_LIGHTMAPS];
extern glpoly_t    *lightmap_polys[MAX_LIGHTMAPS];
extern glRect_t		lightmap_rectchange[MAX_LIGHTMAPS];


/*
  R_TextureAnimation

  Returns the proper texture for a given time and base texture
*/
texture_t  *
R_TextureAnimation (msurface_t *surf)
{
	texture_t  *base = surf->texinfo->texture;
	int         count, relative;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

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

// BRUSH MODELS ===============================================================

static void
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

inline void
R_RenderBrushPoly (msurface_t *fa)
{
	float      *v;
	int i;

	c_brush_polys++;

	qfglBegin (GL_POLYGON);
	v = fa->polys->verts[0];

	if (gl_mtex_active) {
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fv (gl_mtex_enum + 0, &v[3]);
			qglMultiTexCoord2fv (gl_mtex_enum + 1, &v[5]);

			if (gl_mtex_fullbright)
				qglMultiTexCoord2fv (gl_mtex_enum + 2, &v[3]);

			qfglVertex3fv (v);
		}
	} else {
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qfglTexCoord2fv (&v[3]);
			qfglVertex3fv (v);
		}
	}

	qfglEnd ();
}

static inline void
R_AddToLightmapChain (msurface_t *fa)
{
	int		maps, smax, tmax;
	glRect_t 	*theRect;

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

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
			R_BuildLightMap (fa);
		}
	}
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

	if (cl_wateralpha < 1.0) {
		qfglDepthMask (GL_FALSE);
		color_white[3] = cl_wateralpha * 255;
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
	waterchain_tail = &waterchain;

	if (cl_wateralpha < 1.0) {
		qfglDepthMask (GL_TRUE);
		qfglColor3ubv (color_white);
	}
}

static inline void
DrawTextureChains (void)
{
	int         i;
	msurface_t *s;
	texture_t  *tex;

	if (gl_mtex_active) {	
		qglActiveTexture (gl_mtex_enum + 0);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	} else {
		qfglDisable (GL_BLEND);
	}

	for (i = 0; i < r_worldentity.model->numtextures; i++) {
		tex = r_worldentity.model->textures[i];
		if (!tex)
			continue;

		qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);

		if (gl_mtex_fullbright&& gl_fb_bmodels->int_val && tex->gl_fb_texturenum) {
			qglActiveTexture (gl_mtex_enum + 2);
			qfglBindTexture (GL_TEXTURE_2D, tex->gl_fb_texturenum);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			qfglEnable (GL_TEXTURE_2D);
		}

		if (gl_mtex_active) {
			for (s = tex->texturechain; s; s = s->texturechain) {
				qglActiveTexture (gl_mtex_enum + 1);
				qfglBindTexture (GL_TEXTURE_2D, lightmap_textures + 
					s->lightmaptexturenum);
				qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				qfglEnable (GL_TEXTURE_2D);

				R_RenderBrushPoly (s);

				qfglDisable (GL_TEXTURE_2D);
			}
		} else {
			for (s = tex->texturechain; s; s = s->texturechain)
				R_RenderBrushPoly (s);
		}

		if (gl_mtex_fullbright && gl_fb_bmodels->int_val && tex->gl_fb_texturenum) {
			qglActiveTexture (gl_mtex_enum + 2);
			qfglDisable (GL_TEXTURE_2D);
		}

		if (gl_mtex_active)
			qglActiveTexture (gl_mtex_enum + 0);

		tex->texturechain = NULL;
		tex->texturechain_tail = &tex->texturechain;
	}
	tex = r_notexture_mip;
	tex->texturechain = NULL;
	tex->texturechain_tail = &tex->texturechain;

	if (!gl_mtex_active)
		qfglEnable (GL_BLEND);
	else {
		qglActiveTexture (gl_mtex_enum + 0);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}

void
R_DrawBrushModel (entity_t *e)
{
	float       dot, radius;
	float		color[4];
	int         i;
	unsigned int k;
	model_t    *model;
	mplane_t   *pplane;
	msurface_t *psurf;
	qboolean    rotated;
	vec3_t      mins, maxs;

	model = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		radius = model->radius;
#if 0 //QSG FIXME
		if (e->scale != 1.0)
			radius *= e->scale;
#endif
		if (R_CullSphere (e->origin, radius))
			return;
	} else {
		rotated = false;
		VectorAdd (e->origin, model->mins, mins);
		VectorAdd (e->origin, model->maxs, maxs);
#if 0 // QSG FIXME
		if (e->scale != 1.0) {
			VectorScale (mins, e->scale, mins);
			VectorScale (maxs, e->scale, maxs);
		}
#endif
		if (R_CullBox (mins, maxs))
			return;
	}

	VectorCopy (e->colormod, color);
	color[3] = e->colormod[3];

	memset (lightmap_polys, 0, sizeof (lightmap_polys));
	memset (fullbright_polys, 0, sizeof (fullbright_polys));

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t      temp, forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &model->surfaces[model->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (model->firstmodelsurface != 0 && r_dlight_lightmap->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < r_maxdlights; k++) {
			if ((r_dlights[k].die < r_realtime) || (!r_dlights[k].radius))
				continue;

			VectorSubtract (r_dlights[k].origin, e->origin, lightorigin);
			R_RecursiveMarkLights (lightorigin, &r_dlights[k], 1 << k,
							model->nodes + model->hulls[0].firstclipnode);
		}
	}

	qfglPushMatrix ();
	e->angles[0] = -e->angles[0];		// stupid quake bug
	R_RotateForEntity (e);
	e->angles[0] = -e->angles[0];		// stupid quake bug

	// Build lightmap chains
	for (i = 0; i < model->nummodelsurfaces; i++, psurf++)
		R_AddToLightmapChain(psurf);

	R_CalcLightmaps ();

	psurf = &model->surfaces[model->firstmodelsurface];

	// draw texture
	for (i = 0; i < model->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (psurf->flags & SURF_DRAWTURB) {
				qfglBindTexture (GL_TEXTURE_2D,
								 psurf->texinfo->texture->gl_texturenum);
				if (cl_wateralpha < 1.0) {
					qfglDepthMask (GL_FALSE);
					color[3] *= cl_wateralpha;
					qfglColor4fv (color);
					EmitWaterPolys (psurf);
					qfglColor3ubv (color_white);
					qfglDepthMask (GL_TRUE);
				} else {
					qfglColor4fv (color);
					EmitWaterPolys (psurf);
					qfglColor3ubv (color_white);
				}
			} else if (psurf->flags & SURF_DRAWSKY) {
// QSG FIXME: add modelalpha support for sky brushes
				CHAIN_SURF_F2B (psurf, sky_chain);
			} else {
				texture_t  *tex;

				if (!psurf->texinfo->texture->anim_total)
					tex = psurf->texinfo->texture;
				else
					tex = R_TextureAnimation (psurf);
				
				if (gl_mtex_active) {
					qglActiveTexture (gl_mtex_enum + 0);
					qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
					qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				} else {
					qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
				}

				qfglColor4fv (color);

				if (gl_fb_bmodels->int_val && gl_mtex_fullbright
					&& tex->gl_fb_texturenum > 0) {
					qglActiveTexture (gl_mtex_enum + 2);
					qfglBindTexture (GL_TEXTURE_2D, tex->gl_fb_texturenum);
					qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
					qfglEnable (GL_TEXTURE_2D);
				}					

				if (gl_mtex_active) {
					qglActiveTexture (gl_mtex_enum + 1);
					qfglBindTexture (GL_TEXTURE_2D, lightmap_textures + 
						psurf->lightmaptexturenum);
					qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);	
					qfglEnable (GL_TEXTURE_2D);
				}

				R_RenderBrushPoly (psurf);

				if (gl_mtex_active) {
					qfglDisable (GL_TEXTURE_2D);
				}

				if (gl_fb_bmodels->int_val && gl_mtex_fullbright
					&& tex->gl_fb_texturenum > 0) {
					qglActiveTexture (gl_mtex_enum + 1);
					qfglDisable (GL_TEXTURE_2D);
				}

				if (gl_mtex_active) {
					qglActiveTexture (gl_mtex_enum + 0);
					qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}

				qfglColor3ubv (color_white);
	
				if (tex->gl_fb_texturenum > 0 && gl_fb_bmodels->int_val) {
					psurf->polys->fb_chain =
						fullbright_polys[tex->gl_fb_texturenum];
					fullbright_polys[tex->gl_fb_texturenum] = psurf->polys;
				}
			}
		}
	}

	if (!gl_mtex_active)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val && !gl_mtex_fullbright)
		R_RenderFullbrights ();

	qfglPopMatrix ();
}

// WORLD MODEL ================================================================

static void
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
	// node is just a decision point, so go down the appropriate sides

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
				CHAIN_SURF_B2F (surf, waterchain);
			} else if (surf->flags & SURF_DRAWSKY) {
				CHAIN_SURF_F2B (surf, sky_chain);
			} else {
				texture_t  *tex;

				R_AddToLightmapChain (surf);

				if (!surf->texinfo->texture->anim_total)
					tex = surf->texinfo->texture;
				else
					tex = R_TextureAnimation (surf);
				if (tex->gl_fb_texturenum > 0) {
					surf->polys->fb_chain = fullbright_polys[tex->gl_fb_texturenum];
					fullbright_polys[tex->gl_fb_texturenum] = surf->polys;
				}
				CHAIN_SURF_F2B (surf, tex->texturechain);
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
	sky_chain_tail = &sky_chain;
	if (!gl_sky_clip->int_val) {
		R_DrawSky ();
	}

	R_RecursiveWorldNode (r_worldentity.model->nodes);

	R_CalcLightmaps ();

	R_DrawSkyChain (sky_chain);

	DrawTextureChains ();

	if (!gl_mtex_active)
		R_BlendLightmaps ();

	if (gl_fb_bmodels->int_val && !gl_mtex_fullbright)
		R_RenderFullbrights ();
}

void
R_MarkLeaves (void)
{
	byte         solid[4096];
	byte        *vis;
	int			 c;
	unsigned int i;
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

	for (i = 0; (int) i < r_worldentity.model->numleafs; i++) {
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
