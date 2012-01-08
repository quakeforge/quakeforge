/*
	glsl_bsp.c

	GLSL bsps

	Copyright (C) 2012       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_bsp.h"

#include "r_local.h"

#define ALLOC_CHUNK 64

instsurf_t  *waterchain = NULL;
instsurf_t **waterchain_tail = &waterchain;
instsurf_t  *sky_chain;
instsurf_t **sky_chain_tail = &sky_chain;

static texture_t  **r_texture_chains;
static int          r_num_texture_chains;
static int          max_texture_chains;

// for world and non-instance models
static elechain_t  *static_elechains;
static elechain_t **static_elechains_tail = &static_elechains;
static elechain_t  *free_static_elechains;
static elements_t  *static_elements;
static elements_t **static_elements_tail = &static_elements;
static elements_t  *free_static_elements;
static instsurf_t  *static_instsurfs;
static instsurf_t **static_instsurfs_tail = &static_instsurfs;
static instsurf_t  *free_static_instsurfs;

// for instance models
static elechain_t  *elechains;
static elechain_t **elechains_tail = &elechains;
static elechain_t  *free_elechains;
static elements_t  *elements;
static elements_t **elements_tail = &elements;
static elements_t  *free_elements;
static instsurf_t  *instsurfs;
static instsurf_t **instsurfs_tail = &instsurfs;
static instsurf_t  *free_instsurfs;

#define CHAIN_SURF_F2B(surf,chain)							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (!inst) (surf)->tinst = inst = get_instsurf ();	\
		inst->surface = (surf);								\
		*(chain##_tail) = inst;								\
		(chain##_tail) = &inst->tex_chain;					\
		*(chain##_tail) = 0;								\
	} while (0)

#define CHAIN_SURF_B2F(surf,chain) 							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (!inst) (surf)->tinst = inst = get_instsurf ();	\
		inst->surface = (surf);								\
		inst->tex_chain = (chain);							\
		(chain) = inst;										\
	} while (0)

#define GET_RELEASE(type,name) \
static inline type *												\
get_##name (void)													\
{																	\
	type       *ele;												\
	if (!free_##name##s) {											\
		int         i;												\
		free_##name##s = calloc (ALLOC_CHUNK, sizeof (type));		\
		for (i = 0; i < ALLOC_CHUNK - 1; i++)						\
			free_##name##s[i]._next = &free_##name##s[i + 1];		\
	}																\
	ele = free_##name##s;											\
	free_##name##s = ele->_next;									\
	ele->_next = 0;													\
	*name##s_tail = ele;											\
	name##s_tail = &ele->_next;										\
	return ele;														\
}																	\
static inline void													\
release_##name##s (void)											\
{																	\
	if (name##s) {													\
		*name##s_tail = free_##name##s;								\
		free_##name##s = name##s;									\
		name##s = 0;												\
		name##s_tail = &name##s;									\
	}																\
}

GET_RELEASE (elechain_t, static_elechain)
GET_RELEASE (elechain_t, elechain)
GET_RELEASE (elements_t, static_element)
GET_RELEASE (elements_t, element)
GET_RELEASE (instsurf_t, static_instsurf)
GET_RELEASE (instsurf_t, instsurf)

void
R_AddTexture (texture_t *tex)
{
	int         i;
	if (r_num_texture_chains == max_texture_chains) {
		max_texture_chains += 64;
		r_texture_chains = realloc (r_texture_chains,
								  max_texture_chains * sizeof (texture_t *));
		for (i = r_num_texture_chains; i < max_texture_chains; i++)
			r_texture_chains[i] = 0;
	}
	r_texture_chains[r_num_texture_chains++] = tex;
	tex->tex_chain = NULL;
	tex->tex_chain_tail = &tex->tex_chain;
}

void
R_ClearTextures (void)
{
	r_num_texture_chains = 0;
}

void
R_InitSurfaceChains (model_t *model)
{
	int         i;

	release_static_instsurfs ();
	release_instsurfs ();

	for (i = 0; i < model->nummodelsurfaces; i++)
		model->surfaces[i].instsurf = get_static_instsurf ();
}

static void
clear_texture_chains (void)
{
	int			i;
	texture_t  *tex;

	for (i = 0; i < r_num_texture_chains; i++) {
		tex = r_texture_chains[i];
		if (!tex)
			continue;
		tex->tex_chain = NULL;
		tex->tex_chain_tail = &tex->tex_chain;
	}
	tex = r_notexture_mip;
	tex->tex_chain = NULL;
	tex->tex_chain_tail = &tex->tex_chain;
	release_instsurfs ();
}

static void
register_textures (model_t *model)
{
	int         i;
	texture_t  *tex;

	for (i = 0; i < model->numtextures; i++) {
		tex = model->textures[i];
		if (!tex)
			continue;
		R_AddTexture (tex);
	}
}

void
R_ClearElements (void)
{
	release_static_elechains ();
	release_static_elements ();
	release_elechains ();
	release_elements ();
}

static inline void
chain_surface (msurface_t *surf, vec_t *transform, float *color)
{
	instsurf_t *sc;

	if (surf->flags & SURF_DRAWTURB) {
		CHAIN_SURF_B2F (surf, waterchain);
	} else if (surf->flags & SURF_DRAWSKY) {
		CHAIN_SURF_F2B (surf, sky_chain);
	} else {
		texture_t  *tex;

		if (!surf->texinfo->texture->anim_total)
			tex = surf->texinfo->texture;
		else
			tex = R_TextureAnimation (surf);
		CHAIN_SURF_F2B (surf, tex->tex_chain);

		//XXX R_AddToLightmapChain (surf);
	}
	if (!(sc = surf->instsurf))
		sc = surf->tinst;
	sc->transform = transform;
	sc->color = color;
}

void
R_BuildDisplayLists (model_t **models, int num_models)
{
	int         i, j;
	model_t    *m;
	msurface_t *surf;

	R_InitSurfaceChains (r_worldentity.model);
	R_AddTexture (r_notexture_mip);
	register_textures (r_worldentity.model);
	for (i = 0; i < num_models; i++) {
		m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->name == '*')
			continue;
		// world has already been done, not interested in non-brush models
		if (m == r_worldentity.model || m->type != mod_brush)
			continue;
		register_textures (m);
	}
	// now run through all surfaces, chaining them to their textures, thus
	// effectively sorting the surfaces by texture (without worrying about
	// surface order on the same texture chain).
	for (i = 0; i < num_models; i++) {
		m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->name == '*')
			continue;
		// non-bsp models don't have surfaces.
		for (j = 0; j < m->numsurfaces; j++) {
			texture_t  *tex;
			surf = m->surfaces + j;
			tex = surf->texinfo->texture;
			CHAIN_SURF_F2B (surf, tex->tex_chain);
		}
	}
}

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
		return (r_origin[plane->type] - plane->dist) < 0;
	return (DotProduct (r_origin, plane->normal) - plane->dist) < 0;
}

static inline void
visit_node (mnode_t *node, int side)
{
	int         c;
	msurface_t *surf;

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
				continue;               // wrong side

			//chain_surface (surf, 0, 0);
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
R_VisitWorldNodes (mnode_t *node)
{
#define NODE_STACK 1024
	struct {
		mnode_t    *node;
		int         side;
	}          *node_ptr, node_stack[NODE_STACK];
	mnode_t    *front;
	int         side;

	node_ptr = node_stack;

	while (1) {
		while (test_node (node)) {
			side = get_side (node);
			front = node->children[side];
			if (test_node (front)) {
				if (node_ptr - node_stack == NODE_STACK)
					Sys_Error ("node_stack overflow");
				node_ptr->node = node;
				node_ptr->side = side;
				node_ptr++;
				node = front;
				continue;
			}
			if (front->contents < 0 && front->contents != CONTENTS_SOLID)
				visit_leaf ((mleaf_t *) front);
			visit_node (node, side);
			node = node->children[!side];
		}
		if (node->contents < 0 && node->contents != CONTENTS_SOLID)
			visit_leaf ((mleaf_t *) node);
		if (node_ptr != node_stack) {
			node_ptr--;
			node = node_ptr->node;
			side = node_ptr->side;
			visit_node (node, side);
			node = node->children[!side];
			continue;
		}
		break;
	}
	if (node->contents < 0 && node->contents != CONTENTS_SOLID)
		visit_leaf ((mleaf_t *) node);
}

void
R_DrawWorld (void)
{
	entity_t    worldent;

	memset (&worldent, 0, sizeof (worldent));
	worldent.model = r_worldentity.model;

	currententity = &worldent;

	R_VisitWorldNodes (worldent.model->nodes);

	clear_texture_chains ();
}
