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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vrect.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

typedef struct {
	GLushort    count;
	GLushort    indices[1];
} glslpoly_t;

#define ALLOC_CHUNK 64

static instsurf_t  *waterchain = NULL;
static instsurf_t **waterchain_tail = &waterchain;
static instsurf_t  *sky_chain;
static instsurf_t **sky_chain_tail = &sky_chain;

static texture_t  **r_texture_chains;
static int          r_num_texture_chains;
static int          max_texture_chains;

// for world and non-instance models
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

static GLuint   bsp_vbo;
static mat4_t   bsp_vp;

static GLuint   skybox_tex;
static qboolean skybox_loaded;
static quat_t   sky_rotation[2];
static quat_t   sky_velocity;
static quat_t   sky_fix;
static double   sky_time;

static quat_t   default_color = { 1, 1, 1, 1 };
static quat_t   last_color;

static const char *bsp_vert_effects[] =
{
	"QuakeForge.Vertex.bsp",
	0
};

static const char *bsp_lit_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.env.warp.nop",
	"QuakeForge.Fragment.colormap",
	"QuakeForge.Fragment.bsp.lit",
	0
};

static const char *bsp_turb_effects[] =
{
	"QuakeForge.Math.const",
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.palette",
	"QuakeForge.env.warp.turb",
	"QuakeForge.Fragment.bsp.unlit",
	0
};

static const char *bsp_sky_cube_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.env.sky.cube",
	"QuakeForge.Fragment.bsp.sky",
	0
};

static const char *bsp_sky_id_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.palette",
	"QuakeForge.env.sky.id",
	"QuakeForge.Fragment.bsp.sky",
	0
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t tlst;
	shaderparam_t vertex;
	shaderparam_t colormap;
	shaderparam_t texture;
	shaderparam_t lightmap;
	shaderparam_t color;
	shaderparam_t fog;
} quake_bsp = {
	0,
	{"mvp_mat", 1},
	{"tlst", 0},
	{"vertex", 0},
	{"colormap", 1},
	{"texture", 1},
	{"lightmap", 1},
	{"vcolor", 0},
	{"fog", 1},
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t tlst;
	shaderparam_t vertex;
	shaderparam_t palette;
	shaderparam_t texture;
	shaderparam_t time;
	shaderparam_t color;
	shaderparam_t fog;
} quake_turb = {
	0,
	{"mvp_mat", 1},
	{"tlst", 0},
	{"vertex", 0},
	{"palette", 1},
	{"texture", 1},
	{"time", 1},
	{"vcolor", 0},
	{"fog", 1},
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t sky_matrix;
	shaderparam_t vertex;
	shaderparam_t palette;
	shaderparam_t solid;
	shaderparam_t trans;
	shaderparam_t time;
	shaderparam_t fog;
} quake_skyid = {
	0,
	{"mvp_mat", 1},
	{"sky_mat", 1},
	{"vertex", 0},
	{"palette", 1},
	{"solid", 1},
	{"trans", 1},
	{"time", 1},
	{"fog", 1},
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t sky_matrix;
	shaderparam_t vertex;
	shaderparam_t sky;
	shaderparam_t fog;
} quake_skybox = {
	0,
	{"mvp_mat", 1},
	{"sky_mat", 1},
	{"vertex", 0},
	{"sky", 1},
	{"fog", 1},
};

static struct {
	shaderparam_t *mvp_matrix;
	shaderparam_t *sky_matrix;
	shaderparam_t *vertex;
	shaderparam_t *fog;
} sky_params;

#define CHAIN_SURF_F2B(surf,chain)							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			(surf)->tinst = inst = get_instsurf ();			\
		inst->surface = (surf);								\
		*(chain##_tail) = inst;								\
		(chain##_tail) = &inst->tex_chain;					\
		*(chain##_tail) = 0;								\
	} while (0)

#define CHAIN_SURF_B2F(surf,chain) 							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			(surf)->tinst = inst = get_instsurf ();			\
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

GET_RELEASE (elechain_t, elechain)
GET_RELEASE (elements_t, elements)
GET_RELEASE (instsurf_t, static_instsurf)
GET_RELEASE (instsurf_t, instsurf)

void
glsl_R_AddTexture (texture_t *tex)
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
glsl_R_InitSurfaceChains (model_t *model)
{
	int         i;

	release_static_instsurfs ();
	release_instsurfs ();

	for (i = 0; i < model->nummodelsurfaces; i++) {
		model->surfaces[i].instsurf = get_static_instsurf ();
		model->surfaces[i].instsurf->surface = &model->surfaces[i];
	}
}

static inline void
clear_tex_chain (texture_t *tex)
{
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
}

static void
clear_texture_chains (void)
{
	int			i;

	for (i = 0; i < r_num_texture_chains; i++) {
		if (!r_texture_chains[i])
			continue;
		clear_tex_chain (r_texture_chains[i]);
	}
	clear_tex_chain (r_notexture_mip);
	release_elechains ();
	release_elementss ();
	release_instsurfs ();
}

void
glsl_R_ClearElements (void)
{
	release_elechains ();
	release_elementss ();
}

static void
update_lightmap (msurface_t *surf)
{
	int         maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		if (d_lightstylevalue[surf->styles[maps]] != surf->cached_light[maps])
			goto dynamic;

	if ((surf->dlightframe == r_framecount) || surf->cached_dlight) {
dynamic:
		if (r_dynamic->int_val)
			glsl_R_BuildLightMap (surf);
	}
}

static inline void
chain_surface (msurface_t *surf, vec_t *transform, float *color)
{
	instsurf_t *is;

	if (surf->flags & SURF_DRAWSKY) {
		CHAIN_SURF_F2B (surf, sky_chain);
	} else if ((surf->flags & SURF_DRAWTURB) || (color && color[3] < 1.0)) {
		CHAIN_SURF_B2F (surf, waterchain);
	} else {
		texture_t  *tex;

		if (!surf->texinfo->texture->anim_total)
			tex = surf->texinfo->texture;
		else
			tex = R_TextureAnimation (surf);
		CHAIN_SURF_F2B (surf, tex->tex_chain);

		update_lightmap (surf);
	}
	if (!(is = surf->instsurf))
		is = surf->tinst;
	is->transform = transform;
	is->color = color;
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
		glsl_R_AddTexture (tex);
	}
}

void
glsl_R_ClearTextures (void)
{
	r_num_texture_chains = 0;
}

void
glsl_R_RegisterTextures (model_t **models, int num_models)
{
	int         i;
	model_t    *m;

	glsl_R_ClearTextures ();
	glsl_R_InitSurfaceChains (r_worldentity.model);
	glsl_R_AddTexture (r_notexture_mip);
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
}

static elechain_t *
add_elechain (texture_t *tex, int ec_index)
{
	elechain_t *ec;

	ec = get_elechain ();
	ec->elements = get_elements ();
	ec->index = ec_index;
	ec->transform = 0;
	ec->color = 0;
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
glsl_R_BuildDisplayLists (model_t **models, int num_models)
{
	int         i, j;
	int         vertex_index_base;
	model_t    *m;
	dmodel_t   *dm;
	msurface_t *surf;
	dstring_t  *vertices;

	QuatSet (0, 0, sqrt(0.5), sqrt(0.5), sky_fix);	// proper skies
	QuatSet (0, 0, 0, 1, sky_rotation[0]);
	QuatCopy (sky_rotation[0], sky_rotation[1]);
	QuatSet (0, 0, 0, 0, sky_velocity);
	QuatExp (sky_velocity, sky_velocity);
	sky_time = vr_data.realtime;

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
				el->base = (byte *) (intptr_t) vertices->size;
				vertex_index_base = 0;
			}
			if (surf->ec_index != ec->index) {	// next sub-model
				ec = add_elechain (tex, surf->ec_index);
				el = ec->elements;
				el->base = (byte *) (intptr_t) vertices->size;
				vertex_index_base = 0;
			}
			if (vertex_index_base + surf->numedges > 65535) {
				// elements index overflow
				el->next = get_elements ();
				el = el->next;
				el->base = (byte *) (intptr_t) vertices->size;
				vertex_index_base = 0;
			}
			// we don't use it now, but pre-initializing the list won't hurt
			if (!el->list)
				el->list = dstring_new ();
			dstring_clear (el->list);

			surf->base = el->base;
			build_surf_displist (models, surf, vertex_index_base, vertices);
			vertex_index_base += surf->numedges;
		}
	}
	clear_texture_chains ();
	Sys_MaskPrintf (SYS_GLSL, "R_BuildDisplayLists: %ld verts total\n",
					(long) (vertices->size / sizeof (bspvert_t)));
	if (!bsp_vbo)
		qfeglGenBuffers (1, &bsp_vbo);
	qfeglBindBuffer (GL_ARRAY_BUFFER, bsp_vbo);
	qfeglBufferData (GL_ARRAY_BUFFER, vertices->size, vertices->str,
					GL_STATIC_DRAW);
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	dstring_delete (vertices);
}

static void
R_DrawBrushModel (entity_t *e)
{
	float       dot, radius;
	int         i;
	unsigned    k;
	model_t    *model;
	plane_t    *plane;
	msurface_t *surf;
	qboolean    rotated;
	vec3_t      mins, maxs, org;

	model = e->model;
	if (e->transform[0] != 1 || e->transform[5] != 1 || e->transform[10] != 1) {
		rotated = true;
		radius = model->radius;
		if (R_CullSphere (e->origin, radius))
			return;
	} else {
		rotated = false;
		VectorAdd (e->origin, model->mins, mins);
		VectorAdd (e->origin, model->maxs, maxs);
		if (R_CullBox (mins, maxs))
			return;
	}

	VectorSubtract (r_refdef.vieworg, e->origin, org);
	if (rotated) {
		vec3_t      temp;

		VectorCopy (org, temp);
		org[0] = DotProduct (temp, e->transform + 0);
		org[1] = DotProduct (temp, e->transform + 4);
		org[2] = DotProduct (temp, e->transform + 8);
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (model->firstmodelsurface != 0 && r_dlight_lightmap->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < r_maxdlights; k++) {
			if ((r_dlights[k].die < vr_data.realtime)
				|| (!r_dlights[k].radius))
				continue;

			VectorSubtract (r_dlights[k].origin, e->origin, lightorigin);
			R_RecursiveMarkLights (lightorigin, &r_dlights[k], k,
							model->nodes + model->hulls[0].firstclipnode);
		}
	}

	surf = &model->surfaces[model->firstmodelsurface];

	for (i = 0; i < model->nummodelsurfaces; i++, surf++) {
		// find the node side on which we are
		plane = surf->plane;

		dot = PlaneDiff (org, plane);

		// enqueue the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
			|| (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			chain_surface (surf, e->transform, e->colormod);
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
	// find the node side on which we are
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

			chain_surface (surf, 0, 0);
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
R_VisitWorldNodes (model_t *model)
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

	node = model->nodes;
	// +2 for paranoia
	node_stack = alloca ((model->depth + 2) * sizeof (rstack_t));
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

static void
draw_elechain (elechain_t *ec, int matloc, int vertloc, int tlstloc,
			   int colloc)
{
	mat4_t      mat;
	elements_t *el;
	int         count;
	float      *color;

	if (colloc >= 0) {
		color = ec->color;
		if (!color)
			color = default_color;
		if (!QuatCompare (color, last_color)) {
			QuatCopy (color, last_color);
			qfeglVertexAttrib4fv (quake_bsp.color.location, color);
		}
	}
	if (ec->transform) {
		Mat4Mult (bsp_vp, ec->transform, mat);
		qfeglUniformMatrix4fv (matloc, 1, false, mat);
	} else {
		qfeglUniformMatrix4fv (matloc, 1, false, bsp_vp);
	}
	for (el = ec->elements; el; el = el->next) {
		if (!el->list->size)
			continue;
		count = el->list->size / sizeof (GLushort);
		qfeglVertexAttribPointer (vertloc, 4, GL_FLOAT,
								 0, sizeof (bspvert_t),
								 el->base + field_offset (bspvert_t, vertex));
		if (tlstloc >= 0)
			qfeglVertexAttribPointer (tlstloc, 4, GL_FLOAT,
									 0, sizeof (bspvert_t),
									 el->base + field_offset (bspvert_t,tlst));
		qfeglDrawElements (GL_TRIANGLES, count,
						  GL_UNSIGNED_SHORT, el->list->str);
		dstring_clear (el->list);
	}
}

static void
bsp_begin (void)
{
	quat_t      fog;

	default_color[3] = 1;
	QuatCopy (default_color, last_color);
	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	Mat4Mult (glsl_projection, glsl_view, bsp_vp);

	qfeglUseProgram (quake_bsp.program);
	qfeglEnableVertexAttribArray (quake_bsp.vertex.location);
	qfeglEnableVertexAttribArray (quake_bsp.tlst.location);
	qfeglDisableVertexAttribArray (quake_bsp.color.location);

	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_bsp.fog.location, 1, fog);

	qfeglUniform1i (quake_bsp.colormap.location, 2);
	qfeglActiveTexture (GL_TEXTURE0 + 2);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_colormap);

	qfeglUniform1i (quake_bsp.lightmap.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_R_LightmapTexture ());

	qfeglUniform1i (quake_bsp.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, bsp_vbo);
}

static void
bsp_end (void)
{
	qfeglDisableVertexAttribArray (quake_bsp.vertex.location);
	qfeglDisableVertexAttribArray (quake_bsp.tlst.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 2);
	qfeglDisable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
turb_begin (void)
{
	quat_t      fog;

	default_color[3] = bound (0, r_wateralpha->value, 1);
	QuatCopy (default_color, last_color);
	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	Mat4Mult (glsl_projection, glsl_view, bsp_vp);

	qfeglUseProgram (quake_turb.program);
	qfeglEnableVertexAttribArray (quake_turb.vertex.location);
	qfeglEnableVertexAttribArray (quake_turb.tlst.location);
	qfeglDisableVertexAttribArray (quake_turb.color.location);

	qfeglVertexAttrib4fv (quake_turb.color.location, default_color);

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_turb.fog.location, 1, fog);

	qfeglUniform1i (quake_turb.palette.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	qfeglUniform1f (quake_turb.time.location, vr_data.realtime);

	qfeglUniform1i (quake_turb.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, bsp_vbo);
}

static void
turb_end (void)
{
	qfeglDisableVertexAttribArray (quake_turb.vertex.location);
	qfeglDisableVertexAttribArray (quake_turb.tlst.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
spin (mat4_t mat)
{
	quat_t      q;
	mat4_t      m;
	float       blend;

	while (vr_data.realtime - sky_time > 1) {
		QuatCopy (sky_rotation[1], sky_rotation[0]);
		QuatMult (sky_velocity, sky_rotation[0], sky_rotation[1]);
		sky_time += 1;
	}
	blend = bound (0, (vr_data.realtime - sky_time), 1);

	QuatBlend (sky_rotation[0], sky_rotation[1], blend, q);
	QuatMult (sky_fix, q, q);
	Mat4Identity (mat);
	VectorNegate (r_origin, mat + 12);
	QuatToMatrix (q, m, 1, 1);
	Mat4Mult (m, mat, mat);
}

static void
sky_begin (void)
{
	mat4_t      mat;
	quat_t      fog;

	default_color[3] = 1;
	QuatCopy (default_color, last_color);
	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	Mat4Mult (glsl_projection, glsl_view, bsp_vp);

	if (skybox_loaded) {
		sky_params.mvp_matrix = &quake_skybox.mvp_matrix;
		sky_params.vertex = &quake_skybox.vertex;
		sky_params.sky_matrix = &quake_skybox.sky_matrix;
		sky_params.fog = &quake_skybox.fog;

		qfeglUseProgram (quake_skybox.program);
		qfeglEnableVertexAttribArray (quake_skybox.vertex.location);

		qfeglUniform1i (quake_skybox.sky.location, 0);
		qfeglActiveTexture (GL_TEXTURE0 + 0);
		qfeglEnable (GL_TEXTURE_CUBE_MAP);
		qfeglBindTexture (GL_TEXTURE_CUBE_MAP, skybox_tex);
	} else {
		sky_params.mvp_matrix = &quake_skyid.mvp_matrix;
		sky_params.sky_matrix = &quake_skyid.sky_matrix;
		sky_params.vertex = &quake_skyid.vertex;
		sky_params.fog = &quake_skyid.fog;

		qfeglUseProgram (quake_skyid.program);
		qfeglEnableVertexAttribArray (quake_skyid.vertex.location);

		qfeglUniform1i (quake_skyid.palette.location, 2);
		qfeglActiveTexture (GL_TEXTURE0 + 2);
		qfeglEnable (GL_TEXTURE_2D);
		qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

		qfeglUniform1f (quake_skyid.time.location, vr_data.realtime);

		qfeglUniform1i (quake_skyid.trans.location, 0);
		qfeglActiveTexture (GL_TEXTURE0 + 0);
		qfeglEnable (GL_TEXTURE_2D);

		qfeglUniform1i (quake_skyid.solid.location, 1);
		qfeglActiveTexture (GL_TEXTURE0 + 1);
		qfeglEnable (GL_TEXTURE_2D);
	}

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (sky_params.fog->location, 1, fog);

	spin (mat);
	qfeglUniformMatrix4fv (sky_params.sky_matrix->location, 1, false, mat);

	qfeglBindBuffer (GL_ARRAY_BUFFER, bsp_vbo);
}

static void
sky_end (void)
{
	qfeglDisableVertexAttribArray (sky_params.vertex->location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglDisable (GL_TEXTURE_CUBE_MAP);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 2);
	qfeglDisable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
}

static inline void
add_surf_elements (texture_t *tex, instsurf_t *is,
				   elechain_t **ec, elements_t **el)
{
	msurface_t *surf = is->surface;
	glslpoly_t *poly = (glslpoly_t *) surf->polys;

	if (!tex->elechain) {
		(*ec) = add_elechain (tex, surf->ec_index);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*el) = (*ec)->elements;
		(*el)->base = surf->base;
		if (!(*el)->list)
			(*el)->list = dstring_new ();
		dstring_clear ((*el)->list);
	}
	if (is->transform != (*ec)->transform || is->color != (*ec)->color) {
		(*ec) = add_elechain (tex, surf->ec_index);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*el) = (*ec)->elements;
		(*el)->base = surf->base;
		if (!(*el)->list)
			(*el)->list = dstring_new ();
		dstring_clear ((*el)->list);
	}
	if (surf->base != (*el)->base) {
		(*el)->next = get_elements ();
		(*el) = (*el)->next;
		(*el)->base = surf->base;
		if (!(*el)->list)
			(*el)->list = dstring_new ();
		dstring_clear ((*el)->list);
	}
	dstring_append ((*el)->list, (char *) poly->indices,
					poly->count * sizeof (poly->indices[0]));
}

static void
build_tex_elechain (texture_t *tex)
{
	instsurf_t *is;
	elechain_t *ec = 0;
	elements_t *el = 0;

	for (is = tex->tex_chain; is; is = is->tex_chain) {
		add_surf_elements (tex, is, &ec, &el);
	}
}

void
glsl_R_DrawWorld (void)
{
	entity_t    worldent;
	int         i;

	clear_texture_chains ();	// do this first for water and skys

	memset (&worldent, 0, sizeof (worldent));
	worldent.model = r_worldentity.model;

	currententity = &worldent;

	R_VisitWorldNodes (worldent.model);
	if (r_drawentities->int_val) {
		entity_t   *ent;
		for (ent = r_ent_queue; ent; ent = ent->next) {
			if (ent->model->type != mod_brush)
				continue;
			currententity = ent;

			R_DrawBrushModel (ent);
		}
	}

	glsl_R_FlushLightmaps ();
	bsp_begin ();
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	for (i = 0; i < r_num_texture_chains; i++) {
		texture_t  *tex;
		elechain_t *ec = 0;

		tex = r_texture_chains[i];

		build_tex_elechain (tex);

		if (tex->elechain)
			qfeglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);

		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, quake_bsp.mvp_matrix.location,
						   quake_bsp.vertex.location,
						   quake_bsp.tlst.location,
						   quake_bsp.color.location);
		}
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	bsp_end ();
}

void
glsl_R_DrawWaterSurfaces ()
{
	instsurf_t *is;
	msurface_t *surf;
	texture_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!waterchain)
		return;

	turb_begin ();
	for (is = waterchain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture) {
			if (tex) {
				qfeglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
				for (ec = tex->elechain; ec; ec = ec->next)
					draw_elechain (ec, quake_turb.mvp_matrix.location,
								   quake_turb.vertex.location,
								   quake_turb.tlst.location,
								   quake_turb.color.location);
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture;
		}
		add_surf_elements (tex, is, &ec, &el);
	}
	if (tex) {
		qfeglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
		for (ec = tex->elechain; ec; ec = ec->next)
			draw_elechain (ec, quake_turb.mvp_matrix.location,
						   quake_turb.vertex.location,
						   quake_turb.tlst.location,
						   quake_turb.color.location);
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	turb_end ();

	waterchain = 0;
	waterchain_tail = &waterchain;
}

void
glsl_R_DrawSky (void)
{
	instsurf_t *is;
	msurface_t *surf;
	texture_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!sky_chain)
		return;

	sky_begin ();
	for (is = sky_chain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture) {
			if (tex) {
				if (!skybox_loaded) {
					qfeglActiveTexture (GL_TEXTURE0 + 0);
					qfeglBindTexture (GL_TEXTURE_2D, tex->sky_tex[0]);
					qfeglActiveTexture (GL_TEXTURE0 + 1);
					qfeglBindTexture (GL_TEXTURE_2D, tex->sky_tex[1]);
				}
				for (ec = tex->elechain; ec; ec = ec->next)
					draw_elechain (ec, sky_params.mvp_matrix->location,
								   sky_params.vertex->location, -1, -1);
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture;
		}
		add_surf_elements (tex, is, &ec, &el);
	}
	if (tex) {
		if (!skybox_loaded) {
			qfeglActiveTexture (GL_TEXTURE0 + 0);
			qfeglBindTexture (GL_TEXTURE_2D, tex->sky_tex[0]);
			qfeglActiveTexture (GL_TEXTURE0 + 1);
			qfeglBindTexture (GL_TEXTURE_2D, tex->sky_tex[1]);
		}
		for (ec = tex->elechain; ec; ec = ec->next)
			draw_elechain (ec, sky_params.mvp_matrix->location,
						   sky_params.vertex->location, -1, -1);
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	sky_end ();

	sky_chain = 0;
	sky_chain_tail = &sky_chain;
}

void
glsl_R_InitBsp (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;

	vert_shader = GLSL_BuildShader (bsp_vert_effects);
	frag_shader = GLSL_BuildShader (bsp_lit_effects);
	vert = GLSL_CompileShader ("quakebsp.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakebsp.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_bsp.program = GLSL_LinkProgram ("quakebsp", vert, frag);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.mvp_matrix);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.tlst);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.vertex);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.colormap);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.texture);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.lightmap);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.color);
	GLSL_ResolveShaderParam (quake_bsp.program, &quake_bsp.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	frag_shader = GLSL_BuildShader (bsp_turb_effects);
	frag = GLSL_CompileShader ("quaketrb.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_turb.program = GLSL_LinkProgram ("quaketrb", vert, frag);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.mvp_matrix);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.tlst);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.vertex);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.palette);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.texture);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.time);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.color);
	GLSL_ResolveShaderParam (quake_turb.program, &quake_turb.fog);
	GLSL_FreeShader (frag_shader);

	frag_shader = GLSL_BuildShader (bsp_sky_id_effects);
	frag = GLSL_CompileShader ("quakeski.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_skyid.program = GLSL_LinkProgram ("quakeskyid", vert, frag);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.mvp_matrix);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.sky_matrix);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.vertex);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.palette);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.solid);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.trans);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.time);
	GLSL_ResolveShaderParam (quake_skyid.program, &quake_skyid.fog);
	GLSL_FreeShader (frag_shader);

	frag_shader = GLSL_BuildShader (bsp_sky_cube_effects);
	frag = GLSL_CompileShader ("quakeskb.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_skybox.program = GLSL_LinkProgram ("quakeskybox", vert, frag);
	GLSL_ResolveShaderParam (quake_skybox.program, &quake_skybox.mvp_matrix);
	GLSL_ResolveShaderParam (quake_skybox.program, &quake_skybox.sky_matrix);
	GLSL_ResolveShaderParam (quake_skybox.program, &quake_skybox.vertex);
	GLSL_ResolveShaderParam (quake_skybox.program, &quake_skybox.sky);
	GLSL_ResolveShaderParam (quake_skybox.program, &quake_skybox.fog);
	GLSL_FreeShader (frag_shader);
}

static inline __attribute__((const)) int
is_pow2 (unsigned x)
{
	int         count;

	for (count = 0; x; x >>= 1)
		if (x & 1)
			count++;
	return count == 1;
}

// NOTE: this expects the destination tex_t to be set up: memory allocated
// and dimentions/format etc already set. the size of the rect to be copied
// is taken from dst. Also, dst->format and src->format must be the same, and
// either 3 or 4, or bad things will happen. Also, no clipping is done, so if
// x < 0 or y < 0 or x + dst->width > src->width
// or y + dst->height > src->height, bad things will happen.
static void
copy_sub_tex (tex_t *src, int x, int y, tex_t *dst)
{
	int         dstbytes;
	int         srcbytes;
	int         i;

	srcbytes = src->width * src->format;
	dstbytes = dst->width * dst->format;

	x *= src->format;
	for (i = 0; i < dst->height; i++)
		memcpy (dst->data + i * dstbytes, src->data + (i + y) * srcbytes + x,
				dstbytes);
}

void
glsl_R_LoadSkys (const char *sky)
{
	const char *name;
	int         i;
	tex_t      *tex;
	// NOTE: quake's world and GL's world are rotated relative to each other
	// quake has x right, y in, z up. gl has x right, y up, z out
	// quake order:                      +x    -x    +z    -z    +y    -y
	// gl order:                         +x    -x    +y    -y    +z    -z
	// fizquake orger:                   -y    +y    +z    -z    +x    -x
	// to get from quake order to fitzquake order, all that's needed is
	// a -90 degree rotation on the (quake) z-axis. This is taken care of in
	// the sky_matrix setup code.
	// However, from the player's perspective, skymaps have lf and rt
	// swapped, but everythink makes sense if looking at the cube from outside
	// along the positive y axis, with the front of the cube being the nearest
	// face. This matches nicely with Blender's default cube in front (num-1)
	// view.
	static const char *sky_suffix[] = { "ft", "bk", "up", "dn", "rt", "lf"};
	static int  sky_coords[][2] = {
		{2, 0},	// front
		{0, 0}, // back
		{1, 1}, // up
		{0, 1}, // down
		{2, 1}, // left
		{1, 0}, // right
	};

	if (!sky || !*sky)
		sky = r_skyname->string;

	if (!*sky || !strcasecmp (sky, "none")) {
		skybox_loaded = false;
		return;
	}

	if (!skybox_tex)
		qfeglGenTextures (1, &skybox_tex);

	qfeglBindTexture (GL_TEXTURE_CUBE_MAP, skybox_tex);

	//blender envmap
	// bk rt ft
	// dn up lt
	tex = LoadImage (name = va ("env/%s_map", sky));
	if (tex && tex->format >= 3 && tex->height * 3 == tex->width * 2
		&& is_pow2 (tex->height)) {
		tex_t      *sub;
		int         size = tex->height / 2;

		skybox_loaded = true;
		sub = malloc (field_offset (tex_t, data[size * size * tex->format]));
		sub->width = size;
		sub->height = size;
		sub->format = tex->format;
		sub->palette = tex->palette;
		for (i = 0; i < 6; i++) {
			int         x, y;
			x = sky_coords[i][0] * size;
			y = sky_coords[i][1] * size;
			copy_sub_tex (tex, x, y, sub);
			qfeglTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
							sub->format == 3 ? GL_RGB : GL_RGBA,
							sub->width, sub->height, 0,
							sub->format == 3 ? GL_RGB : GL_RGBA,
							GL_UNSIGNED_BYTE, sub->data);
		}
		free (sub);
	} else {
		skybox_loaded = true;
		for (i = 0; i < 6; i++) {
			tex = LoadImage (name = va ("env/%s%s", sky, sky_suffix[i]));
			if (!tex || tex->format < 3) {	// FIXME pcx support
				Sys_MaskPrintf (SYS_GLSL, "Couldn't load %s\n", name);
				// also look in gfx/env, where Darkplaces looks for skies
				tex = LoadImage (name = va ("gfx/env/%s%s", sky,
											sky_suffix[i]));
				if (!tex || tex->format < 3) {  // FIXME pcx support
					Sys_MaskPrintf (SYS_GLSL, "Couldn't load %s\n", name);
					skybox_loaded = false;
					continue;
				}
			}
			Sys_MaskPrintf (SYS_GLSL, "Loaded %s\n", name);
			qfeglTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
							tex->format == 3 ? GL_RGB : GL_RGBA,
							tex->width, tex->height, 0,
							tex->format == 3 ? GL_RGB : GL_RGBA,
							GL_UNSIGNED_BYTE, tex->data);
		}
	}
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
					   GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
					   GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfeglGenerateMipmap (GL_TEXTURE_CUBE_MAP);
}
