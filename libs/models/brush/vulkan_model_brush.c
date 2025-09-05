/*
	vulkan_model_brush.c

	Vulkan support routines for model loading and caching

	Copyright (C) 1996-1997  Id Software, Inc.

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_model.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "qfalloca.h"
#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static void
vulkan_brush_clear (model_t *mod, void *data)
{
	modelctx_t *mctx = data;
	vulkan_ctx_t *ctx = mctx->ctx;
	qfv_device_t *device = ctx->device;

	QFV_DeviceWaitIdle (device);

	QFV_DestroyResource (device, mctx->resource);
}

typedef int (*vprocess_t) (byte *, const byte *, size_t);

static size_t
mipsize (size_t size)
{
	const int   n = MIPLEVELS;
	return size * ((1 << (2 * n)) - 1) / (3 * (1 << (2 * n - 2)));
}

static void
transfer_mip_level (byte *dst, const void *src, const texture_t *tx, int level,
					byte *palette, vprocess_t process)
{
	unsigned    width = tx->width >> level;
	unsigned    height = tx->height >> level;
	unsigned    count = width * height;

	// mip offsets are relative to the texture pointer rather than the
	// end of the texture struct
	unsigned    offset = tx->offsets[level] - sizeof (texture_t);
	// use the upper block of the destination as a temporary buffer for
	// the processed pixels. Vulkan_ExpandPalette works in a linearly
	// increasing manner thus the processed pixels will be overwritten
	// only after they have been read
	byte       *tmp = dst + count * 3;
	process (tmp, src + offset, count);
	Vulkan_ExpandPalette (dst, tmp, palette, 2, count);
}

static void
transfer_mips (byte *dst, const void *src, const texture_t *tx, byte *palette,
			   vprocess_t process)
{
	for (int i = 0; i < MIPLEVELS; i++) {
		transfer_mip_level (dst, src, tx, i, palette, process);

		unsigned    width = tx->width >> i;
		unsigned    height = tx->height >> i;
		unsigned    count = width * height;
		dst += count * 4;
	}
}

static void
copy_mips (qfv_packet_t *packet, texture_t *tx, byte *dst, VkImage image,
		   int layer, int miplevels, qfv_devfuncs_t *dfunc)
{
	VkBufferImageCopy copy = {
		.bufferOffset = QFV_PacketFullOffset (packet, dst),
		.bufferRowLength = tx->width,
		.bufferImageHeight = tx->height,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = layer,
			.layerCount = 1,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {tx->width, tx->height, 1},
	};
	bool        is_sky = false;
	int         sky_offset = 0;
	size_t      size = tx->width * tx->height * 4;
	int         copy_count = MIPLEVELS;

	if (strncmp (tx->name, "sky", 3) == 0) {
		if (tx->width == 2 * tx->height) {
			copy.imageExtent.width /= 2;
			// sky layers are interleaved on each row
			sky_offset = tx->width * 4 / 2;
		}
		is_sky = true;
		copy_count *= 2;
	}

	VkBufferImageCopy copies[copy_count];
	int num_copies = 0;

	for (int i = 0; i < miplevels; i++) {
		auto c = &copies[num_copies++];
		*c = copy;
		if (is_sky) {
			c = &copies[num_copies++];
			*c = copy;
			c->bufferOffset += sky_offset;
			c->imageSubresource.baseArrayLayer = 1;
		}
		copy.bufferOffset += size;
		size >>= 2;
		copy.bufferRowLength >>= 1;
		copy.bufferImageHeight >>= 1;
		copy.imageExtent.width >>= 1;
		copy.imageExtent.height >>= 1;
		copy.imageSubresource.mipLevel++;
		sky_offset >>= 1;
	}
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   num_copies, copies);
}

static void
transfer_texture (texture_t *tx, VkImage image, qfv_packet_t *packet,
				  byte *palette, qfv_device_t *device)
{
	auto dfunc = device->funcs;

	size_t layer_size = mipsize (tx->width * tx->height * 4);
	byte  *dst = QFV_PacketExtend (packet, layer_size);

	auto sb = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	sb.barrier.image = image;
	sb.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	sb.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, sb.srcStages, sb.dstStages,
								 0, 0, 0, 0, 0,
								 1, &sb.barrier);

	if (strncmp (tx->name, "sky", 3) == 0) {
		transfer_mips (dst, tx + 1, tx, palette, (vprocess_t) memcpy);
		copy_mips (packet, tx, dst, image, 0, MIPLEVELS, dfunc);
	} else if (tx->name[0] == '{') {
		transfer_mip_level (dst, tx + 1, tx, 0, palette, (vprocess_t) memcpy);
		copy_mips (packet, tx, dst, image, 0, 1, dfunc);
		QFV_GenerateMipMaps (device, packet->cmd, image, MIPLEVELS,
							 tx->width, tx->height, 1);
	} else {
		transfer_mips (dst, tx + 1, tx, palette, Mod_ClearFullbright);
		copy_mips (packet, tx, dst, image, 0, MIPLEVELS, dfunc);
		byte *glow = QFV_PacketExtend (packet, layer_size);
		transfer_mips (glow, tx + 1, tx, palette, Mod_CalcFullbright);
		copy_mips (packet, tx, glow, image, 1, MIPLEVELS, dfunc);
	}

	auto db = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	db.barrier.image = image;
	db.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	db.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, db.srcStages, db.dstStages,
								 0, 0, 0, 0, 0,
								 1, &db.barrier);
}

static void
load_textures (model_t *mod, vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, vac (ctx->va_ctx, "brush.load_textures: %s", mod->name));

	auto brush = &mod->brush;
	int num_tex = 0;
	int tex_map[brush->numtextures + 1] = {};
	for (unsigned i = 0; i < brush->numtextures; i++) {
		tex_map[i] = num_tex;
		num_tex += !!brush->textures[i];
	}
	size_t size = sizeof (modelctx_t)
				+ sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t[num_tex])	// images
				+ sizeof (qfv_resobj_t[num_tex]);	// views

	modelctx_t *mctx = Hunk_AllocName (nullptr, size, mod->name);
	*mctx = (modelctx_t) {
		.ctx = ctx,
		.resource = (qfv_resource_t *) &mctx[1],
	};
	mod->clear = vulkan_brush_clear;
	mod->data = mctx;

	auto images = (qfv_resobj_t *) &mctx->resource[1];
	auto views = (qfv_resobj_t *) &images[num_tex];
	*mctx->resource = (qfv_resource_t) {
		.name = mod->name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 * num_tex,
		.objects = images,
	};
	for (unsigned i = 0; i < brush->numtextures; i++) {
		auto tx = brush->textures[i];
		if (!tx) {
			continue;
		}

		tex_t tex = {
			.width = tx->width,
			.height = tx->height,
			.format = tex_rgba,	// the 8-bit textures will be palette-expanded
			.loaded = true,
		};
		// Skies are two overlapping layers (one partly transparent), other
		// textures are split into main color and glow color on separate layers
		int layers = 2;
		if (strncmp (tx->name, "sky", 3) == 0) {
			// the sky texture is normally 2 side-by-side squares, but
			// some maps have just a single square
			if (tx->width == 2 * tx->height) {
				tex.width /= 2;
			}
		} else if (tx->name[0] == '{') {
			// trasparent textures don't have glow color
			layers = 1;
		}
		int tex_ind = tex_map[i];
		QFV_ResourceInitTexImage (&images[tex_ind], tx->name, true, &tex);
		images[i].image.num_layers = layers;
		QFV_ResourceInitImageView (&views[tex_ind], tex_ind, &images[tex_ind]);
		// all bsp images need to be arrays, even if they have only one layer
		views[tex_ind].image_view.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}
	QFV_CreateResource (ctx->device, mctx->resource);
	for (unsigned i = 0; i < brush->numtextures; i++) {
		auto tx = brush->textures[i];
		if (!tx) {
			continue;
		}
		auto vtex = (vulktex_t *) tx->render;
		int tex_ind = tex_map[i];
		*vtex = (vulktex_t) {
			.view = views[tex_ind].image_view.view,
		};
	}

	byte        sky_palette[256 * 4];
	byte        trans_palette[256 * 4];

	memcpy (sky_palette, vid.palette32, sizeof (sky_palette));
	memcpy (trans_palette, vid.palette32, sizeof (trans_palette));
	// sky's black is transparent
	// this hits both layers, but so long as the screen is cleared
	// to black, no one should notice :)
	sky_palette[3] = 0;
	// transparent textures want transparent black
	memset (trans_palette + 255*4, 0, 4);

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging, "brush.tex");
	for (unsigned i = 0; i < brush->numtextures; i++) {
		auto tx = brush->textures[i];
		if (!tx) {
			continue;
		}

		byte       *palette = vid.palette32;
		if (tx->name[0] == '{') {
			printf ("%s\n", tx->name);
			palette = trans_palette;
		} else if (strncmp (tx->name, "sky", 3) == 0) {
			palette = sky_palette;
		}
		int tex_ind = tex_map[i];
		auto image = images[tex_ind].image.image;
		transfer_texture (tx, image, packet, palette, ctx->device);
	}
	QFV_PacketSubmit (packet);
	qfvPopDebug (ctx);
}

void
Vulkan_Mod_ProcessTexture (model_t *mod, texture_t *tx, vulkan_ctx_t *ctx)
{
	if (tx) {
		// want to process all the textures at once
		return;
	}
	load_textures (mod, ctx);
}

void
Vulkan_Mod_LoadLighting (model_t *mod, bsp_t *bsp, vulkan_ctx_t *ctx)
{
	mod_brush_t *brush = &mod->brush;

	mod_lightmap_bytes = 3;
	if (!bsp->lightdatasize) {
		brush->lightdata = NULL;
		return;
	}

	byte        d;
	byte       *in, *out, *data;
	size_t      i;
	int         ver;
	QFile      *lit_file;

	brush->lightdata = 0;
	if (mod_lightmap_bytes > 1) {
		// LordHavoc: check for a .lit file to load
		dstring_t  *litfilename = dstring_new ();
		dstring_copystr (litfilename, mod->name);
		QFS_StripExtension (litfilename->str, litfilename->str);
		dstring_appendstr (litfilename, ".lit");
		lit_file = QFS_VOpenFile (litfilename->str, 0, mod->vpath);
		data = (byte *) QFS_LoadHunkFile (lit_file);
		if (data) {
			if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I'
				&& data[3] == 'T') {
				ver = LittleLong (((int32_t *) data)[1]);
				if (ver == 1) {
					Sys_MaskPrintf (SYS_dev, "%s loaded", litfilename->str);
					brush->lightdata = data + 8;
				} else {
					Sys_MaskPrintf (SYS_dev,
									"Unknown .lit file version (%d)\n", ver);
				}
			} else {
				Sys_MaskPrintf (SYS_dev, "Corrupt .lit file (old version?)\n");
			}
		}
		dstring_delete (litfilename);
	}
	if (brush->lightdata || !bsp->lightdatasize) {
		return;
	}
	// LordHavoc: oh well, expand the white lighting data
	brush->lightdata = Hunk_AllocName (0, bsp->lightdatasize * 3, mod->name);
	in = bsp->lightdata;
	out = brush->lightdata;

	for (i = 0; i < bsp->lightdatasize ; i++) {
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}
