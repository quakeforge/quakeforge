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
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vrect.h"

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lightmap.h"
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

#include "r_internal.h"
#include "vid_vulkan.h"

static const char *bsp_pass_names[] = {
	"depth",
	"g-buffer",
	"translucent",
};

static float identity[] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
};

#define ALLOC_CHUNK 64

typedef struct bsppoly_s {
	uint32_t    count;
	uint32_t    indices[1];
} bsppoly_t;

#define CHAIN_SURF_F2B(surf,chain)							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			(surf)->tinst = inst = get_instsurf (bctx);		\
		inst->surface = (surf);								\
		*(chain##_tail) = inst;								\
		(chain##_tail) = &inst->tex_chain;					\
		*(chain##_tail) = 0;								\
	} while (0)

#define CHAIN_SURF_B2F(surf,chain) 							\
	do { 													\
		instsurf_t *inst = (surf)->instsurf;				\
		if (__builtin_expect(!inst, 1))						\
			(surf)->tinst = inst = get_instsurf (bctx);		\
		inst->surface = (surf);								\
		inst->tex_chain = (chain);							\
		(chain) = inst;										\
	} while (0)

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
GET_RELEASE (elements_t, elements)
GET_RELEASE (instsurf_t, static_instsurf)
GET_RELEASE (instsurf_t, instsurf)

static void
add_texture (texture_t *tx, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	vulktex_t  *tex = tx->render;
	DARRAY_APPEND (&bctx->texture_chains, tex);
	tex->tex_chain = 0;
	tex->tex_chain_tail = &tex->tex_chain;
	tex->elechain = 0;
	tex->elechain_tail = &tex->elechain;
}

static void
init_surface_chains (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	int         i;

	release_static_instsurfs (bctx);
	release_instsurfs (bctx);

	for (i = 0; i < brush->nummodelsurfaces; i++) {
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
	release_elementss (bctx);
	release_instsurfs (bctx);
}

void
Vulkan_ClearElements (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	release_elechains (bctx);
	release_elementss (bctx);
}

static void
update_lightmap (mod_brush_t *brush, msurface_t *surf, vulkan_ctx_t *ctx)
{
	int         maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		if (d_lightstylevalue[surf->styles[maps]] != surf->cached_light[maps])
			goto dynamic;

	if ((surf->dlightframe == r_framecount) || surf->cached_dlight) {
dynamic:
		if (r_dynamic->int_val)
			Vulkan_BuildLightMap (brush, surf, ctx);
	}
}

static inline void
chain_surface (mod_brush_t *brush, msurface_t *surf, vec_t *transform,
			   float *color, vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	instsurf_t *is;

	if (surf->flags & SURF_DRAWSKY) {
		CHAIN_SURF_F2B (surf, bctx->sky_chain);
	} else if ((surf->flags & SURF_DRAWTURB) || (color && color[3] < 1.0)) {
		CHAIN_SURF_B2F (surf, bctx->waterchain);
	} else {
		texture_t  *tx;
		vulktex_t  *tex;

		if (!surf->texinfo->texture->anim_total)
			tx = surf->texinfo->texture;
		else
			tx = R_TextureAnimation (surf);
		tex = tx->render;
		CHAIN_SURF_F2B (surf, tex->tex_chain);

		update_lightmap (brush, surf, ctx);
	}
	if (!(is = surf->instsurf))
		is = surf->tinst;
	is->transform = transform;
	is->color = color;
}

static void
register_textures (mod_brush_t *brush, vulkan_ctx_t *ctx)
{
	int         i;
	texture_t  *tex;

	for (i = 0; i < brush->numtextures; i++) {
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
	mod_brush_t *brush = &r_worldentity.model->brush;

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
		if (m == r_worldentity.model || m->type != mod_brush)
			continue;
		brush = &m->brush;
		brush->numsubmodels = 1; // no support for submodels in non-world model
		register_textures (brush, ctx);
	}
}

static elechain_t *
add_elechain (vulktex_t *tex, int ec_index, bspctx_t *bctx)
{
	elechain_t *ec;

	ec = get_elechain (bctx);
	ec->elements = get_elements (bctx);
	ec->index = ec_index;
	ec->transform = 0;
	ec->color = 0;
	*tex->elechain_tail = ec;
	tex->elechain_tail = &ec->next;
	return ec;
}

static void
count_verts_inds (model_t **models, msurface_t *fa,
				  uint32_t *verts, uint32_t *inds)
{
	*verts = fa->numedges;
	*inds = fa->numedges + 1;
}

static bsppoly_t *
build_surf_displist (model_t **models, msurface_t *fa, int base,
					 bspvert_t **vert_list)
{
	int         numverts;
	int         numindices;
	int         i;
	vec_t      *vec;
	mvertex_t  *vertices;
	medge_t    *edges;
	int        *surfedges;
	int         index;
	bspvert_t  *verts;
	bsppoly_t  *poly;
	uint32_t   *ind;
	float       s, t;
	mod_brush_t *brush;

	if (fa->ec_index < 0) {
		// instance model
		brush = &models[~fa->ec_index]->brush;
	} else {
		// main or sub model
		brush = &r_worldentity.model->brush;
	}
	vertices  = brush->vertexes;
	edges     = brush->edges;
	surfedges = brush->surfedges;
	// create a triangle fan
	numverts = fa->numedges;
	numindices = numverts + 1;
	verts = *vert_list;
	// surf->polys is set to the next slot before the call
	poly = (bsppoly_t *) fa->polys;
	poly->count = numindices;
	for (i = 0, ind = poly->indices; i < numverts; i++) {
		*ind++ = base + i;
	}
	*ind++ = -1;	// end of primitive
	fa->polys = (glpoly_t *) poly;

	for (i = 0; i < numverts; i++) {
		index = surfedges[fa->firstedge + i];
		if (index > 0) {
			vec = vertices[edges[index].v[0]].position;
		} else {
			vec = vertices[edges[-index].v[1]].position;
		}

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
	*vert_list += numverts;
	return (bsppoly_t *) &poly->indices[numindices];
}

void
Vulkan_BuildDisplayLists (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	int         i, j;
	int         vertex_index_base;
	model_t    *m;
	dmodel_t   *dm;
	msurface_t *surf;
	qfv_stagebuf_t *stage;
	bspvert_t  *vertices;
	bsppoly_t  *poly;
	mod_brush_t *brush;

	QuatSet (0, 0, sqrt(0.5), sqrt(0.5), bctx->sky_fix);	// proper skies
	QuatSet (0, 0, 0, 1, bctx->sky_rotation[0]);
	QuatCopy (bctx->sky_rotation[0], bctx->sky_rotation[1]);
	QuatSet (0, 0, 0, 0, bctx->sky_velocity);
	QuatExp (bctx->sky_velocity, bctx->sky_velocity);
	bctx->sky_time = vr_data.realtime;

	// now run through all surfaces, chaining them to their textures, thus
	// effectively sorting the surfaces by texture (without worrying about
	// surface order on the same texture chain).
	for (i = 0; i < num_models; i++) {
		m = models[i];
		if (!m)
			continue;
		// sub-models are done as part of the main model
		if (*m->path == '*' || m->type != mod_brush)
			continue;
		brush = &m->brush;
		// non-bsp models don't have surfaces.
		dm = brush->submodels;
		for (j = 0; j < brush->numsurfaces; j++) {
			vulktex_t  *tex;
			if (j == dm->firstface + dm->numfaces) {
				dm++;
				if (dm - brush->submodels == brush->numsubmodels) {
					// limit the surfaces
					// probably never hit
					Sys_Printf ("R_BuildDisplayLists: too many surfaces\n");
					brush->numsurfaces = j;
					break;
				}
			}
			surf = brush->surfaces + j;
			surf->ec_index = dm - brush->submodels;
			if (!surf->ec_index && m != r_worldentity.model)
				surf->ec_index = -1 - i;	// instanced model
			tex = surf->texinfo->texture->render;
			// append surf to the texture chain
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
	stage = QFV_CreateStagingBuffer (device, "bsp", vertex_buffer_size,
									 ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	vertices = QFV_PacketExtend (packet, vertex_buffer_size);
	vertex_index_base = 0;
	// holds all the polygon definitions (count + indices)
	bctx->polys = malloc ((index_count + poly_count) * sizeof (uint32_t));

	// All usable surfaces have been chained to the (base) texture they use.
	// Run through the textures, using their chains to build display maps.
	// For animated textures, if a surface is on one texture of the group, it
	// will be on all.
	poly = bctx->polys;
	int count = 0;
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		vulktex_t  *tex;
		instsurf_t *is;
		elechain_t *ec = 0;

		tex = bctx->texture_chains.a[i];

		for (is = tex->tex_chain; is; is = is->tex_chain) {
			msurface_t *surf = is->surface;
			if (!tex->elechain) {
				ec = add_elechain (tex, surf->ec_index, bctx);
			}
			if (surf->ec_index != ec->index) {	// next sub-model
				ec = add_elechain (tex, surf->ec_index, bctx);
			}

			surf->polys = (glpoly_t *) poly;
			poly = build_surf_displist (models, surf, vertex_index_base,
										&vertices);
			vertex_index_base += surf->numedges;
			count++;
		}
	}
	clear_texture_chains (bctx);
	Sys_MaskPrintf (SYS_VULKAN,
					"R_BuildDisplayLists: verts:%u, inds:%u, polys:%u (%d) %zd\n",
					vertex_count, index_count, poly_count, count,
					((size_t) poly - (size_t) bctx->polys)/sizeof(uint32_t));
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
									 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
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

	VkBufferMemoryBarrier wr_barrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		bctx->vertex_buffer, 0, vertex_buffer_size,
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0, 1, &wr_barrier, 0, 0);
	VkBufferCopy copy_region = { packet->offset, 0, vertex_buffer_size };
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							bctx->vertex_buffer, 1, &copy_region);
	VkBufferMemoryBarrier rd_barrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		bctx->vertex_buffer, 0, vertex_buffer_size,
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
								 0, 0, 0, 1, &rd_barrier, 0, 0);
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);
}

static void
R_DrawBrushModel (entity_t *e, vulkan_ctx_t *ctx)
{
	float       dot, radius;
	int         i;
	unsigned    k;
	model_t    *model;
	plane_t    *plane;
	msurface_t *surf;
	qboolean    rotated;
	vec3_t      mins, maxs, org;
	mod_brush_t *brush;

	model = e->model;
	brush = &model->brush;
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
	if (brush->firstmodelsurface != 0 && r_dlight_lightmap->int_val) {
		vec3_t      lightorigin;

		for (k = 0; k < r_maxdlights; k++) {
			if ((r_dlights[k].die < vr_data.realtime)
				|| (!r_dlights[k].radius))
				continue;

			VectorSubtract (r_dlights[k].origin, e->origin, lightorigin);
			R_RecursiveMarkLights (brush, lightorigin, &r_dlights[k], k,
							brush->nodes + brush->hulls[0].firstclipnode);
		}
	}

	surf = &brush->surfaces[brush->firstmodelsurface];

	for (i = 0; i < brush->nummodelsurfaces; i++, surf++) {
		// find the node side on which we are
		plane = surf->plane;

		dot = PlaneDiff (org, plane);

		// enqueue the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
			|| (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			chain_surface (brush, surf, e->transform, e->colormod, ctx);
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
visit_node (mod_brush_t *brush, mnode_t *node, int side, vulkan_ctx_t *ctx)
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
				continue;               // wrong side

			chain_surface (brush, surf, 0, 0, ctx);
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
R_VisitWorldNodes (mod_brush_t *brush, vulkan_ctx_t *ctx)
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
			visit_node (brush, node, side, ctx);
			node = node->children[!side];
		}
		if (node->contents < 0 && node->contents != CONTENTS_SOLID)
			visit_leaf ((mleaf_t *) node);
		if (node_ptr != node_stack) {
			node_ptr--;
			node = node_ptr->node;
			side = node_ptr->side;
			visit_node (brush, node, side, ctx);
			node = node->children[!side];
			continue;
		}
		break;
	}
	if (node->contents < 0 && node->contents != CONTENTS_SOLID)
		visit_leaf ((mleaf_t *) node);
}

static void
bind_view (qfv_bsp_tex tex, VkImageView view, bspframe_t *bframe,
		   VkCommandBuffer cmd, VkPipelineLayout layout, qfv_devfuncs_t *dfunc)
{
	bframe->imageInfo[tex].imageView = view;
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  layout, 0, 1,
									  bframe->descriptors + tex
									  + BSP_BUFFER_INFOS);
}

static void
push_transform (vec_t *transform, VkPipelineLayout layout,
				qfv_devfuncs_t *dfunc, VkCommandBuffer cmd)
{
	dfunc->vkCmdPushConstants (cmd, layout, VK_SHADER_STAGE_VERTEX_BIT,
							   0, 16 * sizeof (float), transform);
}

static void
push_fragconst (fragconst_t *fragconst, VkPipelineLayout layout,
				qfv_devfuncs_t *dfunc, VkCommandBuffer cmd)
{
	dfunc->vkCmdPushConstants (cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
							   64, sizeof (fragconst_t), fragconst);//FIXME 64
}

static void
push_descriptors (int count, VkWriteDescriptorSet *descriptors,
				  VkPipelineLayout layout, qfv_devfuncs_t *dfunc,
				  VkCommandBuffer cmd)
{
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  layout, 0, count, descriptors);
}

static void
draw_elechain (elechain_t *ec, VkPipelineLayout layout, qfv_devfuncs_t *dfunc,
			   VkCommandBuffer cmd)
{
	elements_t *el;

	if (ec->transform) {
		push_transform (ec->transform, layout, dfunc, cmd);
	} else {
		//FIXME should cache current transform
		push_transform (identity, layout, dfunc, cmd);
	}
	for (el = ec->elements; el; el = el->next) {
		if (!el->index_count)
			continue;
		dfunc->vkCmdDrawIndexed (cmd, el->index_count, 1, el->first_index,
								 0, 0);
	}
}

static void
reset_elechain (elechain_t *ec)
{
	elements_t *el;

	for (el = ec->elements; el; el = el->next) {
		el->first_index = 0;
		el->index_count = 0;
	}
}

static VkImageView
get_view (qfv_tex_t *tex, qfv_tex_t *default_tex)
{
	if (tex) {
		return tex->view;
	}
	if (default_tex) {
		return default_tex->view;
	}
	return 0;
}

static void
bsp_begin_subpass (QFV_BspSubpass subpass, VkPipeline pipeline,
				   vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = bframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		ctx->renderpass, subpass,
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  pipeline);
	VkViewport  viewport = {0, 0, vid.width, vid.height, 0, 1};
	VkRect2D    scissor = { {0, 0}, {vid.width, vid.height} };
	dfunc->vkCmdSetViewport (cmd, 0, 1, &viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &scissor);

	VkDeviceSize offsets[] = { 0 };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &bctx->vertex_buffer, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, bctx->index_buffer, bframe->index_offset,
								 VK_INDEX_TYPE_UINT32);

	// push VP matrices
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  bctx->layout,
									  0, 1, bframe->descriptors + 0);
	// push static images
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  bctx->layout,
									  0, 3, bframe->descriptors + 3);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
bsp_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkEndCommandBuffer (cmd);
}

static void
bsp_begin (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	//XXX quat_t      fog;

	bctx->default_color[3] = 1;
	QuatCopy (bctx->default_color, bctx->last_color);

	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	DARRAY_APPEND (&cframe->cmdSets[QFV_passDepth],
				   bframe->cmdSet.a[QFV_bspDepth]);
	DARRAY_APPEND (&cframe->cmdSets[QFV_passGBuffer],
				   bframe->cmdSet.a[QFV_bspGBuffer]);

	//FIXME need per frame matrices
	bframe->bufferInfo[0].buffer = ctx->matrices.buffer_3d;
	bframe->imageInfo[0].imageView = 0;	// set by tex chain loop
	bframe->imageInfo[1].imageView = 0;	// set by tex chain loop
	bframe->imageInfo[2].imageView = QFV_ScrapImageView (bctx->light_scrap);
	bframe->imageInfo[3].imageView = get_view (bctx->skysheet_tex,
											   bctx->default_skysheet);
	bframe->imageInfo[4].imageView = get_view (bctx->skybox_tex,
											   bctx->default_skybox);

	bsp_begin_subpass (QFV_bspDepth, bctx->depth, ctx);
	bsp_begin_subpass (QFV_bspGBuffer, bctx->gbuf, ctx);
}

static void
bsp_end (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	bsp_end_subpass (bframe->cmdSet.a[QFV_bspDepth], ctx);
	bsp_end_subpass (bframe->cmdSet.a[QFV_bspGBuffer], ctx);
}

/*static void
turb_begin (bspctx_t *bctx)
{
	quat_t      fog;

	bctx->default_color[3] = bound (0, r_wateralpha->value, 1);
	QuatCopy (bctx->default_color, bctx->last_color);
	qfeglVertexAttrib4fv (quake_bsp.color.location, default_color);

	Mat4Mult (glsl_projection, glsl_view, bsp_vp);

	qfeglUseProgram (quake_turb.program);
	qfeglEnableVertexAttribArray (quake_turb.vertex.location);
	qfeglEnableVertexAttribArray (quake_turb.tlst.location);
	qfeglDisableVertexAttribArray (quake_turb.color.location);

	qfeglVertexAttrib4fv (quake_turb.color.location, default_color);

	glsl_Fog_GetColor (fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	fragconst_t frag_constants = { time: vr_data.realtime };
	dfunc->vkCmdPushConstants (cmd, bctx->layout, VK_SHADER_STAGE_FRAGMENT_BIT,
							   64, sizeof (fragconst_t), &frag_constants);
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
}*/

/*static void
turb_end (bspctx_t *bctx)
{
	qfeglDisableVertexAttribArray (quake_turb.vertex.location);
	qfeglDisableVertexAttribArray (quake_turb.tlst.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
}*/

static void
spin (mat4_t mat, bspctx_t *bctx)
{
	quat_t      q;
	mat4_t      m;
	float       blend;

	while (vr_data.realtime - bctx->sky_time > 1) {
		QuatCopy (bctx->sky_rotation[1], bctx->sky_rotation[0]);
		QuatMult (bctx->sky_velocity, bctx->sky_rotation[0],
				  bctx->sky_rotation[1]);
		bctx->sky_time += 1;
	}
	blend = bound (0, (vr_data.realtime - bctx->sky_time), 1);

	QuatBlend (bctx->sky_rotation[0], bctx->sky_rotation[1], blend, q);
	QuatMult (bctx->sky_fix, q, q);
	Mat4Identity (mat);
	VectorNegate (r_origin, mat + 12);
	QuatToMatrix (q, m, 1, 1);
	Mat4Mult (m, mat, mat);
}

static void
sky_begin (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;

	bctx->default_color[3] = 1;
	QuatCopy (bctx->default_color, bctx->last_color);

	spin (ctx->matrices.sky_3d, bctx);

	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	//FIXME where should skys go? g-buffer is overkill. Translucent pre-pass?
	DARRAY_APPEND (&cframe->cmdSets[QFV_passTranslucent],
				   bframe->cmdSet.a[QFV_bspTranslucent]);

	//FIXME need per frame matrices
	bframe->bufferInfo[0].buffer = ctx->matrices.buffer_3d;
	bframe->imageInfo[0].imageView = ctx->default_magenta->view;
	bframe->imageInfo[1].imageView = ctx->default_magenta->view;
	bframe->imageInfo[2].imageView = QFV_ScrapImageView (bctx->light_scrap);
	bframe->imageInfo[3].imageView = get_view (bctx->skysheet_tex,
											   bctx->default_skysheet);
	bframe->imageInfo[4].imageView = get_view (bctx->skybox_tex,
											   bctx->default_skybox);

	//FIXME sky pass
	bsp_begin_subpass (QFV_bspTranslucent, bctx->sky, ctx);
}

static void
sky_end (vulkan_ctx_t *ctx)
{
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];

	//FIXME sky pass
	bsp_end_subpass (bframe->cmdSet.a[QFV_bspTranslucent], ctx);
}

static inline void
add_surf_elements (vulktex_t *tex, instsurf_t *is,
				   elechain_t **ec, elements_t **el,
				   bspctx_t *bctx, bspframe_t *bframe)
{
	msurface_t *surf = is->surface;
	bsppoly_t  *poly = (bsppoly_t *) surf->polys;

	if (!tex->elechain) {
		(*ec) = add_elechain (tex, surf->ec_index, bctx);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*el) = (*ec)->elements;
		(*el)->first_index = bframe->index_count;
	}
	if (is->transform != (*ec)->transform || is->color != (*ec)->color) {
		(*ec) = add_elechain (tex, surf->ec_index, bctx);
		(*ec)->transform = is->transform;
		(*ec)->color = is->color;
		(*el) = (*ec)->elements;
		(*el)->first_index = bframe->index_count;
	}
	memcpy (bframe->index_data + bframe->index_count,
			poly->indices, poly->count * sizeof (poly->indices[0]));
	(*el)->index_count += poly->count;
	bframe->index_count += poly->count;
}

static void
build_tex_elechain (vulktex_t *tex, bspctx_t *bctx, bspframe_t *bframe)
{
	instsurf_t *is;
	elechain_t *ec = 0;
	elements_t *el = 0;

	for (is = tex->tex_chain; is; is = is->tex_chain) {
		add_surf_elements (tex, is, &ec, &el, bctx, bframe);
	}
}

void
Vulkan_DrawWorld (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	entity_t    worldent;
	mod_brush_t *brush;

	clear_texture_chains (bctx);	// do this first for water and skys
	bframe->index_count = 0;

	memset (&worldent, 0, sizeof (worldent));
	worldent.model = r_worldentity.model;
	brush = &r_worldentity.model->brush;

	//vulktex_t  *tex = r_worldentity.model->skytexture->render;
	//bctx->skysheet_tex = tex->tex;

	currententity = &worldent;

	R_VisitWorldNodes (brush, ctx);
	if (r_drawentities->int_val) {
		entity_t   *ent;
		for (ent = r_ent_queue; ent; ent = ent->next) {
			if (ent->model->type != mod_brush)
				continue;
			currententity = ent;

			R_DrawBrushModel (ent, ctx);
		}
	}

	Vulkan_FlushLightmaps (ctx);
	bsp_begin (ctx);

	push_transform (identity, bctx->layout, dfunc,
					bframe->cmdSet.a[QFV_bspDepth]);
	push_transform (identity, bctx->layout, dfunc,
					bframe->cmdSet.a[QFV_bspGBuffer]);
	fragconst_t frag_constants = { time: vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, dfunc,
					bframe->cmdSet.a[QFV_bspGBuffer]);
	//XXX qfeglActiveTexture (GL_TEXTURE0 + 0);
	for (size_t i = 0; i < bctx->texture_chains.size; i++) {
		vulktex_t  *tex;
		elechain_t *ec = 0;

		tex = bctx->texture_chains.a[i];

		build_tex_elechain (tex, bctx, bframe);

		bframe->imageInfo[0].imageView = get_view (tex->tex,
												   ctx->default_white);
		bframe->imageInfo[1].imageView = get_view (tex->glow,
												   ctx->default_black);

		push_descriptors (2, bframe->descriptors + 1, bctx->layout, dfunc,
						  bframe->cmdSet.a[QFV_bspGBuffer]);

		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, bctx->layout, dfunc,
						   bframe->cmdSet.a[QFV_bspDepth]);
			draw_elechain (ec, bctx->layout, dfunc,
						   bframe->cmdSet.a[QFV_bspGBuffer]);
			reset_elechain (ec);
		}
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	bsp_end (ctx);

	size_t      atom = device->physDev->properties.limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
	size_t      offset = bframe->index_offset;
	size_t      size = bframe->index_count * sizeof (uint32_t);

	offset &= ~atom_mask;
	size = (size + atom_mask) & ~atom_mask;

	//FIXME this needs to come at the end of the frame after all passes
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		bctx->index_memory, offset, size
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
}

void
Vulkan_DrawWaterSurfaces (vulkan_ctx_t *ctx)
{
/*	bspctx_t   *bctx = ctx->bsp_context;
	instsurf_t *is;
	msurface_t *surf;
	vulktex_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!bctx->waterchain)
		return;

	turb_begin (bctx);
	for (is = bctx->waterchain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture) {
			if (tex) {
				//XXX qfeglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
				//for (ec = tex->elechain; ec; ec = ec->next)
				//	draw_elechain (ec, quake_turb.mvp_matrix.location,
				//				   quake_turb.vertex.location,
				//				   quake_turb.tlst.location,
				//				   quake_turb.color.location);
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture;
		}
		add_surf_elements (tex, is, &ec, &el, bctx);
	}
	if (tex) {
		//XXX qfeglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
		//for (ec = tex->elechain; ec; ec = ec->next)
		//	draw_elechain (ec, quake_turb.mvp_matrix.location,
		//				   quake_turb.vertex.location,
		//				   quake_turb.tlst.location,
		//				   quake_turb.color.location);
		tex->elechain = 0;
		tex->elechain_tail = &tex->elechain;
	}
	turb_end (bctx);

	bctx->waterchain = 0;
	bctx->waterchain_tail = &bctx->waterchain;*/
}

void
Vulkan_DrawSky (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	bspctx_t   *bctx = ctx->bsp_context;
	bspframe_t *bframe = &bctx->frames.a[ctx->curFrame];
	instsurf_t *is;
	msurface_t *surf;
	vulktex_t  *tex = 0;
	elechain_t *ec = 0;
	elements_t *el = 0;

	if (!bctx->sky_chain)
		return;

	sky_begin (ctx);
	//FIXME sky pass
	push_transform (identity, bctx->layout, dfunc,
					bframe->cmdSet.a[QFV_bspTranslucent]);
	fragconst_t frag_constants = { time: vr_data.realtime };
	push_fragconst (&frag_constants, bctx->layout, dfunc,
					bframe->cmdSet.a[QFV_bspTranslucent]);
	bind_view (qfv_bsp_texture, ctx->default_black->view, bframe,
			   bframe->cmdSet.a[QFV_bspTranslucent],//FIXME
			   bctx->layout, dfunc);
	bind_view (qfv_bsp_glowmap, ctx->default_black->view, bframe,
			   bframe->cmdSet.a[QFV_bspTranslucent],//FIXME
			   bctx->layout, dfunc);
	bind_view (qfv_bsp_lightmap, ctx->default_black->view, bframe,
			   bframe->cmdSet.a[QFV_bspTranslucent],//FIXME
			   bctx->layout, dfunc);
	for (is = bctx->sky_chain; is; is = is->tex_chain) {
		surf = is->surface;
		if (tex != surf->texinfo->texture->render) {
			if (tex) {
				bind_view (qfv_bsp_skysheet,
						   get_view (tex->tex, ctx->default_black),
						   bframe,
						   bframe->cmdSet.a[QFV_bspTranslucent],//FIXME
						   bctx->layout, dfunc);
				for (ec = tex->elechain; ec; ec = ec->next) {
					draw_elechain (ec, bctx->layout, dfunc,//FIXME
								   bframe->cmdSet.a[QFV_bspTranslucent]);
					reset_elechain (ec);
				}
				tex->elechain = 0;
				tex->elechain_tail = &tex->elechain;
			}
			tex = surf->texinfo->texture->render;
		}
		add_surf_elements (tex, is, &ec, &el, bctx, bframe);
	}
	if (tex) {
		bind_view (qfv_bsp_skysheet, get_view (tex->tex, ctx->default_black),
				   bframe, bframe->cmdSet.a[QFV_bspTranslucent],//FIXME
				   bctx->layout, dfunc);
		for (ec = tex->elechain; ec; ec = ec->next) {
			draw_elechain (ec, bctx->layout, dfunc,//FIXME
						   bframe->cmdSet.a[QFV_bspTranslucent]);
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
						 "bsp:iview:default_skysheet");

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
	VkImageMemoryBarrier barrier;
	VkImageMemoryBarrier barriers[2];
	qfv_pipelinestagepair_t stages;

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_TransferDst];
	barrier = imageLayoutTransitionBarriers[qfv_LT_Undefined_to_TransferDst];
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barriers[0] = barrier;
	barriers[1] = barrier;
	barriers[0].image = skybox;
	barriers[1].image = skysheet;
	dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
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

	stages = imageLayoutTransitionStages[qfv_LT_TransferDst_to_ShaderReadOnly];
	barrier=imageLayoutTransitionBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barriers[0] = barrier;
	barriers[1] = barrier;
	barriers[0].image = skybox;
	barriers[1].image = skysheet;
	dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
								 0, 0, 0, 0, 0,
								 2, barriers);
	QFV_PacketSubmit (packet);
}

static VkDescriptorBufferInfo base_buffer_info = {
	0, 0, VK_WHOLE_SIZE
};
static VkDescriptorImageInfo base_image_info = {
	0, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	0, 0, 0
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	0, 0, 0
};

void
Vulkan_Bsp_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	bspctx_t   *bctx = calloc (1, sizeof (bspctx_t));
	ctx->bsp_context = bctx;

	bctx->waterchain_tail = &bctx->waterchain;
	bctx->sky_chain_tail = &bctx->sky_chain;
	bctx->static_instsurfs_tail = &bctx->static_instsurfs;
	bctx->elechains_tail = &bctx->elechains;
	bctx->elementss_tail = &bctx->elementss;
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

	bctx->depth = Vulkan_CreatePipeline (ctx, "bsp_depth");
	bctx->gbuf = Vulkan_CreatePipeline (ctx, "bsp_gbuf");
	bctx->sky = Vulkan_CreatePipeline (ctx, "bsp_skysheet");
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

		for (int j = 0; j < BSP_BUFFER_INFOS; j++) {
			bframe->bufferInfo[j] = base_buffer_info;
			bframe->descriptors[j] = base_buffer_write;
			bframe->descriptors[j].dstBinding = j;
			bframe->descriptors[j].pBufferInfo = &bframe->bufferInfo[j];
		}
		for (int j = 0; j < BSP_IMAGE_INFOS; j++) {
			bframe->imageInfo[j] = base_image_info;
			bframe->imageInfo[j].sampler = bctx->sampler;
			int k = j + BSP_BUFFER_INFOS;
			bframe->descriptors[k] = base_image_write;
			bframe->descriptors[k].dstBinding = k;
			bframe->descriptors[k].pImageInfo = &bframe->imageInfo[j];
		}
	}
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
	dfunc->vkDestroyPipeline (device->dev, bctx->sky, 0);
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

	dfunc->vkDestroyImageView (device->dev, bctx->default_skysheet->view, 0);
	dfunc->vkDestroyImage (device->dev, bctx->default_skysheet->image, 0);

	dfunc->vkDestroyImageView (device->dev, bctx->default_skybox->view, 0);
	dfunc->vkDestroyImage (device->dev, bctx->default_skybox->image, 0);
	dfunc->vkFreeMemory (device->dev, bctx->default_skybox->memory, 0);
	free (bctx->default_skybox);
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
/*XXX static void
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
}*/

/*XXX	void
Vulkan_R_LoadSkys (const char *sky, vulkan_ctx_t *ctx)
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
}*/
