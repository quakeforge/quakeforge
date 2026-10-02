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
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "QF/simd/types.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#include "shader/bsp.h"
static_assert (sizeof (bsp_command_t) == sizeof (VkDrawIndexedIndirectCommand));
#define block_size (workgroup_size * 2)

#define TEX_SET 3
#define SKYBOX_SET 4
#define SKYMAP_SET 5
#define LIGHTMAP_SET 4

static void
add_texture (texture_t *tx, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;

	vulktex_t  *tex = tx->render;
	if (tex->view) {
		tex->tex_id = bctx->registered_textures.size;
		tex->name = tx->name;
		DARRAY_APPEND (&bctx->registered_textures, tex);
		tex->descriptor = Vulkan_CreateCombinedImageSampler (ctx, tex->view,
															 bctx->sampler);
		QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 tex->descriptor,
							 vac (ctx->va_ctx, "dset:%s", tx->name));
		QFV_BspQueue dq = QFV_bspSolid;
		if (tx->flags & SURF_DRAWBACKGROUND) {
			dq = QFV_bspBackground;
		}
		if (tx->flags & SURF_DRAWSKY) {
			dq = QFV_bspSky;
		}
		if (tx->flags & SURF_DRAWALPHA) {
			dq = QFV_bspTrans;
		}
		if (tx->flags & SURF_DRAWTURB) {
			dq = QFV_bspTurb;
		}
		set_add (&bctx->main_pass.tex_set[dq], tex->tex_id);
		if (dq != QFV_bspTrans) {
			set_add (&bctx->shadow_pass.tex_set[dq], tex->tex_id);
		}
		set_add (&bctx->debug_pass.tex_set[dq], tex->tex_id);
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
	for (int i = 0; i < QFV_bspNumPasses; i++) {
		set_empty (&pass->tex_set[i]);
	}
}

static void
shutdown_pass_draw_queues (bsp_pass_t *pass)
{
	EntQueue_Delete (pass->entqueue);
	for (int i = 0; i < QFV_bspNumPasses; i++) {
		auto set = &pass->tex_set[i];
		if (set->map != set->defmap) {
			free (set->map);
		}
	}
	free (pass->tex_set);
}

static void
setup_pass_draw_queues (bsp_pass_t *pass)
{
	pass->tex_set = malloc (sizeof (set_t[QFV_bspNumPasses]));
	for (int i = 0; i < QFV_bspNumPasses; i++) {
		pass->tex_set[i] = SET_STATIC_DEFAULT (pass->tex_set[i]);
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
	texture_t *base_tx[] = {
		&bctx->notexture_tex,
		&bctx->background_tex,
	};
	for (size_t i = 0; i < countof (base_tx); i++) {
		add_texture (base_tx[i], ctx);
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

	set_assign (&bctx->main_pass.tex_set[QFV_bspTransEnt],
				&bctx->main_pass.tex_set[QFV_bspSolid]);
	//Textures that normally go into Trans will wind up in TransEnt if the
	//entity's alpha is reduced
	set_union (&bctx->main_pass.tex_set[QFV_bspTransEnt],
			   &bctx->main_pass.tex_set[QFV_bspTrans]);

	uint32_t    num_tex = bctx->registered_textures.size;

	texture_t *textures[num_tex];
	for (uint32_t i = 0; i < countof (base_tx); i++) {
		textures[i] = base_tx[i];
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

	size_t texdata_size = sizeof (bsp_texanim_t[num_tex])
						+ sizeof (bsp_texanim_t[num_tex])
						+ sizeof (uint16_t[num_tex]);
	bsp_texanim_t *texdata = Hunk_AllocName (r_refdef.hunk, texdata_size,
											 "texdata");
	bctx->texdata.anim_main = texdata;
	bctx->texdata.anim_alt = texdata + num_tex;
	bctx->texdata.frame_map = (uint16_t *) (texdata + 2 * num_tex);
	int16_t     map_index = 0;
	for (uint32_t i = 0; i < num_tex; i++) {
		auto anim = bctx->texdata.anim_main + i;
		if (anim->count) {
			// already done as part of an animation group
			continue;
		}
		*anim = (bsp_texanim_t) { .base = map_index, .offset = 0, .count = 1 };
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
				auto a = bctx->texdata.anim_main + vtex->tex_id;
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
	for (uint32_t i = 0; i < num_tex; i++) {
		auto alt = bctx->texdata.anim_alt + i;
		if (textures[i]->alternate_anims) {
			texture_t  *tx = textures[i]->alternate_anims;
			vulktex_t  *vtex = tx->render;
			*alt = bctx->texdata.anim_main[vtex->tex_id];
		} else {
			*alt = bctx->texdata.anim_main[i];
		}
	}

	if (num_tex > bctx->num_tex_anim) {
		QFV_DestroyResource (ctx->device, bctx->tex_resource);
		bctx->num_tex_anim = num_tex;
		if (!bctx->tex_resource) {
			size_t size = sizeof (qfv_resource_t)
						+ sizeof (qfv_resobj_t)		// anim_main
						+ sizeof (qfv_resobj_t)		// anim_alt
						+ sizeof (qfv_resobj_t); 	// frame_map
			bctx->tex_resource = malloc (size);
			*bctx->tex_resource = (qfv_resource_t) {
				.name = "bsp:tex",
				.va_ctx = ctx->va_ctx,
				.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				.num_objects = 3,
				.objects = (qfv_resobj_t *)&bctx->tex_resource[1],
			};
		}
		auto anim_main = (qfv_resobj_t *) &bctx->tex_resource[1];
		auto anim_alt  = &anim_main[1];
		auto frame_map = &anim_alt[1];
		*anim_main = (qfv_resobj_t) {
			.name = "anim_main",
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (bsp_texanim_t[num_tex]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					   | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*anim_alt = (qfv_resobj_t) {
			.name = "anim_alt",
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (bsp_texanim_t[num_tex]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					   | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*frame_map = (qfv_resobj_t) {
			.name = "frame_map",
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint16_t[num_tex]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					   | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		QFV_CreateResource (ctx->device, bctx->tex_resource);
		*bctx->anim_main  = anim_main->buffer.address;
		*bctx->anim_alt   = anim_alt->buffer.address;
		*bctx->frame_map  = frame_map->buffer.address;
	}

	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.tex");
	auto data = QFV_PacketExtend (packet, texdata_size);
	memcpy (data, bctx->texdata.anim_main, texdata_size);
	auto anim_main = (bsp_texanim_t *) data;
	auto anim_alt  = &anim_main[num_tex];
	auto frame_map = (uint16_t *) &anim_alt[num_tex];

	qfv_scatter_t main_scatter = {
		.srcOffset = QFV_PacketOffset (packet, anim_main),
		.length = sizeof (bsp_texanim_t[num_tex]),
	};
	qfv_scatter_t alt_scatter = {
		.srcOffset = QFV_PacketOffset (packet, anim_alt),
		.length = sizeof (bsp_texanim_t[num_tex]),
	};
	qfv_scatter_t map_scatter = {
		.srcOffset = QFV_PacketOffset (packet, frame_map),
		.length = sizeof (uint16_t[num_tex]),
	};
	auto anim_main_buffer = bctx->tex_resource->objects[0].buffer.buffer;
	auto anim_alt_buffer  = bctx->tex_resource->objects[1].buffer.buffer;
	auto frame_map_buffer = bctx->tex_resource->objects[2].buffer.buffer;
	QFV_PacketScatterBuffer (packet, anim_main_buffer, 1, &main_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketScatterBuffer (packet, anim_alt_buffer, 1, &alt_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketScatterBuffer (packet, frame_map_buffer, 1, &map_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);

	QFV_PacketSubmit (packet);
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
	set_t      *seen_tex_ids;
	set_t      *anim_tex_ids;
	uint32_t    num_tex_ids;
	uint32_t   *tex_clusters;
	uint32_t   *tex_commands;
	uint32_t    mod_cluster_count;
	uint32_t    vert_count;
	uint32_t    ind_count;
	bspvert_t  *vertices;
	uint32_t   *indices;
	bsp_model_t *models;
	uint32_t   *tex_ids;
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
visit_animations (uint32_t tex_id, buildctx_t *build)
{
	auto anim_tex_ids = build->anim_tex_ids;
	auto texdata = build->bctx->texdata;
	set_empty (anim_tex_ids);
	set_add (anim_tex_ids, tex_id);

#define scan_anim(a) \
	do { \
		auto anim = a; \
		for (uint32_t i = 0; i < anim.count; i++) { \
			uint32_t frame = anim.base + i; \
			uint32_t id = texdata.frame_map[frame]; \
			set_add (anim_tex_ids, id); \
		} \
	} while (false)
	scan_anim (texdata.anim_main[tex_id]);
	scan_anim (texdata.anim_alt[tex_id]);
#undef scan_anim
}

static void
scan_surfaces (mod_brush_t *brush, cluster_t *cluster, buildctx_t *build)
{
	texture_t *cur_tex = nullptr;
	auto anim_tex_ids = build->anim_tex_ids;
	build->mod_cluster_count++;
	for (uint32_t i = 0; i < cluster->count; i++) {
		uint32_t ind = cluster->first + i;
		auto surf = brush->surfaces + brush->cluster_surfs[ind];
		if (cur_tex != surf->texinfo->texture) {
			cur_tex = surf->texinfo->texture;
			uint32_t tex_id = surf_tex_id (surf, build->bctx);
			visit_animations (tex_id, build);
			for (auto t = set_first (anim_tex_ids); t; t = set_next (t)) {
				build->tex_commands[t->element]++;
			}
			build->tex_clusters[tex_id]++;
			set_add (build->seen_tex_ids, tex_id);
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
	set_empty (build->seen_tex_ids);
	for (uint32_t j = 0; j < num_clusters; j++) {
		auto cluster = &brush->clusters[j];
		scan_surfaces (brush, cluster, build);
	}
	build->num_tex_ids += set_count (build->seen_tex_ids);
	for (uint32_t j = 1; j < brush->numsubmodels; j++) {
		set_empty (build->seen_tex_ids);
		auto cluster = &brush->clusters[num_clusters + j - 1];
		scan_surfaces (brush, cluster, build);
		build->num_tex_ids += set_count (build->seen_tex_ids);
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
			if (!set_is_member (build->seen_tex_ids, tex_id)) {
				build->tex_ids[build->num_tex_ids++] = tex_id;
				set_add (build->seen_tex_ids, tex_id);
			}
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
	build->models[m->render_id] = (bsp_model_t) {
		.first_cluster = build->cluster_base,
		.cluster_count = num_clusters,
		.first_texture = build->num_tex_ids,
	};
	set_empty (build->seen_tex_ids);
	for (uint32_t j = 0; j < num_clusters; j++) {
		auto cluster = &brush->clusters[j];
		build_surfaces (brush, cluster, build);
	}
	build->models[m->render_id].texture_count = set_count (build->seen_tex_ids);
	for (uint32_t j = 1; j < brush->numsubmodels; j++) {
		auto mod = &build->models[m->render_id + j];
		*mod = (bsp_model_t) {
			.first_cluster = build->cluster_base,
			.cluster_count = 1,
			.first_texture = build->num_tex_ids,
		};
		set_empty (build->seen_tex_ids);
		auto cluster = &brush->clusters[num_clusters + j - 1];
		build_surfaces (brush, cluster, build);
		mod->texture_count = set_count (build->seen_tex_ids);
	}
}

static void
check_cluster (const buildctx_t *build, uint32_t cluster_num)
{
	auto brush = r_refdef.worldmodel->brush;
	SET_DEFER (pvs);
	uint32_t offset = cluster_num ? brush->cluster_offs[cluster_num] : ~0u;
	Mod_LeafPVS_set (offset, &brush->cluster_vis, 0xff, pvs);
	unsigned count = set_count (pvs);
	printf (ONG"%d %d"DFL"\n", cluster_num, count);
	count = 0;
	for (auto ci = set_first (pvs); ci; ci = set_next (ci)) {
		auto cluster = build->clusters[ci->element + 1];
		int has_9 = 0;
		for (uint32_t i = 0; i < cluster.count; i++) {
			auto sc = build->subclusters[build->clustermap[cluster.first + i]];
			if (sc.tex_id == 9) {
				//printf ("      %d %d %d %d\n", cluster.first + i,
				//		build->clustermap[cluster.first + i],
				//		sc.first_index, sc.index_count);
				count++;
				has_9++;
			}
		}
		if (has_9) printf ("%d %d %d %d\n", ci->element + 1, cluster.first, cluster.count, has_9);
	}
	printf ("    %d\n", count);
}

static uint32_t
bsp_prefixsum (uint32_t *array, uint32_t count)
{
	uint32_t sum = 0;
	for (uint32_t i = 1; i < count; i++) {
		uint32_t temp = array[i];
		array[i] = sum;
		sum += temp;
	}
	return sum;
}

void
Vulkan_BuildDisplayLists (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	//qfv_device_t *device = ctx->device;
	bspctx_t   *bctx = ctx->bsp_context;
	visstate_t *visstate = nullptr;

	*bctx->num_models = 0;
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		if (m && m->type == mod_brush) {
			m->render_id = (*bctx->num_models)++;
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
	uint32_t tex_commands[num_tex] = {};

	SET_DEFER (seen_tex_ids);
	SET_DEFER (anim_tex_ids);
	buildctx_t build = {
		.bctx = bctx,
		.seen_tex_ids = seen_tex_ids,
		.anim_tex_ids = anim_tex_ids,
		.tex_clusters = tex_clusters,
		.tex_commands = tex_commands,
	};

	// count sub-clusters, vertices and indices
	model_loop (models, num_models, ctx, model_scan_surfaces, &build);

	free (bctx->command_offsets);
	free (bctx->command_counts);
	bctx->command_offsets = malloc (2 * sizeof (tex_commands));
	bctx->command_counts = malloc (sizeof (tex_commands));
	memcpy (bctx->command_counts, tex_commands, sizeof (tex_commands));

	uint32_t num_commands = bsp_prefixsum (tex_commands, num_tex);
	memcpy (bctx->command_offsets, tex_commands, sizeof (tex_commands));
	for (uint32_t i = 0; i < num_tex; i++) {
		auto dst = &bctx->command_offsets[i + num_tex];
		auto src = &bctx->command_offsets[i];
		*dst = *src + num_commands;
	}

	uint32_t num_clusters = bsp_prefixsum (tex_clusters, num_tex);
	uint32_t mod_clusters = build.mod_cluster_count;

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
	uint32_t mod_count = *bctx->num_models;
	size_t index_buffer_size = sizeof (uint32_t[index_count]);
	size_t model_buffer_size = sizeof (bsp_model_t[mod_count]);
	size_t mod_counts_buffer_size = sizeof (uint32_t[mod_queues * mod_count]);
	size_t mod_tmp_buffer_size = sizeof (uint32_t[mod_queues * mod_count]);
	size_t mod_sums_buffer_size = sizeof (uint32_t[1024+1]);
	size_t mod_offsets_buffer_size = sizeof (uint32_t[mod_queues * mod_count]);
	size_t tex_id_buffer_size = sizeof (uint32_t[build.num_tex_ids]);
	size_t vertex_buffer_size = sizeof (bspvert_t[vertex_count]);
	size_t subcluster_buffer_size = sizeof (bsp_cluster_t[num_clusters]);
	size_t cluster_buffer_size = sizeof (cluster_t[mod_clusters]);
	size_t clustermap_buffer_size = sizeof (uint32_t[num_clusters]);
	size_t instance_queue_buffer_size = sizeof (bsp_cluster_t[num_clusters])
									  + sizeof (uint32_t[4]);//count+ind disp
	// 4 for count + indirect dispatch
	size_t cluster_queue_buffer_size = sizeof (uint32_t[4 + num_clusters]);
	size_t prefixsum_counts_buffer_size = sizeof (uint32_t[4]);
	// two sets of commands: first for solid entities, second for transparent
	size_t command_buffer_size = 2 * sizeof (bsp_command_t[num_commands]);
	size_t command_counts_buffer_size = 2 * sizeof (tex_commands);
	size_t command_offsets_buffer_size = 2 * sizeof (tex_commands);

#define CHECK_SIZE(b) (b##_buffer_size > bctx->b##_buffer.size)
	if (CHECK_SIZE (index)
		|| CHECK_SIZE (model)
		|| CHECK_SIZE (mod_counts)
		|| CHECK_SIZE (mod_tmp)
		|| CHECK_SIZE (mod_sums)
		|| CHECK_SIZE (mod_offsets)
		|| CHECK_SIZE (tex_id)
		|| CHECK_SIZE (vertex)
		|| CHECK_SIZE (command_counts)
		|| CHECK_SIZE (command_offsets)
		|| CHECK_SIZE (command)
		|| CHECK_SIZE (subcluster)
		|| CHECK_SIZE (cluster)
		|| CHECK_SIZE (clustermap)
		|| CHECK_SIZE (instance_queue)
		|| CHECK_SIZE (cluster_queue)
		|| CHECK_SIZE (prefixsum_counts)) {
		QFV_DestroyResource (ctx->device, bctx->bsp_resource);
		if (!bctx->bsp_resource) {
			size_t size = sizeof (qfv_resource_t)
						+ sizeof (qfv_resobj_t[1])	// model
						+ sizeof (qfv_resobj_t[1])	// mod_counts
						+ sizeof (qfv_resobj_t[1])	// mod_tmp
						+ sizeof (qfv_resobj_t[1])	// mod_sums
						+ sizeof (qfv_resobj_t[1])	// mod_offsets
						+ sizeof (qfv_resobj_t[1])	// tex_id
						+ sizeof (qfv_resobj_t[1])	// vertex
						+ sizeof (qfv_resobj_t[1])	// index
						+ sizeof (qfv_resobj_t[1]) 	// command countss
						+ sizeof (qfv_resobj_t[1]) 	// command offsets
						+ sizeof (qfv_resobj_t[1]) 	// command
						+ sizeof (qfv_resobj_t[1])	// subcluster
						+ sizeof (qfv_resobj_t[1])	// cluster
						+ sizeof (qfv_resobj_t[1])	// clustermap
						+ sizeof (qfv_resobj_t[1])	// instance_queue
						+ sizeof (qfv_resobj_t[1])	// cluster_queue
						+ sizeof (qfv_resobj_t[1]);	// prefixsum_counts
			bctx->bsp_resource = malloc (size);
		}
		auto model = (qfv_resobj_t *) &bctx->bsp_resource[1];
		auto mod_counts = &model[1];
		auto mod_tmp = &mod_counts[1];
		auto mod_sums = &mod_tmp[1];
		auto mod_offsets = &mod_sums[1];
		auto tex_id = (qfv_resobj_t *) &mod_offsets[1];
		auto vertex = (qfv_resobj_t *) &tex_id[1];
		auto index = (qfv_resobj_t *) &vertex[1];
		auto command_counts = (qfv_resobj_t *) &index[1];
		auto command_offsets = (qfv_resobj_t *) &command_counts[1];
		auto command = (qfv_resobj_t *) &command_offsets[1];
		auto subcluster = (qfv_resobj_t *) &command[1];
		auto cluster = (qfv_resobj_t *) &subcluster[1];
		auto clustermap = (qfv_resobj_t *) &cluster[1];
		auto instance_queue = (qfv_resobj_t *) &clustermap[1];
		auto cluster_queue = (qfv_resobj_t *) &instance_queue[1];
		auto prefixsum_counts = (qfv_resobj_t *) &cluster_queue[1];

		*bctx->bsp_resource = (qfv_resource_t) {
			.name = "bsp:mdl",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = 17,
			.objects = model,
		};
		*model = (qfv_resobj_t) {
			.name = "model",
			.type = qfv_res_buffer,
			.buffer = {
				.size = model_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*mod_counts = (qfv_resobj_t) {
			.name = "mod_counts",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mod_counts_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*mod_tmp = (qfv_resobj_t) {
			.name = "mod_tmp",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mod_tmp_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*mod_sums = (qfv_resobj_t) {
			.name = "mod_sums",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mod_sums_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*mod_offsets = (qfv_resobj_t) {
			.name = "mod_offsets",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mod_offsets_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*tex_id = (qfv_resobj_t) {
			.name = "tex_id",
			.type = qfv_res_buffer,
			.buffer = {
				.size = tex_id_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*vertex = (qfv_resobj_t) {
			.name = "vertex",
			.type = qfv_res_buffer,
			.buffer = {
				.size = vertex_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		*index = (qfv_resobj_t) {
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
		*command = (qfv_resobj_t) {
			.name = "command",
			.type = qfv_res_buffer,
			.buffer = {
				.size = command_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*subcluster = (qfv_resobj_t) {
			.name = "subcluster",
			.type = qfv_res_buffer,
			.buffer = {
				.size = subcluster_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*cluster = (qfv_resobj_t) {
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
		*instance_queue = (qfv_resobj_t) {
			.name = "instance_queue",
			.type = qfv_res_buffer,
			.buffer = {
				.size = instance_queue_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		size_t frames = bctx->frames.size;
		*cluster_queue = (qfv_resobj_t) {
			.name = "cluster_queue",
			.type = qfv_res_buffer,
			.buffer = {
				.size = cluster_queue_buffer_size * frames,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		*prefixsum_counts = (qfv_resobj_t) {
			.name = "prefixsum_counts",
			.type = qfv_res_buffer,
			.buffer = {
				.size = prefixsum_counts_buffer_size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			},
		};
		QFV_CreateResource (ctx->device, bctx->bsp_resource);

#define BUFFER(b) \
		bctx->b##_buffer = (bsp_buffer_t) { \
			.buffer = b->buffer.buffer, \
			.size = b->buffer.size, \
			.addr = b->buffer.address, \
		};
		BUFFER (model);
		BUFFER (mod_counts);
		BUFFER (mod_tmp);
		BUFFER (mod_sums);
		BUFFER (mod_offsets);
		BUFFER (tex_id);
		BUFFER (vertex);
		BUFFER (index);
		BUFFER (command_counts);
		BUFFER (command_offsets);
		BUFFER (command);
		BUFFER (subcluster);
		BUFFER (cluster);
		BUFFER (clustermap);
		BUFFER (instance_queue);
		BUFFER (cluster_queue);
		BUFFER (prefixsum_counts);

		*bctx->models = bctx->model_buffer.addr;
		*bctx->mod_counts = bctx->mod_counts_buffer.addr;
		*bctx->mod_offsets = bctx->mod_offsets_buffer.addr;
		*bctx->tex_ids = bctx->tex_id_buffer.addr;
		*bctx->command_counts_ptr = bctx->command_counts_buffer.addr;
		*bctx->command_offsets_ptr = bctx->command_offsets_buffer.addr;
		*bctx->commands_ptr = bctx->command_buffer.addr;
		*bctx->subclusters_ptr = bctx->subcluster_buffer.addr;
		*bctx->clusters_ptr = bctx->cluster_buffer.addr;
		*bctx->cluster_map_ptr = bctx->clustermap_buffer.addr;
		*bctx->instance_queue_ptr = bctx->instance_queue_buffer.addr;
		*bctx->prefixsum_counts = bctx->prefixsum_counts_buffer.addr;

		for (size_t i = 0; i < frames; i++) {
			bctx->frames.a[i].queue = cluster_queue_buffer_size * i;
		}
	}
	*bctx->texture_count = num_tex;

	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.build");
	size_t packet_size = model_buffer_size
					   + tex_id_buffer_size
					   + vertex_buffer_size
					   + index_buffer_size
					   + command_offsets_buffer_size
					   + subcluster_buffer_size
					   + cluster_buffer_size
					   + clustermap_buffer_size;
	build.models = (bsp_model_t *) QFV_PacketExtend (packet, packet_size);
	build.tex_ids = (uint32_t *) &build.models[*bctx->num_models];
	build.vertices = (bspvert_t *) &build.models[build.num_tex_ids];
	build.indices = (uint32_t *) &build.vertices[vertex_count];
	uint32_t *command_offsets = (uint32_t *) &build.indices[index_count];
	build.subclusters = (bsp_cluster_t *) &command_offsets[2 * num_tex];
	build.clusters = (cluster_t *) &build.subclusters[num_clusters];
	build.clustermap = (uint32_t *) &build.clusters[mod_clusters];

	memcpy (command_offsets, bctx->command_offsets,
			command_offsets_buffer_size);
	build.num_tex_ids = 0;
	model_loop (models, num_models, ctx, model_build_surfaces, &build);

	if(0)check_cluster (&build, 2168);

	qfv_scatter_t mod_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.models),
		.length = model_buffer_size,
	};
	qfv_scatter_t tex_scatter = {
		.srcOffset = QFV_PacketOffset (packet, build.tex_ids),
		.length = tex_id_buffer_size,
	};
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
	QFV_PacketScatterBuffer (packet, bctx->model_buffer.buffer,
			1, &mod_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketScatterBuffer (packet, bctx->tex_id_buffer.buffer,
			1, &tex_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketScatterBuffer (packet, bctx->vertex_buffer.buffer,
			1, &vert_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead]);
	QFV_PacketScatterBuffer (packet, bctx->index_buffer.buffer,
			1, &ind_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->command_offsets_buffer.buffer,
			1, &ofs_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->subcluster_buffer.buffer,
			1, &sub_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->cluster_buffer.buffer,
			1, &cls_scatter,
			&bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketScatterBuffer (packet, bctx->clustermap_buffer.buffer,
			1, &map_scatter,
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
trans_mem_barrier (qfv_devfuncs_t *dfunc, VkCommandBuffer cmd)
{
	static VkMemoryBarrier2 mb = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
	};
	static VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mb,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
						 | VK_DEPENDENCY_VIEW_LOCAL_BIT,
	};
	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);
}

static void
queue_mem_barrier (qfv_devfuncs_t *dfunc, VkCommandBuffer cmd, bool draw)
{
	static VkMemoryBarrier2 cmb = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
					   | VK_ACCESS_2_SHADER_WRITE_BIT,
	};
	static VkMemoryBarrier2 dmb = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
					  | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
					   | VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
	};
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = draw ? &dmb : &cmb,
	};
	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);
}

static void
draw_queue (bsp_pass_t *pass, QFV_BspQueue queue, VkPipelineLayout layout,
			qfv_device_t *device, VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	auto bctx = ctx->bsp_context;
	qfv_devfuncs_t *dfunc = device->funcs;

	//puts (set_as_string (&pass->tex_set[queue]));
	for (auto t = set_first (&pass->tex_set[queue]); t; t = set_next (t)) {
		uint32_t tex_id = t->element;
		vulktex_t *tex = bctx->registered_textures.a[tex_id];
		QFV_duCmdBeginLabel (device, cmd,
							 vac (ctx->va_ctx, "tex_id:%d %s",
								  tex_id, tex->name),
							 { 0.6, 0.6, 0.5, 1 });
		if (pass->textures) {
			vulktex_t  *tex = pass->textures->a[tex_id];
			bind_texture (tex, TEX_SET, layout, dfunc, cmd);
		}
		if (queue == QFV_bspTrans || queue == QFV_bspTurb
			|| queue == QFV_bspTransEnt) {
			trans_mem_barrier (dfunc, cmd);
		}
		uint32_t trans = 0;
		if (queue == QFV_bspTransEnt) {
			trans = *bctx->texture_count;
		}
		if (queue == QFV_bspBackground) {
			dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
		} else {
			size_t cmd_size = sizeof (VkDrawIndexedIndirectCommand);
			dfunc->vkCmdDrawIndexedIndirectCount (cmd,
					bctx->command_buffer.buffer,
					bctx->command_offsets[tex_id + trans] * cmd_size,
					bctx->command_counts_buffer.buffer,
					(tex_id + trans) * sizeof (uint32_t),
					bctx->command_counts[tex_id],
					cmd_size);
		}
		QFV_duCmdEndLabel(device, cmd);
	}
	if (queue == QFV_bspTrans || queue == QFV_bspTurb) {
		trans_mem_barrier (dfunc, cmd);
	}
}

static void
bsp_flush (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);

	Vulkan_Scene_Flush (ctx);
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
				+ sizeof (qfv_resobj_t[1])	//entid
				+ sizeof (qfv_resobj_t[1]);	//instid
	bctx->base_resource = malloc (size);
	auto images = (qfv_resobj_t *) &bctx->base_resource[1];
	auto views = (qfv_resobj_t *) &images[4];
	auto verts = (qfv_resobj_t *) &views[4];
	auto entid = (qfv_resobj_t *) &verts[1];
	auto instid = (qfv_resobj_t *) &entid[1];
	*bctx->base_resource = (qfv_resource_t) {
		.name = "bsp:dfl",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 4 + 4 + 1 + 1 + 1,
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
			.size = sizeof (uint32_t[entid_count * frames]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*instid = (qfv_resobj_t) {
		.name = "instid",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (uint32_t[entid_count]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};

	QFV_CreateResource (ctx->device, bctx->base_resource);

	bctx->notexture = views[0].image_view.view;
	bctx->default_skysheet = views[1].image_view.view;
	bctx->default_skybox = views[2].image_view.view;
	bctx->default_skymap = views[3].image_view.view;
	bctx->default_verts = verts->buffer.buffer;
	bctx->entid_buffer = (bsp_buffer_t) {
		.buffer = entid->buffer.buffer,
		.size = entid->buffer.size,
		.addr = entid->buffer.address,
	};
	bctx->instid_buffer = (bsp_buffer_t) {
		.buffer = instid->buffer.buffer,
		.size = instid->buffer.size,
		.addr = instid->buffer.address,
	};

	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.base");
	create_notexture (ctx, packet, &images[0]);
	create_default_skys (ctx, packet, &images[1]);
	QFV_PacketSubmit (packet);

	for (size_t i = 0; i < frames; i++) {
		auto bframe = &bctx->frames.a[i];
		bframe->entid_offset = i * entid_count;
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
bsp_count_ent (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;

	uint32_t count = *bctx->ent_count;

	pipeline->dispatch[0] = RUP (count, workgroup_size) / workgroup_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;

	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
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
bsp_clear_ent (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;

	uint32_t count = mod_queues * *bctx->num_models;

	pipeline->dispatch[0] = RUP (count, workgroup_size) / workgroup_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;

	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
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
bsp_queue_insts (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;

	if (!r_refdef.worldmodel) {
		pipeline->dispatch[0] = 0;
		return;
	}

	uint32_t count = *bctx->num_models;

	pipeline->dispatch[0] = RUP (count, workgroup_size) / workgroup_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;

	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
	};
	pipeline->post_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
	};
}

static void
bsp_distribute_insts (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;

	uint32_t count = *bctx->ent_count;

	pipeline->dispatch[0] = RUP (count, workgroup_size) / workgroup_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;

	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_SHADER_READ_BIT,
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

static void
bsp_queue_clusters (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;
	auto frame = &bctx->frames.a[ctx->curFrame];
	auto pipeline = taskctx->pipeline;

	if (!r_refdef.worldmodel) {
		return;
	}

	auto cmd = taskctx->cmd;
	QFV_PushBlackboard (ctx, cmd, pipeline);
	queue_mem_barrier (dfunc, cmd, false);
	dfunc->vkCmdDispatchIndirect (cmd, bctx->cluster_queue_buffer.buffer,
								  frame->queue + sizeof (uint32_t));
	queue_mem_barrier (dfunc, cmd, true);
}

static void
bsp_clear_commands (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	int  count = 2 * bctx->registered_textures.size;
	auto pipeline = taskctx->pipeline;
	pipeline->dispatch[0] = RUP (count, workgroup_size) / workgroup_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;
	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
					  | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT
					   | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
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
bsp_draw_barrier (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;

	if (!r_refdef.worldmodel) {
		return;
	}

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "cmd:bsp_draw_barrier:%zd",
							  ctx->frameNumber));
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	QFV_duCmdBeginLabel (device, cmd,
						 vac (ctx->va_ctx, "bsp_draw_barrier:%zd",
							  ctx->frameNumber),
						 { 1, 0.0, 1, 1 });

	VkBufferMemoryBarrier2 bb[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			.buffer = bctx->command_buffer.buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			.buffer = bctx->command_counts_buffer.buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	dfunc->vkCmdPipelineBarrier2 (cmd, &(VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.bufferMemoryBarrierCount = countof (bb), bb,
			});

	QFV_duCmdEndLabel(device, cmd);
	dfunc->vkEndCommandBuffer (cmd);

	QFV_AppendCmdBuffer (taskctx->job, cmd);
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

	if (queue != QFV_bspBackground && !bctx->vertex_buffer.buffer) {
		return;
	}

	auto pass = (bsp_pass_t *[]) {
		[QFV_bspMain] = &bctx->main_pass,
		[QFV_bspLightmap] = &bctx->main_pass,
		[QFV_bspShadow] = &bctx->shadow_pass,
		[QFV_bspDebug] = &bctx->debug_pass,
	} [pass_ind];

	VkBuffer    buffers[] = {
		bctx->default_verts,
		bctx->instid_buffer.buffer,
	};
	if (bctx->vertex_buffer.buffer) {
		buffers[0] = bctx->vertex_buffer.buffer;
	}
	VkDeviceSize offsets[] = { 0, 0 };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 2, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, bctx->index_buffer.buffer, 0,
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
	taskctx->subpass->call_count += set_count (&pass->tex_set[queue]);
	draw_queue (pass, queue, layout, device, cmd, ctx);
}

static void
bsp_sum_mod_insts (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;
	auto stage = *(int *) params[0]->value;

	*bctx->in_data = 0;
	*bctx->out_data = 0;
	*bctx->sum_data = 0;
	uint32_t count = *bctx->num_models * mod_queues;
	uint32_t sum_offset = 0;
	switch (stage) {
		case 0:
			*bctx->in_data = *bctx->mod_counts;
			*bctx->out_data = bctx->mod_tmp_buffer.addr;
			*bctx->sum_data = *bctx->mod_offsets;// tmp buffer for sums
			break;
		case 1:
			*bctx->in_data = *bctx->mod_offsets;
			*bctx->out_data = bctx->mod_sums_buffer.addr;
			*bctx->sum_data = *bctx->out_data + sizeof(uint32_t[1024]);
			count = RUP (count, block_size) / block_size;
			sum_offset = sizeof (uint32_t);
			break;
		case 2:
			*bctx->in_data = bctx->mod_tmp_buffer.addr;
			*bctx->out_data = *bctx->mod_offsets;
			*bctx->sum_data = bctx->mod_sums_buffer.addr;
			break;
		default:
			Sys_Error ("invalid bsp_sum_mod_insts stage: %d\n", stage);
	}
	*bctx->count = *bctx->prefixsum_counts + sum_offset;

	pipeline->dispatch[0] = RUP (count, block_size) / block_size;
	pipeline->dispatch[1] = 1;
	pipeline->dispatch[2] = 1;

	pipeline->pre_memory_barrier = true;
	pipeline->post_memory_barrier = true;
	pipeline->pre_mb = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT
					   | VK_ACCESS_2_SHADER_WRITE_BIT,
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

static bool
draw_brush_model (entity_t ent, bsp_pass_t *pass, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);

	int render_id = Vulkan_Scene_AddEntity (ctx, ent);
	if (render_id < 0) {
		return false;
	}
	return true;
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

	pass->entid_data = frame->entid_data;
	pass->entid_count = frame->entid_count;

	entity_t    worldent = nullentity;
	Vulkan_Scene_AddEntity (ctx, worldent);

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
	size_t packet_size = sizeof (uint32_t[4]) + sizeof (uint32_t[count]);
	uint32_t *queue = QFV_PacketExtend (packet, packet_size);
	*queue++ = count;
	*queue++ = RUP (count, workgroup_size) / workgroup_size;
	*queue++ = 1;
	*queue++ = 1;
	for (auto c = set_first (&pvs); c; c = set_next (c)) {
		uint32_t cluster = c->element + 1;
		R_StoreEfrags (scene, cluster);
		*queue++ = cluster;
	}
	QFV_PacketCopyBuffer (packet, bctx->cluster_queue_buffer.buffer,
						  frame->queue,
						  &bufferBarriers[qfv_BB_ShaderRW_to_TransferWrite],
						  &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRW]);
	QFV_PacketSubmit (packet);

	uint32_t ent_count = 0;
	if (r_drawentities && bctx->num_models) {
		auto entqueue = pass->entqueue;
		ent_count = entqueue->ent_queues[mod_brush].size;
		for (size_t i = 0; i < ent_count; i++) {
			entity_t    ent = entqueue->ent_queues[mod_brush].a[i];
			if (!draw_brush_model (ent, pass, ctx)) {
				Sys_Printf ("Too many entities!\n");
				break;
			}
		}
	}
	packet = QFV_PacketAcquire (ctx->staging, "bsp.ent_ids");
	auto entqueue = pass->entqueue;
	size_t size = sizeof (uint32_t[ent_count + 1]);
	uint32_t *ent_ids = QFV_PacketExtend (packet, size);
	*ent_ids = 0;
	for (uint32_t i = 0; i < entqueue->ent_queues[mod_brush].size; i++) {
		auto ent = entqueue->ent_queues[mod_brush].a[i];
		auto renderer = Entity_GetRenderer (ent);
		ent_ids[i + 1] = renderer->render_id;
	}
	QFV_PacketCopyBuffer (packet, bctx->entid_buffer.buffer,
					frame->entid_offset,
					&bufferBarriers[qfv_BB_ShaderRO_to_TransferWrite],
					&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketSubmit (packet);

	*bctx->cluster_queue_ptr = bctx->cluster_queue_buffer.addr + frame->queue;

	*bctx->ent_ids = bctx->entid_buffer.addr + frame->entid_offset;
	*bctx->inst_ids = bctx->instid_buffer.addr;
	*bctx->ent_count = ent_count + 1;
	*bctx->entities = Vulkan_Scene_EntBufferAddr (ctx);

	*bctx->anim_index = vr_data.realtime * 5;

	//printf ("command_counts: %zx\n", *bctx->command_counts_ptr);
	//printf ("command_offsets: %zx\n", *bctx->command_offsets_ptr);
	//printf ("commands: %zx\n", *bctx->commands_ptr);
	//printf ("subclusters: %zx\n", *bctx->subclusters_ptr);
	//printf ("clusters: %zx\n", *bctx->clusters_ptr);
	//printf ("cluster_map: %zx\n", *bctx->cluster_map_ptr);
	//printf ("cluster_queue: %zx\n", *bctx->cluster_queue_ptr);

	//printf ("ent_count: %d\n", *bctx->ent_count);
	//printf ("anim_index: %d\n", *bctx->anim_index);
	//printf ("ent_ids: %zx\n", *bctx->ent_ids);
	//printf ("entities: %zx\n", *bctx->entities);
	//printf ("models: %zx\n", *bctx->models);
	//printf ("tex_ids: %zx\n", *bctx->tex_ids);
	//printf ("anim_main: %zx\n", *bctx->anim_main);
	//printf ("anim_alt: %zx\n", *bctx->anim_alt);
	//printf ("frame_map: %zx\n", *bctx->frame_map);
	//printf ("mod_counts: %zx\n", *bctx->mod_counts);
	//printf ("mod_offsets: %zx\n", *bctx->mod_offsets);
	//printf ("num_models: %d\n", *bctx->num_models);

	frame->entid_count = pass->entid_count;

	bsp_flush (ctx);
}

static void
bsp_light_update (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;
	auto layout = pipeline->layout;

	if (!r_refdef.worldmodel) {
		return;
	}

	auto frame = &bctx->frames.a[ctx->curFrame];
	uint32_t style_offset = frame->style_offset;
	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.light_update");
	auto data = QFV_PacketExtend (packet, sizeof (d_lightstylevalue));
	memcpy (data, d_lightstylevalue, sizeof (d_lightstylevalue));
	QFV_PacketCopyBuffer (packet, bctx->light_style_buffer.buffer, style_offset,
						  &bufferBarriers[qfv_BB_ShaderRO_to_TransferWrite],
						  &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketSubmit (packet);
	*bctx->light_style_values = bctx->light_style_buffer.addr + style_offset;

	auto cmd = taskctx->cmd;
	QFV_PushBlackboard (ctx, cmd, pipeline);
	VkDescriptorSet sets[] = {
		bctx->lightmap_image,
	};
	auto sb = imageBarriers[qfv_LT_ShaderReadOnly_to_StorageWrite];
	auto db = imageBarriers[qfv_LT_StorageWrite_to_ShaderReadOnly];
	auto image = QFV_ScrapImage (bctx->light_scrap);
	sb.image = image;
	db.image = image;
	dfunc->vkCmdPipelineBarrier2 (cmd, &(VkDependencyInfo) {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &sb
	});
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									layout, 0, countof (sets), sets, 0, 0);
	dfunc->vkCmdDispatchIndirect (cmd, bctx->light_queue_buffer.buffer,
								  2 * sizeof (uint32_t[4])
									+ sizeof (uint32_t));
	dfunc->vkCmdPipelineBarrier2 (cmd, &(VkDependencyInfo) {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &db
	});
}

static void
bsp_light_sum (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;
	auto pipeline = taskctx->pipeline;
	auto stage = *(int *) params[0]->value;

	if (!r_refdef.worldmodel) {
		return;
	}

	*bctx->in_data = 0;
	*bctx->out_data = 0;
	*bctx->sum_data = 0;
	uint32_t offset = sizeof (uint32_t);
	uint32_t sum_offset = 0;
	switch (stage) {
		case 0:
			*bctx->in_data = *bctx->light_queue_offs;
			*bctx->out_data = bctx->light_queue_tmp_buffer.addr;
			*bctx->sum_data = *bctx->mod_offsets;// tmp buffer for sums
			break;
		case 1:
			*bctx->in_data = *bctx->mod_offsets;
			*bctx->out_data = bctx->mod_sums_buffer.addr;
			*bctx->sum_data = bctx->light_queue_buffer.addr
							+ sizeof(uint32_t[8]);
			offset += sizeof (uint32_t[4]);
			sum_offset = sizeof (uint32_t);
			break;
		case 2:
			*bctx->in_data = bctx->light_queue_tmp_buffer.addr;
			*bctx->out_data = *bctx->light_queue_offs;
			*bctx->sum_data = bctx->mod_sums_buffer.addr;
			break;
		default:
			Sys_Error ("invalid bsp_light_sum stage: %d\n", stage);
	}
	*bctx->count = *bctx->prefixsum_counts + sum_offset;

	auto cmd = taskctx->cmd;
	QFV_PushBlackboard (ctx, cmd, pipeline);
	dfunc->vkCmdDispatchIndirect (cmd, bctx->light_queue_buffer.buffer, offset);
}

static void
bsp_light_queue_surfs (const exprval_t **params, exprval_t *result,
					   exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bctx = ctx->bsp_context;
	auto frame = &bctx->frames.a[ctx->curFrame];
	auto pipeline = taskctx->pipeline;

	if (!r_refdef.worldmodel) {
		return;
	}

	auto cmd = taskctx->cmd;
	QFV_PushBlackboard (ctx, cmd, pipeline);
	dfunc->vkCmdDispatchIndirect (cmd, bctx->cluster_queue_buffer.buffer,
								  frame->queue + sizeof (uint32_t));
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
bsp_build_display_lists (const exprval_t **params, exprval_t *result,
						 exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto scene = (scene_t *) taskctx->data;

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

	clear_textures (ctx);
	DARRAY_CLEAR (&bctx->registered_textures);

	bctx->main_pass.entqueue = 0;	// owned by the scene
	shutdown_pass_draw_queues (&bctx->main_pass);
	shutdown_pass_draw_queues (&bctx->shadow_pass);
	shutdown_pass_draw_queues (&bctx->debug_pass);

	free (bctx->frames.a);

	QFV_DestroyScrap (bctx->light_scrap);
	QFV_DestroyResource (device, bctx->lightmap_resource);

	if (bctx->base_resource) {
		QFV_DestroyResource (device, bctx->base_resource);
	}
	if (bctx->tex_resource) {
		QFV_DestroyResource (device, bctx->tex_resource);
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
	auto dfunc = device->funcs;

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

	bctx->dsmanager = QFV_Render_DSManager (ctx, "lightmap_set");
	if (bctx->dsmanager) {
		bctx->lightmap_image = QFV_DSManager_AllocSet (bctx->dsmanager);
		dfunc->vkUpdateDescriptorSets (device->dev, 1,
			&(VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = bctx->lightmap_image,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &(VkDescriptorImageInfo) {
					.imageView = Vulkan_LightmapImageView (ctx),
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				},
			}, 0, 0);
	}

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
		.notexture_tex = {
			.name = "notexture",
			.render = &bctx->notexture_render,
		},
		.background_tex = {
			.name = "background",
			.render = &bctx->background_render,
			.flags = SURF_DRAWBACKGROUND,
		},

		.command_counts_ptr  = QFV_GetBlackboardVar (ctx, "command_counts"),
		.command_offsets_ptr = QFV_GetBlackboardVar (ctx, "command_offsets"),
		.commands_ptr        = QFV_GetBlackboardVar (ctx, "commands"),
		.subclusters_ptr     = QFV_GetBlackboardVar (ctx, "subclusters"),
		.clusters_ptr        = QFV_GetBlackboardVar (ctx, "clusters"),
		.cluster_map_ptr     = QFV_GetBlackboardVar (ctx, "cluster_map"),
		.instance_queue_ptr  = QFV_GetBlackboardVar (ctx, "instance_queue"),
		.cluster_queue_ptr   = QFV_GetBlackboardVar (ctx, "cluster_queue"),
		.texture_count       = QFV_GetBlackboardVar (ctx, "texture_count"),
		.matrix_base = QFV_GetBlackboardVar (ctx, "MatrixBase"),
		.fog         = QFV_GetBlackboardVar (ctx, "fog"),
		.time        = QFV_GetBlackboardVar (ctx, "time"),
		.alpha       = QFV_GetBlackboardVar (ctx, "alpha"),
		.turb_scale  = QFV_GetBlackboardVar (ctx, "turb_scale"),
		.control     = QFV_GetBlackboardVar (ctx, "control"),

		.ent_count  = QFV_GetBlackboardVar (ctx, "ent_count"),
		.anim_index = QFV_GetBlackboardVar (ctx, "anim_index"),
		.ent_ids    = QFV_GetBlackboardVar (ctx, "ent_ids"),
		.inst_ids   = QFV_GetBlackboardVar (ctx, "inst_ids"),
		.entities   = QFV_GetBlackboardVar (ctx, "entities"),
		.models     = QFV_GetBlackboardVar (ctx, "models"),
		.tex_ids    = QFV_GetBlackboardVar (ctx, "tex_ids"),
		.anim_main  = QFV_GetBlackboardVar (ctx, "anim_main"),
		.anim_alt   = QFV_GetBlackboardVar (ctx, "anim_alt"),
		.frame_map  = QFV_GetBlackboardVar (ctx, "frame_map"),
		.mod_counts = QFV_GetBlackboardVar (ctx, "mod_counts"),
		.mod_offsets = QFV_GetBlackboardVar (ctx, "mod_offsets"),
		.num_models = QFV_GetBlackboardVar (ctx, "num_models"),

		.in_data = QFV_GetBlackboardVar (ctx, "in_data"),
		.out_data = QFV_GetBlackboardVar (ctx, "out_data"),
		.sum_data = QFV_GetBlackboardVar (ctx, "sum_data"),
		.count = QFV_GetBlackboardVar (ctx, "count"),
		.prefixsum_counts = QFV_GetBlackboardVar (ctx, "prefixsum_counts"),

		.lightinfo          = QFV_GetBlackboardVar (ctx, "lightinfo"),
		.surfinfo           = QFV_GetBlackboardVar (ctx, "surfinfo"),
		.light_style_values = QFV_GetBlackboardVar (ctx, "light_style_values"),
		.lightmap_data      = QFV_GetBlackboardVar (ctx, "lightmap_data"),
		.light_clusters     = QFV_GetBlackboardVar (ctx, "light_clusters"),
		.light_cluster_queue= QFV_GetBlackboardVar (ctx, "light_cluster_queue"),
		.light_queue_offs   = QFV_GetBlackboardVar (ctx, "light_queue_offs"),
		.light_queue_inds   = QFV_GetBlackboardVar (ctx, "light_queue_inds"),
		.light_queue        = QFV_GetBlackboardVar (ctx, "light_queue"),
		.num_lightmaps      = QFV_GetBlackboardVar (ctx, "num_lightmaps"),
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
	QFV_bspTransEnt,
};
static exprsym_t bsp_queue_symbols[] = {
	{"solid",       &bsp_queue_type, bsp_queue_values + 0},
	{"background",  &bsp_queue_type, bsp_queue_values + 1},
	{"sky",         &bsp_queue_type, bsp_queue_values + 2},
	{"translucent", &bsp_queue_type, bsp_queue_values + 3},
	{"turbulent",   &bsp_queue_type, bsp_queue_values + 4},
	{"trans_ent",   &bsp_queue_type, bsp_queue_values + 5},
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

static exprtype_t *bsp_sum_params[] = {
	&cexpr_int,
};

static exprfunc_t bsp_reset_queues_func[] = {
	{ .func = bsp_reset_queues },
	{}
};
static exprfunc_t bsp_clear_commands_func[] = {
	{ .func = bsp_clear_commands },
	{}
};
static exprfunc_t bsp_count_ent_func[] = {
	{ .func = bsp_count_ent },
	{}
};
static exprfunc_t bsp_clear_ent_func[] = {
	{ .func = bsp_clear_ent },
	{}
};
static exprfunc_t bsp_queue_insts_func[] = {
	{ .func = bsp_queue_insts },
	{}
};
static exprfunc_t bsp_distribute_insts_func[] = {
	{ .func = bsp_distribute_insts },
	{}
};
static exprfunc_t bsp_queue_clusters_func[] = {
	{ .func = bsp_queue_clusters },
	{}
};
static exprfunc_t bsp_visit_world_func[] = {
	{ 0, 1, bsp_visit_world_params, bsp_visit_world },
	{}
};
static exprfunc_t bsp_draw_barrier_func[] = {
	{ .func = bsp_draw_barrier },
	{}
};
static exprfunc_t bsp_draw_queue_func[] = {
	{ 0, 3, bsp_draw_queue_params, bsp_draw_queue },
	{}
};

static exprfunc_t bsp_sum_mod_insts_func[] = {
	{ .func = bsp_sum_mod_insts, .num_params = 1, bsp_sum_params },
	{}
};

static exprfunc_t bsp_light_update_func[] = {
	{ .func = bsp_light_update },
	{}
};
static exprfunc_t bsp_light_sum_func[] = {
	{ .func = bsp_light_sum, .num_params = 1, bsp_sum_params },
	{}
};
static exprfunc_t bsp_light_queue_surfs_func[] = {
	{ .func = bsp_light_queue_surfs },
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
	{ "bsp_count_ent", &cexpr_function, bsp_count_ent_func },
	{ "bsp_clear_ent", &cexpr_function, bsp_clear_ent_func },
	{ "bsp_queue_insts", &cexpr_function, bsp_queue_insts_func },
	{ "bsp_distribute_insts", &cexpr_function, bsp_distribute_insts_func },
	{ "bsp_queue_clusters", &cexpr_function, bsp_queue_clusters_func },
	{ "bsp_visit_world", &cexpr_function, bsp_visit_world_func },
	{ "bsp_draw_barrier", &cexpr_function, bsp_draw_barrier_func },
	{ "bsp_draw_queue", &cexpr_function, bsp_draw_queue_func },

	{ "bsp_sum_mod_insts", &cexpr_function, bsp_sum_mod_insts_func },

	{ "bsp_light_update", &cexpr_function, bsp_light_update_func },
	{ "bsp_light_sum", &cexpr_function, bsp_light_sum_func },
	{ "bsp_light_queue_surfs", &cexpr_function, bsp_light_queue_surfs_func },
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
