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
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"
#include "QF/ui/font.h"
#include "QF/ui/view.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static const char * __attribute__((used)) draw_pass_names[] = {
	"2d",
};

static QFV_Subpass subpass_map[] = {
	[QFV_draw2d]    = 0,
};

typedef struct descbatch_s {
	int32_t     descid;		// texture or font descriptor id
	uint32_t    count;		// number of objects in batch
} descbatch_t;

typedef struct descbatchset_s
	DARRAY_TYPE (descbatch_t) descbatchset_t;

typedef struct {
	float       xy[2];
	float       st[2];
	float       color[4];
} drawvert_t;

typedef struct {
	uint32_t    index;
	byte        color[4];
	float       position[2];
	float       offset[2];
} sliceinst_t;

typedef struct {
	float       offset[2];
	float       uv[2];
} glyphvert_t;

typedef struct cachepic_s {
	char       *name;
	qpic_t     *pic;
} cachepic_t;

typedef struct vertqueue_s {
	drawvert_t *verts;
	int         count;
	int         size;
} vertqueue_t;

typedef struct slicequeue_s {
	sliceinst_t *slices;
	int         count;
	int         size;
} slicequeue_t;

typedef struct glyphqueue_s {
	sliceinst_t *glyphs;
	int         count;
	int         size;
} glyphqueue_t;

typedef struct drawframe_s {
	size_t      quad_offset;
	size_t      slice_offset;
	size_t      glyph_offset;
	size_t      line_offset;
	VkBuffer    quad_buffer;
	descbatchset_t quad_batch;
	VkBuffer    slice_buffer;
	descbatchset_t slice_batch;
	VkBuffer    glyph_buffer;
	descbatchset_t glyph_batch;
	VkBuffer    line_buffer;
	vertqueue_t quad_verts;
	slicequeue_t slice_insts;
	glyphqueue_t glyph_insts;
	vertqueue_t line_verts;
	qfv_cmdbufferset_t cmdSet;
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
	font_t     *font;
} drawfont_t;

typedef struct drawfontset_s
	DARRAY_TYPE (drawfont_t) drawfontset_t;

typedef struct drawctx_s {
	VkSampler   pic_sampler;
	VkSampler   glyph_sampler;
	scrap_t    *scrap;
	qfv_stagebuf_t *stage;
	qpic_t     *crosshair;
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
	qfv_resource_t *draw_resource;
	qfv_resobj_t *ind_objects;
	qfv_resobj_t *quad_objects;
	qfv_resobj_t *slice_objects;
	qfv_resobj_t *glyph_objects;
	qfv_resobj_t *line_objects;
	VkPipeline  quad_pipeline;
	VkPipeline  slice_pipeline;
	VkPipeline  line_pipeline;
	VkPipelineLayout layout;
	VkPipelineLayout glyph_layout;//slice pipeline uses same layout
	VkDescriptorSet quad_set;
	drawframeset_t frames;
	drawfontset_t fonts;
} drawctx_t;

// enough for a full screen of 8x8 chars at 1920x1080 plus some extras (368)
#define MAX_QUADS (32768)
#define VERTS_PER_QUAD (4)
#define INDS_PER_QUAD (5)	// one per vert plus primitive reset

#define MAX_GLYPHS (32768)

#define MAX_LINES (32768)
#define VERTS_PER_LINE (2)

#define QUADS_OFFSET 0
#define IAQUADS_OFFSET (MAX_QUADS * VERTS_PER_QUAD)
#define LINES_OFFSET (IAQUADS_OFFSET + (MAX_QUADS * VERTS_PER_QUAD))

#define VERTS_PER_FRAME (LINES_OFFSET + MAX_LINES*VERTS_PER_LINE)

static void
generate_quad_indices (qfv_stagebuf_t *staging, qfv_resobj_t *ind_buffer)
{
	qfv_packet_t *packet = QFV_PacketAcquire (staging);
	uint32_t   *ind = QFV_PacketExtend (packet, ind_buffer->buffer.size);
	for (int i = 0; i < MAX_QUADS; i++) {
		for (int j = 0; j < VERTS_PER_QUAD; j++) {
			*ind++ = i * VERTS_PER_QUAD + j;
		}
		// mark end of primitive
		*ind++ = -1;
	}
	QFV_PacketCopyBuffer (packet, ind_buffer->buffer.buffer,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
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
	QFV_PacketCopyBuffer (packet, ind_buffer->buffer.buffer,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
}

static void
create_quad_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	size_t      frames = ctx->frames.size;

	dctx->draw_resource = malloc (2 * sizeof (qfv_resource_t)
								  // index buffers
								  + 2 * sizeof (qfv_resobj_t)
								  // quads: frames vertex buffers
								  + (frames) * sizeof (qfv_resobj_t)
								  // slicess: frames instance vertex buffers
								  + (frames) * sizeof (qfv_resobj_t)
								  // glyphs: frames instance vertex buffers
								  + (frames) * sizeof (qfv_resobj_t)
								  // lines: frames vertex buffers
								  + (frames) * sizeof (qfv_resobj_t));
	dctx->ind_objects = (qfv_resobj_t *) &dctx->draw_resource[2];
	dctx->quad_objects = &dctx->ind_objects[2];
	dctx->slice_objects = &dctx->quad_objects[frames];
	dctx->glyph_objects = &dctx->slice_objects[frames];
	dctx->line_objects = &dctx->glyph_objects[frames];

	dctx->draw_resource[0] = (qfv_resource_t) {
		.name = "draw",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2,	// quad and 9-slice indices
		.objects = dctx->ind_objects,
	};
	dctx->draw_resource[1] = (qfv_resource_t) {
		.name = "draw",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = (frames) + (frames) + (frames) + (frames),
		.objects = dctx->quad_objects,
	};

	dctx->ind_objects[0] = (qfv_resobj_t) {
		.name = "quads.index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = MAX_QUADS * INDS_PER_QUAD * sizeof (uint32_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};
	dctx->ind_objects[1] = (qfv_resobj_t) {
		.name = "9-slice.index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = 26 * sizeof (uint32_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};

	for (size_t i = 0; i < frames; i++) {
		dctx->quad_objects[i] = (qfv_resobj_t) {
			.name = "quads.geom",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_QUADS * VERTS_PER_QUAD * sizeof (drawvert_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		dctx->slice_objects[i] = (qfv_resobj_t) {
			.name = "slices.inst",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_GLYPHS * sizeof (sliceinst_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		dctx->glyph_objects[i] = (qfv_resobj_t) {
			.name = "glyphs.inst",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_GLYPHS * sizeof (sliceinst_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		dctx->line_objects[i] = (qfv_resobj_t) {
			.name = "lines.geom",
			.type = qfv_res_buffer,
			.buffer = {
				.size = MAX_LINES * VERTS_PER_LINE * sizeof (drawvert_t),
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
		frame->quad_buffer = dctx->quad_objects[f].buffer.buffer;
		frame->quad_offset = dctx->quad_objects[f].buffer.offset;
		frame->slice_buffer = dctx->slice_objects[f].buffer.buffer;
		frame->slice_offset = dctx->slice_objects[f].buffer.offset;
		frame->glyph_buffer = dctx->glyph_objects[f].buffer.buffer;
		frame->glyph_offset = dctx->glyph_objects[f].buffer.offset;
		frame->line_buffer = dctx->line_objects[f].buffer.buffer;
		frame->line_offset = dctx->line_objects[f].buffer.offset;

		frame->quad_verts = (vertqueue_t) {
			.verts = (drawvert_t *) ((byte *)data + frame->quad_offset),
			.size = MAX_QUADS,
		};
		DARRAY_INIT (&frame->quad_batch, 16);
		frame->slice_insts = (slicequeue_t) {
			.slices = (sliceinst_t *) ((byte *)data + frame->slice_offset),
			.size = MAX_QUADS,
		};
		DARRAY_INIT (&frame->slice_batch, 16);
		frame->glyph_insts = (glyphqueue_t) {
			.glyphs = (sliceinst_t *) ((byte *)data + frame->glyph_offset),
			.size = MAX_QUADS,
		};
		DARRAY_INIT (&frame->glyph_batch, 16);
		frame->line_verts = (vertqueue_t) {
			.verts = (drawvert_t *) ((byte *)data + frame->line_offset),
			.size = MAX_QUADS,
		};
	}

	// The indices will never change so pre-generate and stash them
	generate_quad_indices (ctx->staging, &dctx->ind_objects[0]);
	generate_slice_indices (ctx->staging, &dctx->ind_objects[1]);
}

static void
flush_draw_scrap (vulkan_ctx_t *ctx)
{
	QFV_ScrapFlush (ctx->draw_context->scrap);
}

static void
pic_free (drawctx_t *dctx, qpic_t *pic)
{
	subpic_t   *subpic = *(subpic_t **) &pic->data[0];
	QFV_SubpicDelete (subpic);
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

static qpic_t *
pic_data (const char *name, int w, int h, const byte *data, drawctx_t *dctx)
{
	qpic_t     *pic;
	subpic_t   *subpic;
	byte       *picdata;

	pic = cmemalloc (dctx->pic_memsuper,
					 field_offset (qpic_t, data[sizeof (subpic_t *)]));
	pic->width = w;
	pic->height = h;

	subpic = QFV_ScrapSubpic (dctx->scrap, w, h);
	*(subpic_t **) pic->data = subpic;

	picdata = QFV_SubpicBatch (subpic, dctx->stage);
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
	//FIXME live updates of the scrap aren't
	//syncronized properly for some reason and result in stale texels being
	//rendered (flashing pink around the Q menu cursor the first time it's
	//displayed). I suspect simple barriers aren't enough and more
	//sophisticated syncronization (events? semaphores?) is needed.
	return pic;
}

qpic_t *
Vulkan_Draw_MakePic (int width, int height, const byte *data,
					 vulkan_ctx_t *ctx)
{
	return pic_data (0, width, height, data, ctx->draw_context);
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
	return pic_data (name, wadpic->width, wadpic->height, wadpic->data,
					 ctx->draw_context);
}

qpic_t *
Vulkan_Draw_CachePic (const char *path, qboolean alpha, vulkan_ctx_t *ctx)
{
	qpic_t     *p;
	qpic_t     *pic;
	cachepic_t *cpic;
	drawctx_t  *dctx = ctx->draw_context;

	if ((cpic = Hash_Find (dctx->pic_cache, path))) {
		return cpic->pic;
	}
	if (strlen (path) < 4 || strcmp (path + strlen (path) - 4, ".lmp")
		|| !(p = (qpic_t *) QFS_LoadFile (QFS_FOpenFile (path), 0))) {
		return 0;
	}

	pic = pic_data (path, p->width, p->height, p->data, dctx);
	free (p);
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
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;

	QFV_DestroyResource (device, &dctx->draw_resource[0]);
	QFV_DestroyResource (device, &dctx->draw_resource[1]);
	for (size_t i = 0; i < dctx->fonts.size; i++) {
		QFV_DestroyResource (device, &dctx->fonts.a[i].resource->resource);
		free (dctx->fonts.a[i].resource);
	}

	dfunc->vkDestroyPipeline (device->dev, dctx->quad_pipeline, 0);
	dfunc->vkDestroyPipeline (device->dev, dctx->slice_pipeline, 0);
	dfunc->vkDestroyPipeline (device->dev, dctx->line_pipeline, 0);
	Hash_DelTable (dctx->pic_cache);
	delete_memsuper (dctx->pic_memsuper);
	delete_memsuper (dctx->string_memsuper);
	QFV_DestroyScrap (dctx->scrap);
	QFV_DestroyStagingBuffer (dctx->stage);
}

void
Vulkan_Draw_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "draw init");

	drawctx_t  *dctx = calloc (1, sizeof (drawctx_t));
	ctx->draw_context = dctx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&dctx->frames, frames);
	DARRAY_RESIZE (&dctx->frames, frames);
	dctx->frames.grow = 0;
	DARRAY_INIT (&dctx->fonts, 16);
	DARRAY_RESIZE (&dctx->fonts, 16);
	dctx->fonts.grow = 0;
	dctx->fonts.size = 0;

	dctx->pic_memsuper = new_memsuper ();
	dctx->string_memsuper = new_memsuper ();
	dctx->pic_cache = Hash_NewTable (127, cachepic_getkey, cachepic_free,
									 dctx, 0);

	create_quad_buffers (ctx);
	dctx->stage = QFV_CreateStagingBuffer (device, "draw", 4 * 1024 * 1024,
										   ctx->cmdpool);
	dctx->scrap = QFV_CreateScrap (device, "draw_atlas", 2048, tex_rgba,
								   dctx->stage);
	dctx->pic_sampler = Vulkan_CreateSampler (ctx, "quakepic");
	dctx->glyph_sampler = Vulkan_CreateSampler (ctx, "glyph");

	draw_chars = W_GetLumpName ("conchars");
	if (draw_chars) {
		for (int i = 0; i < 256 * 64; i++) {
			if (draw_chars[i] == 0) {
				draw_chars[i] = 255;		// proper transparent color
			}
		}
		dctx->conchars = pic_data ("conchars", 128, 128, draw_chars, dctx);
	} else {
		qpic_t     *charspic = Draw_Font8x8Pic ();
		dctx->conchars = pic_data ("conchars", charspic->width,
								   charspic->height, charspic->data, dctx);
		free (charspic);
	}
	{
		qpic_t     *hairpic = Draw_CrosshairPic ();
		dctx->crosshair = pic_data ("crosshair", hairpic->width,
									hairpic->height, hairpic->data, dctx);
		free (hairpic);
	}

	byte white_block = 0xfe;
	dctx->white_pic = pic_data ("white", 1, 1, &white_block, dctx);

	dctx->backtile_pic = Vulkan_Draw_PicFromWad ("backtile", ctx);
	if (!dctx->backtile_pic) {
		dctx->backtile_pic = dctx->white_pic;
	}

	flush_draw_scrap (ctx);

	dctx->quad_pipeline = Vulkan_CreateGraphicsPipeline (ctx, "twod");
	dctx->slice_pipeline = Vulkan_CreateGraphicsPipeline (ctx, "slice");
	dctx->line_pipeline = Vulkan_CreateGraphicsPipeline (ctx, "lines");

	dctx->layout = Vulkan_CreatePipelineLayout (ctx, "twod_layout");
	dctx->glyph_layout = Vulkan_CreatePipelineLayout (ctx, "glyph_layout");

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (1, alloca);
	layouts->a[0] = Vulkan_CreateDescriptorSetLayout (ctx, "twod_set");
	__auto_type pool = Vulkan_CreateDescriptorPool (ctx, "twod_pool");

	VkDescriptorImageInfo imageInfo = {
		dctx->pic_sampler,
		QFV_ScrapImageView (dctx->scrap),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	dctx->quad_set = sets->a[0];
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  dctx->quad_set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 },
	};
	free (sets);

	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
	for (size_t i = 0; i < frames; i++) {
		__auto_type dframe = &dctx->frames.a[i];

		DARRAY_INIT (&dframe->cmdSet, QFV_drawNumPasses);
		DARRAY_RESIZE (&dframe->cmdSet, QFV_drawNumPasses);
		dframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &dframe->cmdSet);

		for (int j = 0; j < QFV_drawNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 dframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:draw:%zd:%s", i,
									 draw_pass_names[j]));
		}
	}
	qfvPopDebug (ctx);
}

static inline void
draw_pic (float x, float y, int w, int h, subpic_t *subpic,
		  int srcx, int srcy, int srcw, int srch,
		  float *color, vertqueue_t *queue)
{
	if (queue->count >= queue->size) {
		return;
	}

	drawvert_t *verts = queue->verts + queue->count * VERTS_PER_QUAD;
	queue->count++;

	srcx += subpic->rect->x;
	srcy += subpic->rect->y;

	float size = subpic->size;
	float sl = srcx * size;
	float sr = (srcx + srcw) * size;
	float st = srcy * size;
	float sb = (srcy + srch) * size;

	verts[0].xy[0] = x;
	verts[0].xy[1] = y;
	verts[0].st[0] = sl;
	verts[0].st[1] = st;
	QuatCopy (color, verts[0].color);

	verts[1].xy[0] = x;
	verts[1].xy[1] = y + h;
	verts[1].st[0] = sl;
	verts[1].st[1] = sb;
	QuatCopy (color, verts[1].color);

	verts[2].xy[0] = x + w;
	verts[2].xy[1] = y;
	verts[2].st[0] = sr;
	verts[2].st[1] = st;
	QuatCopy (color, verts[2].color);

	verts[3].xy[0] = x + w;
	verts[3].xy[1] = y + h;
	verts[3].st[0] = sr;
	verts[3].st[1] = sb;
	QuatCopy (color, verts[3].color);
}

static inline void
queue_character (int x, int y, byte chr, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	quat_t      color = {1, 1, 1, 1};
	int         cx, cy;
	cx = chr % 16;
	cy = chr / 16;

	subpic_t   *subpic = *(subpic_t **) dctx->conchars->data;
	draw_pic (x, y, 8, 8, subpic, cx * 8, cy * 8, 8, 8, color,
			  &frame->quad_verts);
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

	static const int pos[CROSSHAIR_COUNT][4] = {
		{0,               0,                CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{CROSSHAIR_WIDTH, 0,                CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{0,               CROSSHAIR_HEIGHT, CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
		{CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT, CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT},
	};
	const int *p = pos[ch - 1];

	subpic_t   *subpic = *(subpic_t **) dctx->crosshair->data;
	draw_pic (x - CROSSHAIR_WIDTH / 2 + 1, y - CROSSHAIR_HEIGHT / 2 + 1,
			  CROSSHAIR_WIDTH, CROSSHAIR_HEIGHT, subpic,
			  p[0], p[1], p[2], p[3], crosshair_color, &frame->quad_verts);
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

	quat_t      color = {1, 1, 1, 1};
	qpic_t     *p;
	int         cx, cy, n;
#define draw(px, py, pp)													\
	do {																	\
		subpic_t   *subpic = *(subpic_t **) (pp)->data;						\
		draw_pic (px, py, pp->width, pp->height, subpic,					\
				  0, 0, pp->width, pp->height, color, &frame->quad_verts);	\
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

	static quat_t color = { 1, 1, 1, 1};
	subpic_t   *subpic = *(subpic_t **) pic->data;
	draw_pic (x, y, pic->width, pic->height, subpic,
			  0, 0, pic->width, pic->height, color, &frame->quad_verts);
}

void
Vulkan_Draw_Picf (float x, float y, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static quat_t color = { 1, 1, 1, 1};
	subpic_t   *subpic = *(subpic_t **) pic->data;
	draw_pic (x, y, pic->width, pic->height, subpic,
			  0, 0, pic->width, pic->height, color, &frame->quad_verts);
}

void
Vulkan_Draw_SubPic (int x, int y, qpic_t *pic,
					int srcx, int srcy, int width, int height,
					vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static quat_t color = { 1, 1, 1, 1};
	subpic_t   *subpic = *(subpic_t **) pic->data;
	draw_pic (x, y, width, height, subpic, srcx, srcy, width, height,
			  color, &frame->quad_verts);
}

void
Vulkan_Draw_ConsoleBackground (int lines, byte alpha, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	float       a = bound (0, alpha, 255) / 255.0;
	// use pre-multiplied alpha
	quat_t      color = { a, a, a, a};
	qpic_t     *cpic;
	cpic = Vulkan_Draw_CachePic ("gfx/conback.lmp", false, ctx);
	int         s = ctx->twod_scale;
	float       frac = (vid.height - s * lines) / (float) vid.height;
	int         ofs = frac * cpic->height;
	subpic_t   *subpic = *(subpic_t **) cpic->data;
	draw_pic (0, 0, vid.width / s, lines, subpic,
			  0, ofs, cpic->width, cpic->height - ofs, color,
			  &frame->quad_verts);
}

void
Vulkan_Draw_TileClear (int x, int y, int w, int h, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static quat_t color = { 1, 1, 1, 1};
	vrect_t    *tile_rect = VRect_New (x, y, w, h);
	vrect_t    *sub = VRect_New (0, 0, 0, 0);  // filled in later
	qpic_t     *pic = dctx->backtile_pic;
	subpic_t   *subpic = *(subpic_t **) pic->data;
	int         sub_sx, sub_sy, sub_ex, sub_ey;

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
			draw_pic (sub->x, sub->y, sub->width, sub->height, subpic,
					  sub->x % pic->width, sub->y % pic->height,
					  sub->width, sub->height, color, &frame->quad_verts);
		}
	}
}

void
Vulkan_Draw_Fill (int x, int y, int w, int h, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	quat_t      color;

	VectorScale (vid.palette + c * 3, 1.0f/255.0f, color);
	color[3] = 1;
	subpic_t   *subpic = *(subpic_t **) dctx->white_pic->data;
	draw_pic (x, y, w, h, subpic, 0, 0, 1, 1, color,
			  &frame->quad_verts);
}

void
Vulkan_Draw_Line (int x0, int y0, int x1, int y1, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];
	vertqueue_t *queue = &frame->line_verts;

	if (queue->count >= queue->size) {
		return;
	}

	quat_t      color = { VectorExpand (vid.palette + c * 3), 255 };
	QuatScale (color, 1/255.0, color);
	drawvert_t *verts = queue->verts + queue->count * VERTS_PER_LINE;

	subpic_t   *subpic = *(subpic_t **) dctx->white_pic->data;
	int srcx = subpic->rect->x;
	int srcy = subpic->rect->y;
	int srcw = subpic->rect->width;
	int srch = subpic->rect->height;
	float size = subpic->size;
	float sl = (srcx + 0.03125) * size;
	float sr = (srcx + srcw - 0.03125) * size;
	float st = (srcy + 0.03125) * size;
	float sb = (srcy + srch - 0.03125) * size;

	verts[0] = (drawvert_t) {
		.xy = { x0, y0 },
		.st = {sl, st},
		.color = { QuatExpand (color) },
	};
	verts[1] = (drawvert_t) {
		.xy = { x1, y1 },
		.st = {sr, sb},
		.color = { QuatExpand (color) },
	};

	queue->count++;
}

static inline void
draw_blendscreen (quat_t color, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	subpic_t   *subpic = *(subpic_t **) dctx->white_pic->data;
	draw_pic (0, 0, vid.width, vid.height, subpic,
			  0, 0, 1, 1, color, &frame->quad_verts);
}

void
Vulkan_Draw_FadeScreen (vulkan_ctx_t *ctx)
{
	static quat_t color = { 0, 0, 0, 0.7 };
	draw_blendscreen (color, ctx);
}

void
Vulkan_Set2D (vulkan_ctx_t *ctx)
{
}

void
Vulkan_Set2DScaled (vulkan_ctx_t *ctx)
{
}

void
Vulkan_End2D (vulkan_ctx_t *ctx)
{
}

void
Vulkan_DrawReset (vulkan_ctx_t *ctx)
{
}

static void
draw_begin_subpass (QFV_DrawSubpass subpass, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = dframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, subpass_map[subpass],
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "draw:%s",
										  draw_pass_names[subpass]),
						 {0.5, 0.8, 0.1, 1});
}

static void
draw_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

static void
bind_pipeline (qfv_renderframe_t *rFrame, VkPipeline pipeline,
			   VkCommandBuffer cmd)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);
}

static void
draw_quads (qfv_renderframe_t *rFrame, VkCommandBuffer cmd)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	VkBuffer    quad_buffer = dframe->quad_buffer;
	VkBuffer    ind_buffer = dctx->ind_objects[0].buffer.buffer;

	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &quad_buffer, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, ind_buffer, 0, VK_INDEX_TYPE_UINT32);
	VkDescriptorSet set[2] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		dctx->quad_set,
	};
	VkPipelineLayout layout = dctx->layout;
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, set, 0, 0);

	dfunc->vkCmdDrawIndexed (cmd, dframe->quad_verts.count * INDS_PER_QUAD,
							 1, 0, 0, 0);
}

static void
draw_slices (qfv_renderframe_t *rFrame, VkCommandBuffer cmd)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	VkBuffer    slice_buffer = dframe->slice_buffer;
	VkBuffer    ind_buffer = dctx->ind_objects[1].buffer.buffer;
	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &slice_buffer, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, ind_buffer, 0, VK_INDEX_TYPE_UINT32);

	uint32_t    inst_start = 0;
	for (size_t i = 0; i < dframe->slice_batch.size; i++) {
		int         fontid = dframe->slice_batch.a[i].descid;
		uint32_t    inst_count = dframe->slice_batch.a[i].count;
		VkDescriptorSet set[2] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			dctx->fonts.a[fontid].set,
		};
		VkPipelineLayout layout = dctx->glyph_layout;
		dfunc->vkCmdBindDescriptorSets (cmd,
										VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2, set, 0, 0);

		dfunc->vkCmdDrawIndexed (cmd, 26, inst_count, 0, 0, inst_start);
		inst_start += inst_count;
	}
	DARRAY_RESIZE (&dframe->slice_batch, 0);
}

static void
draw_glyphs (qfv_renderframe_t *rFrame, VkCommandBuffer cmd)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	VkBuffer    glyph_buffer = dframe->glyph_buffer;
	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &glyph_buffer, offsets);

	uint32_t    inst_start = 0;
	for (size_t i = 0; i < dframe->glyph_batch.size; i++) {
		int         fontid = dframe->glyph_batch.a[i].descid;
		uint32_t    inst_count = dframe->glyph_batch.a[i].count;
		VkDescriptorSet set[2] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			dctx->fonts.a[fontid].set,
		};
		VkPipelineLayout layout = dctx->glyph_layout;
		dfunc->vkCmdBindDescriptorSets (cmd,
										VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2, set, 0, 0);

		dfunc->vkCmdDraw (cmd, 4, inst_count, 0, inst_start);
		inst_start += inst_count;
	}
	DARRAY_RESIZE (&dframe->glyph_batch, 0);
}

static void
draw_lines (qfv_renderframe_t *rFrame, VkCommandBuffer cmd)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	VkBuffer    line_buffer = dframe->line_buffer;
	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &line_buffer, offsets);
	VkDescriptorSet set[1] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	VkPipelineLayout layout = dctx->layout;
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 1, set, 0, 0);
	dfunc->vkCmdDraw (cmd, dframe->line_verts.count * VERTS_PER_LINE,
					  1, 0, 0);
}

void
Vulkan_FlushText (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	flush_draw_scrap (ctx);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	if (!dframe->quad_verts.count && !dframe->slice_insts.count
		&& !dframe->glyph_insts.count && !dframe->line_verts.count) {
		return;
	}

	VkDeviceMemory memory = dctx->draw_resource[1].memory;
	size_t      atom = device->physDev->properties->limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
#define a(x) (((x) + atom_mask) & ~atom_mask)
	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->quad_offset,
		  a(dframe->quad_verts.count * VERTS_PER_QUAD * sizeof (drawvert_t)) },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->slice_offset,
		  a(dframe->slice_insts.count * sizeof (sliceinst_t)) },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->glyph_offset,
		  a(dframe->glyph_insts.count * sizeof (sliceinst_t)) },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  memory, dframe->line_offset,
		  a(dframe->line_verts.count * VERTS_PER_LINE * sizeof (drawvert_t)) },
	};
#undef a
	dfunc->vkFlushMappedMemoryRanges (device->dev, 3, ranges);

	DARRAY_APPEND (&rFrame->subpassCmdSets[subpass_map[QFV_draw2d]],
				   dframe->cmdSet.a[QFV_draw2d]);

	draw_begin_subpass (QFV_draw2d, rFrame);

	if (dframe->quad_verts.count) {
		bind_pipeline (rFrame, dctx->quad_pipeline,
					   dframe->cmdSet.a[QFV_draw2d]);
		draw_quads (rFrame, dframe->cmdSet.a[QFV_draw2d]);
	}
	if (dframe->slice_insts.count) {
		bind_pipeline (rFrame, dctx->slice_pipeline,
					   dframe->cmdSet.a[QFV_draw2d]);
		draw_slices (rFrame, dframe->cmdSet.a[QFV_draw2d]);
	}
	if (dframe->glyph_insts.count) {
		bind_pipeline (rFrame, dctx->slice_pipeline,
					   dframe->cmdSet.a[QFV_draw2d]);
		draw_glyphs (rFrame, dframe->cmdSet.a[QFV_draw2d]);
	}
	if (dframe->line_verts.count) {
		bind_pipeline (rFrame, dctx->line_pipeline,
					   dframe->cmdSet.a[QFV_draw2d]);
		draw_lines (rFrame, dframe->cmdSet.a[QFV_draw2d]);
	}
	draw_end_subpass (dframe->cmdSet.a[QFV_draw2d], ctx);

	dframe->quad_verts.count = 0;
	dframe->slice_insts.count = 0;
	dframe->glyph_insts.count = 0;
	dframe->line_verts.count = 0;
}

void
Vulkan_Draw_BlendScreen (quat_t color, vulkan_ctx_t *ctx)
{
	if (color[3]) {
		quat_t      c;
		// pre-multiply alpha.
		// FIXME this is kind of silly because q1source pre-multiplies alpha
		// for blends, but this was un-done early in QF's history in order
		// to avoid a pair of state changes
		VectorScale (color, color[3], c);
		c[3] = color[3];
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

	font->font = rfont;

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
			.size = rfont->num_glyphs * 4 * sizeof (glyphvert_t),
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
			.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
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
	glyphvert_t *verts = QFV_PacketExtend (packet, glyph_data->buffer.size);
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
		verts[i * 4 + 0] = (glyphvert_t) {
			.offset = { x,     y },
			.uv     = { u * s, v * t },
		};
		verts[i * 4 + 1] = (glyphvert_t) {
			.offset = { x,      y + h },
			.uv     = { u * s, (v + h) * t },
		};
		verts[i * 4 + 2] = (glyphvert_t) {
			.offset = { x + w,      y },
			.uv     = {(u + w) * s, v * t },
		};
		verts[i * 4 + 3] = (glyphvert_t) {
			.offset = { x + w,       y + h },
			.uv     = {(u + w) * s, (v + h) * t },
		};
	}
	QFV_PacketCopyBuffer (packet, glyph_data->buffer.buffer,
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (ctx->staging);
	byte       *texels = QFV_PacketExtend (packet, tex.width * tex.height);
	memcpy (texels, tex.data, tex.width * tex.height);
	QFV_PacketCopyImage (packet, glyph_image->image.image,
						 tex.width, tex.height,
						 &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly]);
	QFV_PacketSubmit (packet);

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (1, alloca);
	layouts->a[0] = Vulkan_CreateDescriptorSetLayout (ctx, "glyph_data_set");
	__auto_type pool = Vulkan_CreateDescriptorPool (ctx, "glyph_pool");
	__auto_type glyph_sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	font->set = glyph_sets->a[0];
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
	free (glyph_sets);

	return fontid;
}

void
Vulkan_Draw_Glyph (int x, int y, int fontid, int glyph, int c,
						vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	glyphqueue_t *queue = &dframe->glyph_insts;;
	if (queue->count >= queue->size) {
		return;
	}

	descbatch_t *batch = &dframe->glyph_batch.a[dframe->glyph_batch.size - 1];
	if (!dframe->glyph_batch.size || batch->descid != fontid) {
		DARRAY_APPEND(&dframe->glyph_batch,
					  ((descbatch_t) { .descid = fontid }));
		batch = &dframe->glyph_batch.a[dframe->glyph_batch.size - 1];
	}

	batch->count++;
	sliceinst_t *inst = &queue->glyphs[queue->count++];
	inst->index = glyph * 4;	// index is actual vertex index
	VectorCopy (vid.palette + c * 3, inst->color);
	inst->color[3] = 255;
	inst->position[0] = x;
	inst->position[1] = y;
	inst->offset[0] = 0;
	inst->offset[1] = 0;
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
