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

#include "QF/dstring.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/vrect.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_textures.h"

#include "r_local.h"

typedef struct {
	GLushort    count;
	GLushort    indices[1];
} glslpoly_t;

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
static elements_t  *static_elementss;
static elements_t **static_elementss_tail = &static_elementss;
static elements_t  *free_static_elementss;
static instsurf_t  *static_instsurfs;
static instsurf_t **static_instsurfs_tail = &static_instsurfs;
static instsurf_t  *free_static_instsurfs;

// for instance models
static elechain_t  *elechains;
static elechain_t **elechains_tail = &elechains;
static elechain_t  *free_elechains;
static elements_t  *elementss;
static elements_t **elementss_tail = &elementss;
static elements_t  *free_elementss;
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
GET_RELEASE (elements_t, static_elements)
GET_RELEASE (elements_t, elements)
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
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
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

	for (i = 0; i < model->nummodelsurfaces; i++) {
		model->surfaces[i].instsurf = get_static_instsurf ();
		model->surfaces[i].instsurf->surface = &model->surfaces[i];
	}
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
	release_elechains ();
	release_elementss ();
	release_instsurfs ();
}

void
R_ClearElements (void)
{
	release_static_elechains ();
	release_static_elementss ();
	release_elechains ();
	release_elementss ();
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

static elechain_t *
add_elechain (texture_t *tex, int ec_index)
{
	elechain_t *ec;

	if (ec_index < 0) {
		ec = get_elechain ();
		ec->elements = get_elements ();
	} else {
		ec = get_static_elechain ();
		ec->elements = get_static_elements ();
	}
	ec->index = ec_index;
	ec->transform = 0;
	*tex->elechain_tail = ec;
	tex->elechain_tail = &ec->next;
	return ec;
}

static void
build_surf_displist (model_t **models, msurface_t *fa, int base,
					 dstring_t *vert_list)
{
	int         numverts;
	int         numtris;
	int         numindices;
	int         i;
	vec_t      *vec;
	mvertex_t  *vertices;
	medge_t    *edges;
	int        *surfedges;
	int         index;
	bspvert_t  *verts;
	glslpoly_t *poly;
	GLushort   *ind;
	float       s, t;

	if (fa->ec_index < 0) {
		vertices = models[-fa->ec_index - 1]->vertexes;
		edges = models[-fa->ec_index - 1]->edges;
		surfedges = models[-fa->ec_index - 1]->surfedges;
	} else {
		vertices = r_worldentity.model->vertexes;
		edges = r_worldentity.model->edges;
		surfedges = r_worldentity.model->surfedges;
	}
	numverts = fa->numedges;
	numtris = numverts - 2;
	numindices = numtris * 3;
	verts = alloca (numverts * sizeof (bspvert_t));
	//FIXME leak
	poly = malloc (field_offset (glslpoly_t, indices[numindices]));
	poly->count = numindices;
	for (i = 0, ind = poly->indices; i < numtris; i++) {
		*ind++ = base;
		*ind++ = base + i + 1;
		*ind++ = base + i + 2;
	}
	fa->polys = (glpoly_t *) poly;

	for (i = 0; i < numverts; i++) {
		index = surfedges[fa->firstedge + i];
		if (index > 0)
			vec = vertices[edges[index].v[0]].position;
		else
			vec = vertices[edges[-index].v[1]].position;

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		VectorCopy (vec, verts[i].vertex);
		verts[i].vertex[3] = 1;
		verts[i].tlst[0] = s / fa->texinfo->texture->width;
		verts[i].tlst[1] = t / fa->texinfo->texture->height;

		//lightmap texture coordinates
		if (!fa->lightpic) {
			// sky and water textures don't have lightmaps
			verts[i].tlst[2] = 0;
			verts[i].tlst[3] = 0;
			continue;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		s -= fa->texturemins[0];
		t -= fa->texturemins[1];
		s += fa->lightpic->rect->x * 16 + 8;
		t += fa->lightpic->rect->y * 16 + 8;
		s /= 16;
		t /= 16;
		verts[i].tlst[2] = s * fa->lightpic->size;
		verts[i].tlst[3] = t * fa->lightpic->size;
	}
	dstring_append (vert_list, (char *) verts, numverts * sizeof (bspvert_t));
}

void
R_BuildDisplayLists (model_t **models, int num_models)
{
	int         i, j;
	int         vertex_index_base;
	model_t    *m;
	dmodel_t   *dm;
	msurface_t *surf;
	dstring_t  *vertices;

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
		m->numsubmodels = 1; // no support for submodels in non-world model
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
		dm = m->submodels;
		for (j = 0; j < m->numsurfaces; j++) {
			texture_t  *tex;
			if (j == dm->firstface + dm->numfaces) {
				dm++;
				if (dm - m->submodels == m->numsubmodels) {
					// limit the surfaces
					// probably never hit
					Sys_Printf ("R_BuildDisplayLists: too many surfaces\n");
					m->numsurfaces = j;
					break;
				}
			}
			surf = m->surfaces + j;
			surf->ec_index = dm - m->submodels;
			if (!surf->ec_index && m != r_worldentity.model)
				surf->ec_index = -1 - i;	// instanced model
			tex = surf->texinfo->texture;
			CHAIN_SURF_F2B (surf, tex->tex_chain);
			if (surf->instsurf)
				surf->instsurf->elements = 0;
			else
				surf->tinst->elements = 0;
		}
	}
	// All vertices from all brush models go into one giant vbo.
	vertices = dstring_new ();
	vertex_index_base = 0;
	// All usable surfaces have been chained to the (base) texture they use.
	// Run through the textures, using their chains to build display maps.
	// For animated textures, if a surface is on one texture of the group, it
	// will be on all.
	for (i = 0; i < r_num_texture_chains; i++) {
		texture_t  *tex;
		instsurf_t *is;
		elechain_t *ec = 0;
		elements_t *el = 0;

		tex = r_texture_chains[i];

		for (is = tex->tex_chain; is; is = is->tex_chain) {
			msurface_t *surf = is->surface;
			if (!tex->elechain) {
				ec = add_elechain (tex, surf->ec_index);
				el = ec->elements;
				el->base = (byte *) vertices->size;
				vertex_index_base = 0;
			}
			if (surf->ec_index != ec->index) {	// next sub-model
				ec = add_elechain (tex, surf->ec_index);
				el = ec->elements;
				el->base = (byte *) vertices->size;
				vertex_index_base = 0;
			}
			if (vertex_index_base + surf->numedges > 65535) {
				// elements index overflow
				if (surf->ec_index < 0)
					el->next = get_elements ();
				else
					el->next = get_static_elements ();
				el = el->next;
				el->base = (byte *) vertices->size;
				vertex_index_base = 0;
			}
			// we don't use it now, but pre-initializing the list won't hurt
			if (!el->list)
				el->list = dstring_new ();
			dstring_clear (el->list);

			is->elements = el;
			build_surf_displist (models, surf, vertex_index_base, vertices);
			vertex_index_base += surf->numedges;
		}
	}
	clear_texture_chains ();
	Sys_Printf ("%ld verts total\n", vertices->size / sizeof (bspvert_t));
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
