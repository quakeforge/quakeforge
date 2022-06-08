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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
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
#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_scene.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "QF/simd/types.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct bsp_push_constants_s {
	quat_t      fog;
	float       time;
	float       alpha;
	float       turb_scale;
} bsp_push_constants_t;

static const char * __attribute__((used)) bsp_pass_names[] = {
	"depth",
	"g-buffer",
	"sky",
	"turb",
};

static QFV_Subpass subpass_map[] = {
	[QFV_bspDepth]   = QFV_passDepth,
	[QFV_bspGBuffer] = QFV_passGBuffer,
	[QFV_bspSky]     = QFV_passTranslucent,
	[QFV_bspTurb]    = QFV_passTranslucent,
};

static void
add_texture (texture_t *tx, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	vulktex_t  *tex = tx->render;
	if (tex->tex) {
		tex->tex_id = bctx->registered_textures.size;
		DARRAY_APPEND (&bctx->registered_textures, tex);
		tex->descriptor = Vulkan_CreateTextureDescriptor (ctx, tex->tex,
														  bctx->sampler);
	}
}

static inline void
chain_surface (const bsp_face_t *face, bsp_pass_t *pass, const bspctx_t *bctx)
{
	int         ent_frame = pass->ent_frame;
	// if the texture has no alt animations, anim_alt holds the sama data
	// as anim_main
	const texanim_t *anim = ent_frame ? &bctx->texdata.anim_alt[face->tex_id]
									  : &bctx->texdata.anim_main[face->tex_id];
	int         anim_ind = (bctx->anim_index + anim->offset) % anim->count;
	int         tex_id = bctx->texdata.frame_map[anim->base + anim_ind];
	DARRAY_APPEND (&pass->face_queue[tex_id],
				   ((instface_t) { pass->inst_id, face - bctx->faces }));
}

static void
register_textures (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
	texture_t  *tex;

	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex = brush->textures[i];
		if (!tex)
			continue;
		add_texture (tex, ctx);
	}
}

static void
clear_textures (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	if (bctx->main_pass.face_queue) {
		for (size_t i = 0; i < bctx->registered_textures.size; i++) {
			DARRAY_CLEAR (&bctx->main_pass.face_queue[i]);
		}
		free (bctx->main_pass.face_queue);
		bctx->main_pass.face_queue = 0;
	}

	bctx->registered_textures.size = 0;
}

void
Vulkan_RegisterTextures (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	clear_textures (ctx);
	add_texture (r_notexture_mip, ctx);
	{
		// FIXME make worldmodel non-special. needs smarter handling of
		// textures on sub-models but not on main model.
		mod_brush_t *brush = &r_refdef.worldmodel->brush;
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
		mod_brush_t *brush = &m->brush;
		brush->numsubmodels = 1; // no support for submodels in non-world model
		register_textures (brush, ctx);
	}

	bspctx_t   *bctx = ctx->bsp_context;
	int         num_tex = bctx->registered_textures.size;

	texture_t **textures = alloca (num_tex * sizeof (texture_t *));
	textures[0] = r_notexture_mip;
	for (int i = 0, t = 1; i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		if (!m || *m->path == '*') {
			continue;
		}
		mod_brush_t *brush = &m->brush;
		for (unsigned j = 0; j < brush->numtextures; j++) {
			if (brush->textures[j]) {
				textures[t++] = brush->textures[j];
			}
		}
	}

	// 2.5 for two texanim_t structs (32-bits each) and 1 uint16_t for each
	// element
	size_t      texdata_size = 2.5 * num_tex * sizeof (texanim_t);
	texanim_t  *texdata = Hunk_AllocName (0, texdata_size, "texdata");
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
		};
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
	bctx->main_pass.face_queue = malloc (num_tex * sizeof (bsp_instfaceset_t));
	for (int i = 0; i < num_tex; i++) {
		bctx->main_pass.face_queue[i]
			= (bsp_instfaceset_t) DARRAY_STATIC_INIT (128);
	}
}

typedef struct {
	msurface_t *face;
	model_t    *model;
	int         model_face_base;
} faceref_t;

typedef struct DARRAY_TYPE (faceref_t) facerefset_t;

static void
count_verts_inds (const faceref_t *faceref, uint32_t *verts, uint32_t *inds)
{
	msurface_t *surf = faceref->face;
	*verts = surf->numedges;
	*inds = surf->numedges + 1;
}

typedef struct bspvert_s {
	quat_t      vertex;
	quat_t      tlst;
} bspvert_t;

typedef struct {
	bsp_face_t *faces;
	uint32_t   *indices;
	bspvert_t  *vertices;
	uint32_t    index_base;
	uint32_t    vertex_base;
	int         tex_id;
} buildctx_t;

static void
build_surf_displist (const faceref_t *faceref, buildctx_t *build)
{
	msurface_t *surf = faceref->face;
	mod_brush_t *brush = &faceref->model->brush;;

	int         facenum = surf - brush->surfaces;
	bsp_face_t *face = &build->faces[facenum + faceref->model_face_base];
	// create a triangle fan
	int         numverts = surf->numedges;
	face->first_index = build->index_base;
	face->index_count = numverts + 1;	// +1 for primitive restart
	face->tex_id = build->tex_id;
	face->flags = surf->flags;
	build->index_base += face->index_count;
	for (int i = 0; i < numverts; i++) {
		build->indices[face->first_index + i] = build->vertex_base + i;
	}
	build->indices[face->first_index + numverts] = -1;	// primitive restart

	bspvert_t  *verts = build->vertices + build->vertex_base;
	build->vertex_base += numverts;
	mtexinfo_t *texinfo = surf->texinfo;
	mvertex_t  *vertices = brush->vertexes;
	medge_t    *edges     = brush->edges;
	int        *surfedges = brush->surfedges;
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
}

void
Vulkan_BuildDisplayLists (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;

	if (!num_models) {
		return;
	}

	facerefset_t *face_sets = alloca (bctx->registered_textures.size
									  * sizeof (facerefset_t));
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		face_sets[i] = (facerefset_t) DARRAY_STATIC_INIT (1024);
	}

	for (int i = 0; i < bctx->num_models; i++) {
		DARRAY_CLEAR (&bctx->main_pass.instances[i].entities);
	}
	free (bctx->main_pass.instances);
	bctx->num_models = 0;

	// run through all surfaces, chaining them to their textures, thus
	// effectively sorting the surfaces by texture (without worrying about
	// surface order on the same texture chain).
	int         face_base = 0;
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		// and non-bsp models don't have surfaces.
		if (!m || m->type != mod_brush) {
			continue;
		}
		m->render_id = bctx->num_models++;
		if (*m->path == '*') {
			continue;
		}
		mod_brush_t *brush = &m->brush;
		dmodel_t    *dm = brush->submodels;
		for (unsigned j = 0; j < brush->numsurfaces; j++) {
			if (j == dm->firstface + dm->numfaces) {
				// move on to the next sub-model
				dm++;
				if (dm == brush->submodels + brush->numsubmodels) {
					// limit the surfaces
					// probably never hit
					Sys_Printf ("Vulkan_BuildDisplayLists: too many faces\n");
					brush->numsurfaces = j;
					break;
				}
			}
			msurface_t *surf = brush->surfaces + j;
			// append surf to the texture chain
			vulktex_t *tex = surf->texinfo->texture->render;
			DARRAY_APPEND (&face_sets[tex->tex_id],
						   ((faceref_t) { surf, m, face_base }));
		}
		face_base += brush->numsurfaces;
	}
	bctx->main_pass.instances = malloc (bctx->num_models
										* sizeof (bsp_instance_t));
	for (int i = 0; i < bctx->num_models; i++) {
		DARRAY_INIT (&bctx->main_pass.instances[i].entities, 16);
	}
	// All vertices from all brush models go into one giant vbo.
	uint32_t    vertex_count = 0;
	uint32_t    index_count = 0;
	uint32_t    poly_count = 0;
	// This is not optimal as counted vertices are not shared between faces,
	// however this greatly simplifies display list creation as no care needs
	// to be taken when it comes to UVs, and every vertex needs a unique light
	// map UV anyway (when lightmaps are used).
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		for (size_t j = 0; j < face_sets[i].size; j++) {
			faceref_t  *faceref = &face_sets[i].a[j];
			uint32_t    verts, inds;
			count_verts_inds (faceref, &verts, &inds);
			vertex_count += verts;
			index_count += inds;
			poly_count++;
		}
	}

	size_t atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t atom_mask = atom - 1;
	size_t frames = bctx->frames.size;
	size_t index_buffer_size = index_count * frames * sizeof (uint32_t);
	size_t vertex_buffer_size = vertex_count * sizeof (bspvert_t);

	index_buffer_size = (index_buffer_size + atom_mask) & ~atom_mask;
	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device, "bsp",
													 vertex_buffer_size,
													 ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	bspvert_t  *vertices = QFV_PacketExtend (packet, vertex_buffer_size);
	// holds all the polygon definitions: vertex indices + poly_count
	// primitive restart markers. The primitive restart markers are included
	// in index_count.
	// so each polygon within the list:
	//     index        count-1 indices
	//     index
	//     ...
	//     "end of primitive" (~0u)
	free (bctx->faces);
	free (bctx->poly_indices);
	free (bctx->models);
	bctx->models = malloc (bctx->num_models * sizeof (bsp_model_t));
	bctx->faces = malloc (face_base * sizeof (bsp_face_t));
	bctx->poly_indices = malloc (index_count * sizeof (uint32_t));

	face_base = 0;
	for (int i = 0; i < num_models; i++) {
		if (!models[i] || models[i]->type != mod_brush) {
			continue;
		}
		int         num_faces = models[i]->brush.numsurfaces;
		bsp_model_t *m = &bctx->models[models[i]->render_id];
		m->first_face = face_base + models[i]->brush.firstmodelsurface;
		m->face_count = models[i]->brush.nummodelsurfaces;
		while (i < num_models - 1 && models[i + 1]
			   && models[i + 1]->path[0] == '*') {
			i++;
			m = &bctx->models[models[i]->render_id];
			m->first_face = face_base + models[i]->brush.firstmodelsurface;
			m->face_count = models[i]->brush.nummodelsurfaces;
		}
		face_base += num_faces;;
	}

	// All usable surfaces have been chained to the (base) texture they use.
	// Run through the textures, using their chains to build display lists.
	// For animated textures, if a surface is on one texture of the group, it
	// will effectively be on all (just one at a time).
	buildctx_t  build = {
		.faces = bctx->faces,
		.indices = bctx->poly_indices,
		.vertices = vertices,
		.index_base = 0,
		.vertex_base = 0,
	};
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		build.tex_id = i;
		for (size_t j = 0; j < face_sets[i].size; j++) {
			faceref_t  *faceref = &face_sets[i].a[j];
			build_surf_displist (faceref, &build);
		}
	}
	Sys_MaskPrintf (SYS_vulkan,
					"R_BuildDisplayLists: verts:%u, inds:%u, polys:%u\n",
					vertex_count, index_count, poly_count);
	if (index_buffer_size > bctx->index_buffer_size) {
		if (bctx->index_buffer) {
			dfunc->vkUnmapMemory (device->dev, bctx->index_memory);
			dfunc->vkDestroyBuffer (device->dev, bctx->index_buffer, 0);
			dfunc->vkFreeMemory (device->dev, bctx->index_memory, 0);
		}
		bctx->index_buffer
			= QFV_CreateBuffer (device, index_buffer_size,
								VK_BUFFER_USAGE_TRANSFER_DST_BIT
								| VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, bctx->index_buffer,
							 "buffer:bsp:index");
		bctx->index_memory
			= QFV_AllocBufferMemory (device, bctx->index_buffer,
									 VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
									 index_buffer_size, 0);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
							 bctx->index_memory, "memory:bsp:index");
		QFV_BindBufferMemory (device,
							  bctx->index_buffer, bctx->index_memory, 0);
		bctx->index_buffer_size = index_buffer_size;
		void       *data;
		dfunc->vkMapMemory (device->dev, bctx->index_memory, 0,
							index_buffer_size, 0, &data);
		uint32_t   *index_data = data;
		for (size_t i = 0; i < frames; i++) {
			uint32_t    offset = index_count * i;
			bctx->frames.a[i].index_data = index_data + offset;
			bctx->frames.a[i].index_offset = offset * sizeof (uint32_t);
			bctx->frames.a[i].index_count = 0;
		}
	}
	if (vertex_buffer_size > bctx->vertex_buffer_size) {
		if (bctx->vertex_buffer) {
			dfunc->vkDestroyBuffer (device->dev, bctx->vertex_buffer, 0);
			dfunc->vkFreeMemory (device->dev, bctx->vertex_memory, 0);
		}
		bctx->vertex_buffer
			= QFV_CreateBuffer (device, vertex_buffer_size,
								VK_BUFFER_USAGE_TRANSFER_DST_BIT
								| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER,
							 bctx->vertex_buffer, "buffer:bsp:vertex");
		bctx->vertex_memory
			= QFV_AllocBufferMemory (device, bctx->vertex_buffer,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									 vertex_buffer_size, 0);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
							 bctx->vertex_memory, "memory:bsp:vertex");
		QFV_BindBufferMemory (device,
							  bctx->vertex_buffer, bctx->vertex_memory, 0);
		bctx->vertex_buffer_size = vertex_buffer_size;
	}

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	bb.barrier.buffer = bctx->vertex_buffer;
	bb.barrier.size = vertex_buffer_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	VkBufferCopy copy_region = { packet->offset, 0, vertex_buffer_size };
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							bctx->vertex_buffer, 1, &copy_region);
	bb = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb.barrier.buffer = bctx->vertex_buffer;
	bb.barrier.size = vertex_buffer_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		DARRAY_CLEAR (&face_sets[i]);
	}

}

static int
R_DrawBrushModel (entity_t *e, bsp_pass_t *pass, vulkan_ctx_t *ctx)
{
	float       radius;
	model_t    *model;
	vec3_t      mins, maxs;
	bspctx_t   *bctx = ctx->bsp_context;

	model = e->renderer.model;
	mat4f_t mat;
	Transform_GetWorldMatrix (e->transform, mat);
	if (mat[0][0] != 1 || mat[1][1] != 1 || mat[2][2] != 1) {
		radius = model->radius;
		if (R_CullSphere (pass->frustum, (vec_t*)&mat[3], radius)) { //FIXME
			return 1;
		}
	} else {
		VectorAdd (mat[3], model->mins, mins);
		VectorAdd (mat[3], model->maxs, maxs);
		if (R_CullBox (pass->frustum, mins, maxs))
			return 1;
	}
	if (Vulkan_Scene_AddEntity (ctx, e) < 0) {
		return 0;
	}

	pass->ent_frame = e->animation.frame & 1;
	pass->inst_id = model->render_id;
	pass->inst_id |= e->renderer.colormod[3] < 1 ? INST_ALPHA : 0;
	if (!pass->instances[model->render_id].entities.size) {
		bsp_model_t *m = &bctx->models[model->render_id];
		bsp_face_t *face = &bctx->faces[m->first_face];
		for (unsigned i = 0; i < m->face_count; i++, face++) {
			// enqueue the polygon
			chain_surface (face, pass, bctx);
		}
	}
	DARRAY_APPEND (&pass->instances[model->render_id].entities,
				   e->renderer.render_id);
	return 1;
}

static inline void
visit_leaf (mleaf_t *leaf)
{
	// since this leaf will be rendered, any entities in the leaf also need
	// to be rendered (the bsp tree doubles as an entity cull structure)
	if (leaf->efrags)
		R_StoreEfrags (leaf->efrags);
}

// 1 = back side, 0 = front side
static inline int
get_side (const bsp_pass_t *pass, const mnode_t *node)
{
	// find the node side on which we are
	vec4f_t     org = pass->position;

	return dotf (org, node->plane)[0] < 0;
}

static inline void
visit_node (bsp_pass_t *pass, const mnode_t *node, int side)
{
	bspctx_t   *bctx = pass->bsp_context;
	int         c;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	// seems to be microscopically faster even on modern hardware
	side = (-side) & SURF_PLANEBACK;
	// chain any visible surfaces on the node that face the camera.
	// not all nodes have any surfaces to draw (purely a split plane)
	if ((c = node->numsurfaces)) {
		const bsp_face_t *face = bctx->faces + node->firstsurface;
		const int  *frame = pass->face_frames + node->firstsurface;
		int         vis_frame = pass->vis_frame;
		for (; c; c--, face++, frame++) {
			if (*frame != vis_frame)
				continue;

			// side is either 0 or SURF_PLANEBACK
			// if side and the surface facing differ, then the camera is
			// on backside of the surface
			if (side ^ (face->flags & SURF_PLANEBACK))
				continue;               // wrong side

			chain_surface (face, pass, bctx);
		}
	}
}

static inline int
test_node (const bsp_pass_t *pass, int node_id)
{
	if (node_id < 0)
		return 0;
	if (pass->node_frames[node_id] != pass->vis_frame)
		return 0;
	mnode_t    *node = pass->brush->nodes + node_id;
	if (R_CullBox (pass->frustum, node->minmaxs, node->minmaxs + 3))
		return 0;
	return 1;
}

static void
R_VisitWorldNodes (bsp_pass_t *pass, vulkan_ctx_t *ctx)
{
	const mod_brush_t *brush = pass->brush;
	typedef struct {
		int         node_id;
		int         side;
	} rstack_t;
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
		while (test_node (pass, node_id)) {
			mnode_t    *node = brush->nodes + node_id;
			side = get_side (pass, node);
			front = node->children[side];
			if (test_node (pass, front)) {
				node_ptr->node_id = node_id;
				node_ptr->side = side;
				node_ptr++;
				node_id = front;
				continue;
			}
			// front is either not a node (ie, is a leaf) or is not visible
			// if node is visible, then at least one of its child nodes
			// must also be visible, and a leaf child in front of the node
			// will be visible, so no need for vis checks on a leaf
			if (front < 0) {
				mleaf_t    *leaf = brush->leafs + ~front;
				if (leaf->contents != CONTENTS_SOLID) {
					visit_leaf (leaf);
				}
			}
			visit_node (pass, node, side);
			node_id = node->children[side ^ 1];
		}
		if (node_id < 0) {
			mleaf_t    *leaf = brush->leafs + ~node_id;
			if (leaf->contents != CONTENTS_SOLID) {
				visit_leaf (leaf);
			}
		}
		if (node_ptr != node_stack) {
			node_ptr--;
			node_id = node_ptr->node_id;
			side = node_ptr->side;
			mnode_t    *node = brush->nodes + node_id;
			visit_node (pass, node, side);
			node_id = node->children[side ^ 1];
			continue;
		}
		break;
	}
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
push_fragconst (bsp_push_constants_t *constants, VkPipelineLayout layout,
				qfv_device_t *device, VkCommandBuffer cmd)
{
	qfv_push_constants_t push_constants[] = {
		//{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (mat), mat },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (bsp_push_constants_t, fog),
			sizeof (constants->fog), &constants->fog },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (bsp_push_constants_t, time),
			sizeof (constants->time), &constants->time },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (bsp_push_constants_t, alpha),
			sizeof (constants->alpha), &constants->alpha },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (bsp_push_constants_t, turb_scale),
			sizeof (constants->turb_scale), &constants->turb_scale },
	};
	QFV_PushConstants (device, cmd, layout, 4, push_constants);
}

static void
bsp_begin_subpass (QFV_BspSubpass subpass, VkPipeline pipeline,
				   VkPipelineLayout layout, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = bframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, subpass_map[subpass],
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "bsp:%s",
										  bsp_pass_names[subpass]),
						 {0, 0.5, 0.6, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	VkBuffer    buffers[] = { bctx->vertex_buffer, bctx->entid_buffer };
	VkDeviceSize offsets[] = { 0, bframe->entid_offset };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 2, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, bctx->index_buffer, bframe->index_offset,
								 VK_INDEX_TYPE_UINT32);

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		Vulkan_Scene_Descriptors (ctx),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, sets, 0, 0);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
bsp_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

static void
bsp_begin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	bspctx_t   *bctx = ctx->bsp_context;
	//XXX quat_t      fog;

	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passDepth],
				   bframe->cmdSet.a[QFV_bspDepth]);
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passGBuffer],
				   bframe->cmdSet.a[QFV_bspGBuffer]);

	qfvPushDebug (ctx, "bsp_begin_subpass");
	bsp_begin_subpass (QFV_bspDepth, bctx->depth, bctx->layout, rFrame);
	bsp_begin_subpass (QFV_bspGBuffer, bctx->gbuf, bctx->layout, rFrame);
	qfvPopDebug (ctx);
}

static void
bsp_end (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	bsp_end_subpass (bframe->cmdSet.a[QFV_bspDepth], ctx);
	bsp_end_subpass (bframe->cmdSet.a[QFV_bspGBuffer], ctx);
}

static void
turb_begin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	bspctx_t   *bctx = ctx->bsp_context;

	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passTranslucent],
				   bframe->cmdSet.a[QFV_bspTurb]);

	qfvPushDebug (ctx, "bsp_begin_subpass");
	bsp_begin_subpass (QFV_bspTurb, bctx->turb, bctx->layout, rFrame);
	qfvPopDebug (ctx);
}

static void
turb_end (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	bsp_end_subpass (bframe->cmdSet.a[QFV_bspTurb], ctx);
}

static void
sky_begin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	bspctx_t   *bctx = ctx->bsp_context;

	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passTranslucent],
				   bframe->cmdSet.a[QFV_bspSky]);

	qfvPushDebug (ctx, "bsp_begin_subpass");
	if (bctx->skybox_tex) {
		bsp_begin_subpass (QFV_bspSky, bctx->skybox, bctx->layout, rFrame);
	} else {
		bsp_begin_subpass (QFV_bspSky, bctx->skysheet, bctx->layout, rFrame);
	}
	qfvPopDebug (ctx);
}

static void
sky_end (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	bsp_end_subpass (bframe->cmdSet.a[QFV_bspSky], ctx);
}

static void
clear_queues (bspctx_t *bctx, bsp_pass_t *pass)
{
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
queue_faces (bsp_pass_t *pass, const bspctx_t *bctx, bspframe_t *bframe)
{
	pass->indices = bframe->index_data + bframe->index_count;
	for (size_t i = 0; i < bctx->registered_textures.size; i++) {
		__auto_type queue = &pass->face_queue[i];
		if (!queue->size) {
			continue;
		}
		for (size_t j = 0; j < queue->size; j++) {
			__auto_type is = queue->a[j];
			__auto_type f = bctx->faces[is.face];

			f.flags |= ((is.inst_id & INST_ALPHA)
						>> (BITOP_LOG2(INST_ALPHA)
							- BITOP_LOG2(SURF_DRAWALPHA))) & SURF_DRAWALPHA;
			is.inst_id &= ~INST_ALPHA;
			if (pass->instances[is.inst_id].first_instance == -1) {
				uint32_t    count = pass->instances[is.inst_id].entities.size;
				pass->instances[is.inst_id].first_instance = pass->entid_count;
				memcpy (pass->entid_data + pass->entid_count,
						pass->instances[is.inst_id].entities.a,
						count * sizeof (uint32_t));
				pass->entid_count += count;
			}

			int         dq = 0;
			if (f.flags & SURF_DRAWSKY) {
				dq = 1;
			}
			if (f.flags & SURF_DRAWALPHA) {
				dq = 2;
			}
			if (f.flags & SURF_DRAWTURB) {
				dq = 3;
			}

			size_t      dq_size = pass->draw_queues[dq].size;
			bsp_draw_t *draw = &pass->draw_queues[dq].a[dq_size - 1];
			if (!pass->draw_queues[dq].size
				|| draw->tex_id != i
				|| draw->inst_id != is.inst_id) {
				bsp_instance_t *instance = &pass->instances[is.inst_id];
				DARRAY_APPEND (&pass->draw_queues[dq], ((bsp_draw_t) {
					.tex_id = i,
					.inst_id = is.inst_id,
					.instance_count = instance->entities.size,
					.first_index = pass->index_count,
					.first_instance = instance->first_instance,
				}));
				dq_size = pass->draw_queues[dq].size;
				draw = &pass->draw_queues[dq].a[dq_size - 1];
			}

			memcpy (pass->indices + pass->index_count,
					bctx->poly_indices + f.first_index,
					f.index_count * sizeof (uint32_t));
			draw->index_count += f.index_count;
			pass->index_count += f.index_count;
		}
	}
	bframe->index_count += pass->index_count;
}

static void
draw_queue (bsp_pass_t *pass, int queue, VkPipelineLayout layout,
			qfv_device_t *device, VkCommandBuffer cmd)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < pass->draw_queues[queue].size; i++) {
		__auto_type d = pass->draw_queues[queue].a[i];
		if (pass->textures) {
			vulktex_t  *tex = pass->textures->a[d.tex_id];
			bind_texture (tex, 2, layout, dfunc, cmd);
		}
		dfunc->vkCmdDrawIndexed (cmd, d.index_count, d.instance_count,
								 d.first_index, 0, d.first_instance);
	}
}

static int
ent_model_cmp (const void *_a, const void *_b)
{
	const entity_t * const *a = _a;
	const entity_t * const *b = _b;
	return (*a)->renderer.model->render_id - (*b)->renderer.model->render_id;
}

void
Vulkan_DrawWorld (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	//qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	bctx->main_pass.bsp_context = bctx;
	bctx->main_pass.position = r_refdef.frame.position;
	bctx->main_pass.frustum = r_refdef.frustum;
	bctx->main_pass.vis_frame = r_visframecount;
	bctx->main_pass.face_frames = r_face_visframes;
	bctx->main_pass.leaf_frames = r_leaf_visframes;
	bctx->main_pass.node_frames = r_node_visframes;
	bctx->main_pass.entid_data = bframe->entid_data;
	bctx->main_pass.entid_count = 0;

	bctx->anim_index = r_data->realtime * 5;

	clear_queues (bctx, &bctx->main_pass);	// do this first for water and skys
	bframe->index_count = 0;

	entity_t    worldent = {
		.renderer = {
			.model = r_refdef.worldmodel,
			.colormod = { 1, 1, 1, 1 },
		},
	};

	Vulkan_Scene_AddEntity (ctx, &worldent);

	int         world_id = worldent.renderer.model->render_id;
	bctx->main_pass.ent_frame = 0;	// world is always frame 0
	bctx->main_pass.inst_id = world_id;
	bctx->main_pass.brush = &worldent.renderer.model->brush;
	if (bctx->main_pass.instances) {
		DARRAY_APPEND (&bctx->main_pass.instances[world_id].entities,
					   worldent.renderer.render_id);
	}
	R_VisitWorldNodes (&bctx->main_pass, ctx);
	if (!bctx->vertex_buffer) {
		return;
	}
	if (r_drawentities) {
		heapsort (r_ent_queue->ent_queues[mod_brush].a,
				  r_ent_queue->ent_queues[mod_brush].size,
				  sizeof (entity_t *), ent_model_cmp);
		for (size_t i = 0; i < r_ent_queue->ent_queues[mod_brush].size; i++) {
			entity_t   *ent = r_ent_queue->ent_queues[mod_brush].a[i];
			if (!R_DrawBrushModel (ent, &bctx->main_pass, ctx)) {
				Sys_Printf ("Too many entities!\n");
				break;
			}
		}
	}
	bframe->entid_count = bctx->main_pass.entid_count;

	queue_faces (&bctx->main_pass, bctx, bframe);

	bsp_begin (rFrame);

	bsp_push_constants_t frag_constants = { .time = vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspGBuffer]);
	VkPipelineLayout layout = bctx->layout;

	__auto_type pass = &bctx->main_pass;
	pass->textures = 0;
	draw_queue (pass, 0, layout, device, bframe->cmdSet.a[QFV_bspDepth]);
	pass->textures = &bctx->registered_textures;
	draw_queue (pass, 0, layout, device, bframe->cmdSet.a[QFV_bspGBuffer]);
	bsp_end (ctx);
}

void
Vulkan_Bsp_Flush (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	size_t      atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
	size_t      index_offset = bframe->index_offset;
	size_t      index_size = bframe->index_count * sizeof (uint32_t);
	size_t      entid_offset = bframe->entid_offset;
	size_t      entid_size = bframe->entid_count * sizeof (uint32_t);

	if (!bframe->index_count) {
		return;
	}
	index_offset &= ~atom_mask;
	index_size = (index_size + atom_mask) & ~atom_mask;
	entid_offset &= ~atom_mask;
	entid_size = (entid_size + atom_mask) & ~atom_mask;

	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  bctx->index_memory, index_offset, index_size
		},
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  bctx->entid_memory, entid_offset, entid_size
		},
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 2, ranges);
}

void
Vulkan_DrawWaterSurfaces (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	//qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	if (!bctx->main_pass.draw_queues[3].size)
		return;

	turb_begin (rFrame);

	VkPipelineLayout layout = bctx->layout;
	bsp_push_constants_t frag_constants = {
		.time = vr_data.realtime,
		.alpha = 1,
		.turb_scale = 0,
	};
	push_fragconst (&frag_constants, layout, device,
					bframe->cmdSet.a[QFV_bspTurb]);

	__auto_type pass = &bctx->main_pass;
	pass->textures = &bctx->registered_textures;
	draw_queue (pass, 2, layout, device, bframe->cmdSet.a[QFV_bspTurb]);

	frag_constants.alpha = r_wateralpha;
	frag_constants.turb_scale = 1;
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspTurb]);
	draw_queue (pass, 3, layout, device, bframe->cmdSet.a[QFV_bspTurb]);

	turb_end (ctx);
}

void
Vulkan_DrawSky (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	if (!bctx->main_pass.draw_queues[1].size)
		return;

	sky_begin (rFrame);
	vulktex_t skybox = { .descriptor = bctx->skybox_descriptor };
	bind_texture (&skybox, 3, bctx->layout, dfunc,
				  bframe->cmdSet.a[QFV_bspSky]);
	bsp_push_constants_t frag_constants = { .time = vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspSky]);

	VkPipelineLayout layout = bctx->layout;
	__auto_type pass = &bctx->main_pass;
	pass->textures = &bctx->registered_textures;
	draw_queue (pass, 1, layout, device, bframe->cmdSet.a[QFV_bspSky]);

	sky_end (ctx);
}

static void
create_default_skys (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	VkImage     skybox;
	VkImage     skysheet;
	VkDeviceMemory memory;
	VkImageView boxview;
	VkImageView sheetview;

	bctx->default_skybox = calloc (2, sizeof (qfv_tex_t));
	bctx->default_skysheet = bctx->default_skybox + 1;

	VkExtent3D extents = { 1, 1, 1 };
	skybox = QFV_CreateImage (device, 1, VK_IMAGE_TYPE_2D,
							  VK_FORMAT_B8G8R8A8_UNORM, extents, 1, 1,
							  VK_SAMPLE_COUNT_1_BIT,
							  VK_IMAGE_USAGE_SAMPLED_BIT
							  | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, skybox,
						 "bsp:image:default_skybox");

	skysheet = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
							  VK_FORMAT_B8G8R8A8_UNORM, extents, 1, 2,
							  VK_SAMPLE_COUNT_1_BIT,
							  VK_IMAGE_USAGE_SAMPLED_BIT
							  | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, skysheet,
						 "bsp:image:default_skysheet");

	VkMemoryRequirements requirements;
	dfunc->vkGetImageMemoryRequirements (device->dev, skybox, &requirements);
	size_t      boxsize = requirements.size;
	dfunc->vkGetImageMemoryRequirements (device->dev, skysheet, &requirements);
	size_t      sheetsize = requirements.size;

	memory = QFV_AllocImageMemory (device, skybox,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								   boxsize + sheetsize,
								   VK_IMAGE_USAGE_TRANSFER_DST_BIT
								   | VK_IMAGE_USAGE_SAMPLED_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, memory,
						 "bsp:memory:default_skys");

	QFV_BindImageMemory (device, skybox, memory, 0);
	QFV_BindImageMemory (device, skysheet, memory, boxsize);

	boxview = QFV_CreateImageView (device, skybox, VK_IMAGE_VIEW_TYPE_CUBE,
								   VK_FORMAT_B8G8R8A8_UNORM,
								   VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, boxview,
						 "bsp:iview:default_skybox");

	sheetview = QFV_CreateImageView (device, skysheet,
									 VK_IMAGE_VIEW_TYPE_2D_ARRAY,
								     VK_FORMAT_B8G8R8A8_UNORM,
								     VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, sheetview,
						 "bsp:iview:default_skysheet");

	bctx->default_skybox->image = skybox;
	bctx->default_skybox->view = boxview;
	bctx->default_skybox->memory = memory;
	bctx->default_skysheet->image = skysheet;
	bctx->default_skysheet->view = sheetview;

	// temporarily commandeer the light map's staging buffer
	qfv_packet_t *packet = QFV_PacketAcquire (bctx->light_stage);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	VkImageMemoryBarrier barriers[2] = { ib.barrier, ib.barrier };
	barriers[0].image = skybox;
	barriers[1].image = skysheet;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 2, barriers);

	VkClearColorValue color = {};
	VkImageSubresourceRange range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, VK_REMAINING_MIP_LEVELS,
		0, VK_REMAINING_ARRAY_LAYERS
	};
	dfunc->vkCmdClearColorImage (packet->cmd, skybox,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 &color, 1, &range);
	dfunc->vkCmdClearColorImage (packet->cmd, skysheet,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 &color, 1, &range);

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barriers[0] = ib.barrier;
	barriers[1] = ib.barrier;
	barriers[0].image = skybox;
	barriers[1].image = skysheet;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 2, barriers);
	QFV_PacketSubmit (packet);
}

static void
create_notexture (vulkan_ctx_t *ctx)
{
	const char *missing = "Missing";
	byte        data[2][64 * 64 * 4];	// 2 * 64x64 rgba (8x8 chars)
	tex_t       tex[2] = {
		{	.width = 64,
			.height = 64,
			.format = tex_rgba,
			.loaded = 1,
			.data = data[0],
		},
		{	.width = 64,
			.height = 64,
			.format = tex_rgba,
			.loaded = 1,
			.data = data[1],
		},
	};

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

	bspctx_t   *bctx = ctx->bsp_context;
	bctx->notexture.tex = Vulkan_LoadTexArray (ctx, tex, 2, 1, "notexture");
}

void
Vulkan_Bsp_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "bsp init");

	bspctx_t   *bctx = calloc (1, sizeof (bspctx_t));
	ctx->bsp_context = bctx;

	bctx->light_scrap = QFV_CreateScrap (device, "lightmap_atlas", 2048,
										 tex_frgba, ctx->staging);
	size_t      size = QFV_ScrapSize (bctx->light_scrap);
	bctx->light_stage = QFV_CreateStagingBuffer (device, "lightmap", size,
												 ctx->cmdpool);

	create_default_skys (ctx);
	create_notexture (ctx);

	DARRAY_INIT (&bctx->registered_textures, 64);

	bctx->main_pass.num_queues = 4;//solid, sky, water, transparent
	bctx->main_pass.draw_queues = malloc (bctx->main_pass.num_queues
										  * sizeof (bsp_drawset_t));
	for (int i = 0; i < bctx->main_pass.num_queues; i++) {
		DARRAY_INIT (&bctx->main_pass.draw_queues[i], 64);
	}

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&bctx->frames, frames);
	DARRAY_RESIZE (&bctx->frames, frames);
	bctx->frames.grow = 0;

	bctx->depth = Vulkan_CreateGraphicsPipeline (ctx, "bsp_depth");
	bctx->gbuf = Vulkan_CreateGraphicsPipeline (ctx, "bsp_gbuf");
	bctx->skybox = Vulkan_CreateGraphicsPipeline (ctx, "bsp_skybox");
	bctx->skysheet = Vulkan_CreateGraphicsPipeline (ctx, "bsp_skysheet");
	bctx->turb = Vulkan_CreateGraphicsPipeline (ctx, "bsp_turb");
	bctx->layout = Vulkan_CreatePipelineLayout (ctx, "quakebsp_layout");
	bctx->sampler = Vulkan_CreateSampler (ctx, "quakebsp_sampler");

	size_t      entid_count = Vulkan_Scene_MaxEntities (ctx);
	size_t      entid_size = entid_count * sizeof (uint32_t);
	size_t atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t atom_mask = atom - 1;
	entid_size = (entid_size + atom_mask) & ~atom_mask;
	bctx->entid_buffer
		= QFV_CreateBuffer (device, frames * entid_size,
							VK_BUFFER_USAGE_TRANSFER_DST_BIT
							| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, bctx->entid_buffer,
						 "buffer:bsp:entid");
	bctx->entid_memory
		= QFV_AllocBufferMemory (device, bctx->entid_buffer,
								 VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
								 frames * entid_size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 bctx->entid_memory, "memory:bsp:entid");
	QFV_BindBufferMemory (device,
						  bctx->entid_buffer, bctx->entid_memory, 0);
	uint32_t   *entid_data;
	dfunc->vkMapMemory (device->dev, bctx->entid_memory, 0,
						frames * entid_size, 0, (void **) &entid_data);

	for (size_t i = 0; i < frames; i++) {
		__auto_type bframe = &bctx->frames.a[i];

		DARRAY_INIT (&bframe->cmdSet, QFV_bspNumPasses);
		DARRAY_RESIZE (&bframe->cmdSet, QFV_bspNumPasses);
		bframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &bframe->cmdSet);

		for (int j = 0; j < QFV_bspNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 bframe->cmdSet.a[i],
								 va (ctx->va_ctx, "cmd:bsp:%zd:%s", i,
									 bsp_pass_names[j]));
		}
		bframe->entid_data = entid_data + i * entid_count;
		bframe->entid_offset = i * entid_size;
	}

	bctx->skybox_descriptor
		= Vulkan_CreateTextureDescriptor (ctx, bctx->default_skybox,
										  bctx->sampler);
	bctx->notexture.descriptor
		= Vulkan_CreateTextureDescriptor (ctx, bctx->notexture.tex,
										  bctx->sampler);

	r_notexture_mip->render = &bctx->notexture;

	qfvPopDebug (ctx);
}

void
Vulkan_Bsp_Shutdown (struct vulkan_ctx_s *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;

	for (size_t i = 0; i < bctx->frames.size; i++) {
		__auto_type bframe = &bctx->frames.a[i];
		free (bframe->cmdSet.a);
	}

	dfunc->vkDestroyPipeline (device->dev, bctx->depth, 0);
	dfunc->vkDestroyPipeline (device->dev, bctx->gbuf, 0);
	dfunc->vkDestroyPipeline (device->dev, bctx->skybox, 0);
	dfunc->vkDestroyPipeline (device->dev, bctx->skysheet, 0);
	dfunc->vkDestroyPipeline (device->dev, bctx->turb, 0);

	DARRAY_CLEAR (&bctx->registered_textures);
	for (int i = 0; i < bctx->main_pass.num_queues; i++) {
		DARRAY_CLEAR (&bctx->main_pass.draw_queues[i]);
	}

	free (bctx->faces);
	free (bctx->models);

	free (bctx->main_pass.draw_queues);
	for (int i = 0; i < bctx->num_models; i++) {
		DARRAY_CLEAR (&bctx->main_pass.instances[i].entities);
	}
	free (bctx->main_pass.instances);
	DARRAY_CLEAR (&bctx->frames);

	QFV_DestroyStagingBuffer (bctx->light_stage);
	QFV_DestroyScrap (bctx->light_scrap);
	if (bctx->vertex_buffer) {
		dfunc->vkDestroyBuffer (device->dev, bctx->vertex_buffer, 0);
		dfunc->vkFreeMemory (device->dev, bctx->vertex_memory, 0);
	}
	if (bctx->index_buffer) {
		dfunc->vkDestroyBuffer (device->dev, bctx->index_buffer, 0);
		dfunc->vkFreeMemory (device->dev, bctx->index_memory, 0);
	}
	dfunc->vkDestroyBuffer (device->dev, bctx->entid_buffer, 0);
	dfunc->vkFreeMemory (device->dev, bctx->entid_memory, 0);

	if (bctx->skybox_tex) {
		Vulkan_UnloadTex (ctx, bctx->skybox_tex);
	}
	if (bctx->notexture.tex) {
		Vulkan_UnloadTex (ctx, bctx->notexture.tex);
	}

	dfunc->vkDestroyImageView (device->dev, bctx->default_skysheet->view, 0);
	dfunc->vkDestroyImage (device->dev, bctx->default_skysheet->image, 0);

	dfunc->vkDestroyImageView (device->dev, bctx->default_skybox->view, 0);
	dfunc->vkDestroyImage (device->dev, bctx->default_skybox->image, 0);
	dfunc->vkFreeMemory (device->dev, bctx->default_skybox->memory, 0);
	free (bctx->default_skybox);
}

void
Vulkan_LoadSkys (const char *sky, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	const char *name;
	int         i;
	tex_t      *tex;
	static const char *sky_suffix[] = { "ft", "bk", "up", "dn", "rt", "lf"};

	if (bctx->skybox_tex) {
		Vulkan_UnloadTex (ctx, bctx->skybox_tex);
		Vulkan_FreeTexture (ctx, bctx->skybox_descriptor);
	}
	bctx->skybox_tex = 0;

	if (!sky || !*sky) {
		sky = r_skyname;
	}

	if (!*sky || !strcasecmp (sky, "none")) {
		Sys_MaskPrintf (SYS_vulkan, "Skybox unloaded\n");
		bctx->skybox_descriptor
			= Vulkan_CreateTextureDescriptor (ctx, bctx->default_skybox,
											  bctx->sampler);
		return;
	}

	name = va (ctx->va_ctx, "env/%s_map", sky);
	tex = LoadImage (name, 1);
	if (tex) {
		bctx->skybox_tex = Vulkan_LoadEnvMap (ctx, tex, sky);
		Sys_MaskPrintf (SYS_vulkan, "Loaded %s\n", name);
	} else {
		int         failed = 0;
		tex_t      *sides[6] = { };

		for (i = 0; i < 6; i++) {
			name = va (ctx->va_ctx, "env/%s%s", sky, sky_suffix[i]);
			tex = LoadImage (name, 1);
			if (!tex) {
				Sys_MaskPrintf (SYS_vulkan, "Couldn't load %s\n", name);
				// also look in gfx/env, where Darkplaces looks for skies
				name = va (ctx->va_ctx, "gfx/env/%s%s", sky, sky_suffix[i]);
				tex = LoadImage (name, 1);
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
			bctx->skybox_tex = Vulkan_LoadEnvSides (ctx, sides, sky);
		}
		for (i = 0; i < 6; i++) {
			free (sides[i]);
		}
	}
	if (bctx->skybox_tex) {
		bctx->skybox_descriptor
			= Vulkan_CreateTextureDescriptor (ctx, bctx->skybox_tex,
											  bctx->sampler);
		Sys_MaskPrintf (SYS_vulkan, "Skybox %s loaded\n", sky);
	}
}
