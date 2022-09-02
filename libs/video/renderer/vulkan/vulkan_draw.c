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
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"
#include "QF/ui/view.h"

#include "r_font.h"
#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	float       xy[2];
	float       st[2];
	float       color[4];
} drawvert_t;

typedef struct cachepic_s {
	char       *name;
	qpic_t     *pic;
} cachepic_t;

typedef struct drawframe_s {
	size_t      vert_offset;
	size_t      line_offset;
	drawvert_t *quad_verts;
	drawvert_t *line_verts;
	uint32_t    num_quads;
	uint32_t    num_lines;
	VkCommandBuffer cmd;
	VkDescriptorSet descriptors;
} drawframe_t;

typedef struct drawframeset_s
	DARRAY_TYPE (drawframe_t) drawframeset_t;

typedef struct drawctx_s {
	VkSampler   sampler;
	scrap_t    *scrap;
	rfont_t    *font;
	qfv_tex_t  *font_tex;
	qfv_stagebuf_t *stage;
	qpic_t     *crosshair;
	qpic_t     *conchars;
	qpic_t     *conback;
	qpic_t     *white_pic;
	// use two separate cmem blocks for pics and strings (cachepic names)
	// to ensure the names are never in the same cacheline as a pic since the
	// names are used only for lookup
	memsuper_t *pic_memsuper;
	memsuper_t *string_memsuper;
	hashtab_t  *pic_cache;
	VkBuffer    vert_buffer;
	VkDeviceMemory vert_memory;
	VkBuffer    ind_buffer;
	VkDeviceMemory ind_memory;
	VkPipeline  quad_pipeline;
	VkPipeline  line_pipeline;
	VkPipelineLayout layout;
	drawframeset_t frames;
} drawctx_t;

// enough for a full screen of 8x8 chars at 1920x1080 plus some extras (368)
#define MAX_QUADS (32768)
#define VERTS_PER_QUAD (4)
#define INDS_PER_QUAD (5)	// one per vert plus primitive reset

#define MAX_LINES (32768)
#define VERTS_PER_LINE (2)

#define VERTS_PER_FRAME (MAX_LINES*VERTS_PER_LINE + MAX_QUADS*VERTS_PER_QUAD)

static void
create_quad_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	drawctx_t  *dctx = ctx->draw_context;

	size_t      vert_size;
	size_t      ind_size;
	size_t      frames = ctx->frames.size;
	VkBuffer    vbuf, ibuf;
	VkDeviceMemory vmem, imem;

	vert_size = frames * VERTS_PER_FRAME * sizeof (drawvert_t);
	ind_size = MAX_QUADS * INDS_PER_QUAD * sizeof (uint32_t);

	vbuf = QFV_CreateBuffer (device, vert_size,
							 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	ibuf = QFV_CreateBuffer (device, ind_size,
							 VK_BUFFER_USAGE_INDEX_BUFFER_BIT
							 | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vmem = QFV_AllocBufferMemory (device, vbuf,
								  VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
								  vert_size, 0);
	imem = QFV_AllocBufferMemory (device, ibuf,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								  ind_size, 0);
	QFV_BindBufferMemory (device, vbuf, vmem, 0);
	QFV_BindBufferMemory (device, ibuf, imem, 0);

	dctx->vert_buffer = vbuf;
	dctx->vert_memory = vmem;
	dctx->ind_buffer = ibuf;
	dctx->ind_memory = imem;

	void       *data;
	dfunc->vkMapMemory (device->dev, vmem, 0, vert_size, 0, &data);
	drawvert_t *vert_data = data;

	for (size_t f = 0; f < frames; f++) {
		drawframe_t *frame = &dctx->frames.a[f];
		size_t       ind = f * VERTS_PER_FRAME;
		size_t       lind = ind + MAX_QUADS * VERTS_PER_QUAD;
		frame->vert_offset = ind * sizeof (drawvert_t);
		frame->line_offset = lind;
		frame->quad_verts = vert_data + ind;
		frame->line_verts = frame->quad_verts + MAX_QUADS * VERTS_PER_QUAD;
		frame->num_quads = 0;
		frame->num_lines = 0;
	}

	// The indices will never change so pre-generate and stash them
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	uint32_t   *ind = QFV_PacketExtend (packet, ind_size);
	for (int i = 0; i < MAX_QUADS; i++) {
		for (int j = 0; j < VERTS_PER_QUAD; j++) {
			*ind++ = i * VERTS_PER_QUAD + j;
		}
		// mark end of primitive
		*ind++ = -1;
	}

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	bb.barrier.buffer = ibuf;
	bb.barrier.size = ind_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	VkBufferCopy copy_region = { packet->offset, 0, ind_size };
	dfunc->vkCmdCopyBuffer (packet->cmd, ctx->staging->buffer, ibuf,
							1, &copy_region);
	bb = bufferBarriers[qfv_BB_TransferWrite_to_IndexRead];
	bb.barrier.buffer = ibuf;
	bb.barrier.size = ind_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	QFV_PacketSubmit (packet);
}

static void
destroy_quad_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	drawctx_t  *dctx = ctx->draw_context;

	dfunc->vkUnmapMemory (device->dev, dctx->vert_memory);
	dfunc->vkFreeMemory (device->dev, dctx->vert_memory, 0);
	dfunc->vkFreeMemory (device->dev, dctx->ind_memory, 0);
	dfunc->vkDestroyBuffer (device->dev, dctx->vert_buffer, 0);
	dfunc->vkDestroyBuffer (device->dev, dctx->ind_buffer, 0);
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

	destroy_quad_buffers (ctx);

	dfunc->vkDestroyPipeline (device->dev, dctx->quad_pipeline, 0);
	dfunc->vkDestroyPipeline (device->dev, dctx->line_pipeline, 0);
	Hash_DelTable (dctx->pic_cache);
	delete_memsuper (dctx->pic_memsuper);
	delete_memsuper (dctx->string_memsuper);
	QFV_DestroyScrap (dctx->scrap);
	QFV_DestroyStagingBuffer (dctx->stage);
	if (dctx->font_tex) {
		Vulkan_UnloadTex (ctx, dctx->font_tex);
	}
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

	dctx->pic_memsuper = new_memsuper ();
	dctx->string_memsuper = new_memsuper ();
	dctx->pic_cache = Hash_NewTable (127, cachepic_getkey, cachepic_free,
									 dctx, 0);

	create_quad_buffers (ctx);
	dctx->stage = QFV_CreateStagingBuffer (device, "draw", 4 * 1024 * 1024,
										   ctx->cmdpool);
	dctx->scrap = QFV_CreateScrap (device, "draw_atlas", 2048, tex_rgba,
								   dctx->stage);
	dctx->sampler = Vulkan_CreateSampler (ctx, "quakepic");

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

	flush_draw_scrap (ctx);

	dctx->quad_pipeline = Vulkan_CreateGraphicsPipeline (ctx, "twod");
	dctx->line_pipeline = Vulkan_CreateGraphicsPipeline (ctx, "lines");

	dctx->layout = Vulkan_CreatePipelineLayout (ctx, "twod_layout");

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = Vulkan_CreateDescriptorSetLayout (ctx, "twod_set");
	}
	__auto_type pool = Vulkan_CreateDescriptorPool (ctx, "twod_pool");

	VkDescriptorImageInfo imageInfo = {
		dctx->sampler,
		QFV_ScrapImageView (dctx->scrap),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (frames, alloca);
	QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdBuffers);

	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	for (size_t i = 0; i < frames; i++) {
		__auto_type dframe = &dctx->frames.a[i];
		dframe->descriptors = sets->a[i];

		VkWriteDescriptorSet write[] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			  dframe->descriptors, 0, 0, 1,
			  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			  &imageInfo, 0, 0 },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
		dframe->cmd = cmdBuffers->a[i];
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 dframe->cmd,
							 va (ctx->va_ctx, "cmd:draw:%zd", i));
	}
	free (sets);
	qfvPopDebug (ctx);
}

static inline void
draw_pic (float x, float y, int w, int h, qpic_t *pic,
		  int srcx, int srcy, int srcw, int srch,
		  float *color, drawframe_t *frame)
{
	if (frame->num_quads >= MAX_QUADS) {
		return;
	}

	drawvert_t *verts = frame->quad_verts + frame->num_quads * VERTS_PER_QUAD;
	frame->num_quads++;

	subpic_t   *subpic = *(subpic_t **) pic->data;
	srcx += subpic->rect->x;
	srcy += subpic->rect->y;

	float size = subpic->size;
	float sl = (srcx + 0.03125) * size;
	float sr = (srcx + srcw - 0.03125) * size;
	float st = (srcy + 0.03125) * size;
	float sb = (srcy + srch - 0.03125) * size;

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

	draw_pic (x, y, 8, 8, dctx->conchars, cx * 8, cy * 8, 8, 8, color, frame);
}

void
Vulkan_Draw_Character (int x, int y, unsigned int chr, vulkan_ctx_t *ctx)
{
	if (chr == ' ') {
		return;
	}
	if (y <= -8 || y >= vid.conview->ylen) {
		return;
	}
	if (x <= -8 || x >= vid.conview->xlen) {
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
	if (y <= -8 || y >= vid.conview->ylen) {
		return;
	}
	while (*str) {
		if ((chr = *str++) != ' ' && x >= -8 && x < vid.conview->xlen) {
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
	if (y <= -8 || y >= vid.conview->ylen) {
		return;
	}
	while (count-- > 0 && *str) {
		if ((chr = *str++) != ' ' && x >= -8 && x < vid.conview->xlen) {
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
	if (y <= -8 || y >= vid.conview->ylen) {
		return;
	}
	while (*str) {
		if ((chr = *str++ | 0x80) != (' ' | 0x80)
			&& x >= -8 && x < vid.conview->xlen) {
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

	draw_pic (x - CROSSHAIR_WIDTH + 1, y - CROSSHAIR_HEIGHT + 1,
			  CROSSHAIR_WIDTH * 2, CROSSHAIR_HEIGHT * 2, dctx->crosshair,
			  p[0], p[1], p[2], p[3], crosshair_color, frame);
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

	x = vid.conview->xlen / 2 + cl_crossx;
	y = vid.conview->ylen / 2 + cl_crossy;

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
#define draw(px, py, pp) \
	draw_pic (px, py, pp->width, pp->height, pp, \
			  0, 0, pp->width, pp->height, color, frame);

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
	draw_pic (x, y, pic->width, pic->height, pic,
			  0, 0, pic->width, pic->height, color, frame);
}

void
Vulkan_Draw_Picf (float x, float y, qpic_t *pic, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, pic->width, pic->height, pic,
			  0, 0, pic->width, pic->height, color, frame);
}

void
Vulkan_Draw_SubPic (int x, int y, qpic_t *pic,
					int srcx, int srcy, int width, int height,
					vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	static quat_t color = { 1, 1, 1, 1};
	draw_pic (x, y, width, height, pic, srcx, srcy, width, height,
			  color, frame);
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
	int         ofs = max (0, cpic->height - lines);
	lines = min (lines, cpic->height);
	draw_pic (0, 0, vid.conview->xlen, lines, cpic,
			  0, ofs, cpic->width, lines, color, frame);
}

void
Vulkan_Draw_TileClear (int x, int y, int w, int h, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_Fill (int x, int y, int w, int h, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	quat_t      color;

	VectorScale (vid.palette + c * 3, 1.0f/255.0f, color);
	color[3] = 1;
	draw_pic (x, y, w, h, dctx->white_pic, 0, 0, 1, 1, color, frame);
}

void
Vulkan_Draw_Line (int x0, int y0, int x1, int y1, int c, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	if (frame->num_quads >= MAX_QUADS) {
		return;
	}

	quat_t      color = { VectorExpand (vid.palette + c * 3), 255 };
	QuatScale (color, 1/255.0, color);
	drawvert_t *verts = frame->line_verts + frame->num_lines * VERTS_PER_LINE;

	verts[0] = (drawvert_t) {
		.xy = { x0, y0 },
		.color = { QuatExpand (color) },
	};
	verts[1] = (drawvert_t) {
		.xy = { x1, y1 },
		.color = { QuatExpand (color) },
	};

	frame->num_lines++;
}

static inline void
draw_blendscreen (quat_t color, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *frame = &dctx->frames.a[ctx->curFrame];

	draw_pic (0, 0, vid.conview->xlen, vid.conview->ylen, dctx->white_pic,
			  0, 0, 1, 1, color, frame);
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

void
Vulkan_FlushText (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	flush_draw_scrap (ctx);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	drawctx_t  *dctx = ctx->draw_context;
	drawframe_t *dframe = &dctx->frames.a[ctx->curFrame];

	if (!dframe->num_quads && !dframe->num_lines) {
		return;
	}

	VkCommandBuffer cmd = dframe->cmd;
	//FIXME which pass?
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passTranslucent], cmd);

	VkMappedMemoryRange ranges[] = {
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  dctx->vert_memory, dframe->vert_offset,
		  dframe->num_quads * VERTS_PER_QUAD * sizeof (drawvert_t) },
		{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		  dctx->vert_memory, dframe->line_offset,
		  dframe->num_lines * VERTS_PER_LINE * sizeof (drawvert_t) },
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 2, ranges);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, QFV_passTranslucent,
		cframe->framebuffer,
		0, 0, 0
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, "twod", { 0.6, 0.2, 0, 1});

	VkDeviceSize offsets[] = {dframe->vert_offset};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &dctx->vert_buffer, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, dctx->ind_buffer, 0,
								 VK_INDEX_TYPE_UINT32);
	VkDescriptorSet set[2] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		dframe->descriptors,
	};
	VkPipelineLayout layout = dctx->layout;
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, set, 0, 0);
	if (dframe->num_quads) {
		dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
								  dctx->quad_pipeline);
		dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
		dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);
		dfunc->vkCmdDrawIndexed (cmd, dframe->num_quads * INDS_PER_QUAD,
								 1, 0, 0, 0);
	}

	if (dframe->num_lines) {
		dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
								  dctx->line_pipeline);
		dfunc->vkCmdDraw (cmd, dframe->num_lines * VERTS_PER_LINE,
						  1, MAX_QUADS * VERTS_PER_QUAD, 0);
	}

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);

	dframe->num_quads = 0;
	dframe->num_lines = 0;
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

void
Vulkan_Draw_AddFont (rfont_t *font, vulkan_ctx_t *ctx)
{
	drawctx_t  *dctx = ctx->draw_context;

	if (dctx->font_tex) {
		Vulkan_UnloadTex (ctx, dctx->font_tex);
		dctx->font_tex = 0;
	}
	dctx->font = font;
	if (dctx->font) {
		tex_t       tex = {
			.width = font->scrap.width,
			.height = font->scrap.height,
			.format = tex_l,
			.loaded = 1,
			.data = font->scrap_bitmap,
		};
		dctx->font_tex = Vulkan_LoadTex (ctx, &tex, 0, "draw.font");
	}
}

void
Vulkan_Draw_FontString (int x, int y, const char *str, vulkan_ctx_t *ctx)
{
}
