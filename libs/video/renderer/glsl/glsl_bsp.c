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

#include "QF/scene/entity.h"

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

static glsltex_t  **r_texture_chains;
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
static mat4f_t  bsp_vp;

static GLuint   skybox_tex;
static bool skybox_loaded;
static quat_t   sky_rotation[2];
static quat_t   sky_velocity;
static quat_t   sky_fix;
static double   sky_time;

static quat_t   default_color = { 1, 1, 1, 1 };
static quat_t   last_color;
static glsltex_t glsl_notexture = { };

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

typedef struct glslbspctx_s {
	mod_brush_t *brush;
	animation_t *animation;
	vec4f_t    *transform;
	float      *color;
} glslbspctx_t;


#define CHAIN_SURF_F2B(surf,chain)							\
	({	 													\
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

static void
glsl_R_AddTexture (texture_t *tx)
{
	int         i;
	if (r_num_texture_chains == max_texture_chains) {
		max_texture_chains += 64;
		r_texture_chains = realloc (r_texture_chains,
								  max_texture_chains * sizeof (glsltex_t *));
		for (i = r_num_texture_chains; i < max_texture_chains; i++)
			r_texture_chains[i] = 0;
	}
	glsltex_t  *tex = tx->render;
	r_texture_chains[r_num_texture_chains++] = tex;
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
}

static void
glsl_R_InitSurfaceChains (mod_brush_t *brush)
{
	release_static_instsurfs ();
	release_instsurfs ();

	for (unsigned i = 0; i < brush->nummodelsurfaces; i++) {
		brush->surfaces[i].instsurf = get_static_instsurf ();
		brush->surfaces[i].instsurf->surface = &brush->surfaces[i];
	}
}

static inline void
clear_tex_chain (glsltex_t *tex)
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
	clear_tex_chain (r_notexture_mip->render);
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
update_lightmap (glslbspctx_t *bctx, msurface_t *surf)
{
	int         maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		if (d_lightstylevalue[surf->styles[maps]] != surf->cached_light[maps])
			goto dynamic;

	if ((surf->dlightframe == r_framecount) || surf->cached_dlight) {
dynamic:
		if (r_dynamic)
			glsl_R_BuildLightMap (bctx->transform, bctx->brush, surf);
	}
}

static inline void
chain_surface (glslbspctx_t *bctx, msurface_t *surf)
{
	instsurf_t *is;

	if (surf->flags & SURF_DRAWSKY) {
		is = CHAIN_SURF_F2B (surf, sky_chain);
	} else if ((surf->flags & SURF_DRAWTURB)
			   || (bctx->color && bctx->color[3] < 1.0)) {
		is = CHAIN_SURF_B2F (surf, waterchain);
	} else {
		texture_t  *tx;
		glsltex_t  *tex;

		if (!surf->texinfo->texture->anim_total)
			tx = surf->texinfo->texture;
		else
			tx = R_TextureAnimation (bctx->animation->frame, surf);
		tex = tx->render;
		is = CHAIN_SURF_F2B (surf, tex->tex_chain);

		update_lightmap (bctx, surf);
	}
	is->transform = bctx->transform;
	is->color = bctx->color;
}

static void
register_textures (mod_brush_t *brush)
{
	texture_t  *tex;

	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex = brush->textures[i];
		if (!tex)
			continue;
		glsl_R_AddTexture (tex);
	}
}

static void
glsl_R_ClearTextures (void)
{
	r_num_texture_chains = 0;
}

void
glsl_R_RegisterTextures (model_t **models, int num_models)
{
	int         i;
	model_t    *m;
	mod_brush_t *brush;

	glsl_R_ClearTextures ();
	glsl_R_InitSurfaceChains (&r_refdef.worldmodel->brush);
	glsl_R_AddTexture (r_notexture_mip);
	register_textures (&r_refdef.worldmodel->brush);
	for (i = 0; i < num_models; i++) {
		m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->path == '*')
			continue;
		// world has already been done, not interested in non-brush models
		if (m == r_refdef.worldmodel || m->type != mod_brush)
			continue;
		brush = &m->brush;
		brush->numsubmodels = 1; // no support for submodels in non-world model
		register_textures (brush);
	}
}

static elechain_t *
add_elechain (glsltex_t *tex, int model_index)
{
	elechain_t *ec;

	ec = get_elechain ();
	ec->elements = get_elements ();
	ec->model_index = model_index;
	ec->transform = 0;
	ec->color = 0;
	*tex->elechain_tail = ec;
	tex->elechain_tail = &ec->next;
	return ec;
}

static void
build_surf_displist (model_t **models, msurface_t *surf, int base,
					 dstring_t *vert_list)
{
	mod_brush_t *brush;
	if (surf->model_index < 0) {
		brush = &models[-surf->model_index - 1]->brush;
	} else {
		brush = &r_refdef.worldmodel->brush;
	}
	mvertex_t  *vertices = brush->vertexes;
	medge_t    *edges = brush->edges;
	int        *surfedges = brush->surfedges;

	// create triangle soup for the polygon (this was written targeting
	// GLES 2, which didn't have primitive restart)
	int         numverts = surf->numedges;
	int         numtris = numverts - 2;
	int         numindices = numtris * 3;
	glslpoly_t *poly = malloc (field_offset (glslpoly_t, indices[numindices]));
	poly->count = numindices;
	for (int i = 0, ind = 0; i < numtris; i++) {
		// pretend we can use a triangle fan
		poly->indices[ind++] = base;
		poly->indices[ind++] = base + i + 1;
		poly->indices[ind++] = base + i + 2;
	}
	surf->polys = (glpoly_t *) poly;

	bspvert_t  *verts = alloca (numverts * sizeof (bspvert_t));
	mtexinfo_t *texinfo = surf->texinfo;
	for (int i = 0; i < numverts; i++) {
		vec_t      *vec;
		int         index = surfedges[surf->firstedge + i];
		if (index > 0) {
			// forward edge
			vec = vertices[edges[index].v[0]].position;
		} else {
			// reverse edge
			vec = vertices[edges[-index].v[1]].position;
		}
		VectorCopy (vec, verts[i].vertex);
		verts[i].vertex[3] = 1;	// homogeneous coord

		vec2f_t     st = {
			DotProduct (vec, texinfo->vecs[0]) + texinfo->vecs[0][3],
			DotProduct (vec, texinfo->vecs[1]) + texinfo->vecs[1][3],
		};
		verts[i].tlst[0] = st[0] / texinfo->texture->width;
		verts[i].tlst[1] = st[1] / texinfo->texture->height;

		if (surf->lightpic) {
			//lightmap texture coordinates
			//every lit surface has its own lighmap at a 1/16 resolution
			//(ie, 16 albedo pixels for every lightmap pixel)
			const vrect_t *rect = surf->lightpic->rect;
			vec2f_t     lmorg = (vec2f_t) { VEC2_EXP (&rect->x) } * 16 + 8;
			vec2f_t     texorg = { VEC2_EXP (surf->texturemins) };
			st = ((st - texorg + lmorg) / 16) * surf->lightpic->size;
			verts[i].tlst[2] = st[0];
			verts[i].tlst[3] = st[1];
		} else {
			// no lightmap for this surface (probably sky or water), so
			// make the lightmap texture polygone degenerate
			verts[i].tlst[2] = 0;
			verts[i].tlst[3] = 0;
		}
	}
	dstring_append (vert_list, (char *) verts, numverts * sizeof (bspvert_t));
}

void
glsl_R_BuildDisplayLists (model_t **models, int num_models)
{
	int         vertex_index_base;
	model_t    *m;
	dmodel_t   *dm;
	msurface_t *surf;
	dstring_t  *vertices;
	mod_brush_t *brush;

	QuatSet (0, 0, sqrt(0.5), sqrt(0.5), sky_fix);	// proper skies
	QuatSet (0, 0, 0, 1, sky_rotation[0]);
	QuatCopy (sky_rotation[0], sky_rotation[1]);
	QuatSet (0, 0, 0, 0, sky_velocity);
	QuatExp (sky_velocity, sky_velocity);
	sky_time = vr_data.realtime;

	// now run through all surfaces, chaining them to their textures, thus
	// effectively sorting the surfaces by texture (without worrying about
	// surface order on the same texture chain).
	for (int i = 0; i < num_models; i++) {
		m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->path == '*' || m->type != mod_brush)
			continue;
		brush = &m->brush;
		// non-bsp models don't have surfaces.
		dm = brush->submodels;
		for (uint32_t j = 0; j < brush->numsurfaces; j++) {
			glsltex_t  *tex;
			if (j == dm->firstface + dm->numfaces) {
				dm++;
				if (dm == brush->submodels + brush->numsubmodels) {
					// limit the surfaces
					// probably never hit
					Sys_Printf ("R_BuildDisplayLists: too many surfaces\n");
					brush->numsurfaces = j;
					break;
				}
			}
			surf = brush->surfaces + j;
			surf->model_index = dm - brush->submodels;
			if (!surf->model_index && m != r_refdef.worldmodel)
				surf->model_index = -1 - i;	// instanced model
			tex = surf->texinfo->texture->render;
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
	for (int i = 0; i < r_num_texture_chains; i++) {
		glsltex_t  *tex;
		instsurf_t *is;
		elechain_t *ec = 0;
		elements_t *el = 0;

		tex = r_texture_chains[i];

		for (is = tex->tex_chain; is; is = is->tex_chain) {
			msurface_t *surf = is->surface;
			if (!tex->elechain) {
				ec = add_elechain (tex, surf->model_index);
				el = ec->elements;
				el->base = (byte *) (intptr_t) vertices->size;
				vertex_index_base = 0;
			}
			if (surf->model_index != ec->model_index) {	// next sub-model
				ec = add_elechain (tex, surf->model_index);
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
	Sys_MaskPrintf (SYS_glsl, "R_BuildDisplayLists: %ld verts total\n",
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
R_DrawBrushModel (entity_t e)
{
	float       dot, radius;
	renderer_t *renderer = Ent_GetComponent (e.id, scene_renderer, e.reg);
	model_t    *model = renderer->model;
	mod_brush_t *brush = &model->brush;
	plane_t    *plane;
	msurface_t *surf;
	bool        rotated;
	vec3_t      mins, maxs;
	vec4f_t     org;
	glslbspctx_t bctx = {
		brush,
		Ent_GetComponent (e.id, scene_animation, e.reg),
		renderer->full_transform,
		renderer->colormod,
	};

	mat4f_t mat;
	transform_t transform = Entity_Transform (e);
	Transform_GetWorldMatrix (transform, mat);
	memcpy (renderer->full_transform, mat, sizeof (mat));//FIXME
	if (mat[0][0] != 1 || mat[1][1] != 1 || mat[2][2] != 1) {
		rotated = true;
		radius = model->radius;
		if (R_CullSphere (r_refdef.frustum, (vec_t*)&mat[3], radius)) { // FIXME
			return;
		}
	} else {
		rotated = false;
		VectorAdd (mat[3], model->mins, mins);
		VectorAdd (mat[3], model->maxs, maxs);
		if (R_CullBox (r_refdef.frustum, mins, maxs))
			return;
	}

	org = r_refdef.frame.position - mat[3];
	if (rotated) {
		vec4f_t     temp = org;

		org[0] = dotf (temp, mat[0])[0];
		org[1] = dotf (temp, mat[1])[0];
		org[2] = dotf (temp, mat[2])[0];
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	auto dlight_pool = &r_refdef.registry->comp_pools[scene_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;
	if (brush->firstmodelsurface != 0 && r_dlight_lightmap) {
		for (uint32_t i = 0; i < dlight_pool->count; i++) {
			auto dlight = &dlight_data[i];
			vec4f_t     lightorigin;
			VectorSubtract (dlight->origin, mat[3], lightorigin);
			lightorigin[3] = 1;
			R_RecursiveMarkLights (brush, lightorigin, dlight, i,
								   brush->hulls[0].firstclipnode);
		}
	}

	surf = &brush->surfaces[brush->firstmodelsurface];

	for (unsigned i = 0; i < brush->nummodelsurfaces; i++, surf++) {
		// find the node side on which we are
		plane = surf->plane;

		dot = PlaneDiff (org, plane);

		// enqueue the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
			|| (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			chain_surface (&bctx, surf);
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

// 1 = back side, 0 = front side
static inline int
get_side (mnode_t *node)
{
	// find the node side on which we are
	vec4f_t     org = r_refdef.frame.position;

	return dotf (org, node->plane)[0] < 0;
}

static inline void
visit_node (glslbspctx_t *bctx, mnode_t *node, int side)
{
	int         c;
	msurface_t *surf;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	// seems to be microscopically faster even on modern hardware
	side = (-side) & SURF_PLANEBACK;
	// chain any visible surfaces on the node that face the camera.
	// not all nodes have any surfaces to draw (purely a split plane)
	if ((c = node->numsurfaces)) {
		int         surf_id = node->firstsurface;
		surf = bctx->brush->surfaces + surf_id;
		for (; c; c--, surf++, surf_id++) {
			if (r_visstate.face_visframes[surf_id] != r_visstate.visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			// if side and the surface facing differ, then the camera is
			// on backside of the surface
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;               // wrong side

			chain_surface (bctx, surf);
		}
	}
}

static inline int
test_node (glslbspctx_t *bctx, int node_id)
{
	if (node_id < 0)
		return 0;
	if (r_visstate.node_visframes[node_id] != r_visstate.visframecount)
		return 0;
	mnode_t    *node = bctx->brush->nodes + node_id;
	if (R_CullBox (r_refdef.frustum, node->minmaxs, node->minmaxs + 3))
		return 0;
	return 1;
}

static void
R_VisitWorldNodes (glslbspctx_t *bctx)
{
	typedef struct {
		int         node_id;
		int         side;
	} rstack_t;
	mod_brush_t *brush = bctx->brush;
	rstack_t   *node_ptr;
	rstack_t   *node_stack;
	int         node_id;
	int         front;
	int         side;

	node_id = 0;
	// +2 for paranoia
	node_stack = alloca ((brush->depth + 2) * sizeof (rstack_t));
	node_ptr = node_stack;

	while (1) {
		while (test_node (bctx, node_id)) {
			mnode_t    *node = bctx->brush->nodes + node_id;
			side = get_side (node);
			front = node->children[side];
			if (test_node (bctx, front)) {
				node_ptr->node_id = node_id;
				node_ptr->side = side;
				node_ptr++;
				node_id = front;
				continue;
			}
			if (front < 0) {
				mleaf_t    *leaf = bctx->brush->leafs + ~front;
				if (leaf->contents != CONTENTS_SOLID) {
					visit_leaf (leaf);
				}
			}
			visit_node (bctx, node, side);
			node_id = node->children[side ^ 1];
		}
		if (node_id < 0) {
			mleaf_t    *leaf = bctx->brush->leafs + ~node_id;
			if (leaf->contents != CONTENTS_SOLID) {
				visit_leaf (leaf);
			}
		}
		if (node_ptr != node_stack) {
			node_ptr--;
			node_id = node_ptr->node_id;
			side = node_ptr->side;
			mnode_t    *node = bctx->brush->nodes + node_id;
			visit_node (bctx, node, side);
			node_id = node->children[side ^ 1];
			continue;
		}
		break;
	}
}

static void
draw_elechain (elechain_t *ec, int matloc, int vertloc, int tlstloc,
			   int colloc)
{
	mat4f_t     mat;
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
		mmulf (mat, bsp_vp, ec->transform);
		qfeglUniformMatrix4fv (matloc, 1, false, (vec_t*)&mat[0]);//FIXME
	} else {
		qfeglUniformMatrix4fv (matloc, 1, false, (vec_t*)&bsp_vp[0]);//FIXME
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

	mmulf (bsp_vp, glsl_projection, glsl_view);

	qfeglUseProgram (quake_bsp.program);
	qfeglEnableVertexAttribArray (quake_bsp.vertex.location);
	qfeglEnableVertexAttribArray (quake_bsp.tlst.location);
	qfeglDisableVertexAttribArray (quake_bsp.color.location);

	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
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

	default_color[3] = bound (0, r_wateralpha, 1);
	QuatCopy (default_color, last_color);
	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	mmulf (bsp_vp, glsl_projection, glsl_view);

	qfeglUseProgram (quake_turb.program);
	qfeglEnableVertexAttribArray (quake_turb.vertex.location);
	qfeglEnableVertexAttribArray (quake_turb.tlst.location);
	qfeglDisableVertexAttribArray (quake_turb.color.location);

	qfeglVertexAttrib4fv (quake_turb.color.location, default_color);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
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
	VectorNegate (r_refdef.frame.position, mat + 12);
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

	mmulf (bsp_vp, glsl_projection, glsl_view);

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

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
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
add_surf_elements (glsltex_t *tex, instsurf_t *is,
				   elechain_t **ec, elements_t **el)
{
	msurface_t *surf = is->surface;
	glslpoly_t *poly = (glslpoly_t *) surf->polys;

	if (!tex->elechain) {
		(*ec) = add_elechain (tex, surf->model_index);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*el) = (*ec)->elements;
		(*el)->base = surf->base;
		if (!(*el)->list)
			(*el)->list = dstring_new ();
		dstring_clear ((*el)->list);
	}
	if (is->transform != (*ec)->transform || is->color != (*ec)->color) {
		(*ec) = add_elechain (tex, surf->model_index);
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
build_tex_elechain (glsltex_t *tex)
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
	animation_t animation = {};
	glslbspctx_t bctx = { };
	int         i;

	clear_texture_chains ();	// do this first for water and skys

	bctx.brush = &r_refdef.worldmodel->brush;
	bctx.animation = &animation;

	R_VisitWorldNodes (&bctx);
	if (r_drawentities) {
		for (size_t i = 0; i < r_ent_queue->ent_queues[mod_brush].size; i++) {
			entity_t    ent = r_ent_queue->ent_queues[mod_brush].a[i];
			R_DrawBrushModel (ent);
		}
	}

	glsl_R_FlushLightmaps ();
	bsp_begin ();
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	for (i = 0; i < r_num_texture_chains; i++) {
		glsltex_t  *tex;
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
glsl_R_DrawWaterSurfaces (void)
{
	instsurf_t *is;
	msurface_t *surf;
	glsltex_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!waterchain)
		return;

	turb_begin ();
	for (is = waterchain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture->render) {
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
			tex = surf->texinfo->texture->render;
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
	glsltex_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!sky_chain)
		return;

	sky_begin ();
	for (is = sky_chain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture->render) {
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
			tex = surf->texinfo->texture->render;
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

	r_notexture_mip->render = &glsl_notexture;

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

void
glsl_R_ShutdownBsp (void)
{
	free (r_texture_chains);
	r_texture_chains = 0;
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
	// swapped, but everything makes sense if looking at the cube from outside
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
		sky = r_skyname;

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
	tex = LoadImage (name = va (0, "env/%s_map", sky), 1);
	if (tex && tex->format >= 3 && tex->height * 3 == tex->width * 2
		&& is_pow2 (tex->height)) {
		tex_t      *sub;
		int         size = tex->height / 2;

		skybox_loaded = true;
		sub = malloc (sizeof (tex_t) + size * size * tex->format);
		sub->data = (byte *) (sub + 1);
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
			tex = LoadImage (name = va (0, "env/%s%s", sky, sky_suffix[i]), 1);
			if (!tex || tex->format < 3) {	// FIXME pcx support
				Sys_MaskPrintf (SYS_glsl, "Couldn't load %s\n", name);
				// also look in gfx/env, where Darkplaces looks for skies
				tex = LoadImage (name = va (0, "gfx/env/%s%s", sky,
											sky_suffix[i]), 1);
				if (!tex || tex->format < 3) {  // FIXME pcx support
					Sys_MaskPrintf (SYS_glsl, "Couldn't load %s\n", name);
					skybox_loaded = false;
					continue;
				}
			}
			Sys_MaskPrintf (SYS_glsl, "Loaded %s\n", name);
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
