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
#include "QF/image.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "QF/simd/types.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct bsp_push_constants_s {
	mat4f_t     Model;
	quat_t      fog;
	float       time;
	float       alpha;
} bsp_push_constants_t;

static const char * __attribute__((used)) bsp_pass_names[] = {
	"depth",
	"g-buffer",
	"sky",
	"turb",
};

static QFV_Subpass subpass_map[] = {
	QFV_passDepth,			// QFV_bspDepth
	QFV_passGBuffer,		// QFV_bspGBuffer
	QFV_passTranslucent,	// QFV_bspSky
	QFV_passTranslucent,	// QFV_bspTurb
};

static float identity[] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
};

static vulktex_t vulkan_notexture = { };

#define ALLOC_CHUNK 64

typedef struct bsppoly_s {
	uint32_t    count;
	uint32_t    indices[1];
} bsppoly_t;

#define CHAIN_SURF_F2B(surf,chain)							\
	({ 														\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			inst = get_instsurf (bctx);						\
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
			inst = get_instsurf (bctx);						\
		inst->surface = (surf);								\
		inst->tex_chain = (chain);							\
		(chain) = inst;										\
		inst;												\
	})

#define GET_RELEASE(type,name) \
static inline type *												\
get_##name (bspctx_t *bctx)											\
{																	\
	type       *ele;												\
	if (!bctx->free_##name##s) {									\
		int         i;												\
		bctx->free_##name##s = calloc (ALLOC_CHUNK, sizeof (type));	\
		for (i = 0; i < ALLOC_CHUNK - 1; i++)						\
			bctx->free_##name##s[i]._next = &bctx->free_##name##s[i + 1];	\
	}																\
	ele = bctx->free_##name##s;										\
	bctx->free_##name##s = ele->_next;								\
	ele->_next = 0;													\
	*bctx->name##s_tail = ele;										\
	bctx->name##s_tail = &ele->_next;								\
	return ele;														\
}																	\
static inline void													\
release_##name##s (bspctx_t *bctx)									\
{																	\
	if (bctx->name##s) {											\
		*bctx->name##s_tail = bctx->free_##name##s;					\
		bctx->free_##name##s = bctx->name##s;						\
		bctx->name##s = 0;											\
		bctx->name##s_tail = &bctx->name##s;						\
	}																\
}

GET_RELEASE (elechain_t, elechain)
GET_RELEASE (instsurf_t, static_instsurf)
GET_RELEASE (instsurf_t, instsurf)

static void
add_texture (texture_t *tx, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	vulktex_t  *tex = tx->render;
	if (tex->tex) {
		DARRAY_APPEND (&bctx->texture_chains, tex);
		tex->descriptor = Vulkan_CreateTextureDescriptor (ctx, tex->tex,
														  bctx->sampler);
	}
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
}

static void
init_surface_chains (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	release_static_instsurfs (bctx);
	release_instsurfs (bctx);

	for (unsigned i = 0; i < brush->nummodelsurfaces; i++) {
		brush->surfaces[i].instsurf = get_static_instsurf (bctx);
		brush->surfaces[i].instsurf->surface = &brush->surfaces[i];
	}
}

static inline void
clear_tex_chain (vulktex_t *tex)
{
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
}

static void
clear_texture_chains (bspctx_t *bctx)
{
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		if (!bctx->texture_chains.a[i])
			continue;
		clear_tex_chain (bctx->texture_chains.a[i]);
	}
	clear_tex_chain (r_notexture_mip->render);
	release_elechains (bctx);
	release_instsurfs (bctx);
}

void
Vulkan_ClearElements (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	release_elechains (bctx);
}

static inline void
chain_surface (msurface_t *surf, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	instsurf_t *is;

	if (surf->flags & SURF_DRAWSKY) {
		is = CHAIN_SURF_F2B (surf, bctx->sky_chain);
	} else if ((surf->flags & SURF_DRAWTURB)
			   || (bctx->color && bctx->color[3] < 1.0)) {
		is = CHAIN_SURF_B2F (surf, bctx->waterchain);
	} else {
		texture_t  *tx;
		vulktex_t  *tex;

		if (!surf->texinfo->texture->anim_total)
			tx = surf->texinfo->texture;
		else
			tx = R_TextureAnimation (bctx->entity, surf);
		tex = tx->render;
		is = CHAIN_SURF_F2B (surf, tex->tex_chain);
	}
	is->transform = bctx->transform;
	is->color = bctx->color;
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
	bctx->texture_chains.size = 0;
}

void
Vulkan_RegisterTextures (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	int         i;
	model_t    *m;
	mod_brush_t *brush = &r_refdef.worldmodel->brush;

	clear_textures (ctx);
	init_surface_chains (brush, ctx);
	add_texture (r_notexture_mip, ctx);
	register_textures (brush, ctx);
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
		register_textures (brush, ctx);
	}
}

static elechain_t *
add_elechain (vulktex_t *tex, bspctx_t *bctx)
{
	elechain_t *ec;

	ec = get_elechain (bctx);
	ec->first_index = 0;
	ec->index_count = 0;
	ec->transform = 0;
	ec->color = 0;
	*tex->elechain_tail = ec;
	tex->elechain_tail = &ec->next;
	return ec;
}

static void
count_verts_inds (model_t **models, msurface_t *surf,
				  uint32_t *verts, uint32_t *inds)
{
	*verts = surf->numedges;
	*inds = surf->numedges + 1;
}

static bsppoly_t *
build_surf_displist (model_t **models, msurface_t *surf, int base,
					 bspvert_t **vert_list)
{
	mod_brush_t *brush;
	if (surf->model_index < 0) {
		// instance model
		brush = &models[~surf->model_index]->brush;
	} else {
		// main or sub model
		brush = &r_refdef.worldmodel->brush;
	}
	mvertex_t  *vertices  = brush->vertexes;
	medge_t    *edges     = brush->edges;
	int        *surfedges = brush->surfedges;

	// surf->polys is set to the next slot before the call
	bsppoly_t  *poly = (bsppoly_t *) surf->polys;
	// create a triangle fan
	int         numverts = surf->numedges;
	poly->count = numverts + 1;	// +1 for primitive restart
	for (int i = 0; i < numverts; i++) {
		poly->indices[i] = base + i;
	}
	poly->indices[numverts] = -1;	// primitive restart
	surf->polys = (glpoly_t *) poly;

	bspvert_t  *verts = *vert_list;
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
	*vert_list += numverts;
	return (bsppoly_t *) &poly->indices[poly->count];
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

	// run through all surfaces, chaining them to their textures, thus
	// effectively sorting the surfaces by texture (without worrying about
	// surface order on the same texture chain).
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		// and non-bsp models don't have surfaces.
		if (!m || *m->path == '*' || m->type != mod_brush)
			continue;
		mod_brush_t *brush = &m->brush;
		dmodel_t    *dm = brush->submodels;
		for (unsigned j = 0; j < brush->numsurfaces; j++) {
			if (j == dm->firstface + dm->numfaces) {
				// move on to the next sub-model
				dm++;
				if (dm == brush->submodels + brush->numsubmodels) {
					// limit the surfaces
					// probably never hit
					Sys_Printf ("R_BuildDisplayLists: too many surfaces\n");
					brush->numsurfaces = j;
					break;
				}
			}
			msurface_t *surf = brush->surfaces + j;
			surf->model_index = dm - brush->submodels;
			if (!surf->model_index && m != r_refdef.worldmodel) {
				surf->model_index = -1 - i;	// instanced model
			}
			// append surf to the texture chain
			vulktex_t *tex = surf->texinfo->texture->render;
			CHAIN_SURF_F2B (surf, tex->tex_chain);
		}
	}
	// All vertices from all brush models go into one giant vbo.
	uint32_t    vertex_count = 0;
	uint32_t    index_count = 0;
	uint32_t    poly_count = 0;
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		vulktex_t  *tex = bctx->texture_chains.a[i];
		for (instsurf_t *is = tex->tex_chain; is; is = is->tex_chain) {
			uint32_t    verts, inds;
			count_verts_inds (models, is->surface, &verts, &inds);
			vertex_count += verts;
			index_count += inds;
			poly_count++;
		}
	}

	size_t atom = device->physDev->properties.limits.nonCoherentAtomSize;
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
	// primitive restart markers + poly_count index counts. The primitive
	// restart markers are included in index_count, so poly_count below is
	// for the per-polygon index count.
	// so each polygon within the list:
	//     count        includes the end of primitive marker
	//     index        count-1 indices
	//     index
	//     ...
	//     "end of primitive" (~0u)
	free (bctx->polys);
	bctx->polys = malloc ((index_count + poly_count) * sizeof (uint32_t));

	// All usable surfaces have been chained to the (base) texture they use.
	// Run through the textures, using their chains to build display lists.
	// For animated textures, if a surface is on one texture of the group, it
	// will effectively be on all (just one at a time).
	int         count = 0;
	int         vertex_index_base = 0;
	bsppoly_t  *poly = bctx->polys;
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		vulktex_t  *tex = bctx->texture_chains.a[i];

		for (instsurf_t *is = tex->tex_chain; is; is = is->tex_chain) {
			msurface_t *surf = is->surface;

			surf->polys = (glpoly_t *) poly;
			poly = build_surf_displist (models, surf, vertex_index_base,
										&vertices);
			vertex_index_base += surf->numedges;
			count++;
		}
	}
	clear_texture_chains (bctx);
	Sys_MaskPrintf (SYS_vulkan,
					"R_BuildDisplayLists: verts:%u, inds:%u, "
					"polys:%u (%d) %zd\n",
					vertex_count, index_count, poly_count, count,
					((size_t) poly - (size_t) bctx->polys) / sizeof(uint32_t));
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
}

static void
R_DrawBrushModel (entity_t *e, vulkan_ctx_t *ctx)
{
	float       dot, radius;
	model_t    *model;
	plane_t    *plane;
	msurface_t *surf;
	qboolean    rotated;
	vec3_t      mins, maxs;
	vec4f_t     org;
	mod_brush_t *brush;
	bspctx_t   *bctx = ctx->bsp_context;

	bctx->entity = e;
	bctx->transform = e->renderer.full_transform;
	bctx->color = e->renderer.colormod;

	model = e->renderer.model;
	brush = &model->brush;
	mat4f_t mat;
	Transform_GetWorldMatrix (e->transform, mat);
	memcpy (e->renderer.full_transform, mat, sizeof (mat));//FIXME
	if (mat[0][0] != 1 || mat[1][1] != 1 || mat[2][2] != 1) {
		rotated = true;
		radius = model->radius;
		if (R_CullSphere (r_refdef.frustum, (vec_t*)&mat[3], radius)) { //FIXME
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

		org[0] = DotProduct (temp, mat[0]);
		org[1] = DotProduct (temp, mat[1]);
		org[2] = DotProduct (temp, mat[2]);
	}

	surf = &brush->surfaces[brush->firstmodelsurface];

	for (unsigned i = 0; i < brush->nummodelsurfaces; i++, surf++) {
		// find the node side on which we are
		plane = surf->plane;

		dot = PlaneDiff (org, plane);

		// enqueue the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
			|| (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			chain_surface (surf, ctx);
		}
	}
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
get_side (mnode_t *node)
{
	// find the node side on which we are
	vec4f_t     org = r_refdef.frame.position;

	return dotf (org, node->plane)[0] < 0;
}

static inline void
visit_node (mod_brush_t *brush, mnode_t *node, int side, vulkan_ctx_t *ctx)
{
	int         c;
	msurface_t *surf;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	// seems to be microscopically faster even on modern hardware
	side = (-side) & SURF_PLANEBACK;
	// chain any visible surfaces on the node that face the camera.
	// not all nodes have any surfaces to draw (purely a split plane)
	if ((c = node->numsurfaces)) {
		surf = brush->surfaces + node->firstsurface;
		for (; c; c--, surf++) {
			if (surf->visframe != r_visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			// if side and the surface facing differ, then the camera is
			// on backside of the surface
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;               // wrong side

			chain_surface (surf, ctx);
		}
	}
}

static inline int
test_node (mod_brush_t *brush, int node_id)
{
	if (node_id < 0)
		return 0;
	if (r_node_visframes[node_id] != r_visframecount)
		return 0;
	mnode_t    *node = brush->nodes + node_id;
	if (R_CullBox (r_refdef.frustum, node->minmaxs, node->minmaxs + 3))
		return 0;
	return 1;
}

static void
R_VisitWorldNodes (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
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
		while (test_node (brush, node_id)) {
			mnode_t    *node = brush->nodes + node_id;
			side = get_side (node);
			front = node->children[side];
			if (test_node (brush, front)) {
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
			visit_node (brush, node, side, ctx);
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
			visit_node (brush, node, side, ctx);
			node_id = node->children[side ^ 1];
			continue;
		}
		break;
	}
}

static void
push_transform (vec_t *transform, VkPipelineLayout layout,
				qfv_device_t *device, VkCommandBuffer cmd)
{
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (bsp_push_constants_t, Model),
			sizeof (mat4f_t), transform },
	};
	QFV_PushConstants (device, cmd, layout, 1, push_constants);
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
	};
	QFV_PushConstants (device, cmd, layout, 3, push_constants);
}

static void
draw_elechain (elechain_t *ec, VkPipelineLayout layout, qfv_device_t *device,
			   VkCommandBuffer cmd)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	if (ec->transform) {
		push_transform (ec->transform, layout, device, cmd);
	} else {
		//FIXME should cache current transform
		push_transform (identity, layout, device, cmd);
	}
	if (ec->index_count) {
		dfunc->vkCmdDrawIndexed (cmd, ec->index_count, 1, ec->first_index,
								 0, 0);
	}
}

static void
reset_elechain (elechain_t *ec)
{
	ec->first_index = 0;
	ec->index_count = 0;
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

	VkDeviceSize offsets[] = { 0 };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &bctx->vertex_buffer, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, bctx->index_buffer, bframe->index_offset,
								 VK_INDEX_TYPE_UINT32);

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 1, sets, 0, 0);

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

	bctx->default_color[3] = 1;
	QuatCopy (bctx->default_color, bctx->last_color);

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

	bctx->default_color[3] = bound (0, r_wateralpha, 1);

	QuatCopy (bctx->default_color, bctx->last_color);

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

	bctx->default_color[3] = 1;
	QuatCopy (bctx->default_color, bctx->last_color);

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

static inline void
add_surf_elements (vulktex_t *tex, instsurf_t *is, elechain_t **ec,
				   bspctx_t *bctx, bspframe_t *bframe)
{
	bsppoly_t  *poly = (bsppoly_t *) is->surface->polys;

	if (!tex->elechain) {
		(*ec) = add_elechain (tex, bctx);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*ec)->first_index = bframe->index_count;
	}
	if (is->transform != (*ec)->transform || is->color != (*ec)->color) {
		(*ec) = add_elechain (tex, bctx);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*ec)->first_index = bframe->index_count;
	}
	memcpy (bframe->index_data + bframe->index_count,
			poly->indices, poly->count * sizeof (poly->indices[0]));
	(*ec)->index_count += poly->count;
	bframe->index_count += poly->count;
}

static void
build_tex_elechain (vulktex_t *tex, bspctx_t *bctx, bspframe_t *bframe)
{
	instsurf_t *is;
	elechain_t *ec = 0;

	for (is = tex->tex_chain; is; is = is->tex_chain) {
		// emit the polygon indices for the surface to the texture's
		// element chain
		add_surf_elements (tex, is, &ec, bctx, bframe);
	}
}

void
Vulkan_DrawWorld (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	entity_t    worldent;
	mod_brush_t *brush;

	clear_texture_chains (bctx);	// do this first for water and skys
	bframe->index_count = 0;

	memset (&worldent, 0, sizeof (worldent));
	worldent.renderer.model = r_refdef.worldmodel;
	brush = &r_refdef.worldmodel->brush;

	bctx->entity = &worldent;
	bctx->transform = 0;
	bctx->color = 0;

	R_VisitWorldNodes (brush, ctx);
	if (!bctx->vertex_buffer) {
		return;
	}
	if (r_drawentities) {
		for (size_t i = 0; i < r_ent_queue->ent_queues[mod_brush].size; i++) {
			entity_t   *ent = r_ent_queue->ent_queues[mod_brush].a[i];
			R_DrawBrushModel (ent, ctx);
		}
	}

	bsp_begin (rFrame);

	push_transform (identity, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspDepth]);
	push_transform (identity, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspGBuffer]);
	bsp_push_constants_t frag_constants = { .time = vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspGBuffer]);
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		vulktex_t  *tex;
		elechain_t *ec = 0;

		tex = bctx->texture_chains.a[i];

		build_tex_elechain (tex, bctx, bframe);

		bind_texture (tex, 1, bctx->layout, dfunc,
					  bframe->cmdSet.a[QFV_bspGBuffer]);

		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, bctx->layout, device,
						   bframe->cmdSet.a[QFV_bspDepth]);
			draw_elechain (ec, bctx->layout, device,
						   bframe->cmdSet.a[QFV_bspGBuffer]);
			reset_elechain (ec);
		}
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	bsp_end (ctx);
}

void
Vulkan_Bsp_Flush (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	size_t      atom = device->physDev->properties.limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
	size_t      offset = bframe->index_offset;
	size_t      size = bframe->index_count * sizeof (uint32_t);

	if (!bframe->index_count) {
		return;
	}
	offset &= ~atom_mask;
	size = (size + atom_mask) & ~atom_mask;

	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		bctx->index_memory, offset, size
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
}

void
Vulkan_DrawWaterSurfaces (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	instsurf_t *is;
	vulktex_t  *tex = 0;
	elechain_t *ec = 0;

	if (!bctx->waterchain)
		return;

	turb_begin (rFrame);
	push_transform (identity, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspTurb]);
	bsp_push_constants_t frag_constants = {
		.time = vr_data.realtime,
		.alpha = r_wateralpha
	};
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspTurb]);
	for (is = bctx->waterchain; is; is = is->tex_chain) {
		msurface_t *surf = is->surface;
		if (tex != surf->texinfo->texture->render) {
			if (tex) {
				bind_texture (tex, 1, bctx->layout, dfunc,
							  bframe->cmdSet.a[QFV_bspTurb]);
				for (ec = tex->elechain; ec; ec = ec->next) {
					draw_elechain (ec, bctx->layout, device,
								   bframe->cmdSet.a[QFV_bspTurb]);
					reset_elechain (ec);
				}
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture->render;
		}
		// emit the polygon indices for the surface to the texture's
		// element chain
		add_surf_elements (tex, is, &ec, bctx, bframe);
	}
	if (tex) {
		bind_texture (tex, 1, bctx->layout, dfunc,
					  bframe->cmdSet.a[QFV_bspTurb]);
		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, bctx->layout, device,
						   bframe->cmdSet.a[QFV_bspTurb]);
			reset_elechain (ec);
		}
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	turb_end (ctx);

	bctx->waterchain = 0;
	bctx->waterchain_tail = &bctx->waterchain;
}

void
Vulkan_DrawSky (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	instsurf_t *is;
	vulktex_t  *tex = 0;
	elechain_t *ec = 0;

	if (!bctx->sky_chain)
		return;

	sky_begin (rFrame);
	vulktex_t skybox = { .descriptor = bctx->skybox_descriptor };
	bind_texture (&skybox, 2, bctx->layout, dfunc,
				  bframe->cmdSet.a[QFV_bspSky]);
	push_transform (identity, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspSky]);
	bsp_push_constants_t frag_constants = { .time = vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, device,
					bframe->cmdSet.a[QFV_bspSky]);
	for (is = bctx->sky_chain; is; is = is->tex_chain) {
		msurface_t *surf = is->surface;
		if (tex != surf->texinfo->texture->render) {
			if (tex) {
				bind_texture (tex, 1, bctx->layout, dfunc,
							  bframe->cmdSet.a[QFV_bspSky]);
				for (ec = tex->elechain; ec; ec = ec->next) {
					draw_elechain (ec, bctx->layout, device,
								   bframe->cmdSet.a[QFV_bspSky]);
					reset_elechain (ec);
				}
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture->render;
		}
		// emit the polygon indices for the surface to the texture's
		// element chain
		add_surf_elements (tex, is, &ec, bctx, bframe);
	}
	if (tex) {
		bind_texture (tex, 1, bctx->layout, dfunc,
					  bframe->cmdSet.a[QFV_bspSky]);
		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, bctx->layout, device,
						   bframe->cmdSet.a[QFV_bspSky]);
			reset_elechain (ec);
		}
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	sky_end (ctx);

	bctx->sky_chain = 0;
	bctx->sky_chain_tail = &bctx->sky_chain;
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

void
Vulkan_Bsp_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	r_notexture_mip->render = &vulkan_notexture;

	qfvPushDebug (ctx, "bsp init");

	bspctx_t   *bctx = calloc (1, sizeof (bspctx_t));
	ctx->bsp_context = bctx;

	bctx->waterchain_tail = &bctx->waterchain;
	bctx->sky_chain_tail = &bctx->sky_chain;
	bctx->static_instsurfs_tail = &bctx->static_instsurfs;
	bctx->elechains_tail = &bctx->elechains;
	bctx->instsurfs_tail = &bctx->instsurfs;

	bctx->light_scrap = QFV_CreateScrap (device, "lightmap_atlas", 2048,
										 tex_frgba, ctx->staging);
	size_t      size = QFV_ScrapSize (bctx->light_scrap);
	bctx->light_stage = QFV_CreateStagingBuffer (device, "lightmap", size,
												 ctx->cmdpool);

	create_default_skys (ctx);

	DARRAY_INIT (&bctx->texture_chains, 64);

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
	}

	bctx->skybox_descriptor
		= Vulkan_CreateTextureDescriptor (ctx, bctx->default_skybox,
										  bctx->sampler);

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
	DARRAY_CLEAR (&bctx->texture_chains);
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

	if (bctx->skybox_tex) {
		Vulkan_UnloadTex (ctx, bctx->skybox_tex);
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
