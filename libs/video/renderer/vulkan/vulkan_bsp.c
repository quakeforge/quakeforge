/*
	vulkan_bsp.c

	Vulkan bsp

	Copyright (C) 2012       Bill Currie <bill@taniwha.org>
	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
	Date: 2021/1/18

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

#include <string.h>
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/heapsort.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/math/bitop.h"
#include "QF/scene/efrags.h"
#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_scene.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "QF/simd/types.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#define TEX_SET 3
#define SKYBOX_SET 4
#define SKYMAP_SET 5
#define LIGHTMAP_SET 4
//FIXME share
#define workgoup_size 512

static void
add_texture (texture_t *tx, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;

	vulktex_t  *tex = tx->render;
	if (tex->view) {
		tex->tex_id = bctx->registered_textures.size;
		DARRAY_APPEND (&bctx->registered_textures, tex);
		tex->descriptor = Vulkan_CreateCombinedImageSampler (ctx, tex->view,
															 bctx->sampler);
	}
}

static void
register_textures (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	texture_t  *tex;

	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex = brush->textures[i];
		if (!tex)
			continue;
		add_texture (tex, ctx);
	}
}

static void
init_visstate (bspctx_t *bctx, visstate_t *visstate)
{
	qfZoneScoped (true);
	mod_brush_t *brush = r_refdef.worldmodel->brush;
	int     count = brush->numnodes + brush->modleafs
					+ brush->numsurfaces;
	int         size = count * sizeof (int);
	int        *shadow_node_frames = Hunk_AllocName (r_refdef.hunk, size,
													 "visframes");
	int        *shadow_leaf_frames = shadow_node_frames + brush->numnodes;
	int        *shadow_face_frames = shadow_leaf_frames + brush->modleafs;
	int        *debug_node_frames = Hunk_AllocName (r_refdef.hunk, size,
													"visframes");
	int        *debug_leaf_frames = debug_node_frames + brush->numnodes;
	int        *debug_face_frames = debug_leaf_frames + brush->modleafs;
	bctx->shadow_pass.visstate = (visstate_t) {
		.face_visframes = shadow_face_frames,
		.leaf_visframes = shadow_leaf_frames,
		.node_visframes = shadow_node_frames,
		.brush = brush,
	};

	bctx->debug_pass.visstate = (visstate_t) {
		.face_visframes = debug_face_frames,
		.leaf_visframes = debug_leaf_frames,
		.node_visframes = debug_node_frames,
		.brush = brush,
	};
}

static void
clear_pass_face_queues (bsp_pass_t *pass, const bspctx_t *bctx)
{
	if (pass->face_queue) {
		for (size_t i = 0; i < bctx->registered_textures.size; i++) {
			DARRAY_CLEAR (&pass->face_queue[i]);
		}
		free (pass->face_queue);
		pass->face_queue = 0;
	}
}

static void
shutdown_pass_draw_queues (bsp_pass_t *pass)
{
	EntQueue_Delete (pass->entqueue);
	for (int i = 0; i < pass->num_queues; i++) {
		DARRAY_CLEAR (&pass->draw_queues[i]);
	}
	free (pass->draw_queues);
}

static void
shutdown_pass_instances (bsp_pass_t *pass, const bspctx_t *bctx)
{
	if (pass->instances) {
		for (int i = 0; i < bctx->num_models; i++) {
			DARRAY_CLEAR (&pass->instances[i].entities);
		}
		free (pass->instances);
	}
}

static void
setup_pass_instances (bsp_pass_t *pass, const bspctx_t *bctx)
{
	pass->instances = malloc (sizeof (bsp_instance_t[bctx->num_models]));
	for (int i = 0; i < bctx->num_models; i++) {
		DARRAY_INIT (&pass->instances[i].entities, 16);
	}
}

static void
setup_pass_draw_queues (bsp_pass_t *pass)
{
	pass->num_queues = QFV_bspNumPasses;
	pass->draw_queues = malloc (sizeof (bsp_drawset_t[pass->num_queues]));
	for (int i = 0; i < pass->num_queues; i++) {
		DARRAY_INIT (&pass->draw_queues[i], 64);
	}
}

static void
setup_pass_face_queues (bsp_pass_t *pass, int num_tex, bspctx_t *bctx)
{
	qfZoneScoped (true);
	pass->face_queue = malloc (sizeof (bsp_instfaceset_t[num_tex]));
	for (int i = 0; i < num_tex; i++) {
		pass->face_queue[i] = (bsp_instfaceset_t) DARRAY_STATIC_INIT (128);
	}
}

static void
clear_textures (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;

	clear_pass_face_queues (&bctx->main_pass, bctx);
	clear_pass_face_queues (&bctx->shadow_pass, bctx);
	clear_pass_face_queues (&bctx->debug_pass, bctx);

	bctx->registered_textures.size = 0;
}

void
Vulkan_RegisterTextures (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	auto bctx = ctx->bsp_context;

	clear_textures (ctx);

	bctx->notexture_render = (vulktex_t) { .view = bctx->notexture };
	bctx->background_render = (vulktex_t) { .view = bctx->default_skysheet };
	texture_t base_tx[] = {
		{ .render = &bctx->notexture_render },
		{ .render = &bctx->background_render },
	};
	for (size_t i = 0; i < countof (base_tx); i++) {
		add_texture (&base_tx[i], ctx);
	}
	{
		// FIXME make worldmodel non-special. needs smarter handling of
		// textures on sub-models but not on main model.
		mod_brush_t *brush = r_refdef.worldmodel->brush;
		register_textures (brush, ctx);
	}
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->path == '*')
			continue;
		// world has already been done, not interested in non-brush models
		// FIXME see above
		if (m == r_refdef.worldmodel || m->type != mod_brush)
			continue;
		mod_brush_t *brush = m->brush;
		brush->numsubmodels = 1; // no support for submodels in non-world model
		register_textures (brush, ctx);
	}

	int         num_tex = bctx->registered_textures.size;

	texture_t **textures = alloca (num_tex * sizeof (texture_t *));
	for (uint32_t i = 0; i < countof (base_tx); i++) {
		textures[i] = &base_tx[i];
	}
	for (int i = 0, t = countof (base_tx); i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		if (!m || *m->path == '*') {
			continue;
		}
		mod_brush_t *brush = m->brush;
		if (!brush) {
			continue;
		}
		for (unsigned j = 0; j < brush->numtextures; j++) {
			if (brush->textures[j]) {
				textures[t++] = brush->textures[j];
			}
		}
	}

	// 2.5 for two texanim_t structs (32-bits each) and 1 uint16_t for each
	// element
	size_t      texdata_size = 2.5 * num_tex * sizeof (texanim_t);
	texanim_t  *texdata = Hunk_AllocName (r_refdef.hunk, texdata_size,
										  "texdata");
	bctx->texdata.anim_main = texdata;
	bctx->texdata.anim_alt = texdata + num_tex;
	bctx->texdata.frame_map = (uint16_t *) (texdata + 2 * num_tex);
	int16_t     map_index = 0;
	for (int i = 0; i < num_tex; i++) {
		texanim_t  *anim = bctx->texdata.anim_main + i;
		if (anim->count) {
			// already done as part of an animation group
			continue;
		}
		*anim = (texanim_t) { .base = map_index, .offset = 0, .count = 1 };
		bctx->texdata.frame_map[anim->base] = i;

		if (textures[i]->anim_total > 1) {
			// bsp loader multiplies anim_total by ANIM_CYCLE to slow the
			// frame rate
			anim->count = textures[i]->anim_total / ANIM_CYCLE;
			texture_t  *tx = textures[i]->anim_next;
			for (int j = 1; j < anim->count; j++) {
				if (!tx) {
					Sys_Error ("broken cycle");
				}
				vulktex_t  *vtex = tx->render;
				texanim_t  *a = bctx->texdata.anim_main + vtex->tex_id;
				if (a->count) {
					Sys_Error ("crossed cycle");
				}
				*a = *anim;
				a->offset = j;
				bctx->texdata.frame_map[a->base + a->offset] = vtex->tex_id;
				tx = tx->anim_next;
			}
			if (tx != textures[i]) {
				Sys_Error ("infinite cycle");
			}
		}
		map_index += bctx->texdata.anim_main[i].count;
	}
	for (int i = 0; i < num_tex; i++) {
		texanim_t  *alt = bctx->texdata.anim_alt + i;
		if (textures[i]->alternate_anims) {
			texture_t  *tx = textures[i]->alternate_anims;
			vulktex_t  *vtex = tx->render;
			*alt = bctx->texdata.anim_main[vtex->tex_id];
		} else {
			*alt = bctx->texdata.anim_main[i];
		}
	}

	// create face queue arrays
	setup_pass_face_queues (&bctx->main_pass, num_tex, bctx);
	setup_pass_face_queues (&bctx->shadow_pass, num_tex, bctx);
	setup_pass_face_queues (&bctx->debug_pass, num_tex, bctx);
}

typedef struct {
	msurface_t *face;
	model_t    *model;
	int         model_face_base;
} faceref_t;

typedef struct DARRAY_TYPE (faceref_t) facerefset_t;

//static void
//count_verts_inds (const faceref_t *faceref, uint32_t *verts, uint32_t *inds)
//{
//	msurface_t *surf = faceref->face;
//	if (surf->flags & SURF_DRAWBACKGROUND) {
//		*verts = 0;
//		*inds = 0;
//	} else {
//		*verts = surf->numedges;
//		*inds = surf->numedges + 1;
//	}
//}

typedef struct bspvert_s {
	vec3_t      vertex;
	vec3_t      normal;
	quat_t      tlst;
} bspvert_t;

typedef struct {
	bspctx_t   *bctx;
	uint32_t   *tex_clusters;
	uint32_t    mod_cluster_count;
	uint32_t    sub_cluster_count;
	uint32_t    vert_count;
	uint32_t    ind_count;
	bspvert_t  *vertices;
	uint32_t   *indices;
	bsp_cluster_t *subclusters;
	cluster_t  *clusters;
	uint32_t   *clustermap;
	uint32_t    vertex_base;
	uint32_t    index_base;
	uint32_t    cluster_base;
	uint32_t    clustermap_base;
} buildctx_t;

static vec_t *
vbsp_get_vertex (const mod_brush_t *brush, int edge_index)
{
	auto vertices = brush->vertexes;
	auto edges = brush->edges;
	if (edge_index > 0) {
		// forward edge
		return vertices[edges[edge_index].v[0]].position;
	} else {
		// reverse edge
		return vertices[edges[-edge_index].v[1]].position;
	}
}

// Build the vertices for a single polygon face
static uint32_t
build_surf_vertices (const msurface_t *surf, const mod_brush_t *brush,
					 buildctx_t *build)
{
	// create a triangle fan
	int         numverts = surf->numedges;

	uint32_t    first_index = build->index_base;
	build->index_base += numverts + 1;
	for (int i = 0; i < numverts; i++) {
		build->indices[first_index + i] = build->vertex_base + i;
	}
	build->indices[first_index + numverts] = ~0u;	// primitive restart

	bspvert_t  *verts = build->vertices + build->vertex_base;
	build->vertex_base += numverts;
	mtexinfo_t *texinfo = surf->texinfo;
	int        *surfedges = brush->surfedges;
	vec3_t      normal;
	VectorCopy(surf->plane->normal, normal);
	if (surf->flags & SURF_PLANEBACK) {
		VectorNegate (normal, normal);
	}
	for (int i = 0; i < numverts; i++) {
		int         index = surfedges[surf->firstedge + i];
		vec_t      *vec = vbsp_get_vertex (brush, index);;
		VectorCopy (vec, verts[i].vertex);
		VectorCopy (normal, verts[i].normal);

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
			// make the lightmap texture polygon degenerate
			verts[i].tlst[2] = 0;
			verts[i].tlst[3] = 0;
		}
	}
	return numverts + 1;
}

static void
model_loop (model_t **models, int num_models, vulkan_ctx_t *ctx,
			void (*func)(model_t *, buildctx_t *), buildctx_t *build)
{
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		// and non-bsp models don't have surfaces.
		if (!m || m->type != mod_brush || *m->path == '*') {
			continue;
		}
		func (m, build);
	}
}

static uint32_t
surf_tex_id (const msurface_t *surf, const bspctx_t *bctx)
{
	if (surf->flags & SURF_DRAWBACKGROUND) {
		return bctx->background_render.tex_id;
	}
	vulktex_t *tex = surf->texinfo->texture->render;
	return tex->tex_id;
}

static void
scan_surfaces (mod_brush_t *brush, cluster_t *cluster, buildctx_t *build)
{
	texture_t *cur_tex = nullptr;
	build->mod_cluster_count++;
	for (uint32_t i = 0; i < cluster->count; i++) {
		uint32_t ind = cluster->first + i;
		auto surf = brush->surfaces + brush->cluster_surfs[ind];
		if (cur_tex != surf->texinfo->texture) {
			cur_tex = surf->texinfo->texture;
			uint32_t tex_id = surf_tex_id (surf, build->bctx);
			build->tex_clusters[tex_id]++;
			build->sub_cluster_count++;
		}
		if (surf->flags & SURF_DRAWBACKGROUND) {
		} else {
			build->vert_count += surf->numedges;
			build->ind_count += surf->numedges + 1;
		}
	}
}

static void
model_scan_surfaces (model_t *m, buildctx_t *build)
{
	mod_brush_t *brush = m->brush;
	// vis_clusters does not include the solid cluster 0
	uint32_t num_clusters = brush->cluster_vis.count + 1;
	for (uint32_t j = 0; j < num_clusters; j++) {
		auto cluster = &brush->clusters[j];
		scan_surfaces (brush, cluster, build);
	}
	for (uint32_t j = 1; j < brush->numsubmodels; j++) {
		auto cluster = &brush->clusters[num_clusters + j - 1];
		scan_surfaces (brush, cluster, build);
	}
}

static void
build_surfaces (mod_brush_t *brush, cluster_t *cluster, buildctx_t *build)
{
	texture_t *cur_tex = nullptr;
	bsp_cluster_t *sc = nullptr;
	cluster_t *c = &build->clusters[build->cluster_base++];
	*c = (cluster_t) {
		.first = build->clustermap_base,
	};
	for (uint32_t i = 0; i < cluster->count; i++) {
		uint32_t ind = cluster->first + i;
		auto surf = brush->surfaces + brush->cluster_surfs[ind];
		if (cur_tex != surf->texinfo->texture) {
			cur_tex = surf->texinfo->texture;
			uint32_t tex_id = surf_tex_id (surf, build->bctx);
			uint32_t sub_cluster_ind = build->tex_clusters[tex_id]++;
			sc = &build->subclusters[sub_cluster_ind];
			*sc = (bsp_cluster_t) {
				.first_index = build->index_base,
				.tex_id = tex_id,
			};
			build->clustermap[c->first + c->count++] = sub_cluster_ind;
		}
		if (surf->flags & SURF_DRAWBACKGROUND) {
		} else {
			sc->index_count += build_surf_vertices (surf, brush, build);
		}
	}
	build->clustermap_base += c->count;
}

static void
model_build_surfaces (model_t *m, buildctx_t *build)
{
	mod_brush_t *brush = m->brush;
	// vis_clusters does not include the solid cluster 0
	uint32_t num_clusters = brush->cluster_vis.count + 1;
	for (uint32_t j = 0; j < num_clusters; j++) {
		auto cluster = &brush->clusters[j];
		build_surfaces (brush, cluster, build);
	}
	for (uint32_t j = 1; j < brush->numsubmodels; j++) {
		auto cluster = &brush->clusters[num_clusters + j - 1];
		build_surfaces (brush, cluster, build);
	}
}

void
Vulkan_BuildDisplayLists (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	//qfv_device_t *device = ctx->device;
	bspctx_t   *bctx = ctx->bsp_context;
	visstate_t *visstate = nullptr;

	bctx->num_models = 0;
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		if (m && m->type == mod_brush) {
			m->render_id = bctx->num_models++;
			if (!visstate) {
				visstate = m->brush->visstate;
			}
		}
	}

	init_visstate (bctx, visstate);
	if (!num_models) {
		return;
	}

	uint32_t num_tex = bctx->registered_textures.size;
	uint32_t tex_clusters[num_tex] = {};

	shutdown_pass_instances (&bctx->main_pass, bctx);
	shutdown_pass_instances (&bctx->shadow_pass, bctx);
	shutdown_pass_instances (&bctx->debug_pass, bctx);

	buildctx_t build = {
		.bctx = bctx,
		.tex_clusters = tex_clusters,
	};

	// count sub-clusters, vertices and indices
	model_loop (models, num_models, ctx, model_scan_surfaces, &build);

	free (bctx->command_offsets);
	free (bctx->command_counts);
	free (bctx->models);
	uint32_t mod_clusters = build.mod_cluster_count;
	uint32_t num_clusters = build.sub_cluster_count;
	bctx->models = malloc (sizeof (bsp_model_t[bctx->num_models]));

	setup_pass_instances (&bctx->main_pass, bctx);
	setup_pass_instances (&bctx->shadow_pass, bctx);
	setup_pass_instances (&bctx->debug_pass, bctx);
	// All vertices from all brush models go into one giant vbo.
	uint32_t    vertex_count = build.vert_count;
	uint32_t    index_count = build.ind_count;

	// vulkan doesn't like 0-length buffers, but vertex_count and index_count
	// will be 0 for the empty world model.
	if (vertex_count < 3) {
		vertex_count = 3;
	}
	if (index_count < 3) {
		index_count = 3;
	}
	size_t index_buffer_size = sizeof (uint32_t[index_count]);
	size_t vertex_buffer_size = sizeof (bspvert_t[vertex_count]);
	size_t command_counts_buffer_size = sizeof (uint32_t[num_tex]);
	size_t command_offsets_buffer_size = sizeof (tex_clusters);
	size_t command_buffer_size
		= sizeof (VkDrawIndexedIndirectCommand[num_clusters]);
	size_t subcluster_buffer_size = sizeof (bsp_cluster_t[num_clusters]);
	size_t cluster_buffer_size = sizeof (cluster_t[mod_clusters]);
	size_t clustermap_buffer_size = sizeof (uint32_t[num_clusters]);
	size_t queue_buffer_size = sizeof (uint32_t[num_clusters]);

	bctx->command_offsets = malloc (command_offsets_buffer_size);
	bctx->command_counts = malloc (command_offsets_buffer_size);

	memcpy (bctx->command_counts, tex_clusters, sizeof (tex_clusters));
	uint32_t sum = 0;
	auto pass = &bctx->main_pass;
	DARRAY_APPEND (&pass->draw_queues[QFV_bspSolid],
					(bsp_draw_t) { .tex_id = 0 });
	for (uint32_t i = 1; i < num_tex; i++) {
		//FIXME this is broken
		DARRAY_APPEND (&pass->draw_queues[QFV_bspSolid],
						(bsp_draw_t) { .tex_id = i });
		uint32_t temp = tex_clusters[i];
		tex_clusters[i] = sum;
		sum += temp;
	}
	memcpy (bctx->command_offsets, tex_clusters, sizeof (tex_clusters));

	if (index_buffer_size > bctx->index_buffer_size
		|| vertex_buffer_size > bctx->vertex_buffer_size
		|| command_counts_buffer_size > bctx->command_counts_buffer_size
		|| command_offsets_buffer_size > bctx->command_offsets_buffer_size
		|| command_buffer_size > bctx->command_buffer_size
		|| subcluster_buffer_size > bctx->subcluster_buffer_size
		|| cluster_buffer_size > bctx->cluster_buffer_size
		|| clustermap_buffer_size > bctx->clustermap_buffer_size
		|| queue_buffer_size > bctx->queue_buffer_size) {
		QFV_DestroyResource (ctx->device, bctx->bsp_resource);
		if (!bctx->bsp_resource) {
			size_t size = sizeof (qfv_resource_t)
						+ sizeof (qfv_resobj_t[1])	// vertices
						+ sizeof (qfv_resobj_t[1])	// indices
						+ sizeof (qfv_resobj_t[1]) 	// command countss
						+ sizeof (qfv_resobj_t[1]) 	// command offsets
						+ sizeof (qfv_resobj_t[1]) 	// draw commands
						+ sizeof (qfv_resobj_t[1])	// subclusters
						+ sizeof (qfv_resobj_t[1])	// clusters
						+ sizeof (qfv_resobj_t[1])	// clustermap
						+ sizeof (qfv_resobj_t[1]);	// queue
			bctx->bsp_resource = malloc (size);
		}
		auto vertices = (qfv_resobj_t *) &bctx->bsp_resource[1];
		auto indices = (qfv_resobj_t *) &vertices[1];
		auto command_counts = (qfv_resobj_t *) &indices[1];
		auto command_offsets = (qfv_resobj_t *) &command_counts[1];
		auto commands = (qfv_resobj_t *) &command_offsets[1];
		auto subclusters = (qfv_resobj_t *) &commands[1];
		auto clusters = (qfv_resobj_t *) &subclusters[1];
		auto clustermap = (qfv_resobj_t *) &clusters[1];
		auto queue = (qfv_resobj_t *) &clustermap[1];

		*bctx->bsp_resource = (qfv_resource_t) {
			.name = "bsp",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = 9,
			.objects = vertices,
		};
		*vertices = (qfv_resobj_t) {
			.name = "vertex",
			.type = qfv_res_buffer,
			.buffer = {
				.size = vertex_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		*indices = (qfv_resobj_t) {
			.name = "index",
			.type = qfv_res_buffer,
			.buffer = {
				.size = index_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			},
		};
		*command_counts = (qfv_resobj_t) {
			.name = "command_counts",
			.type = qfv_res_buffer,
			.buffer = {
				.size = command_counts_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*command_offsets = (qfv_resobj_t) {
			.name = "command_offsets",
			.type = qfv_res_buffer,
			.buffer = {
				.size = command_offsets_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*commands = (qfv_resobj_t) {
			.name = "command",
			.type = qfv_res_buffer,
			.buffer = {
				.size = command_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*subclusters = (qfv_resobj_t) {
			.name = "subcluster",
			.type = qfv_res_buffer,
			.buffer = {
				.size = subcluster_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*clusters = (qfv_resobj_t) {
			.name = "cluster",
			.type = qfv_res_buffer,
			.buffer = {
				.size = cluster_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*clustermap = (qfv_resobj_t) {
			.name = "clustermap",
			.type = qfv_res_buffer,
			.buffer = {
				.size = clustermap_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		size_t frames = bctx->frames.size;
		*queue = (qfv_resobj_t) {
			.name = "queue",
			.type = qfv_res_buffer,
			.buffer = {
				.size = queue_buffer_size * frames,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		QFV_CreateResource (ctx->device, bctx->bsp_resource);

		bctx->vertex_buffer_size = vertex_buffer_size;
		bctx->index_buffer_size = index_buffer_size;
		bctx->command_counts_buffer_size = command_counts_buffer_size;
		bctx->command_offsets_buffer_size = command_offsets_buffer_size;
		bctx->command_buffer_size = command_buffer_size;
		bctx->subcluster_buffer_size = subcluster_buffer_size;
		bctx->cluster_buffer_size = cluster_buffer_size;
		bctx->clustermap_buffer_size = clustermap_buffer_size;

		bctx->vertex_buffer = vertices->buffer.buffer;
		bctx->index_buffer = indices->buffer.buffer;
		bctx->command_counts_buffer = command_counts->buffer.buffer;
		bctx->command_offsets_buffer = command_offsets->buffer.buffer;
		bctx->command_buffer = commands->buffer.buffer;
		bctx->subcluster_buffer = subclusters->buffer.buffer;
		bctx->cluster_buffer = clusters->buffer.buffer;
		bctx->clustermap_buffer = clustermap->buffer.buffer;
		bctx->queue_buffer = queue->buffer.buffer;

		*bctx->command_counts_ptr = command_counts->buffer.address;
		*bctx->command_offsets_ptr = command_offsets->buffer.address;
		*bctx->commands_ptr = commands->buffer.address;
		*bctx->subclusters_ptr = subclusters->buffer.address;
		*bctx->clusters_ptr = clusters->buffer.address;
		*bctx->cluster_map_ptr = clustermap->buffer.address;
		bctx->queue_buffer_addr = queue->buffer.address;

		for (size_t i = 0; i < frames; i++) {
			bctx->frames.a[i].queue = queue_buffer_size * i;
		}
	}
	*bctx->texture_count = num_tex;

	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.build");
	size_t packet_size = vertex_buffer_size
					   + index_buffer_size
					   + command_offsets_buffer_size
					   + subcluster_buffer_size
					   + cluster_buffer_size
					   + clustermap_buffer_size;
	build.vertices = (bspvert_t *) QFV_PacketExtend (packet, packet_size);
	build.indices = (uint32_t *) &build.vertices[vertex_count];
	uint32_t *command_offsets = (uint32_t *) &build.indices[index_count];
	build.subclusters = (bsp_cluster_t *) &command_offsets[num_tex];
	build.clusters = (cluster_t *) &build.subclusters[num_clusters];
	build.clustermap = (uint32_t *) &build.clusters[mod_clusters];

	memcpy (command_offsets, tex_clusters, sizeof (tex_clusters));
	model_loop (models, num_models, ctx, model_build_surfaces, &build);

	qfv_scatter_t vert_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.vertices),
		.length = vertex_buffer_size,
	};
	qfv_scatter_t ind_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.indices),
		.length = index_buffer_size,
	};
	qfv_scatter_t ofs_scatter = {
		.srcOffset = QFV_PacketOffset (packet, command_offsets),
		.length = command_offsets_buffer_size,
	};
	qfv_scatter_t sub_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.subclusters),
		.length = subcluster_buffer_size,
	};
	qfv_scatter_t cls_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.clusters),
		.length = cluster_buffer_size,
	};
	qfv_scatter_t map_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.clustermap),
		.length = clustermap_buffer_size,
	};
	QFV_PacketScatterBuffer (packet, bctx->vertex_buffer, 1, &vert_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead]);
	QFV_PacketScatterBuffer (packet, bctx->index_buffer, 1, &ind_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->command_offsets_buffer,
			1, &ofs_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->subcluster_buffer, 1, &sub_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->cluster_buffer, 1, &cls_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->clustermap_buffer, 1, &map_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
}

static void
bind_texture (vulktex_t *tex, uint32_t setnum, VkPipelineLayout layout,
			  qfv_devfuncs_t *dfunc, VkCommandBuffer cmd)
{
	VkDescriptorSet sets[] = {
		tex->descriptor,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, setnum, 1, sets, 0, 0);
}

static void
push_bspconst (uint16_t *matrix_base, QFV_BspQueue queue,
			   qfv_pipeline_t *pipeline,
			   VkCommandBuffer cmd, bspctx_t *bctx)
{
	if (matrix_base) {
		*bctx->matrix_base = *matrix_base;
	} else {
		uint32_t control = 0;
		if (bctx->skymap_tex || bctx->skymap_id != nullent) {
			control |= 4;
		} else if (bctx->skybox_tex || bctx->skybox_id != nullent) {
			control |= 2;
		} else {
			control |= 1;
		}
		control |= (queue == QFV_bspBackground) << 3;
		*bctx->fog = Fog_Get ();
		*bctx->time = vr_data.realtime;
		*bctx->alpha = queue == QFV_bspTurb ? r_wateralpha : 1;
		*bctx->turb_scale = queue == QFV_bspTurb ? 1 : 0;
		*bctx->control = control;
	}

	QFV_PushBlackboard (bctx->vulkan_ctx, cmd, pipeline);
}

static void
draw_queue (bsp_pass_t *pass, QFV_BspQueue queue, VkPipelineLayout layout,
			qfv_device_t *device, VkCommandBuffer cmd, bspctx_t *bctx)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < pass->draw_queues[queue].size; i++) {
		auto d = pass->draw_queues[queue].a[i];
		if (pass->textures) {
			vulktex_t  *tex = pass->textures->a[d.tex_id];
			bind_texture (tex, TEX_SET, layout, dfunc, cmd);
		}
		if (queue == QFV_bspBackground) {
			dfunc->vkCmdDraw (cmd, 3, d.instance_count, 0, d.first_instance);
		} else {
			size_t cmd_size = sizeof (VkDrawIndexedIndirectCommand);
			dfunc->vkCmdDrawIndexedIndirectCount (cmd,
					bctx->command_buffer,
					bctx->command_offsets[d.tex_id] * cmd_size,
					bctx->command_counts_buffer,
					d.tex_id * sizeof (uint32_t),
					bctx->command_counts[d.tex_id],
					cmd_size);
		}
	}
}

static void
bsp_flush (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	Vulkan_Scene_Flush (ctx);

	if (!bframe->entid_count) {
		return;
	}

	qfv_packet_t *packet;
	void *data;
	size_t size;

	packet = QFV_PacketAcquire (ctx->staging, "bsp.entid");
	size = sizeof (uint32_t[bframe->entid_count]);
	data = QFV_PacketExtend (packet, size);
	memcpy (data, bframe->entid_data, size);
	QFV_PacketCopyBuffer (packet, bctx->entid_buffer, bframe->entid_offset,
					&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
					&bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead]);
	QFV_PacketSubmit (packet);

	QFV_ScrapFlush (bctx->light_scrap);
}

static void
create_default_skys (vulkan_ctx_t *ctx, qfv_packet_t *packet, qfv_resobj_t *res)
{
	qfZoneScoped (true);
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	auto ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	VkImageMemoryBarrier2 barriers[] = { [0 ... 2] = ib };
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = countof (barriers),
		.pImageMemoryBarriers = barriers,
	};
	barriers[0].image = res[0].image.image;
	barriers[1].image = res[1].image.image;
	barriers[2].image = res[2].image.image;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);

	VkClearColorValue color = {};
	VkImageSubresourceRange range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, VK_REMAINING_MIP_LEVELS,
		0, VK_REMAINING_ARRAY_LAYERS
	};
	dfunc->vkCmdClearColorImage (packet->cmd, res[0].image.image,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 &color, 1, &range);

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barriers[0] = ib;
	barriers[1] = ib;
	barriers[2] = ib;
	barriers[0].image = res[0].image.image;
	barriers[1].image = res[1].image.image;
	barriers[2].image = res[2].image.image;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);
}

static void
create_notexture (vulkan_ctx_t *ctx, qfv_packet_t *packet, qfv_resobj_t *res)
{
	qfZoneScoped (true);
	int image_size = 64 * 64 * 4;	//64x64 rgba (8x8 chars)
	byte *data_bytes = QFV_PacketExtend (packet, image_size * 2);// two layers
	byte *data[2] = {
		data_bytes,
		data_bytes + image_size,
	};
	const char *missing = "Missing";

	for (int i = 0; i < 64 * 64; i++) {
		data[0][i * 4 + 0] = 0x20;
		data[0][i * 4 + 1] = 0x20;
		data[0][i * 4 + 2] = 0x20;
		data[0][i * 4 + 3] = 0xff;

		data[1][i * 4 + 0] = 0x00;
		data[1][i * 4 + 1] = 0x00;
		data[1][i * 4 + 2] = 0x00;
		data[1][i * 4 + 3] = 0xff;
	}
	int         x = 4;
	int         y = 4;
	for (const char *c = missing; *c; c++) {
		byte       *bitmap = font8x8_data + *c * 8;
		for (int l = 0; l < 8; l++) {
			byte        d = *bitmap++;
			for (int b = 0; b < 8; b++) {
				if (d & 0x80) {
					int         base = ((y + l) * 64 + x + b) * 4;
					data[0][base + 0] = 0x00;
					data[0][base + 1] = 0x00;
					data[0][base + 2] = 0x00;
					data[0][base + 3] = 0xff;

					data[1][base + 0] = 0xff;
					data[1][base + 1] = 0x00;
					data[1][base + 2] = 0xff;
					data[1][base + 3] = 0xff;
				}
				d <<= 1;
			}
		}
		x += 8;
	}
	for (int i = 1; i < 7; i++) {
		y += 8;
		memcpy (data[0] + y * 64 * 4, data[0] + 4 * 64 * 4, 8 * 64 * 4);
		memcpy (data[1] + y * 64 * 4, data[1] + 4 * 64 * 4, 8 * 64 * 4);
	}

	size_t offset = QFV_PacketOffset (packet, data_bytes);
	auto sb = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	auto db = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	QFV_PacketCopyImage (packet, res->image.image, (qfv_offset_t){},
						 (qfv_extent_t) {64, 64, 1, 1}, offset, &sb, &db);
}

static void
create_base_resources (vulkan_ctx_t *ctx)
{
	auto bctx = ctx->bsp_context;
	size_t frames = bctx->frames.size;

	static tex_t notexture_tex = {
		.width = 64,
		.height = 64,
		.format = tex_rgba,
		.loaded = true,
	};
	// shared by both skysheet and skybox as they have the same 2d dimensions
	static tex_t default_sky_tex = {
		.width = 1,
		.height = 1,
		.format = tex_rgba,
		.loaded = true,
	};

	size_t size = sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t[4])	//images
				+ sizeof (qfv_resobj_t[4]) 	//views
				+ sizeof (qfv_resobj_t[1])	//default vertices
				+ sizeof (qfv_resobj_t[1]);	//entid
	bctx->base_resource = malloc (size);
	auto images = (qfv_resobj_t *) &bctx->base_resource[1];
	auto views = (qfv_resobj_t *) &images[4];
	auto verts = (qfv_resobj_t *) &views[4];
	auto entid = (qfv_resobj_t *) &verts[1];
	*bctx->base_resource = (qfv_resource_t) {
		.name = "bsp",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 4 + 4 + 1 + 1,
		.objects = images,
	};
	QFV_ResourceInitTexImage (&images[0], "notexture", true,
							  &notexture_tex);
	QFV_ResourceInitTexImage (&images[1], "default_skysheet", true,
							  &default_sky_tex);
	QFV_ResourceInitTexImage (&images[2], "default_skybox", true,
							  &default_sky_tex);
	QFV_ResourceInitTexImage (&images[3], "default_skymap", true,
							  &default_sky_tex);
	images[0].image.num_layers = 2;
	images[1].image.num_layers = 2;
	images[2].image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	images[2].image.num_layers = 6;
	QFV_ResourceInitImageView (&views[0], 0, &images[0]);
	QFV_ResourceInitImageView (&views[1], 1, &images[1]);
	QFV_ResourceInitImageView (&views[2], 2, &images[2]);
	QFV_ResourceInitImageView (&views[3], 3, &images[3]);

	*verts = (qfv_resobj_t) {
		.name = "verts",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (bspvert_t[3]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	size_t      entid_count = Vulkan_Scene_MaxEntities (ctx);
	*entid = (qfv_resobj_t) {
		.name = "entid",
		.type = qfv_res_buffer,
		.buffer = {
			.size = entid_count * sizeof (uint32_t) * frames,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};

	QFV_CreateResource (ctx->device, bctx->base_resource);

	bctx->notexture = views[0].image_view.view;
	bctx->default_skysheet = views[1].image_view.view;
	bctx->default_skybox = views[2].image_view.view;
	bctx->default_skymap = views[3].image_view.view;
	bctx->default_verts = verts->buffer.buffer;
	bctx->entid_buffer = entid->buffer.buffer;

	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.base");
	create_notexture (ctx, packet, &images[0]);
	create_default_skys (ctx, packet, &images[1]);
	QFV_PacketSubmit (packet);

	bctx->entid_data = malloc (sizeof (uint32_t[entid_count]));
	for (size_t i = 0; i < frames; i++) {
		auto bframe = &bctx->frames.a[i];
		bframe->entid_data = bctx->entid_data;
		bframe->entid_offset = 0;
		bframe->entid_count = 0;
	}
}

static void
bsp_reset_queues (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto bframe = &bctx->frames.a[ctx->curFrame];

	bframe->entid_count = 0;

	Vulkan_Scene_Clear (ctx);
}

static void
bsp_clear_commands (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	int  num_tex = bctx->registered_textures.size;
	auto pipeline = taskctx->pipeline;
	pipeline->dispatch[0] = RUP (num_tex, workgoup_size);
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;
	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
	};
	pipeline->post_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
	};
}

static void
bsp_draw_queue (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;
	auto layout = pipeline->layout;
	auto cmd = taskctx->cmd;
	uint16_t *matrix_base = taskctx->data;

	if (!r_refdef.worldmodel) {
		return;
	}

	// params are in reverse order
	auto pass_ind = *(QFV_BspPass *) params[2]->value;
	auto queue = *(QFV_BspQueue *) params[1]->value;
	auto textured = *(int *) params[0]->value;

	if (queue != QFV_bspBackground && !bctx->vertex_buffer) {
		return;
	}

	auto pass = (bsp_pass_t *[]) {
		[QFV_bspMain] = &bctx->main_pass,
		[QFV_bspLightmap] = &bctx->main_pass,
		[QFV_bspShadow] = &bctx->shadow_pass,
		[QFV_bspDebug] = &bctx->debug_pass,
	} [pass_ind];
	if (!pass->draw_queues[queue].size) {
		return;
	}

	auto bframe = &bctx->frames.a[ctx->curFrame];
	VkBuffer    buffers[] = { bctx->default_verts, bctx->entid_buffer };
	if (bctx->vertex_buffer) {
		buffers[0] = bctx->vertex_buffer;
	}
	VkDeviceSize offsets[] = { 0, bframe->entid_offset };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 2, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, bctx->index_buffer, 0,
								 VK_INDEX_TYPE_UINT32);

	if (matrix_base) {
		VkDescriptorSet sets[] = {
			Vulkan_Lighting_Descriptors (ctx, ctx->curFrame),
			Vulkan_Scene_Descriptors (ctx),
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2, sets, 0, 0);
	} else {
		VkDescriptorSet sets[] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			Vulkan_Scene_Descriptors (ctx),
			Vulkan_Translucent_Descriptors (ctx, ctx->curFrame),
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 3, sets, 0, 0);
	}

	push_bspconst (matrix_base, queue, pipeline, cmd, bctx);

	if (queue == QFV_bspSky || queue == QFV_bspBackground) {
		vulktex_t skybox = { .descriptor = bctx->skybox_descriptor };
		bind_texture (&skybox, SKYBOX_SET, layout, dfunc, cmd);
		vulktex_t skymap = { .descriptor = bctx->skymap_descriptor };
		bind_texture (&skymap, SKYMAP_SET, layout, dfunc, cmd);
	} else if (pass_ind == QFV_bspLightmap) {
		vulktex_t lightmap = { .descriptor = bctx->lightmap_descriptor };
		bind_texture (&lightmap, LIGHTMAP_SET, layout, dfunc, cmd);
	}

	pass->textures = textured ? &bctx->registered_textures : 0;
	taskctx->subpass->call_count += pass->draw_queues[queue].size;
	draw_queue (pass, queue, layout, device, cmd, bctx);
}

static void
bsp_visit_world (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pass_ind = *(QFV_BspPass *) params[0]->value;
	auto frame = &bctx->frames.a[ctx->curFrame];

	auto pass = (bsp_pass_t *[]) {
		[QFV_bspMain] = &bctx->main_pass,
		[QFV_bspLightmap] = &bctx->main_pass,
		[QFV_bspShadow] = &bctx->shadow_pass,
		[QFV_bspDebug] = &bctx->debug_pass,
	} [pass_ind];

	pass->brush = nullptr;
	if (r_refdef.worldmodel) {
		pass->brush = r_refdef.worldmodel->brush;
	}
	if (pass_ind == QFV_bspMain || pass_ind == QFV_bspLightmap) {
		pass->entqueue = Vulkan_Scene_EntQueue (ctx);
		pass->position = r_refdef.frame.position;
		if (pass->brush) {
			pass->visstate = *pass->brush->visstate;
		}
	}

	if (pass->entqueue) {
		EntQueue_Clear (pass->entqueue);
	}
	if (!pass->brush) {
		return;
	}
	auto brush = pass->brush;

	entity_t    worldent = nullentity;
	int         world_id = Vulkan_Scene_AddEntity (ctx, worldent);
	pass->ent_frame = 0;    // world is always frame 0
	pass->inst_id = world_id;
	if (pass->instances) {
		DARRAY_APPEND (&pass->instances[world_id].entities, world_id);
	}

	//FIXME r_refdef ref. scene from taskctx?
	auto scene = r_refdef.scene;
	uint32_t view_leafnum = scene->view_leafnum;
	uint32_t cluster_num = brush->cluster_map[view_leafnum];

	set_t pvs = SET_STATIC_INIT (brush->cluster_vis.count, alloca);
	uint32_t offset = cluster_num ? brush->cluster_offs[cluster_num] : ~0u;
	Mod_LeafPVS_set (offset, &brush->cluster_vis, 0xff, &pvs);
	unsigned count = set_count (&pvs);
	if (!count) {
		return;
	}
	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.queue");
	uint32_t *queue = QFV_PacketExtend (packet, sizeof (uint32_t[count]));
	for (auto c = set_first (&pvs); c; c = set_next (c)) {
		uint32_t cluster = c->element + 1;
		R_StoreEfrags (scene, cluster);
		*queue++ = cluster;
	}
	QFV_PacketCopyBuffer (packet, bctx->queue_buffer, frame->queue,
						  &bufferBarriers[qfv_BB_ShaderRO_to_TransferWrite],
						  &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketSubmit (packet);

	*bctx->cluster_queue_ptr = bctx->queue_buffer_addr + frame->queue;
	*bctx->cluster_count = count;

	auto pipeline = taskctx->pipeline;
	if (pipeline) {
		pipeline->dispatch[0] = RUP (count, workgoup_size);
		pipeline->dispatch[1] = 1;
		pipeline->dispatch[2] = 1;

		pipeline->pre_memory_barrier = true;
		pipeline->post_memory_barrier = true;
		pipeline->pre_mb = (VkMemoryBarrier2) {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
						   | VK_ACCESS_2_SHADER_READ_BIT,
		};
		pipeline->post_mb = (VkMemoryBarrier2) {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
						   | VK_ACCESS_2_SHADER_READ_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
		};
	}

	bsp_flush (ctx);
}

static void
bsp_build_lightmaps (const exprval_t **params, exprval_t *result,
					 exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto scene = (scene_t *) taskctx->data;

	Vulkan_BuildLightmaps (scene->models, scene->num_models, ctx);
}

static void
clear_queues (bspctx_t *bctx, bsp_pass_t *pass)
{
	qfZoneScoped (true);
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		DARRAY_RESIZE (&pass->face_queue[i], 0);
	}
	for (int i = 0; i < pass->num_queues; i++) {
		DARRAY_RESIZE (&pass->draw_queues[i], 0);
	}
	for (int i = 0; i < bctx->num_models; i++) {
		pass->instances[i].first_instance = -1;
		DARRAY_RESIZE (&pass->instances[i].entities, 0);
	}
	pass->index_count = 0;
}

static void
bsp_build_display_lists (const exprval_t **params, exprval_t *result,
						 exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto scene = (scene_t *) taskctx->data;

	clear_queues (bctx, &bctx->main_pass);
	clear_queues (bctx, &bctx->shadow_pass);
	clear_queues (bctx, &bctx->debug_pass);

	Vulkan_BuildDisplayLists (scene->models, scene->num_models, ctx);
}

static void
bsp_register_textures (const exprval_t **params, exprval_t *result,
					   exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto scene = (scene_t *) taskctx->data;

	Vulkan_RegisterTextures (scene->models, scene->num_models, ctx);
}

static void
bsp_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "bsp shutdown");

	auto device = ctx->device;
	auto bctx = ctx->bsp_context;

	bctx->main_pass.entqueue = 0;	// owned by the scene
	shutdown_pass_draw_queues (&bctx->main_pass);
	shutdown_pass_draw_queues (&bctx->shadow_pass);
	shutdown_pass_draw_queues (&bctx->debug_pass);

	clear_textures (ctx);
	DARRAY_CLEAR (&bctx->registered_textures);

	free (bctx->models);

	shutdown_pass_instances (&bctx->main_pass, bctx);
	shutdown_pass_instances (&bctx->shadow_pass, bctx);
	shutdown_pass_instances (&bctx->debug_pass, bctx);

	free (bctx->frames.a);

	QFV_DestroyScrap (bctx->light_scrap);

	if (bctx->base_resource) {
		QFV_DestroyResource (device, bctx->base_resource);
	}
	if (bctx->bsp_resource) {
		QFV_DestroyResource (device, bctx->bsp_resource);
	}

	if (bctx->skybox_tex) {
		Vulkan_UnloadTex (ctx, bctx->skybox_tex);
	}
	if (bctx->skymap_tex) {
		Vulkan_UnloadTex (ctx, bctx->skymap_tex);
	}
	free (bctx);
	qfvPopDebug (ctx);
}

static void
bsp_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "bsp startup");
	auto bctx = ctx->bsp_context;

	auto device = ctx->device;

	bctx->main_pass.bsp_context = bctx;
	bctx->shadow_pass.bsp_context = bctx;
	bctx->shadow_pass.entqueue = EntQueue_New (mod_num_types);
	bctx->debug_pass.bsp_context = bctx;
	bctx->debug_pass.entqueue = EntQueue_New (mod_num_types);

	bctx->sampler = QFV_Render_Sampler (ctx, "quakebsp_sampler");
	bctx->equrect = QFV_Render_Sampler (ctx, "equirectangular_sampler");

	bctx->light_scrap = QFV_CreateScrap (device, "lightmap_atlas", 4096,
										 tex_frgba, ctx->staging);

	DARRAY_INIT (&bctx->registered_textures, 64);

	setup_pass_draw_queues (&bctx->main_pass);
	setup_pass_draw_queues (&bctx->shadow_pass);
	setup_pass_draw_queues (&bctx->debug_pass);

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&bctx->frames, frames);
	DARRAY_RESIZE (&bctx->frames, frames);
	bctx->frames.grow = 0;

	create_base_resources (ctx);

	bctx->lightmap_descriptor
		= Vulkan_CreateCombinedImageSampler (ctx,
											 Vulkan_LightmapImageView (ctx),
											 bctx->sampler);
	bctx->skybox_descriptor
		= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skybox,
											 bctx->sampler);
	bctx->skymap_descriptor
		= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skymap,
											 bctx->equrect);
	bctx->notexture_descriptor
		= Vulkan_CreateCombinedImageSampler (ctx, bctx->notexture,
											 bctx->sampler);

	qfvPopDebug (ctx);
}

static void
bsp_clearstate (exprctx_t *ectx)
{
	qfZoneScoped (true);
	//auto taskctx = (qfv_taskctx_t *) ectx;
	//auto ctx = taskctx->ctx;
	//auto bctx = ctx->bsp_context;
}

static void
bsp_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, bsp_shutdown);
	QFV_Render_AddStartup (ctx, bsp_startup);
	QFV_Render_AddClearState (ctx, bsp_clearstate);

	bspctx_t   *bctx = calloc (1, sizeof (bspctx_t));
	ctx->bsp_context = bctx;
	bctx->vulkan_ctx = ctx;
	*bctx = (bspctx_t) {
		.vulkan_ctx = ctx,

		.skybox_id   = nullent,
		.skymap_id   = nullent,

		.command_counts_ptr  = QFV_GetBlackboardVar (ctx, "command_counts"),
		.command_offsets_ptr = QFV_GetBlackboardVar (ctx, "command_offsets"),
		.commands_ptr        = QFV_GetBlackboardVar (ctx, "commands"),
		.subclusters_ptr     = QFV_GetBlackboardVar (ctx, "subclusters"),
		.clusters_ptr        = QFV_GetBlackboardVar (ctx, "clusters"),
		.cluster_map_ptr     = QFV_GetBlackboardVar (ctx, "cluster_map"),
		.cluster_queue_ptr   = QFV_GetBlackboardVar (ctx, "cluster_queue"),
		.cluster_count       = QFV_GetBlackboardVar (ctx, "cluster_count"),
		.texture_count       = QFV_GetBlackboardVar (ctx, "texture_count"),
		.matrix_base = QFV_GetBlackboardVar (ctx, "MatrixBase"),
		.fog         = QFV_GetBlackboardVar (ctx, "fog"),
		.time        = QFV_GetBlackboardVar (ctx, "time"),
		.alpha       = QFV_GetBlackboardVar (ctx, "alpha"),
		.turb_scale  = QFV_GetBlackboardVar (ctx, "turb_scale"),
		.control     = QFV_GetBlackboardVar (ctx, "control"),
	};
}

static exprenum_t bsp_pass_enum;
static exprtype_t bsp_pass_type = {
	.name = "bsp_pass",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &bsp_pass_enum,
};
static int bsp_pass_values[] = {
	QFV_bspMain,
	QFV_bspLightmap,
	QFV_bspShadow,
	QFV_bspDebug,
};
static exprsym_t bsp_pass_symbols[] = {
	{"main", &bsp_pass_type, bsp_pass_values + 0},
	{"lightmap", &bsp_pass_type, bsp_pass_values + 1},
	{"shadow", &bsp_pass_type, bsp_pass_values + 2},
	{"debug", &bsp_pass_type, bsp_pass_values + 3},
	{}
};
static exprtab_t bsp_pass_symtab = { .symbols = bsp_pass_symbols };
static exprenum_t bsp_pass_enum = {
	.type = &bsp_pass_type,
	.symtab = &bsp_pass_symtab,
};

static exprenum_t bsp_queue_enum;
static exprtype_t bsp_queue_type = {
	.name = "bsp_queue",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &bsp_queue_enum,
};
static int bsp_queue_values[] = {
	QFV_bspSolid,
	QFV_bspBackground,
	QFV_bspSky,
	QFV_bspTrans,
	QFV_bspTurb,
};
static exprsym_t bsp_queue_symbols[] = {
	{"solid",       &bsp_queue_type, bsp_queue_values + 0},
	{"background",  &bsp_queue_type, bsp_queue_values + 1},
	{"sky",         &bsp_queue_type, bsp_queue_values + 2},
	{"translucent", &bsp_queue_type, bsp_queue_values + 3},
	{"turbulent",   &bsp_queue_type, bsp_queue_values + 4},
	{}
};
static exprtab_t bsp_queue_symtab = { .symbols = bsp_queue_symbols };
static exprenum_t bsp_queue_enum = {
	.type = &bsp_queue_type,
	.symtab = &bsp_queue_symtab,
};

static exprtype_t *bsp_visit_world_params[] = {
	&bsp_pass_type,
};

static exprtype_t *bsp_draw_queue_params[] = {
	&cexpr_int,
	&bsp_queue_type,
	&bsp_pass_type,
};

static exprfunc_t bsp_reset_queues_func[] = {
	{ .func = bsp_reset_queues },
	{}
};
static exprfunc_t bsp_clear_commands_func[] = {
	{ .func = bsp_clear_commands },
	{}
};
static exprfunc_t bsp_visit_world_func[] = {
	{ 0, 1, bsp_visit_world_params, bsp_visit_world },
	{}
};
static exprfunc_t bsp_draw_queue_func[] = {
	{ 0, 3, bsp_draw_queue_params, bsp_draw_queue },
	{}
};

static exprfunc_t bsp_build_lightmaps_func[] = {
	{ .func = bsp_build_lightmaps },
	{}
};
static exprfunc_t bsp_build_display_lists_func[] = {
	{ .func = bsp_build_display_lists },
	{}
};
static exprfunc_t bsp_register_textures_func[] = {
	{ .func = bsp_register_textures },
	{}
};

static exprfunc_t bsp_init_func[] = {
	{ .func = bsp_init },
	{}
};

static exprsym_t bsp_task_syms[] = {
	{ "bsp_reset_queues", &cexpr_function, bsp_reset_queues_func },
	{ "bsp_clear_commands", &cexpr_function, bsp_clear_commands_func },
	{ "bsp_visit_world", &cexpr_function, bsp_visit_world_func },
	{ "bsp_draw_queue", &cexpr_function, bsp_draw_queue_func },

	{ "bsp_build_lightmaps", &cexpr_function, bsp_build_lightmaps_func },
	{ "bsp_build_display_lists", &cexpr_function,
		bsp_build_display_lists_func },
	{ "bsp_register_textures", &cexpr_function, bsp_register_textures_func },
	{ "bsp_init", &cexpr_function, bsp_init_func },
	{}
};

void
Vulkan_Bsp_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, bsp_task_syms);
}

static void
set_sky_tex (vulkan_ctx_t *ctx, qfv_tex_t *sky_tex, VkDescriptorSet texture,
			 bool is_box)
{
	bspctx_t   *bctx = ctx->bsp_context;

	if (bctx->skybox_tex && bctx->skybox_id == nullent) {
		Vulkan_UnloadTex (ctx, bctx->skybox_tex);
		Vulkan_FreeTexture (ctx, bctx->skybox_descriptor);
	}
	bctx->skybox_tex = 0;
	bctx->skybox_id = nullent;
	if (bctx->skymap_tex && bctx->skymap_id == nullent) {
		Vulkan_UnloadTex (ctx, bctx->skymap_tex);
		Vulkan_FreeTexture (ctx, bctx->skymap_descriptor);
	}
	bctx->skymap_tex = 0;
	bctx->skymap_id = nullent;

	if (!sky_tex && !texture) {
		bctx->skybox_descriptor
			= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skybox,
												 bctx->sampler);
		bctx->skymap_descriptor
			= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skymap,
												 bctx->equrect);
		return;
	}
	if (is_box) {
		bctx->skybox_tex = sky_tex;
		if (sky_tex) {
			bctx->skybox_descriptor
				= Vulkan_CreateTextureDescriptor (ctx, sky_tex, bctx->sampler);
		} else {
			bctx->skybox_descriptor = texture;
		}
		bctx->skymap_descriptor
			= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skymap,
												 bctx->equrect);
	} else {
		bctx->skymap_tex = sky_tex;
		bctx->skybox_descriptor
			= Vulkan_CreateCombinedImageSampler (ctx, bctx->default_skybox,
												 bctx->sampler);
		if (sky_tex) {
			bctx->skymap_descriptor
				= Vulkan_CreateTextureDescriptor (ctx, sky_tex, bctx->equrect);
		} else {
			bctx->skymap_descriptor = texture;
		}
	}
}

void
Vulkan_SetSkyId (uint32_t id, struct vulkan_ctx_s *ctx)
{
	if (id == nullent) {
		set_sky_tex (ctx, nullptr, nullptr, false);
	} else {
		auto bctx = ctx->bsp_context;
		auto texture = QFV_GetTexture (ctx, id);
		bool is_box = QFV_TexIsCubemap (ctx, id);
		set_sky_tex (ctx, nullptr, texture, is_box);
		if (is_box) {
			bctx->skybox_id = id;
		} else {
			bctx->skymap_id = id;
		}
	}
}

void
Vulkan_LoadSkys (const char *sky, vulkan_ctx_t *ctx)
{
	const char *name;
	int         i;
	tex_t      *tex;
	static const char *sky_suffix[] = { "ft", "bk", "up", "dn", "rt", "lf"};

	if (!sky || !*sky) {
		sky = r_skyname;
	}

	if (!*sky || !strcasecmp (sky, "none")) {
		Sys_MaskPrintf (SYS_vulkan, "Skybox unloaded\n");
		set_sky_tex (ctx, nullptr, 0, false);
		return;
	}

	name = vac (ctx->va_ctx, "env/%s_map", sky);
	tex = LoadImage (name, 1, r_refdef.hunk);
	qfv_tex_t *sky_tex = nullptr;
	bool is_box = true;
	if (tex) {
		// if the tex is 2:1, it's an equirectangular map
		is_box = tex->height * 2 != tex->width;
		sky_tex = Vulkan_LoadEnvMap (ctx, tex, sky);
		Sys_MaskPrintf (SYS_vulkan, "Loaded %s\n", name);
	} else {
		// always a box
		int         failed = 0;
		tex_t      *sides[6] = { };

		for (i = 0; i < 6; i++) {
			name = vac (ctx->va_ctx, "env/%s%s", sky, sky_suffix[i]);
			tex = LoadImage (name, 1, r_refdef.hunk);
			if (!tex) {
				Sys_MaskPrintf (SYS_vulkan, "Couldn't load %s\n", name);
				// also look in gfx/env, where Darkplaces looks for skies
				name = vac (ctx->va_ctx, "gfx/env/%s%s", sky, sky_suffix[i]);
				tex = LoadImage (name, 1, r_refdef.hunk);
				if (!tex) {
					Sys_MaskPrintf (SYS_vulkan, "Couldn't load %s\n", name);
					failed = 1;
					continue;
				}
			}
			//FIXME find a better way (also, assumes data and struct together)
			sides[i] = malloc (ImageSize (tex, 1));
			memcpy (sides[i], tex, ImageSize (tex, 1));
			sides[i]->data = (byte *)(sides[i] + 1);
			Sys_MaskPrintf (SYS_vulkan, "Loaded %s\n", name);
		}
		if (!failed) {
			sky_tex = Vulkan_LoadEnvSides (ctx, sides, sky);
		}
		for (i = 0; i < 6; i++) {
			free (sides[i]);
		}
	}

	set_sky_tex (ctx, sky_tex, 0, is_box);
	if (sky_tex) {
		Sys_MaskPrintf (SYS_vulkan, "Sky %s loaded\n", sky);
	}
}

bsp_pass_t *
Vulkan_Bsp_GetPass (struct vulkan_ctx_s *ctx, QFV_BspPass pass_ind)
{
	auto bctx = ctx->bsp_context;
	if (!r_refdef.worldmodel) {
		return 0;
	}
	auto pass = (bsp_pass_t *[]) {
		[QFV_bspMain] = &bctx->main_pass,
		[QFV_bspLightmap] = &bctx->main_pass,
		[QFV_bspShadow] = &bctx->shadow_pass,
		[QFV_bspDebug] = &bctx->debug_pass,
	}[pass_ind];
	auto bframe = &bctx->frames.a[ctx->curFrame];
	pass->entid_data = bframe->entid_data;
	pass->entid_count = bframe->entid_count;
	pass->brush = r_refdef.worldmodel->brush;

	return pass;
}
