/*
	vulkan_texuture.c

	Quake specific Vulkan texuture manager

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmem.h"
#include "QF/mathlib.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "vid_vulkan.h"

enum qfv_tex_components {
	qfv_tex_texinfo,
	qfv_tex_view,
	qfv_tex_sampler,
	qfv_tex_tex,
	qfv_tex_texture,
	qfv_tex_skinid,

	qfv_tex_num_components
};

void
Vulkan_ExpandPalette (byte *dst, const byte *src, const byte *palette,
					  int alpha, int count)
{
	if (alpha > 1) {
		while (count-- > 0) {
			int         pix = *src++;
			const byte *col = palette + pix * 4;
			*dst++ = *col++;
			*dst++ = *col++;
			*dst++ = *col++;
			*dst++ = *col++;
		}
	} else if (alpha) {
		while (count-- > 0) {
			byte        pix = *src++;
			const byte *col = palette + pix * 3;
			*dst++ = *col++;
			*dst++ = *col++;
			*dst++ = *col++;
			*dst++ = 0xff;
		}
	} else {
		while (count-- > 0) {
			byte        pix = *src++;
			const byte *col = palette + pix * 3;
			*dst++ = *col++;
			*dst++ = *col++;
			*dst++ = *col++;
		}
	}
}

static int
tex_format (const tex_t *tex, VkFormat *format, int *bpp)
{
	switch (tex->format) {
		case tex_l:
		case tex_a:
			*format = VK_FORMAT_R8_UNORM;
			*bpp = 1;
			return 1;
		case tex_la:
			*format = VK_FORMAT_R8G8_UNORM;
			*bpp = 2;
			return 1;
		case tex_palette:
			if (!tex->palette) {
				return 0;
			}
			*format = VK_FORMAT_R8G8B8A8_UNORM;
			*bpp = 4;
			return 1;
		case tex_rgb:
			*format = VK_FORMAT_R8G8B8A8_UNORM;
			*bpp = 4;
			return 1;
		case tex_rgba:
			*format = VK_FORMAT_R8G8B8A8_UNORM;
			*bpp = 4;
			return 1;
		case tex_frgba:
			*format = VK_FORMAT_R32G32B32A32_SFLOAT;
			*bpp = 16;
			return 1;
	}
	return 0;
}

static byte *
stage_pal (byte *out, byte *in, int texels, int bpp, const byte *palette)
{
	Vulkan_ExpandPalette (out, in, palette, 1, texels);
	return out + texels * 4;
}

static byte *
stage_rgb (byte *out, byte *in, int texels, int bpp, const byte *palette)
{
	while (texels-- > 0) {
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 255;
	}
	return out;
}

static byte *
stage_bpp (byte *out, byte *in, int texels, int bpp, const byte *palette)
{
	memcpy (out, in, bpp * texels);
	return out + texels * bpp;
}

static byte *
stage_tex_data_rows (qfv_packet_t *packet, tex_t *tex, int y, int rows,
					 int bpp, int x, int row_texels)
{
	if (!row_texels) {
		row_texels = tex->width;
	}
	if (!rows) {
		rows = tex->height;
	}
	int in_bpp = bpp;
	auto stage = stage_bpp;
	if (tex->format == tex_palette) {
		stage = stage_pal;
		in_bpp = 1;
	} else if (tex->format == tex_rgb) {
		stage = stage_rgb;
		in_bpp = 3;
	}
	int         row_bytes = row_texels * in_bpp;
	size_t      texels = tex->width;
	byte       *tex_data = QFV_PacketExtend (packet, bpp * texels * rows);
	byte       *in = tex->data + y * row_bytes + x * in_bpp;

	if (row_bytes == tex->width * in_bpp) {
		stage (tex_data, in, texels * rows, bpp, tex->palette);
	} else {
		auto out = tex_data;
		for (int i = 0; i < rows; i++) {
			out = stage (out, in, texels, bpp, tex->palette);
			in += row_bytes;
		}
	}
	return tex_data;
}

typedef void (*load_tex_t) (tex_t *tex, void *data);

//NOTE: all textures must have the same dimensions or bad things will happen
static byte *
stage_multi_tex_data (qfv_packet_t *packet, tex_t **tex, int count, int bpp,
					  load_tex_t load_tex, void *load_tex_data)
{
	size_t      texels = tex[0]->width * tex[0]->height;
	byte       *tex_data = QFV_PacketExtend (packet, bpp * texels * count);
	byte       *out_data = tex_data;

	auto stage = stage_bpp;
	auto palette = tex[0]->palette;
	if (tex[0]->format == tex_palette) {
		stage = stage_pal;
	} else if (tex[0]->format == tex_rgb) {
		stage = stage_rgb;
	}

	for (int i = 0; i < count; i++, out_data += bpp * texels) {
		if (!tex[i]->loaded) {
			load_tex (tex[i], load_tex_data);
		}
		stage (out_data, tex[i]->data, texels, bpp, palette);
	}
	return tex_data;
}

typedef struct {
	tex_t     **tex;
	int         layer;
	int         count;
	int         bpp;
	int         out_x;
	int         out_y;
	int         rows;
	int         in_x;
	int         in_y;
	int         row_texels;
} tex_cmd_t;

#define MAX_TEX_BYTES (16*1024*1024)

//NOTE: all textures must have the same dimensions or bad things will happen
static int
calc_tex_stage_commands (tex_t **tex, int count, int bpp)
{
	if (count < 1 || bpp < 1) {
		Sys_Error ("invalid count or bpp: %d %d", count, bpp);
	}
	size_t      row_bytes = tex[0]->width * bpp;
	size_t      bytes = tex[0]->height * row_bytes;
	if (bytes > MAX_TEX_BYTES) {
		// a single layer is too big to handle in one submit
		// number of rows that can be done per submit
		int r = MAX_TEX_BYTES / row_bytes;
		return ((tex[0]->height + r - 1) / r) * count;
	}
	// number of layers that can be done per submit
	int n = MAX_TEX_BYTES / bytes;
	return (count + n - 1) / n;
}

static void
init_tex_stage_commands (tex_cmd_t *cmd, tex_t **tex, int count, int bpp)
{
	size_t      row_bytes = tex[0]->width * bpp;
	size_t      bytes = tex[0]->height * row_bytes;
	if (bytes > MAX_TEX_BYTES) {
		const int r = MAX_TEX_BYTES / row_bytes;
		for (int l = 0; l < count; l++) {
			int rows = tex[l]->height;
			for (int y = 0; y < rows; y += r) {
				*cmd++ = (tex_cmd_t) {
					.tex = &tex[l],
					.layer = l,
					.count = 1,
					.bpp = bpp,
					.out_y = y,
					.rows = min (r, rows - y),
					.in_y = y,
				};
			}
		}
		return;
	}
	int n = MAX_TEX_BYTES / bytes;
	for (int l = 0; l < count; l += n) {
		*cmd++ = (tex_cmd_t) {
			.tex = &tex[l],
			.layer = l,
			.count = min (n, count - l),
			.bpp = bpp,
			.rows = tex[l]->height,
		};
	}
}

static qfv_tex_t *
alloc_qfv_tex ()
{
	size_t size = sizeof (qfv_tex_t)
				+ sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t[2]);
	qfv_tex_t  *qtex = malloc (size);
	*qtex = (qfv_tex_t) {
		.resource = (qfv_resource_t *) &qtex[1],
	};
	return qtex;
}

static void
stage_tex_set (qfv_tex_t *qtex, tex_cmd_t *tex_cmd, int num_cmd,
			   int layers, int mip, vulkan_ctx_t *ctx,
			   load_tex_t load_tex, void *load_tex_data)
{
	auto sb = &imageBarriers[qfv_LT_Undefined_to_TransferDst];
	auto db = &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	if (mip > 1) {
		// transition done by mipmaps
		db = nullptr;
	}
	qfv_packet_t *packet = nullptr;
	for (int i = 0; i < num_cmd; i++) {
		auto cmd = &tex_cmd[i];
		if (packet) {
			QFV_PacketSubmit (packet);
		}
		packet = QFV_PacketAcquire (ctx->staging, "tex.loadarray");
		if (cmd->count > 1) {
			stage_multi_tex_data (packet, cmd->tex, cmd->count, cmd->bpp,
								  load_tex, load_tex_data);
		} else {
			if (!(*cmd->tex)->loaded) {
				load_tex (*cmd->tex, load_tex_data);
			}
			stage_tex_data_rows (packet, *cmd->tex, cmd->in_y, cmd->rows,
								 cmd->bpp, cmd->in_x, cmd->row_texels);
		}
		qfv_offset_t offset = {
			.x = cmd->out_x,
			.y = cmd->out_y,
			.z = 0,
			.layer = cmd->layer,
		};
		qfv_extent_t extent = {
			.width = cmd->tex[0]->width,
			.height = cmd->rows,
			.depth = 1,
			.layers = cmd->count,
		};
		QFV_PacketCopyImage (packet, qtex->image, offset, extent, 0, sb,
							 i < num_cmd - 1 ? nullptr : db);
		// want a transition for only the first copy
		sb = nullptr;
	}
	if (mip > 1) {
		auto tex = tex_cmd[0].tex[0];
		QFV_GenerateMipMaps (ctx->device, packet->cmd, qtex->image, 0, mip,
							 tex->width, tex->height, layers);
	}
	QFV_PacketSubmit (packet);
}

static void
load_tex_set (tex_t **tex_set, int layers, int mip, int bpp, qfv_tex_t *qtex,
			  vulkan_ctx_t *ctx, load_tex_t load_tex, void *load_tex_data)
{
	int num_cmd = calc_tex_stage_commands (tex_set, layers, bpp);
	tex_cmd_t tex_cmd[num_cmd];
	init_tex_stage_commands (tex_cmd, tex_set, layers, bpp);

	stage_tex_set (qtex, tex_cmd, num_cmd, layers, mip, ctx,
				   load_tex, load_tex_data);
}

static qfv_tex_t *
load_tex_array (vulkan_ctx_t *ctx, tex_t *tex, int layers, int mip,
				const char *name, VkFormat format, int bpp,
				load_tex_t load_tex, void *load_tex_data)
{
	qfv_device_t *device = ctx->device;

	if (mip) {
		mip = QFV_MipLevels (tex[0].width, tex[0].height);
	} else {
		mip = 1;
	}

	auto qtex = alloc_qfv_tex ();
	auto image = (qfv_resobj_t *) &qtex->resource[1];
	auto view = &image[1];
	*qtex->resource = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2,
		.objects = image,
	};

	QFV_ResourceInitTexImage (image, "tex", mip > 1, &tex[0]);
	image->image.format = format;
	image->image.num_layers = layers;
	QFV_ResourceInitImageView (view, 0, image);
	QFV_CreateResource (device, qtex->resource);
	qtex->image = image->image.image;
	qtex->view = view->image_view.view;

	tex_t *tex_set[layers];
	for (int i = 0; i < layers; i++) {
		tex_set[i] = &tex[i];
	}
	load_tex_set (tex_set, layers, mip, bpp, qtex, ctx,
				  load_tex, load_tex_data);
	return qtex;
}

qfv_tex_t *
Vulkan_LoadTexArray (vulkan_ctx_t *ctx, tex_t *tex, int layers, int mip,
					 const char *name)
{
	VkFormat    format;
	int         bpp;

	if (!tex_format (tex, &format, &bpp)) {
		return 0;
	}

	for (int i = 1; i < layers; i++) {
		if (tex[i].width != tex[0].width || tex[i].height != tex[0].height
			|| tex[i].format != tex[0].format) {
			return 0;
		}
	}

	return load_tex_array (ctx, tex, layers, mip, name, format, bpp,
						   nullptr, nullptr);
}

qfv_tex_t *
Vulkan_LoadTex (vulkan_ctx_t *ctx, tex_t *tex, int mip, const char *name)
{
	return Vulkan_LoadTexArray (ctx, tex, 1, mip, name);
}

static qfv_tex_t *
create_cubetex (vulkan_ctx_t *ctx, int size, VkFormat format,
				const char *name)
{
	qfv_device_t *device = ctx->device;

	auto qtex = alloc_qfv_tex ();
	auto image = (qfv_resobj_t *) &qtex->resource[1];
	auto view = &image[1];
	*qtex->resource = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2,
		.objects = image,
	};
	tex_t tex = {
		.width = size,
		.height = size,
	};
	QFV_ResourceInitTexImage (image, "envmap", true, &tex);
	image->image.format = format;
	image->image.num_layers = 6;
	image->image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	QFV_ResourceInitImageView (view, 0, image);
	QFV_CreateResource (device, qtex->resource);
	qtex->image = image->image.image;
	qtex->view = view->image_view.view;

	return qtex;
}

static qfv_tex_t *
load_env_map (vulkan_ctx_t *ctx, tex_t *tex, int mip, const char *name,
			  VkFormat format, int bpp,
			  load_tex_t load_tex, void *load_tex_data)
{
	static int env_coords_3x2[][2] = {
		{2, 0},	// right
		{0, 0}, // left
		{1, 1}, // top
		{0, 1}, // bottom
		{2, 1}, // front
		{1, 0}, // back
	};

	static int env_coords_4x3[][2] = {
		{2, 1},	// right
		{0, 1}, // left
		{1, 0}, // top
		{1, 2}, // bottom
		{1, 1}, // front
		{3, 1}, // back
	};

	static int env_coords_3x4[][2] = {
		{2, 1},	// right
		{0, 1}, // left
		{1, 0}, // top
		{1, 2}, // bottom
		{1, 1}, // front
		{1, 3}, // back upside down?
	};

	static int env_coords_6x1[][2] = {
		{0, 0},	// right
		{1, 0}, // left
		{2, 0}, // top
		{3, 0}, // bottom
		{4, 0}, // front
		{5, 0}, // back
	};

	static int env_coords_1x6[][2] = {
		{0, 0},	// right
		{0, 1}, // left
		{0, 2}, // top
		{0, 3}, // bottom
		{0, 4}, // front
		{0, 5}, // back
	};
	int (*env_coords)[2] = nullptr;

	if (tex->height * 2 == tex->width) {
		return load_tex_array (ctx, tex, 1, mip, name, format, bpp,
							   load_tex, load_tex_data);
	}
	int size;
	if (tex->height * 3 == tex->width * 2) {
		env_coords = env_coords_3x2;
		size = tex->height / 2;
	} else if (tex->height * 4 == tex->width * 3) {
		env_coords = env_coords_4x3;
		size = tex->width / 4;
	} else if (tex->height * 3 == tex->width * 4) {
		env_coords = env_coords_3x4;
		size = tex->height / 4;
	} else if (tex->height * 6 == tex->width * 1) {
		env_coords = env_coords_6x1;
		size = tex->height;
	} else if (tex->height * 1 == tex->width * 6) {
		env_coords = env_coords_1x6;
		size = tex->width;
	} else {
		return 0;
	}

	qfv_tex_t  *qtex = create_cubetex (ctx, size, format, name);

	tex_t dummy = {
		.width = size,
		.height = size,
		.format = tex->format,
		.palette = tex->palette,
		.data = tex->data,
	};
	tex_t *tex_set[6] = { [0 ... 5] = &dummy };
	int num_cmd = calc_tex_stage_commands (tex_set, 1, bpp);
	tex_cmd_t tex_cmd[num_cmd * 6];
	init_tex_stage_commands (tex_cmd, tex_set, 1, bpp);
	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < num_cmd; j++) {
			int ind = i * num_cmd + j;
			tex_cmd[ind] = tex_cmd[j];
		}
	}
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < num_cmd; j++) {
			int ind = i * num_cmd + j;
			int x = env_coords[i][0] * size;
			int y = env_coords[i][1] * size;
			tex_cmd[ind].in_x = tex_cmd[ind].out_x + x;
			tex_cmd[ind].in_y = tex_cmd[ind].out_y + y;
			tex_cmd[ind].row_texels = tex->width;
			tex_cmd[ind].layer = i;
		}
	}

	if (mip) {
		mip = QFV_MipLevels (size, size);
	} else {
		mip = 1;
	}
	stage_tex_set (qtex, tex_cmd, num_cmd * 6, 6, mip, ctx,
				   load_tex, load_tex_data);
	return qtex;
}

qfv_tex_t *
Vulkan_LoadEnvMap (vulkan_ctx_t *ctx, tex_t *tex, const char *name)
{
	int         bpp;
	VkFormat    format;

	if (!tex_format (tex, &format, &bpp)) {
		return 0;
	}
	return load_env_map (ctx, tex, 1, name, format, bpp, nullptr, nullptr);
}

static qfv_tex_t *
load_env_sides (vulkan_ctx_t *ctx, tex_t **tex, int mip, const char *name,
				VkFormat format, int bpp,
				load_tex_t load_tex, void *load_tex_data)
{
	if (tex[0]->height != tex[0]->width) {
		return 0;
	}
	for (int i = 1; i < 6; i++) {
		if (tex[i]->format != tex[0]->format
			|| tex[i]->width != tex[0]->width
			|| tex[i]->height != tex[0]->height) {
			return 0;
		}
	}

	int size = tex[0]->height;
	qfv_tex_t  *qtex = create_cubetex (ctx, size, format, name);

	if (mip) {
		mip = QFV_MipLevels (size, size);
	} else {
		mip = 1;
	}
	load_tex_set (tex, 6, mip, bpp, qtex, ctx, load_tex, load_tex_data);
	return qtex;
}

qfv_tex_t *
Vulkan_LoadEnvSides (vulkan_ctx_t *ctx, tex_t **tex, const char *name)
{
	int         bpp;
	VkFormat    format;

	if (!tex_format (tex[0], &format, &bpp)) {
		return 0;
	}
	return load_env_sides (ctx, tex, 1, name, format, bpp, nullptr, nullptr);
}

VkImageView
Vulkan_TexImageView (qfv_tex_t *tex)
{
	return tex->view;
}

void
Vulkan_UpdateTex (vulkan_ctx_t *ctx, qfv_tex_t *tex, tex_t *src,
				  int x, int y, int layer, int mip, bool vert)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	int         bpp;
	VkFormat    format;
	if (!tex_format (src, &format, &bpp)) {
		return;
	}
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging, "tex.update");

	auto ib = imageBarriers[qfv_LT_ShaderReadOnly_to_TransferDst];
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &ib,
	};
	ib.image = tex->image;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);

	auto data = stage_tex_data_rows (packet, src, 0, src->height, bpp, 0, 0);
	auto offset = QFV_PacketOffset (packet, data);
	VkBufferImageCopy copy = {
		.bufferOffset = packet->offset + offset,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = mip,
			.baseArrayLayer = layer,
			.layerCount = 1,
		},
		.imageOffset = { .x = x, .y = y, .z = 0 },
		.imageExtent = {
			.width = src->width,
			.height = src->height,
			.depth = 1,
		},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   tex->image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy);
	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	if (vert) {
		ib.dstStageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
	}
	ib.image = tex->image;
	ib.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);
	QFV_PacketSubmit (packet);
}

void
Vulkan_UnloadTex (vulkan_ctx_t *ctx, qfv_tex_t *tex)
{
	if (!tex) {
		return;
	}
	qfv_device_t *device = ctx->device;

	QFV_DestroyResource (device, tex->resource);
	free (tex);
}

static byte default_black_data[] = {0, 0, 0, 0};
static byte default_white_data[] = {255, 255, 255, 255};
static byte default_magenta_data[] = {255, 0, 255, 255};
static byte default_normal_data[] = {128, 128, 255, 255};
static byte skin_data_main[] = {240, 240, 240, 255};	// main color
static byte skin_data_glow[] = {  0,   0,   0,   0};	// fullbright
static byte skin_data_cmap[] = {  0,   0,   0,   0};	// color map

typedef struct {
	byte *data;
	const char *names[4];
} default_tex_t;

#define deftex(name) { \
	.data = name##_data, \
	.names = {#name, #name "_array", #name "_cube", #name "_cube_array"} \
}
static default_tex_t default_textures[] = {
	deftex(default_black),
	deftex(default_white),
	deftex(default_magenta),
	deftex(default_normal),
};
static tex_t *default_skin_tex[] = {
	&(tex_t) {
		.width = 1,
		.height = 1,
		.format = tex_rgba,
		.loaded = true,
		.palette =0,
		.data = skin_data_main,
	},
	&(tex_t) {
		.width = 1,
		.height = 1,
		.format = tex_rgba,
		.loaded = true,
		.palette =0,
		.data = skin_data_glow,
	},
	&(tex_t) {
		.width = 1,
		.height = 1,
		.format = tex_rgba,
		.loaded = true,
		.palette =0,
		.data = skin_data_cmap,
	},
};

static void
texture_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto tctx = ctx->texture_context;

	QFV_DestroyResource (ctx->device, tctx->tex_resource);

	auto reg = tctx->reg;
	auto pools = reg->comp_pools;

	uint32_t c_tex = tctx->comp_base + qfv_tex_tex;
	for (uint32_t i = 0; i < pools[c_tex].count; i++) {
		auto tex = ((qfv_tex_t **)pools[c_tex].data)[i];
		Vulkan_UnloadTex (ctx, tex);
	}

	uint32_t c_texinfo = tctx->comp_base + qfv_tex_texinfo;
	for (uint32_t i = 0; i < pools[c_texinfo].count; i++) {
		auto texinfo = ((qfv_textureinfo_t **)pools[c_texinfo].data)[i];
		auto sampler = texinfo->sampler.sampler;
		dfunc->vkDestroySampler (device->dev, sampler, nullptr);
		delete_memsuper (texinfo->memsuper);
	}

	free (tctx->tex_resource);
	free (ctx->texture_context);
}

static VkImageViewType qfv_tex_view_types[] = {
	VK_IMAGE_VIEW_TYPE_2D,
	VK_IMAGE_VIEW_TYPE_2D_ARRAY,
	VK_IMAGE_VIEW_TYPE_CUBE,
	VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
};
static_assert (countof (qfv_tex_view_types) == countof (default_textures[0].names));

static void
texture_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "texture init");
	auto tctx = ctx->texture_context;

	tctx->dsmanager = QFV_Render_DSManager (ctx, "texture_set");

	const int num_deftex = countof (default_textures);
	const int num_skin_tex = countof (default_skin_tex);
	const int num_defvar = countof (default_textures[0].names);
	// +1 for default_skin
	const int num_images = num_deftex + 1;
	const int num_views = num_defvar * num_deftex + 1;
	size_t size = sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t[num_images])
				+ sizeof (qfv_resobj_t[num_views]);
	tctx->tex_resource = malloc (size);
	auto images = (qfv_resobj_t *) &tctx->tex_resource[1];
	auto views = (qfv_resobj_t *) &images[num_images];
	*tctx->tex_resource = (qfv_resource_t) {
		.name = "texture",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = num_images + num_views,
		.objects = images,
	};
	// three tex_t for default_skin
	tex_t deftex[num_deftex];
	for (int i = 0; i < num_deftex; i++) {
		auto dt = &default_textures[i];
		deftex[i] = (tex_t) {
			.width = 1,
			.height = 1,
			.format = tex_rgba,
			.loaded = true,
			.data = dt->data,
		};
		QFV_ResourceInitTexImage (&images[i], dt->names[0], true, &deftex[i]);
		images[i].image.num_layers = 6;
		images[i].image.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}
	const int skin_img = num_deftex;
	const int skin_view = num_defvar * num_deftex;
	QFV_ResourceInitTexImage (&images[skin_img], "default_skin", true,
							  default_skin_tex[0]);
	images[skin_img].image.num_layers = num_skin_tex;
	for (int i = 0; i < num_deftex; i++) {
		auto dt = &default_textures[i];
		for (int j = 0; j < num_defvar; j++) {
			int vind = i + j * num_deftex;
			QFV_ResourceInitImageView (&views[vind], i, &images[i]);
			views[vind].name = dt->names[j];
			views[vind].image_view.type = qfv_tex_view_types[j];
			if (!j) {
				views[vind].image_view.subresourceRange.layerCount = 1;
			}
		}
	}
	QFV_ResourceInitImageView (&views[skin_view], 4, &images[skin_img]);
	QFV_CreateResource (ctx->device, tctx->tex_resource);

	for (int i = 0; i < num_defvar; i++) {
		ctx->default_black[i]   = views[i + 0].image_view.view;
		ctx->default_white[i]   = views[i + 1].image_view.view;
		ctx->default_magenta[i] = views[i + 2].image_view.view;
		ctx->default_normal[i]  = views[i + 3].image_view.view;
	}
	ctx->default_skin = views[skin_view].image_view.view;

	byte *def_bytes[num_deftex];
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging, "tex.startup");
	for (int i = 0; i < num_deftex; i++) {
		tex_t *tex[6];
		for (int j = 0; j < 6; j++) {
			tex[j] = &deftex[i];
		}
		def_bytes[i] = stage_multi_tex_data (packet, tex, 6, 4,
											 nullptr, nullptr);
	}
	auto skin_bytes = stage_multi_tex_data (packet, default_skin_tex, 3, 4,
											nullptr, nullptr);

	auto sb = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	auto db = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];

	qfv_offset_t offset = {};
	for (int i = 0; i < num_deftex; i++) {
		QFV_PacketCopyImage (packet, images[i].image.image,
							 offset, (qfv_extent_t) { 1, 1, 1, 6 },
							 QFV_PacketOffset (packet, def_bytes[i]), &sb, &db);
	}
	QFV_PacketCopyImage (packet, images[4].image.image,
						 offset, (qfv_extent_t) { 1, 1, 1, 3 },
						 QFV_PacketOffset (packet, skin_bytes), &sb, &db);

	QFV_PacketSubmit (packet);

	qfvPopDebug (ctx);
}

VkImageView
QFV_Tex_View (vulkan_ctx_t *ctx, uint32_t texid)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t comp = qfv_tex_view + tctx->comp_base;
	return *(VkImageView *) Ent_GetComponent (texid, comp, reg);
}

VkSampler
QFV_Tex_Sampler (vulkan_ctx_t *ctx, uint32_t texid)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t comp = qfv_tex_sampler + tctx->comp_base;
	return *(VkSampler *) Ent_GetComponent (texid, comp, reg);
}

static const component_t qfv_tex_components[] = {
	[qfv_tex_texinfo] = {
		.size = sizeof (qfv_textureinfo_t *),
		.name = "texinfo",
	},
	[qfv_tex_view] = {
		.size = sizeof (VkImageView),
		.name = "view",
	},
	[qfv_tex_sampler] = {
		.size = sizeof (VkSampler),
		.name = "sampler",
	},
	[qfv_tex_tex] = {
		.size = sizeof (qfv_tex_t *),
		.name = "tex",
	},
	[qfv_tex_texture] = {
		.size = sizeof (VkDescriptorSet *),
		.name = "texture",
	},
	[qfv_tex_skinid] = {
		.size = sizeof (uint32_t),
		.name = "skinid",
	},
};

typedef struct {
	tex_t *base;
	qfv_textureinfo_t *texinfo;
	vulkan_ctx_t *ctx;
} load_tex_data_t;

static void
load_tex (tex_t *tex, void *data)
{
	load_tex_data_t *lt_data = data;
	uint32_t index = tex - lt_data->base;
	auto ctx = lt_data->ctx;
	auto texinfo = lt_data->texinfo;
	auto t = LoadImage (texinfo->layer_files[index], 1, ctx->hunk);
	tex->data = t->data;
	tex->loaded = true;
}

uint32_t
QFV_LoadTexinfo (vulkan_ctx_t *ctx, qfv_textureinfo_t *texinfo,
				 const char *name)
{
	if (!texinfo->num_layers) {
		Sys_Error ("%s: no layers", name);
	}
	if (texinfo->num_layers > 8) {
		Sys_Error ("%s: too many layers", name);
	}
	if (texinfo->image.arrayLayers
		&& texinfo->image.arrayLayers != texinfo->num_layers) {
		Sys_Error ("%s: invalid arrayLayers", name);
	}
	tex_t layers[texinfo->num_layers] = {};
	for (uint32_t i = 0; i < texinfo->num_layers; i++) {
		auto tex = LoadImage (texinfo->layer_files[i], 0, ctx->hunk);
		layers[i] = *tex;
	}

	for (uint32_t i = 1; i < texinfo->num_layers; i++) {
		if (layers[i].width != layers[0].width
			|| layers[i].height != layers[0].height
			|| layers[i].format != layers[0].format) {
			Sys_Error ("%s: incompatible layers", name);
		}
	}

	VkFormat    format;
	int         bpp;

	if (!tex_format (layers, &format, &bpp)) {
		Sys_Error ("%s: unsupported image", name);
	}
	name = cmemstrdup (texinfo->memsuper, name);

	load_tex_data_t lt_data = {
		.base = layers,
		.texinfo = texinfo,
		.ctx = ctx,
	};
	qfv_tex_t *tex = nullptr;
	if (texinfo->type == qft_unknown) {
		tex = load_tex_array (ctx, layers, texinfo->num_layers,
							  texinfo->image.mipLevels + 1, name,
							  texinfo->image.format, bpp,
							  load_tex, &lt_data);
	} else if (texinfo->type == qft_envmap) {
		//FIXME multi-layer maps?
		if (texinfo->num_layers == 1) {
			tex = load_env_map (ctx, layers, texinfo->image.mipLevels + 1,
								name, texinfo->image.format, bpp,
								load_tex, &lt_data);
		} else if (texinfo->num_layers == 6) {
			tex_t *layer_ptrs[texinfo->num_layers];
			for (uint32_t i = 0; i < texinfo->num_layers; i++) {
				layer_ptrs[i] = &layers[i];
			}
			tex = load_env_sides (ctx, layer_ptrs, texinfo->image.mipLevels + 1,
								  name, texinfo->image.format, bpp,
								  load_tex, &lt_data);
		}
	}
	QFV_CreateSampler (ctx, &texinfo->sampler);
	auto texture = Vulkan_CreateCombinedImageSampler (ctx, tex->view,
													  texinfo->sampler.sampler);

	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t c_texinfo = tctx->comp_base + qfv_tex_texinfo;
	uint32_t c_view = tctx->comp_base + qfv_tex_view;
	uint32_t c_samp = tctx->comp_base + qfv_tex_sampler;
	uint32_t c_tex = tctx->comp_base + qfv_tex_tex;
	uint32_t c_texture = tctx->comp_base + qfv_tex_texture;

	uint32_t texid = ECS_NewEntity (reg);
	Ent_SetComponent (texid, ecs_name, reg, &name);
	Ent_SetComponent (texid, c_texinfo, reg, &texinfo);
	Ent_SetComponent (texid, c_view, reg, &tex->view);
	Ent_SetComponent (texid, c_samp, reg, &texinfo->sampler.sampler);
	Ent_SetComponent (texid, c_tex, reg, &tex);
	Ent_SetComponent (texid, c_texture, reg, &texture);

	return texid;
}

uint32_t
QFV_TexGetSkinId (vulkan_ctx_t *ctx, uint32_t id)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t c_skinid = tctx->comp_base + qfv_tex_skinid;
	if (!Ent_HasComponent (id, c_skinid, reg)) {
		return nullent;
	}
	return *(uint32_t *) Ent_GetComponent (id, c_skinid, reg);
}

void
QFV_TexSetSkinId (vulkan_ctx_t *ctx, uint32_t id, uint32_t skinid)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t c_skinid = tctx->comp_base + qfv_tex_skinid;
	Ent_SetComponent (id, c_skinid, reg, &skinid);
}

VkDescriptorSet
QFV_GetTexture (vulkan_ctx_t *ctx, uint32_t texid)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t c_texture = tctx->comp_base + qfv_tex_texture;
	return *(VkDescriptorSet *) Ent_GetComponent (texid, c_texture, reg);
}

bool
QFV_TexIsCubemap (vulkan_ctx_t *ctx, uint32_t texid)
{
	auto tctx = ctx->texture_context;
	auto reg = tctx->reg;
	uint32_t c_tex = tctx->comp_base + qfv_tex_tex;
	auto tex = *(qfv_tex_t **) Ent_GetComponent (texid, c_tex, reg);
	auto flags = tex->resource->objects[0].image.flags;
	return (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0;
}

static void
texture_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, texture_shutdown);
	QFV_Render_AddStartup (ctx, texture_startup);

	texturectx_t   *tctx = malloc (sizeof (texturectx_t));
	ctx->texture_context = tctx;

	auto reg = ECS_NewRegistry ("textures");
	*tctx = (texturectx_t) {
		.reg = reg,
		.comp_base = ECS_RegisterComponents (reg, qfv_tex_components,
											 qfv_tex_num_components),
	};
	ECS_CreateComponentPools (reg);
}

static exprfunc_t texture_init_func[] = {
	{ .func = texture_init },
	{}
};

static exprsym_t texture_task_syms[] = {
	{ "texture_init", &cexpr_function, texture_init_func },
	{}
};

void
Vulkan_Texture_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, texture_task_syms);
}

static VkDescriptorImageInfo base_image_info = {
	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static VkWriteDescriptorSet base_image_write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstBinding = 0,
	.descriptorCount = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
};

VkDescriptorSet
Vulkan_CreateCombinedImageSampler (vulkan_ctx_t *ctx, VkImageView view,
								   VkSampler sampler)
{
	qfvPushDebug (ctx, "Vulkan_CreateCombinedImageSampler");

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto tctx = ctx->texture_context;

	auto descriptor = QFV_DSManager_AllocSet (tctx->dsmanager);

	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0] = base_image_info;
	imageInfo[0].sampler = sampler;
	imageInfo[0].imageView = view;

	VkWriteDescriptorSet write[1];
	write[0] = base_image_write;
	write[0].dstSet = descriptor;
	write[0].pImageInfo = imageInfo;
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);

	qfvPopDebug (ctx);

	return descriptor;
}

VkDescriptorSet
Vulkan_CreateTextureDescriptor (vulkan_ctx_t *ctx, qfv_tex_t *tex,
								VkSampler sampler)
{
	return Vulkan_CreateCombinedImageSampler (ctx, tex->view, sampler);
}

void
Vulkan_FreeTexture (vulkan_ctx_t *ctx, VkDescriptorSet texture)
{
	auto tctx = ctx->texture_context;

	QFV_DSManager_FreeSet (tctx->dsmanager, texture);
}
