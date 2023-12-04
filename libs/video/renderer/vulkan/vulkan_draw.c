/*
	vulkan_draw.c

	2D drawing support for Vulkan

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/1/10

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

#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "compat.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"
#include "QF/ui/font.h"
#include "QF/ui/view.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct pic_data_s {
	uint32_t    vert_index;
	uint32_t    slice_index;
	uint32_t    descid;
	subpic_t   *subpic;
} picdata_t;

typedef struct descbatch_s {
	int32_t     descid;		// texture or font descriptor id
	uint32_t    count;		// number of objects in batch
} descbatch_t;

typedef struct descbatchset_s
	DARRAY_TYPE (descbatch_t) descbatchset_t;

typedef struct {
	float       xy[2];
	byte        color[4];
} linevert_t;

typedef struct {
	uint32_t    index;
	byte        color[4];
	float       position[2];
	float       offset[2];
} quadinst_t;

typedef struct {
	float       offset[2];
	float       uv[2];
} quadvert_t;

typedef struct linequeue_s {
	linevert_t *verts;
	int         count;
	int         size;
} linequeue_t;

typedef struct quadqueue_s {
	quadinst_t *quads;
	int         count;
	int         size;
} quadqueue_t;

typedef struct cachepic_s {
	char       *name;
	qpic_t     *pic;
} cachepic_t;

// core pic atlas + static verts
#define CORE_DESC 0
// FIXME make dynamic
#define MAX_DESCIPTORS 64

typedef struct descpool_s {
	VkDescriptorSet sets[MAX_DESCIPTORS];
	struct drawctx_s *dctx;
	uint32_t    users[MAX_DESCIPTORS];// picdata_t.descid
	int         in_use;
} descpool_t;

typedef struct drawframe_s {
	size_t      instance_offset;
	size_t      dvert_offset;
	size_t      line_offset;
	VkBuffer    instance_buffer;
	VkBuffer    dvert_buffer;
	VkBuffer    line_buffer;
	VkBufferView dvert_view;

	uint32_t    dvertex_index;
	uint32_t    dvertex_max;
	descbatchset_t quad_batch;
	quadqueue_t quad_insts;
	linequeue_t line_verts;
	descpool_t  dyn_descs;
} drawframe_t;

typedef struct drawframeset_s
	DARRAY_TYPE (drawframe_t) drawframeset_t;

typedef struct drawfontres_s {
	qfv_resource_t resource;
	qfv_resobj_t glyph_data;
	qfv_resobj_t glyph_bview;
	qfv_resobj_t glyph_image;
	qfv_resobj_t glyph_iview;
} drawfontres_t;

typedef struct drawfont_s {
	VkDescriptorSet set;
	drawfontres_t *resource;
} drawfont_t;

typedef struct drawfontset_s
	DARRAY_TYPE (drawfont_t) drawfontset_t;

typedef struct drawctx_s {
	VkSampler   pic_sampler;
	VkSampler   glyph_sampler;
	scrap_t    *scrap;
	qfv_stagebuf_t *stage;
	int        *crosshair_inds;
	qpic_t     *crosshair;
	int        *conchar_inds;
	qpic_t     *conchars;
	qpic_t     *conback;
	qpic_t     *white_pic;
	qpic_t     *backtile_pic;
	// use two separate cmem blocks for pics and strings (cachepic names)
	// to ensure the names are never in the same cacheline as a pic since the
	// names are used only for lookup
	memsuper_t *pic_memsuper;
	memsuper_t *string_memsuper;
	hashtab_t  *pic_cache;
	qfv_dsmanager_t *dsmanager;
	qfv_resource_t *draw_resource;
	qfv_resobj_t *index_object;
	qfv_resobj_t *svertex_objects;
	qfv_resobj_t *instance_objects;
	qfv_resobj_t *dvertex_objects;
	qfv_resobj_t *lvertex_objects;
	uint32_t    svertex_index;
	uint32_t    svertex_max;
	VkDescriptorSet core_quad_set;
	drawframeset_t frames;
	drawfontset_t fonts;
	SCR_Func *scr_funcs;
} drawctx_t;

#define MAX_QUADS (32768)
#define VERTS_PER_QUAD (4)
#define BYTES_PER_QUAD (VERTS_PER_QUAD * sizeof (quadvert_t))
#define VERTS_PER_SLICE (16)
#define BYTES_PER_SLICE (VERTS_PER_SLICE * sizeof (quadvert_t))
#define INDS_PER_QUAD (4)
#define INDS_PER_SLICE (26)

#define MAX_INSTANCES (1024*1024)

#define MAX_LINES (32768)
#define VERTS_PER_LINE (2)
#define BYTES_PER_LINE (VERTS_PER_LINE * sizeof (linevert_t))

static int
get_dyn_descriptor (descpool_t *pool, qpic_t *pic, VkBufferView buffer_view,
					vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;
	auto pd = (picdata_t *) pic->data;
	uint32_t    id = pd->descid;

	for (int i = 0; i < pool->in_use; i++) {
		if (pool->users[i] == id) {
			return ~i;
		}
	}
	if (pool->in_use >= MAX_DESCIPTORS) {
		Sys_Error ("get_dyn_descriptor: out of dynamic descriptors");
	}
	int         descid = pool->in_use++;
	pool->users[descid] = id;
	if (!pool->sets[descid]) {
		pool->sets[descid] = QFV_DSManager_AllocSet (dctx->dsmanager);
	}
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  pool->sets[descid], 1, 0, 1,
		  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		  0, 0, &buffer_view },
	};
	VkCopyDescriptorSet copy[] = {
		{ VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, 0,
		  pool->dctx->fonts.a[id].set, 0, 0,
		  pool->sets[descid], 0, 0, 1 },
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 1, copy);
	return ~descid;
}

static void
generate_slice_indices (qfv_stagebuf_t *staging, qfv_resobj_t *ind_buffer)
{
	qfv_packet_t *packet = QFV_PacketAcquire (staging);
	uint32_t   *ind = QFV_PacketExtend (packet, ind_buffer->buffer.size);
	for (int i = 0; i < 8; i++) {
		ind[i] = i;
		ind[i + 9] = i + 1 + (i & 1) * 6;
		ind[i + 18] = i + 8;
	}
	ind[8] = ind[17] = ~0;
	QFV_PacketCopyBuffer (packet, ind_buffer->buffer.buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
}

static void
create_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;

	dctx->draw_resource = malloc (2 * sizeof (qfv_resource_t)
								  // index buffer
								  + sizeof (qfv_resobj_t)
								  // svertex buffer and view
								  + 2 * sizeof (qfv_resobj_t)
								  // frames dynamic vertex buffers and views
								  + (frames) * 2 * sizeof (qfv_resobj_t)
								  // frames line vertex buffers
								  + (frames) * sizeof (qfv_resobj_t)
								  // frames instance buffers
								  + (frames) * sizeof (qfv_resobj_t));
	dctx->index_object = (qfv_resobj_t *) &dctx->draw_resource[2];
	dctx->svertex_objects = &dctx->index_object[1];
	dctx->dvertex_objects = &dctx->svertex_objects[2];
	dctx->lvertex_objects = &dctx->dvertex_objects[2 * frames];
	dctx->instance_objects = &dctx->lvertex_objects[frames];

	dctx->svertex_index = 0;
	dctx->svertex_max = MAX_QUADS * VERTS_PER_QUAD;

	dctx->draw_resource[0] = (qfv_resource_t) {
		.name = "draw",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 1 + 2,	// quad and 9-slice indices, and static verts
		.objects = dctx->index_object,
	};
	dctx->draw_resource[1] = (qfv_resource_t) {
		.name = "draw",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
						   | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		.num_objects = (2 * frames) + (frames) + (frames),
		.objects = dctx->dvertex_objects,
	};

	dctx->index_object[0] = (qfv_resobj_t) {
		.name = "quads.index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = INDS_PER_SLICE * sizeof (uint32_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};
	dctx->svertex_objects[0] = (qfv_resobj_t) {
		.name = "sverts",
		.type = qfv_res_buffer,
		.buffer = {
			.size = MAX_QUADS * BYTES_PER_QUAD,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		},
	};
	dctx->svertex_objects[1] = (qfv_resobj_t) {
		.name = "sverts",
		.type = qfv_res_buffer_view,
		.buffer_view = {
			.buffer = 1,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = 0,
			.size = dctx->svertex_objects[0].buffer.size,
		},
	};

	for (size_t i = 0; i < frames; i++) {
		dctx->dvertex_objects[i * 2 + 0] = (qfv_resobj_t) {
			.name = "dverts",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_QUADS * BYTES_PER_QUAD,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
			},
		};
		dctx->dvertex_objects[i * 2 + 1] = (qfv_resobj_t) {
			.name = "dverts",
			.type = qfv_res_buffer_view,
			.buffer_view = {
				.buffer = &dctx->dvertex_objects[i * 2 + 0]
						- dctx->draw_resource[1].objects,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 0,
				.size = dctx->dvertex_objects[i * 2 + 0].buffer.size,
			},
		};
		dctx->lvertex_objects[i] = (qfv_resobj_t) {
			.name = "line",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_LINES * BYTES_PER_LINE,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		dctx->instance_objects[i] = (qfv_resobj_t) {
			.name = "inst",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_INSTANCES * sizeof (quadinst_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, &dctx->draw_resource[0]);
	QFV_CreateResource (device, &dctx->draw_resource[1]);

	void       *data;
	VkDeviceMemory memory = dctx->draw_resource[1].memory;
	dfunc->vkMapMemory (device->dev, memory, 0, VK_WHOLE_SIZE, 0, &data);

	for (size_t f = 0; f < frames; f++) {
		drawframe_t *frame = &dctx->frames.a[f];
		frame->instance_buffer = dctx->instance_objects[f].buffer.buffer;
		frame->instance_offset = dctx->instance_objects[f].buffer.offset;
		frame->dvert_buffer = dctx->dvertex_objects[f * 2 + 0].buffer.buffer;
		frame->dvert_view = dctx->dvertex_objects[f * 2 + 1].buffer_view.view;
		frame->line_buffer = dctx->lvertex_objects[f].buffer.buffer;
		frame->line_offset = dctx->lvertex_objects[f].buffer.offset;

		frame->dvertex_index = 0;
		frame->dvertex_max = MAX_QUADS * VERTS_PER_QUAD;

		DARRAY_INIT (&frame->quad_batch, 16);
		frame->quad_insts = (quadqueue_t) {
			.quads = (quadinst_t *) ((byte *)data + frame->instance_offset),
			.size = MAX_INSTANCES,
		};

		frame->line_verts = (linequeue_t) {
			.verts = (linevert_t *) ((byte *)data + frame->line_offset),
			.size = MAX_INSTANCES,
		};
	}

	// The indices will never change so pre-generate and stash them
	generate_slice_indices (ctx->staging, &dctx->index_object[0]);
}

static void
flush_draw_scrap (vulkan_ctx_t *ctx)
{
	QFV_ScrapFlush (ctx->draw_context->scrap);
}

static void
pic_free (drawctx_t *dctx, qpic_t *pic)
{
	__auto_type pd = (picdata_t *) pic->data;
	if (pd->subpic) {
		QFV_SubpicDelete (pd->subpic);
	}
	cmemfree (dctx->pic_memsuper, pic);
}

static cachepic_t *
new_cachepic (drawctx_t *dctx, const char *name, qpic_t *pic)
{
	cachepic_t *cp;
	size_t      size = strlen (name) + 1;

	cp = cmemalloc (dctx->pic_memsuper, sizeof (cachepic_t));
	cp->name = cmemalloc (dctx->string_memsuper, size);
	memcpy (cp->name, name, size);
	cp->pic = pic;
	return cp;
}

static void
cachepic_free (void *_cp, void *_dctx)
{
	drawctx_t  *dctx = _dctx;
	cachepic_t *cp = (cachepic_t *) _cp;
	pic_free (dctx, cp->pic);
	cmemfree (dctx->string_memsuper, cp->name);
	cmemfree (dctx->pic_memsuper, cp);
}

static const char *
cachepic_getkey (const void *_cp, void *unused)
{
	return ((cachepic_t *) _cp)->name;
}

static uint32_t
create_slice (vec4i_t rect, vec4i_t border, qpic_t *pic,
			  uint32_t *vertex_index, VkBuffer buffer, vulkan_ctx_t *ctx)
{
	__auto_type pd = (picdata_t *) pic->data;

	int         x = rect[0];
	int         y = rect[1];
	int         w = rect[2];
	int         h = rect[3];
	int         l = border[0];
	int         t = border[1];
	int         r = w - border[2];
	int         b = h - border[3];

	float       sx = 1.0 / pic->width;
	float       sy = 1.0 / pic->height;
	if (pd->subpic) {
		x += pd->subpic->rect->x;
		y += pd->subpic->rect->y;
		sx = sy = pd->subpic->size;
	}

	vec4f_t     p[16] = {
		{ 0, 0, 0, 0 }, { 0, t, 0, t }, { l, 0, l, 0 }, { l, t, l, t },
		{ r, 0, r, 0 }, { r, t, r, t }, { w, 0, w, 0 }, { w, t, w, t },
		{ 0, b, 0, b }, { 0, h, 0, h }, { l, b, l, b }, { l, h, l, h },
		{ r, b, r, b }, { r, h, r, h }, { w, b, w, b }, { w, h, w, h },
	};

	vec4f_t     size = { 1, 1, sx, sy };
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	quadvert_t *verts = QFV_PacketExtend (packet, BYTES_PER_SLICE);
	for (int i = 0; i < VERTS_PER_SLICE; i++) {
		vec4f_t     v = ((vec4f_t) {0, 0, x, y} + p[i]) * size;
		verts[i] = (quadvert_t) { {v[0], v[1]}, {v[2], v[3]} };
	}

	int         ind = *vertex_index;
	*vertex_index += VERTS_PER_SLICE;
	QFV_PacketCopyBuffer (packet, buffer, ind * sizeof (quadvert_t),
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);

	return ind;
}

static uint32_t
make_static_slice (vec4i_t rect, vec4i_t border, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	VkBuffer    buffer = dctx->svertex_objects[0].buffer.buffer;

	return create_slice (rect, border, pic, &dctx->svertex_index, buffer, ctx);
}

static uint32_t
create_quad (int x, int y, int w, int h, qpic_t *pic, uint32_t *vertex_index,
			 VkBuffer buffer, vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	__auto_type pd = (picdata_t *) pic->data;

	float sl = 0, sr = 1, st = 0, sb = 1;

	if (pd->subpic) {
		x += pd->subpic->rect->x;
		y += pd->subpic->rect->y;
		float size = pd->subpic->size;
		sl = (x + 0) * size;
		sr = (x + w) * size;
		st = (y + 0) * size;
		sb = (y + h) * size;
	}

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	quadvert_t *verts = QFV_PacketExtend (packet, BYTES_PER_QUAD);
	verts[0] = (quadvert_t) { {0, 0}, {sl, st} };
	verts[1] = (quadvert_t) { {0, h}, {sl, sb} };
	verts[2] = (quadvert_t) { {w, 0}, {sr, st} };
	verts[3] = (quadvert_t) { {w, h}, {sr, sb} };

	int         ind = *vertex_index;
	*vertex_index += VERTS_PER_QUAD;
	QFV_PacketCopyBuffer (packet, buffer, ind * sizeof (quadvert_t),
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);

	return ind;
}

static uint32_t
make_static_quad (int w, int h, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;

	return create_quad (0, 0, w, h, pic, &dctx->svertex_index,
						dctx->svertex_objects[0].buffer.buffer, ctx);
}

static int
make_dyn_quad (int x, int y, int w, int h, qpic_t *pic, vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	return create_quad (x, y, w, h, pic, &frame->dvertex_index,
						frame->dvert_buffer, ctx);
}

static qpic_t *
pic_data (const char *name, int w, int h, const byte *data, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	qpic_t     *pic;
	byte       *picdata;

	pic = cmemalloc (dctx->pic_memsuper,
					 field_offset (qpic_t, data[sizeof (picdata_t)]));
	pic->width = w;
	pic->height = h;
	__auto_type pd = (picdata_t *) pic->data;
	pd->subpic = QFV_ScrapSubpic (dctx->scrap, w, h);
	pd->vert_index = make_static_quad (w, h, pic, ctx);
	pd->slice_index = ~0;
	pd->descid = CORE_DESC;

	picdata = QFV_SubpicBatch (pd->subpic, dctx->stage);
	size_t size = w * h;
	for (size_t i = 0; i < size; i++) {
		byte        pix = *data++;
		byte       *col = vid.palette + pix * 3;
		byte        alpha = (pix == 255) - 1;
		// pre-multiply alpha.
		*picdata++ = *col++ & alpha;
		*picdata++ = *col++ & alpha;
		*picdata++ = *col++ & alpha;
		*picdata++ = alpha;
	}
	return pic;
}

qpic_t *
Vulkan_Draw_MakePic (int width, int height, const byte *data,
					 vulkan_ctx_t *ctx)
{
	return pic_data (0, width, height, data, ctx);
}

void
Vulkan_Draw_DestroyPic (qpic_t *pic, vulkan_ctx_t *ctx)
{
}

qpic_t *
Vulkan_Draw_PicFromWad (const char *name, vulkan_ctx_t *ctx)
{
	qpic_t     *wadpic = W_GetLumpName (name);

	if (!wadpic) {
		return 0;
	}
	return pic_data (name, wadpic->width, wadpic->height, wadpic->data, ctx);
}

static qpic_t *
load_lmp (const char *path, vulkan_ctx_t *ctx)
{
	qpic_t     *p;
	if (strlen (path) < 4 || strcmp (path + strlen (path) - 4, ".lmp")
		|| !(p = (qpic_t *) QFS_LoadFile (QFS_FOpenFile (path), 0))) {
		return 0;
	}

	if (p->width < 32 && p->height < 32) {
		qpic_t     *pic = pic_data (path, p->width, p->height, p->data, ctx);
		free (p);
		return pic;
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	int         fontid = dctx->fonts.size;
	DARRAY_OPEN_AT (&dctx->fonts, fontid, 1);
	drawfont_t *font = &dctx->fonts.a[fontid];

	font->resource = malloc (sizeof (drawfontres_t));
	font->resource->resource = (qfv_resource_t) {
		.name = va (ctx->va_ctx, "cachepic:%d", fontid),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2,
		.objects = &font->resource->glyph_image,
	};

	tex_t       tex = {
		.width = p->width,
		.height = p->height,
		.format = tex_rgba,
		.loaded = 1,
		.data = p->data,
	};
	QFV_ResourceInitTexImage (&font->resource->glyph_image, "image", 0, &tex);
	__auto_type cache_image = &font->resource->glyph_image;

	font->resource->glyph_iview = (qfv_resobj_t) {
		.name = "image_view",
		.type = qfv_res_image_view,
		.image_view = {
			.image = 0,
			.type = VK_IMAGE_VIEW_TYPE_2D,
			.format = font->resource->glyph_image.image.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
		},
	};
	__auto_type cache_iview = &font->resource->glyph_iview;

	QFV_CreateResource (ctx->device, &font->resource->resource);

	__auto_type packet = QFV_PacketAcquire (ctx->staging);
	int         count = tex.width * tex.height;
	byte       *texels = QFV_PacketExtend (packet, 4 * count);
	byte        palette[256 * 4];
	memcpy (palette, vid.palette32, sizeof (palette));
	palette[255*4 + 0] = 0;
	palette[255*4 + 1] = 0;
	palette[255*4 + 2] = 0;
	Vulkan_ExpandPalette (texels, tex.data, palette, 2, count);
	QFV_PacketCopyImage (packet, cache_image->image.image,
						 tex.width, tex.height,
						 &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly]);
	QFV_PacketSubmit (packet);

	font->set = QFV_DSManager_AllocSet (dctx->dsmanager);;
	VkDescriptorImageInfo imageInfo = {
		dctx->pic_sampler,
		cache_iview->image_view.view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  font->set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  font->set, 1, 0, 1,
		  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		  0, 0, &dctx->svertex_objects[1].buffer_view.view },
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);

	qpic_t *pic;
	pic = cmemalloc (dctx->pic_memsuper,
					 field_offset (qpic_t, data[sizeof (picdata_t)]));
	pic->width = p->width;
	pic->height = p->height;
	__auto_type pd = (picdata_t *) pic->data;
	pd->subpic = 0;
	pd->vert_index = make_static_quad (p->width, p->height, pic, ctx);
	pd->slice_index = ~0;
	pd->descid = fontid;

	free (p);
	return pic;
}

qpic_t *
Vulkan_Draw_CachePic (const char *path, bool alpha, vulkan_ctx_t *ctx)
{
	cachepic_t *cpic;
	drawctx_t  *dctx = ctx->draw_context;

	if ((cpic = Hash_Find (dctx->pic_cache, path))) {
		return cpic->pic;
	}
	qpic_t     *pic = load_lmp (path, ctx);
	cpic = new_cachepic (dctx, path, pic);
	Hash_Add (dctx->pic_cache, cpic);
	return pic;
}

void
Vulkan_Draw_UncachePic (const char *path, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	Hash_Free (dctx->pic_cache, Hash_Del (dctx->pic_cache, path));
}

void
Vulkan_Draw_Shutdown (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dctx = ctx->draw_context;

	QFV_DestroyResource (device, &dctx->draw_resource[0]);
	QFV_DestroyResource (device, &dctx->draw_resource[1]);
	for (size_t i = 0; i < dctx->fonts.size; i++) {
		if (dctx->fonts.a[i].resource) {
			QFV_DestroyResource (device, &dctx->fonts.a[i].resource->resource);
			free (dctx->fonts.a[i].resource);
		}
	}

	Hash_DelTable (dctx->pic_cache);
	delete_memsuper (dctx->pic_memsuper);
	delete_memsuper (dctx->string_memsuper);
	QFV_DestroyScrap (dctx->scrap);
	QFV_DestroyStagingBuffer (dctx->stage);
}

static void
load_conchars (vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;

	draw_chars = W_GetLumpName ("conchars");
	if (draw_chars) {
		for (int i = 0; i < 256 * 64; i++) {
			if (draw_chars[i] == 0) {
				draw_chars[i] = 255;		// proper transparent color
			}
		}
		dctx->conchars = pic_data ("conchars", 128, 128, draw_chars, ctx);
	} else {
		qpic_t     *charspic = Draw_Font8x8Pic ();
		dctx->conchars = pic_data ("conchars", charspic->width,
								   charspic->height, charspic->data, ctx);
		free (charspic);
	}
	dctx->conchar_inds = malloc (256 * sizeof (int));
	VkBuffer    buffer = dctx->svertex_objects[0].buffer.buffer;
	for (int i = 0; i < 256; i++) {
		int         cx = i % 16;
		int         cy = i / 16;
		dctx->conchar_inds[i] = create_quad (cx * 8, cy * 8, 8, 8,
											 dctx->conchars,
											 &dctx->svertex_index, buffer, ctx);
	}
}

static void
load_crosshairs (vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	qpic_t     *hairpic = Draw_CrosshairPic ();
	dctx->crosshair = pic_data ("crosshair", hairpic->width,
								hairpic->height, hairpic->data, ctx);
	free (hairpic);

	dctx->crosshair_inds = malloc (4 * sizeof (int));
	VkBuffer    buffer = dctx->svertex_objects[0].buffer.buffer;
#define W CROSSHAIR_WIDTH
#define H CROSSHAIR_HEIGHT
	dctx->crosshair_inds[0] = create_quad (0, 0, W, H, dctx->crosshair,
										   &dctx->svertex_index, buffer, ctx);
	dctx->crosshair_inds[1] = create_quad (W, 0, W, H, dctx->crosshair,
										   &dctx->svertex_index, buffer, ctx);
	dctx->crosshair_inds[2] = create_quad (0, H, W, H, dctx->crosshair,
										   &dctx->svertex_index, buffer, ctx);
	dctx->crosshair_inds[3] = create_quad (W, H, W, H, dctx->crosshair,
										   &dctx->svertex_index, buffer, ctx);
#undef W
#undef H
}

static void
load_white_pic (vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	byte        white_block = 0xfe;

	dctx->white_pic = pic_data ("white", 1, 1, &white_block, ctx);
	__auto_type pd = (picdata_t *) dctx->white_pic->data;
	pd->slice_index = make_static_slice ((vec4i_t) {0, 0, 1, 1},
										 (vec4i_t) {0, 0, 0, 0},
										 dctx->white_pic, ctx);
}

static void
draw_quads (qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;
	auto dframe = &dctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkBuffer    instance_buffer = dframe->instance_buffer;
	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &instance_buffer, offsets);

	VkBuffer    ind_buffer = dctx->index_object[0].buffer.buffer;
	dfunc->vkCmdBindIndexBuffer (cmd, ind_buffer, 0, VK_INDEX_TYPE_UINT32);

	uint32_t    inst_start = 0;
	for (size_t i = 0; i < dframe->quad_batch.size; i++) {
		int         fontid = dframe->quad_batch.a[i].descid;
		uint32_t    inst_count = dframe->quad_batch.a[i].count;
		uint32_t    ind_count = inst_count >> 24;
		inst_count &= 0xffffff;
		VkDescriptorSet set[2] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			fontid < 0 ? dframe->dyn_descs.sets[~fontid]
					   : dctx->fonts.a[fontid].set,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2, set, 0, 0);

		dfunc->vkCmdDrawIndexed (cmd, ind_count, inst_count, 0, 0, inst_start);
		inst_start += inst_count;
	}
	DARRAY_RESIZE (&dframe->quad_batch, 0);
}

static void
draw_lines (qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;
	auto dframe = &dctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkBuffer    line_buffer = dframe->line_buffer;
	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &line_buffer, offsets);
	VkDescriptorSet set[1] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 1, set, 0, 0);
	dfunc->vkCmdDraw (cmd, dframe->line_verts.count * VERTS_PER_LINE,
					  1, 0, 0);
}

static void
flush_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	flush_draw_scrap (ctx);
}

static void
slice_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;
	auto dframe = &dctx->frames.a[ctx->curFrame];
	if (!dframe->quad_insts.count) {
		return;
	}

	qftVkScopedZone (taskctx->frame->qftVkCtx, taskctx->cmd, "slice_draw");
	VkDeviceMemory memory = dctx->draw_resource[1].memory;
	size_t      atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
#define a(x) (((x) + atom_mask) & ~atom_mask)
	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->instance_offset,
		  a(dframe->quad_insts.count * BYTES_PER_QUAD) },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->dvert_offset,
		  a(dframe->dvertex_index * sizeof (quadvert_t)) },
	};
#undef a
	dfunc->vkFlushMappedMemoryRanges (device->dev, 2, ranges);

	draw_quads (taskctx);

	dframe->quad_insts.count = 0;
	dframe->dvertex_index = 0;
	dframe->dyn_descs.in_use = 0;
}

static void
line_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;
	auto dframe = &dctx->frames.a[ctx->curFrame];

	if (!dframe->line_verts.count) {
		return;
	}

	VkDeviceMemory memory = dctx->draw_resource[1].memory;
	size_t      atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
#define a(x) (((x) + atom_mask) & ~atom_mask)
	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->line_offset,
		  a(dframe->line_verts.count * BYTES_PER_LINE) },
	};
#undef a
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, ranges);

	draw_lines (taskctx);

	dframe->line_verts.count = 0;
}

static void
draw_scr_funcs (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto dctx = ctx->draw_context;
	auto scr_funcs = dctx->scr_funcs;
	if (!scr_funcs) {
		return;
	}
	while (*scr_funcs) {
		(*scr_funcs) ();
		scr_funcs++;
	}
	dctx->scr_funcs = 0;
}

static exprfunc_t flush_draw_func[] = {
	{ .func = flush_draw },
	{}
};
static exprfunc_t slice_draw_func[] = {
	{ .func = slice_draw },
	{}
};
static exprfunc_t line_draw_func[] = {
	{ .func = line_draw },
	{}
};
static exprfunc_t draw_scr_funcs_func[] = {
	{ .func = draw_scr_funcs },
	{}
};
static exprsym_t draw_task_syms[] = {
	{ "flush_draw", &cexpr_function, flush_draw_func },
	{ "slice_draw", &cexpr_function, slice_draw_func },
	{ "line_draw", &cexpr_function, line_draw_func },
	{ "draw_scr_funcs", &cexpr_function, draw_scr_funcs_func },
	{}
};

void
Vulkan_Draw_Init (vulkan_ctx_t *ctx)
{
	QFV_Render_AddTasks (ctx, draw_task_syms);

	drawctx_t  *dctx = calloc (1, sizeof (drawctx_t));
	ctx->draw_context = dctx;
}

void
Vulkan_Draw_Setup (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "draw init");

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dctx = ctx->draw_context;

	dctx->pic_sampler = QFV_Render_Sampler (ctx, "quakepic");
	dctx->glyph_sampler = QFV_Render_Sampler (ctx, "glyph");

	dctx->dsmanager = QFV_Render_DSManager (ctx, "quad_data_set");

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&dctx->frames, frames);
	DARRAY_RESIZE (&dctx->frames, frames);
	dctx->frames.grow = 0;
	memset (dctx->frames.a, 0, dctx->frames.size * sizeof (drawframe_t));

	DARRAY_INIT (&dctx->fonts, 16);
	DARRAY_RESIZE (&dctx->fonts, 16);
	dctx->fonts.size = 0;

	dctx->pic_memsuper = new_memsuper ();
	dctx->string_memsuper = new_memsuper ();
	dctx->pic_cache = Hash_NewTable (127, cachepic_getkey, cachepic_free,
									 dctx, 0);

	create_buffers (ctx);
	dctx->stage = QFV_CreateStagingBuffer (device, "draw", 4 * 1024 * 1024,
										   ctx->cmdpool);
	dctx->scrap = QFV_CreateScrap (device, "draw_atlas", 2048, tex_rgba,
								   dctx->stage);

	load_conchars (ctx);
	load_crosshairs (ctx);
	load_white_pic (ctx);

	dctx->backtile_pic = Vulkan_Draw_PicFromWad ("backtile", ctx);
	if (!dctx->backtile_pic) {
		dctx->backtile_pic = dctx->white_pic;
	}

	flush_draw_scrap (ctx);

	// core set + dynamic sets

	VkDescriptorImageInfo imageInfo = {
		dctx->pic_sampler,
		QFV_ScrapImageView (dctx->scrap),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	for (size_t i = 0; i < frames; i++) {
		__auto_type frame = &dctx->frames.a[i];
		frame->dyn_descs = (descpool_t) { .dctx = dctx };
	}
	dctx->core_quad_set = QFV_DSManager_AllocSet (dctx->dsmanager);

	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  dctx->core_quad_set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  dctx->core_quad_set, 1, 0, 1,
		  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		  0, 0, &dctx->svertex_objects[1].buffer_view.view },
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);

	DARRAY_APPEND (&dctx->fonts, (drawfont_t) { .set = dctx->core_quad_set });

	qfvPopDebug (ctx);
}

static inline descbatch_t *
get_desc_batch (drawframe_t *frame, int descid, uint32_t ind_count)
{
	descbatch_t *batch = &frame->quad_batch.a[frame->quad_batch.size - 1];
	if (!frame->quad_batch.size || batch->descid != descid
		|| ((batch->count & (0xff << 24)) != (ind_count << 24))) {
		DARRAY_APPEND(&frame->quad_batch, ((descbatch_t) { .descid = descid }));
		batch = &frame->quad_batch.a[frame->quad_batch.size - 1];
		batch->count = ind_count << 24;
	}

	return batch;
}

static inline void
draw_slice (float x, float y, float ox, float oy, int descid, uint32_t vertid,
			const byte *color, drawframe_t *frame)
{
	__auto_type queue = &frame->quad_insts;
	if (queue->count >= queue->size) {
		return;
	}

	__auto_type batch = get_desc_batch (frame, descid, INDS_PER_SLICE);
	batch->count++;

	quadinst_t *quad = &queue->quads[queue->count++];
	*quad = (quadinst_t) {
		.index = vertid,
		.color = { QuatExpand (color) },
		.position = { x, y },
		.offset = { ox, oy },
	};
}

static inline void
draw_quad (float x, float y, int descid, uint32_t vertid, const byte *color,
		   drawframe_t *frame)
{
	__auto_type queue = &frame->quad_insts;
	if (queue->count >= queue->size) {
		return;
	}

	__auto_type batch = get_desc_batch (frame, descid, INDS_PER_QUAD);
	batch->count++;

	quadinst_t *quad = &queue->quads[queue->count++];
	*quad = (quadinst_t) {
		.index = vertid,
		.color = { QuatExpand (color) },
		.position = { x, y },
		.offset = { 0, 0 },
	};
}

static inline void
queue_character (int x, int y, byte chr, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	byte        color[4] = {255, 255, 255, 255};
	draw_quad (x, y, CORE_DESC, dctx->conchar_inds[chr], color, frame);
}

void
Vulkan_Draw_CharBuffer (int x, int y, draw_charbuffer_t *buffer,
						vulkan_ctx_t *ctx)
{
	const byte *line = (byte *) buffer->chars;
	int         width = buffer->width;
	int         height = buffer->height;
	while (height-- > 0) {
		for (int i = 0; i < width; i++) {
			Vulkan_Draw_Character (x + i * 8, y, line[i], ctx);
		}
		line += width;
		y += 8;
	}
}

void
Vulkan_Draw_Character (int x, int y, unsigned int chr, vulkan_ctx_t *ctx)
{
	if (chr == ' ') {
		return;
	}
	if (y <= -8 || y >= (int) vid.height) {
		return;
	}
	if (x <= -8 || x >= (int) vid.width) {
		return;
	}
	queue_character (x, y, chr, ctx);
}

void
Vulkan_Draw_String (int x, int y, const char *str, vulkan_ctx_t *ctx)
{
	byte        chr;

	if (!str || !str[0]) {
		return;
	}
	if (y <= -8 || y >= (int) vid.height) {
		return;
	}
	while (*str) {
		if ((chr = *str++) != ' ' && x >= -8 && x < (int) vid.width) {
			queue_character (x, y, chr, ctx);
		}
		x += 8;
	}
}

void
Vulkan_Draw_nString (int x, int y, const char *str, int count,
					 vulkan_ctx_t *ctx)
{
	byte        chr;

	if (!str || !str[0]) {
		return;
	}
	if (y <= -8 || y >= (int) vid.height) {
		return;
	}
	while (count-- > 0 && *str) {
		if ((chr = *str++) != ' ' && x >= -8 && x < (int) vid.width) {
			queue_character (x, y, chr, ctx);
		}
		x += 8;
	}
}

void
Vulkan_Draw_AltString (int x, int y, const char *str, vulkan_ctx_t *ctx)
{
	byte        chr;

	if (!str || !str[0]) {
		return;
	}
	if (y <= -8 || y >= (int) vid.height) {
		return;
	}
	while (*str) {
		if ((chr = *str++ | 0x80) != (' ' | 0x80)
			&& x >= -8 && x < (int) vid.width) {
			queue_character (x, y, chr, ctx);
		}
		x += 8;
	}
}

static void
draw_crosshair_plus (int ch, int x, int y, vulkan_ctx_t *ctx)
{
	Vulkan_Draw_Character (x - 4, y - 4, '+', ctx);
}

static void
draw_crosshair_pic (int ch, int x, int y, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	byte       *color = &vid.palette32[bound (0, crosshaircolor, 255) * 4];
	draw_quad (x, y, CORE_DESC, dctx->crosshair_inds[ch - 1], color, frame);
}

static void (*crosshair_func[]) (int ch, int x, int y, vulkan_ctx_t *ctx) = {
	draw_crosshair_plus,
	draw_crosshair_pic,
	draw_crosshair_pic,
	draw_crosshair_pic,
	draw_crosshair_pic,
};

void
Vulkan_Draw_CrosshairAt (int ch, int x, int y, vulkan_ctx_t *ctx)
{
	unsigned    c = ch - 1;

	if (c >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[c] (c, x, y, ctx);
}

void
Vulkan_Draw_Crosshair (vulkan_ctx_t *ctx)
{
	int         x, y;
	int         s = 2 * ctx->twod_scale;

	x = vid.width / s + cl_crossx;
	y = vid.height / s + cl_crossy;

	Vulkan_Draw_CrosshairAt (crosshair, x, y, ctx);
}

void
Vulkan_Draw_TextBox (int x, int y, int width, int lines, byte alpha,
					 vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	byte        color[4] = {255, 255, 255, 255};
	qpic_t     *p;
	int         cx, cy, n;
#define draw(px, py, pp)													\
	do {																	\
		__auto_type pd = (picdata_t *) pp->data;							\
		draw_quad (px, py, pd->descid, pd->vert_index, color, frame);		\
	} while (0)

	color[3] = alpha;
	// draw left side
	cx = x;
	cy = y;
	p = Vulkan_Draw_CachePic ("gfx/box_tl.lmp", true, ctx);
	draw (cx, cy, p);
	p = Vulkan_Draw_CachePic ("gfx/box_ml.lmp", true, ctx);
	for (n = 0; n < lines; n++) {
		cy += 8;
		draw (cx, cy, p);
	}
	p = Vulkan_Draw_CachePic ("gfx/box_bl.lmp", true, ctx);
	draw (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Vulkan_Draw_CachePic ("gfx/box_tm.lmp", true, ctx);
		draw (cx, cy, p);
		p = Vulkan_Draw_CachePic ("gfx/box_mm.lmp", true, ctx);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Vulkan_Draw_CachePic ("gfx/box_mm2.lmp", true, ctx);
			draw (cx, cy, p);
		}
		p = Vulkan_Draw_CachePic ("gfx/box_bm.lmp", true, ctx);
		draw (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Vulkan_Draw_CachePic ("gfx/box_tr.lmp", true, ctx);
	draw (cx, cy, p);
	p = Vulkan_Draw_CachePic ("gfx/box_mr.lmp", true, ctx);
	for (n = 0; n < lines; n++) {
		cy += 8;
		draw (cx, cy, p);
	}
	p = Vulkan_Draw_CachePic ("gfx/box_br.lmp", true, ctx);
	draw (cx, cy + 8, p);
#undef draw
}

void
Vulkan_Draw_Pic (int x, int y, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static byte color[4] = { 255, 255, 255, 255};
	__auto_type pd = (picdata_t *) pic->data;
	draw_quad (x, y, pd->descid, pd->vert_index, color, frame);
}

void
Vulkan_Draw_FitPic (int x, int y, int width, int height, qpic_t *pic,
					vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];
	__auto_type pd = (picdata_t *) pic->data;
	if (pd->slice_index == ~0u) {
		vec4i_t     rect = (vec4i_t) {0, 0, pic->width, pic->height};
		vec4i_t     border = (vec4i_t) {0, 0, 0, 0};
		pd->slice_index = make_static_slice (rect, border, pic, ctx);
	}
	static byte color[4] = { 255, 255, 255, 255};
	draw_slice (x, y, width - pic->width, height - pic->height,
				pd->descid, pd->slice_index, color, frame);
}

void
Vulkan_Draw_Picf (float x, float y, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static byte color[4] = { 255, 255, 255, 255};
	__auto_type pd = (picdata_t *) pic->data;
	draw_quad (x, y, pd->descid, pd->vert_index, color, frame);
}

void
Vulkan_Draw_SubPic (int x, int y, qpic_t *pic,
					int srcx, int srcy, int width, int height,
					vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	uint32_t    vind = make_dyn_quad (srcx, srcy, width, height, pic, ctx);
	static byte color[4] = { 255, 255, 255, 255};
	int         descid = get_dyn_descriptor (&frame->dyn_descs, pic,
											 frame->dvert_view, ctx);
	draw_quad (x, y, descid, vind, color, frame);
}

void
Vulkan_Draw_ConsoleBackground (int lines, byte alpha, vulkan_ctx_t *ctx)
{
	//FIXME fitpic with color
	//float       a = bound (0, alpha, 255) / 255.0;
	// use pre-multiplied alpha
	//quat_t      color = { a, a, a, a};
	qpic_t     *cpic;
	cpic = Vulkan_Draw_CachePic ("gfx/conback.lmp", false, ctx);
	float       s = 1.0 / ctx->twod_scale;
	int         y = lines - vid.height * s;
	Vulkan_Draw_FitPic (0, y, vid.width * s, vid.height * s, cpic, ctx);
}

void
Vulkan_Draw_TileClear (int x, int y, int w, int h, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static byte color[4] = { 255, 255, 255, 255};
	vrect_t    *tile_rect = VRect_New (x, y, w, h);
	vrect_t    *sub = VRect_New (0, 0, 0, 0);  // filled in later
	qpic_t     *pic = dctx->backtile_pic;
	int         sub_sx, sub_sy, sub_ex, sub_ey;
	int         descid = get_dyn_descriptor (&frame->dyn_descs, pic,
											 frame->dvert_view, ctx);

	sub_sx = x / pic->width;
	sub_sy = y / pic->height;
	sub_ex = (x + w + pic->width - 1) / pic->width;
	sub_ey = (y + h + pic->height - 1) / pic->height;
	for (int j = sub_sy; j < sub_ey; j++) {
		for (int i = sub_sx; i < sub_ex; i++) {
			vrect_t    *t = sub;

			sub->x = i * pic->width;
			sub->y = j * pic->height;
			sub->width = pic->width;
			sub->height = pic->height;
			sub = VRect_Intersect (sub, tile_rect);
			VRect_Delete (t);
			int         sx = sub->x % pic->width;
			int         sy = sub->y % pic->height;
			int         sw = sub->width;
			int         sh = sub->height;
			uint32_t    vind = make_dyn_quad (sx, sy, sw, sh, pic, ctx);
			draw_quad (sub->x, sub->y, descid, vind, color, frame);
		}
	}
}

void
Vulkan_Draw_Fill (int x, int y, int w, int h, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	byte        color[4] =  {VectorExpand (vid.palette + c * 3), 255 };
	__auto_type pd = (picdata_t *) dctx->white_pic->data;
	draw_slice (x, y, w - 1, h - 1, pd->descid, pd->slice_index, color, frame);
}

void
Vulkan_Draw_Line (int x0, int y0, int x1, int y1, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];
	linequeue_t *queue = &frame->line_verts;

	if (queue->count >= queue->size) {
		return;
	}

	linevert_t *verts = queue->verts + queue->count * VERTS_PER_LINE;
	verts[0] = (linevert_t) {
		.xy = { x0, y0 },
		.color = { VectorExpand (vid.palette + c * 3), 255 },
	};
	verts[1] = (linevert_t) {
		.xy = { x1, y1 },
		.color = { VectorExpand (vid.palette + c * 3), 255 },
	};

	queue->count++;
}

static inline void
draw_blendscreen (const byte *color, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];
	float       s = 1.0 / ctx->twod_scale;

	__auto_type pd = (picdata_t *) dctx->white_pic->data;
	draw_slice (0, 0, vid.width * s - 1, vid.height * s - 1,
				pd->descid, pd->slice_index, color, frame);
}

void
Vulkan_Draw_FadeScreen (vulkan_ctx_t *ctx)
{
	static byte color[4] =  { 0, 0, 0, 179 };
	draw_blendscreen (color, ctx);
}

void
Vulkan_Draw_BlendScreen (quat_t color, vulkan_ctx_t *ctx)
{
	if (color[3]) {
		byte        c[4];
		// pre-multiply alpha.
		// FIXME this is kind of silly because q1source pre-multiplies alpha
		// for blends, but this was un-done early in QF's history in order
		// to avoid a pair of state changes
		VectorScale (color, color[3] * 255, c);
		c[3] = color[3] * 255;
		draw_blendscreen (c, ctx);
	}
}

int
Vulkan_Draw_AddFont (font_t *rfont, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	int         fontid = dctx->fonts.size;
	DARRAY_OPEN_AT (&dctx->fonts, fontid, 1);
	drawfont_t *font = &dctx->fonts.a[fontid];

	font->resource = malloc (sizeof (drawfontres_t));
	font->resource->resource = (qfv_resource_t) {
		.name = va (ctx->va_ctx, "glyph_data:%d", fontid),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 4,
		.objects = &font->resource->glyph_data,
	};

	font->resource->glyph_data = (qfv_resobj_t) {
		.name = "geom",
		.type = qfv_res_buffer,
		.buffer = {
			.size = rfont->num_glyphs * 4 * sizeof (quadvert_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		},
	};
	__auto_type glyph_data = &font->resource->glyph_data;

	font->resource->glyph_bview = (qfv_resobj_t) {
		.name = "geom_view",
		.type = qfv_res_buffer_view,
		.buffer_view = {
			.buffer = 0,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = 0,
			.size = font->resource->glyph_data.buffer.size,
		},
	};
	__auto_type glyph_bview = &font->resource->glyph_bview;

	tex_t       tex = {
		.width = rfont->scrap.width,
		.height = rfont->scrap.height,
		.format = tex_a,
		.loaded = 1,
		.data = rfont->scrap_bitmap,
	};
	QFV_ResourceInitTexImage (&font->resource->glyph_image, "image", 0, &tex);
	__auto_type glyph_image = &font->resource->glyph_image;

	font->resource->glyph_iview = (qfv_resobj_t) {
		.name = "image_view",
		.type = qfv_res_image_view,
		.image_view = {
			.image = 2,
			.type = VK_IMAGE_VIEW_TYPE_2D,
			.format = font->resource->glyph_image.image.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
			.components = {
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_R,
				.b = VK_COMPONENT_SWIZZLE_R,
				.a = VK_COMPONENT_SWIZZLE_R,
			},
		},
	};
	__auto_type glyph_iview = &font->resource->glyph_iview;

	QFV_CreateResource (ctx->device, &font->resource->resource);

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	quadvert_t *verts = QFV_PacketExtend (packet, glyph_data->buffer.size);
	for (FT_Long i = 0; i < rfont->num_glyphs; i++) {
		vrect_t    *rect = &rfont->glyph_rects[i];
		float       x = 0;
		float       y = 0;
		float       w = rect->width;
		float       h = rect->height;
		float       u = rect->x;
		float       v = rect->y;
		float       s = 1.0 / rfont->scrap.width;
		float       t = 1.0 / rfont->scrap.height;
		verts[i * 4 + 0] = (quadvert_t) {
			.offset = { x,     y },
			.uv     = { u * s, v * t },
		};
		verts[i * 4 + 1] = (quadvert_t) {
			.offset = { x,      y + h },
			.uv     = { u * s, (v + h) * t },
		};
		verts[i * 4 + 2] = (quadvert_t) {
			.offset = { x + w,      y },
			.uv     = {(u + w) * s, v * t },
		};
		verts[i * 4 + 3] = (quadvert_t) {
			.offset = { x + w,       y + h },
			.uv     = {(u + w) * s, (v + h) * t },
		};
	}
	QFV_PacketCopyBuffer (packet, glyph_data->buffer.buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (ctx->staging);
	byte       *texels = QFV_PacketExtend (packet, tex.width * tex.height);
	memcpy (texels, tex.data, tex.width * tex.height);
	QFV_PacketCopyImage (packet, glyph_image->image.image,
						 tex.width, tex.height,
						 &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly]);
	QFV_PacketSubmit (packet);

	font->set = QFV_DSManager_AllocSet (dctx->dsmanager);;
	VkDescriptorImageInfo imageInfo = {
		dctx->glyph_sampler,
		glyph_iview->image_view.view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  font->set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  font->set, 1, 0, 1,
		  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		  0, 0, &glyph_bview->buffer_view.view },
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);

	return fontid;
}

void
Vulkan_Draw_Glyph (int x, int y, int fontid, int glyph, int c,
						vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	quadqueue_t *queue = &frame->quad_insts;
	if (queue->count >= queue->size) {
		return;
	}

	byte        color[4] = { VectorExpand (vid.palette + c * 3), 255 };
	draw_quad (x, y, fontid, glyph * 4, color, frame);
}

void
Vulkan_LineGraph (int x, int y, int *h_vals, int count, int height,
				  vulkan_ctx_t *ctx)
{
	static int colors[] = { 0xd0, 0x4f, 0x6f };

	while (count-- > 0) {
		int         h = *h_vals++;
		int         c = h < 9998 || h > 10000 ? 0xfe : colors[h - 9998];
		h = min (h, height);
		Vulkan_Draw_Line (x, y, x, y - h, c, ctx);
		x++;
	}
}

void
Vulkan_SetScrFuncs (SCR_Func *scr_funcs, vulkan_ctx_t *ctx)
{
	auto dctx = ctx->draw_context;
	dctx->scr_funcs = scr_funcs;
}
