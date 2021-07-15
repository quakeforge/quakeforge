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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "qfalloca.h"

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
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"

static instsurf_t  *waterchain = NULL;
static instsurf_t **waterchain_tail = &waterchain;
static instsurf_t  *sky_chain;
static instsurf_t **sky_chain_tail;

#define CHAIN_SURF_F2B(surf,chain)							\
	({ 														\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			inst = get_instsurf ();							\
		inst->surface = (surf);								\
		*(chain##_tail) = inst;								\
		(chain##_tail) = &inst->tex_chain;					\
		*(chain##_tail) = 0;								\
		inst;												\
	})

#define CHAIN_SURF_B2F(surf,chain) 							\
	({	 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			inst = get_instsurf ();							\
		inst->surface = (surf);								\
		inst->tex_chain = (chain);							\
		(chain) = inst;										\
		inst;												\
	})

static gltex_t   **r_texture_chains;
static int  r_num_texture_chains;
static int  max_texture_chains;

static instsurf_t *static_chains;
static instsurf_t *free_instsurfs;
static instsurf_t *alloced_instsurfs;
static instsurf_t **alloced_instsurfs_tail = &alloced_instsurfs;
#define NUM_INSTSURFS (64 * 6)	// most brush models are simple boxes.

static inline instsurf_t *
get_instsurf (void)
{
	instsurf_t *instsurf;

	if (!free_instsurfs) {
		int         i;

		free_instsurfs = calloc (NUM_INSTSURFS, sizeof (instsurf_t));
		for (i = 0; i < NUM_INSTSURFS - 1; i++)
			free_instsurfs[i]._next = &free_instsurfs[i + 1];
	}
	instsurf = free_instsurfs;
	free_instsurfs = instsurf->_next;
	instsurf->_next = 0;
	//build the chain of allocated instance surfaces so they can all be freed
	//in one go
	*alloced_instsurfs_tail = instsurf;
	alloced_instsurfs_tail = &instsurf->_next;
	return instsurf;
}

static inline void
release_instsurfs (void)
{
	if (alloced_instsurfs) {
		*alloced_instsurfs_tail = free_instsurfs;
		free_instsurfs = alloced_instsurfs;
		alloced_instsurfs = 0;
		alloced_instsurfs_tail = &alloced_instsurfs;
	}
}

void
gl_R_AddTexture (texture_t *tx)
{
	int         i;
	if (r_num_texture_chains == max_texture_chains) {
		max_texture_chains += 64;
		r_texture_chains = realloc (r_texture_chains,
								  max_texture_chains * sizeof (gltex_t *));
		for (i = r_num_texture_chains; i < max_texture_chains; i++)
			r_texture_chains[i] = 0;
	}
	gltex_t    *tex = tx->render;
	r_texture_chains[r_num_texture_chains++] = tex;
	tex->tex_chain = NULL;
	tex->tex_chain_tail = &tex->tex_chain;
}

void
gl_R_InitSurfaceChains (mod_brush_t *brush)
{
	int         i;

	if (static_chains)
		free (static_chains);
	static_chains = calloc (brush->nummodelsurfaces, sizeof (instsurf_t));
	for (i = 0; i < brush->nummodelsurfaces; i++)
		brush->surfaces[i].instsurf = static_chains + i;

	release_instsurfs ();
}

void
gl_R_ClearTextures (void)
{
	r_num_texture_chains = 0;
}


// BRUSH MODELS ===============================================================

static void
R_RenderFullbrights (void)
{
	float      *v;
	int         i, j;
	glpoly_t   *p;
	instsurf_t *sc;
	gltex_t    *tex;

	for (i = 0; i < r_num_texture_chains; i++) {
		if (!(tex = r_texture_chains[i]) || !tex->gl_fb_texturenum)
			continue;
		qfglBindTexture (GL_TEXTURE_2D, tex->gl_fb_texturenum);
		for (sc = tex->tex_chain; sc; sc = sc->tex_chain) {
			if (sc->transform) {
				qfglPushMatrix ();
				qfglLoadMatrixf (sc->transform);
			}
			if (sc->color)
				qfglColor4fv (sc->color);
			for (p = sc->surface->polys; p; p = p->next) {
				qfglBegin (GL_POLYGON);
				for (j = 0, v = p->verts[0]; j < p->numverts;
					 j++, v += VERTEXSIZE)
				{
					qfglTexCoord2fv (&v[3]);
					qfglVertex3fv (v);
				}
				qfglEnd ();
			}
			if (sc->transform)
				qfglPopMatrix ();
			if (sc->color)
				qfglColor3ubv (color_white);
		}
	}
}

static inline void
R_RenderBrushPoly_3 (msurface_t *surf)
{
	float      *v;
	int			i;

	gl_c_brush_polys++;

	qfglBegin (GL_POLYGON);
	v = surf->polys->verts[0];

	for (i = 0; i < surf->polys->numverts; i++, v += VERTEXSIZE) {
		qglMultiTexCoord2fv (gl_mtex_enum + 0, &v[3]);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, &v[5]);
		qglMultiTexCoord2fv (gl_mtex_enum + 2, &v[3]);
		qfglVertex3fv (v);
	}

	qfglEnd ();
}

static inline void
R_RenderBrushPoly_2 (msurface_t *surf)
{
	float      *v;
	int			i;

	gl_c_brush_polys++;

	qfglBegin (GL_POLYGON);
	v = surf->polys->verts[0];

	for (i = 0; i < surf->polys->numverts; i++, v += VERTEXSIZE) {
		qglMultiTexCoord2fv (gl_mtex_enum + 0, &v[3]);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, &v[5]);
		qfglVertex3fv (v);
	}

	qfglEnd ();
}

static inline void
R_RenderBrushPoly_1 (msurface_t *surf)
{
	float      *v;
	int			i;

	gl_c_brush_polys++;

	qfglBegin (GL_POLYGON);
	v = surf->polys->verts[0];

	for (i = 0; i < surf->polys->numverts; i++, v += VERTEXSIZE) {
		qfglTexCoord2fv (&v[3]);
		qfglVertex3fv (v);
	}

	qfglEnd ();
}

static inline void
R_AddToLightmapChain (mod_brush_t *brush, msurface_t *surf, instsurf_t *sc)
{
	int		maps, smax, tmax;
	glRect_t 	*theRect;

	// add the poly to the proper lightmap chain
	sc->lm_chain = gl_lightmap_polys[surf->lightmaptexturenum];
	gl_lightmap_polys[surf->lightmaptexturenum] = sc;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		if (d_lightstylevalue[surf->styles[maps]] != surf->cached_light[maps])
			goto dynamic;

	if ((surf->dlightframe == r_framecount) || surf->cached_dlight) {
	  dynamic:
		if (r_dynamic->int_val) {
			gl_lightmap_modified[surf->lightmaptexturenum] = true;
			theRect = &gl_lightmap_rectchange[surf->lightmaptexturenum];
			if (surf->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - surf->light_t;
				theRect->t = surf->light_t;
			}
			if (surf->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - surf->light_s;
				theRect->l = surf->light_s;
			}
			smax = (surf->extents[0] >> 4) + 1;
			tmax = (surf->extents[1] >> 4) + 1;
			if ((theRect->w + theRect->l) < (surf->light_s + smax))
				theRect->w = (surf->light_s - theRect->l) + smax;
			if ((theRect->h + theRect->t) < (surf->light_t + tmax))
				theRect->h = (surf->light_t - theRect->t) + tmax;
			gl_R_BuildLightMap (brush, surf);
		}
	}
}

void
gl_R_DrawWaterSurfaces (void)
{
	int         i;
	instsurf_t *s;
	msurface_t *surf;
	float       wateralpha = max (vr_data.min_wateralpha, r_wateralpha->value);

	if (!waterchain)
		return;

	// go back to the world matrix
	qfglLoadMatrixf (gl_r_world_matrix);

	if (wateralpha < 1.0) {
		qfglDepthMask (GL_FALSE);
		color_white[3] = wateralpha * 255;
		qfglColor4ubv (color_white);
	}

	i = -1;
	for (s = waterchain; s; s = s->tex_chain) {
		gltex_t    *tex;
		surf = s->surface;
		if (s->transform)
			qfglLoadMatrixf (s->transform);
		else
			qfglLoadMatrixf (gl_r_world_matrix);
		tex = surf->texinfo->texture->render;
		if (i != tex->gl_texturenum) {
			i = tex->gl_texturenum;
			qfglBindTexture (GL_TEXTURE_2D, i);
		}
		GL_EmitWaterPolys (surf);
	}
	qfglLoadMatrixf (gl_r_world_matrix);

	waterchain = NULL;
	waterchain_tail = &waterchain;

	if (wateralpha < 1.0) {
		qfglDepthMask (GL_TRUE);
		qfglColor3ubv (color_white);
	}
}

static void
DrawTextureChains (int disable_blend, int do_bind)
{
	int			i;
	instsurf_t *s;
	msurface_t *surf;
	gltex_t    *tex;

	if (gl_mtex_active_tmus >= 2) {
		// Lightmaps
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);

		// Base Texture
		qglActiveTexture (gl_mtex_enum + 0);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		for (i = 0; i < r_num_texture_chains; i++) {
			tex = r_texture_chains[i];
			if (!tex)
				continue;
			qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);

			if (tex->gl_fb_texturenum && gl_mtex_fullbright) {
				qglActiveTexture (gl_mtex_enum + 2);
				qfglEnable (GL_TEXTURE_2D);
				qfglBindTexture (GL_TEXTURE_2D, tex->gl_fb_texturenum);

				qglActiveTexture (gl_mtex_enum + 1);
				for (s = tex->tex_chain; s; s = s->tex_chain) {
					surf = s->surface;
					if (s->transform) {
						qfglPushMatrix ();
						qfglLoadMatrixf (s->transform);
					}
					if (s->color && do_bind)
						qfglColor4fv (s->color);
					qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures +
									 surf->lightmaptexturenum);

					R_RenderBrushPoly_3 (surf);

					if (s->transform)
						qfglPopMatrix ();
					if (s->color && do_bind)
						qfglColor3ubv (color_white);
				}

				qglActiveTexture (gl_mtex_enum + 2);
				qfglDisable (GL_TEXTURE_2D);

				qglActiveTexture (gl_mtex_enum + 0);
			} else {
				qglActiveTexture (gl_mtex_enum + 1);
				for (s = tex->tex_chain; s; s = s->tex_chain) {
					surf = s->surface;
					qfglBindTexture (GL_TEXTURE_2D, gl_lightmap_textures +
									 surf->lightmaptexturenum);

					if (s->transform) {
						qfglPushMatrix ();
						qfglLoadMatrixf (s->transform);
					}
					if (s->color && do_bind)
						qfglColor4fv (s->color);
					R_RenderBrushPoly_2 (surf);

					if (s->transform)
						qfglPopMatrix ();
					if (s->color && do_bind)
						qfglColor3ubv (color_white);
				}

				qglActiveTexture (gl_mtex_enum + 0);
			}
		}
		// Turn off lightmaps for other entities
		qglActiveTexture (gl_mtex_enum + 1);
		qfglDisable (GL_TEXTURE_2D);

		// Reset mode for default TMU
		qglActiveTexture (gl_mtex_enum + 0);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else {
		if (disable_blend)
			qfglDisable (GL_BLEND);
		for (i = 0; i < r_num_texture_chains; i++) {
			tex = r_texture_chains[i];
			if (!tex)
				continue;
			if (do_bind)
				qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);

			for (s = tex->tex_chain; s; s = s->tex_chain) {
				if (s->transform) {
					qfglPushMatrix ();
					qfglLoadMatrixf (s->transform);
				}
				R_RenderBrushPoly_1 (s->surface);

				if (s->transform)
					qfglPopMatrix ();
				if (s->color && do_bind)
					qfglColor3ubv (color_white);
			}
		}
		if (disable_blend)
			qfglEnable (GL_BLEND);
	}
}

static void
clear_texture_chains (void)
{
	int			i;
	gltex_t    *tex;

	for (i = 0; i < r_num_texture_chains; i++) {
		tex = r_texture_chains[i];
		if (!tex)
			continue;
		tex->tex_chain = NULL;
		tex->tex_chain_tail = &tex->tex_chain;
	}
	tex = r_notexture_mip->render;
	tex->tex_chain = NULL;
	tex->tex_chain_tail = &tex->tex_chain;
	release_instsurfs ();

	memset (gl_lightmap_polys, 0, sizeof (gl_lightmap_polys));
}

static inline void
chain_surface (mod_brush_t *brush, msurface_t *surf, vec_t *transform,
			   float *color)
{
	instsurf_t *sc;

	if (surf->flags & SURF_DRAWTURB) {
		sc = CHAIN_SURF_B2F (surf, waterchain);
	} else if (surf->flags & SURF_DRAWSKY) {
		sc = CHAIN_SURF_F2B (surf, sky_chain);
	} else {
		texture_t  *tx;
		gltex_t    *tex;

		if (!surf->texinfo->texture->anim_total)
			tx = surf->texinfo->texture;
		else
			tx = R_TextureAnimation (surf);
		tex = tx->render;
		sc = CHAIN_SURF_F2B (surf, tex->tex_chain);

		R_AddToLightmapChain (brush, surf, sc);
	}
	sc->transform = transform;
	sc->color = color;
}

void
gl_R_DrawBrushModel (entity_t *e)
{
	float       dot, radius;
	int         i;
	unsigned int k;
	model_t    *model;
	msurface_t *surf;
	qboolean    rotated;
	vec3_t      mins, maxs;
	mod_brush_t *brush;
	mat4f_t     worldMatrix;

	model = e->renderer.model;
	brush = &model->brush;

	Transform_GetWorldMatrix (e->transform, worldMatrix);
	if (worldMatrix[0][0] != 1 || worldMatrix[1][1] != 1
		|| worldMatrix[2][2] != 1) {
		rotated = true;
		radius = model->radius;
#if 0 //QSG FIXME
		if (e->scale != 1.0)
			radius *= e->scale;
#endif
		if (R_CullSphere (&worldMatrix[3][0], radius)) {//FIXME
			return;
		}
	} else {
		rotated = false;
		VectorAdd (worldMatrix[3], model->mins, mins);
		VectorAdd (worldMatrix[3], model->maxs, maxs);
#if 0 // QSG FIXME
		if (e->scale != 1.0) {
			VectorScale (mins, e->scale, mins);
			VectorScale (maxs, e->scale, maxs);
		}
#endif
		if (R_CullBox (mins, maxs))
			return;
	}

	VectorSubtract (r_refdef.viewposition, worldMatrix[3], modelorg);
	if (rotated) {
		vec4f_t     temp = { modelorg[0], modelorg[1], modelorg[2], 0 };

		modelorg[0] = dotf (temp, worldMatrix[0])[0];
		modelorg[1] = dotf (temp, worldMatrix[1])[0];
		modelorg[2] = dotf (temp, worldMatrix[2])[0];
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (brush->firstmodelsurface != 0 && r_dlight_lightmap->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < r_maxdlights; k++) {
			if ((r_dlights[k].die < vr_data.realtime)
				 || (!r_dlights[k].radius))
				continue;

			VectorSubtract (r_dlights[k].origin, worldMatrix[3], lightorigin);
			R_RecursiveMarkLights (brush, lightorigin, &r_dlights[k], k,
							brush->nodes + brush->hulls[0].firstclipnode);
		}
	}

	qfglPushMatrix ();
	gl_R_RotateForEntity (e);
	qfglGetFloatv (GL_MODELVIEW_MATRIX, e->renderer.full_transform);
	qfglPopMatrix ();

	surf = &brush->surfaces[brush->firstmodelsurface];

	// draw texture
	for (i = 0; i < brush->nummodelsurfaces; i++, surf++) {
		// find which side of the node we are on
		plane_t    *plane = surf->plane;

		dot = DotProduct (modelorg, plane->normal) - plane->dist;

		// draw the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			chain_surface (brush, surf, e->renderer.full_transform,
						   e->renderer.colormod);
		}
	}
}

// WORLD MODEL ================================================================

static inline void
visit_leaf (mleaf_t *leaf)
{
	// deal with model fragments in this leaf
	if (leaf->efrags)
		R_StoreEfrags (leaf->efrags);
}

static inline int
get_side (mnode_t *node)
{
	// find which side of the node we are on
	plane_t    *plane = node->plane;

	if (plane->type < 3)
		return (modelorg[plane->type] - plane->dist) < 0;
	return (DotProduct (modelorg, plane->normal) - plane->dist) < 0;
}

static inline void
visit_node (mod_brush_t *brush, mnode_t *node, int side)
{
	int         c;
	msurface_t *surf;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	side = (~side + 1) & SURF_PLANEBACK;
	// draw stuff
	if ((c = node->numsurfaces)) {
		surf = brush->surfaces + node->firstsurface;
		for (; c; c--, surf++) {
			if (surf->visframe != r_visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;				// wrong side

			chain_surface (brush, surf, 0, 0);
		}
	}
}

static inline int
test_node (mnode_t *node)
{
	if (node->contents < 0)
		return 0;
	if (node->visframe != r_visframecount)
		return 0;
	if (R_CullBox (node->minmaxs, node->minmaxs + 3))
		return 0;
	return 1;
}

static void
R_VisitWorldNodes (mod_brush_t *brush)
{
	typedef struct {
		mnode_t    *node;
		int         side;
	} rstack_t;
	rstack_t   *node_ptr;
	rstack_t   *node_stack;
	mnode_t    *node;
	mnode_t    *front;
	int         side;

	node = brush->nodes;
	// +2 for paranoia
	node_stack = alloca ((brush->depth + 2) * sizeof (rstack_t));
	node_ptr = node_stack;

	while (1) {
		while (test_node (node)) {
			side = get_side (node);
			front = node->children[side];
			if (test_node (front)) {
				node_ptr->node = node;
				node_ptr->side = side;
				node_ptr++;
				node = front;
				continue;
			}
			if (front->contents < 0 && front->contents != CONTENTS_SOLID)
				visit_leaf ((mleaf_t *) front);
			visit_node (brush, node, side);
			node = node->children[!side];
		}
		if (node->contents < 0 && node->contents != CONTENTS_SOLID)
			visit_leaf ((mleaf_t *) node);
		if (node_ptr != node_stack) {
			node_ptr--;
			node = node_ptr->node;
			side = node_ptr->side;
			visit_node (brush, node, side);
			node = node->children[!side];
			continue;
		}
		break;
	}
	if (node->contents < 0 && node->contents != CONTENTS_SOLID)
		visit_leaf ((mleaf_t *) node);
}

void
gl_R_DrawWorld (void)
{
	entity_t    worldent;

	memset (&worldent, 0, sizeof (worldent));
	worldent.renderer.model = r_worldentity.renderer.model;

	VectorCopy (r_refdef.viewposition, modelorg);

	currententity = &worldent;

	sky_chain = 0;
	sky_chain_tail = &sky_chain;
	if (!gl_sky_clip->int_val) {
		gl_R_DrawSky ();
	}

	R_VisitWorldNodes (&r_worldentity.renderer.model->brush);
	if (r_drawentities->int_val) {
		entity_t   *ent;
		for (ent = r_ent_queue; ent; ent = ent->next) {
			if (ent->renderer.model->type != mod_brush) {
				continue;
			}
			currententity = ent;

			gl_R_DrawBrushModel (currententity);
		}
	}

	gl_R_CalcLightmaps ();

	gl_R_DrawSkyChain (sky_chain);

	if (!gl_Fog_GetDensity ()
		|| (gl_fb_bmodels->int_val && gl_mtex_fullbright)
		|| gl_mtex_active_tmus > 1) {
		// we have enough active TMUs to render everything in one go
		// or we're not doing fog
		DrawTextureChains (1, 1);

		if (gl_mtex_active_tmus <= 1)
			gl_R_BlendLightmaps ();

		if (gl_fb_bmodels->int_val && !gl_mtex_fullbright)
			R_RenderFullbrights ();
	} else {
		if (gl_mtex_active_tmus > 1) {
			// textures and lightmaps in one pass
			// black fog
			// no blending
			gl_Fog_StartAdditive ();
			DrawTextureChains (1, 1);
			// buf = fTL + (1-f)0
			//     = fTL
		} else {
			// texture pass + lightmap pass
			// no fog
			// no blending
			gl_Fog_DisableGFog ();
			DrawTextureChains (1, 1);
			// buf = T
			// black fog
			// blend: buf = zero, src (non-overbright)
			// FIXME overbright broken?
			gl_Fog_EnableGFog ();
			gl_Fog_StartAdditive ();
			gl_R_BlendLightmaps ();	//leaves blending as As, 1-As
			// buf = I*0 + buf*I
			//     = T*C
			//     = T(fL + (1-f)0)
			//     = fTL
		}
		// fullbright pass
		// fog is still black
		R_RenderFullbrights ();
		// buf = aI + (1-a)buf
		//     = aC + (1-a)fTL
		//     = a(fG + (1-f)0) + (1-a)fTL
		//     = afG + (1-a)fTL
		//     = f((1-a)TL + aG)
		gl_Fog_StopAdditive ();		// use fog color
		qfglDepthMask (GL_FALSE);	// don't write Z
		qfglBlendFunc (GL_ONE, GL_ONE);
		// draw black polys
		qfglColor4f (0, 0, 0, 1);
		DrawTextureChains (0, 0);
		// buf = I + buf
		//     = C + f((1-a)TL + aG)
		//     = (f0 + (1-f)F) + f((1-a)TL + aG)
		//     = (1-f)F + f((1-a)TL + aG)
		//     = f((1-a)TL + aG) + (1-f)F
		// restore state
		qfglColor4ubv (color_white);
		qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qfglDepthMask (GL_TRUE);
	}

	clear_texture_chains ();
}

model_t    *gl_currentmodel;

void
GL_BuildSurfaceDisplayList (msurface_t *surf)
{
	float       s, t;
	float      *vec;
	int         lindex, lnumverts, i;
	glpoly_t   *poly;
	medge_t    *pedges, *r_pedge;

	// reconstruct the polygon
	pedges = gl_currentmodel->brush.edges;
	lnumverts = surf->numedges;

	// draw texture
	poly = Hunk_Alloc (sizeof (glpoly_t) + (lnumverts - 4) *
					   VERTEXSIZE * sizeof (float));
	poly->next = surf->polys;
	poly->flags = surf->flags;
	surf->polys = poly;
	poly->numverts = lnumverts;

	mtexinfo_t *texinfo = surf->texinfo;
	for (i = 0; i < lnumverts; i++) {
		lindex = gl_currentmodel->brush.surfedges[surf->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, texinfo->vecs[0]) + texinfo->vecs[0][3];
		s /= texinfo->texture->width;

		t = DotProduct (vec, texinfo->vecs[1]) + texinfo->vecs[1][3];
		t /= texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, texinfo->vecs[0]) + texinfo->vecs[0][3];
		s -= surf->texturemins[0];
		s += surf->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16;

		t = DotProduct (vec, texinfo->vecs[1]) + texinfo->vecs[1][3];
		t -= surf->texturemins[1];
		t += surf->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	// remove co-linear points - Ed
	if (!gl_keeptjunctions->int_val && !(surf->flags & SURF_UNDERWATER)) {
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
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}
