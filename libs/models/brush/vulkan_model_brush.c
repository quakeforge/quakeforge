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
	qfv_devfuncs_t *dfunc = device->funcs;
	mod_brush_t *brush = &mod->brush;

	QFV_DeviceWaitIdle (device);

	for (unsigned i = 0; i < brush->numtextures; i++) {
		texture_t  *tx = brush->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		dfunc->vkDestroyImage (device->dev, tex->tex->image, 0);
		dfunc->vkDestroyImageView (device->dev, tex->tex->view, 0);
		if (tex->descriptor) {
			Vulkan_FreeTexture (ctx, tex->descriptor);
			tex->descriptor = 0;
		}
	}
	dfunc->vkFreeMemory (device->dev, mctx->texture_memory, 0);
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
copy_mips (qfv_packet_t *packet, texture_t *tx, size_t offset, VkImage image,
		   int layer, int miplevels, qfv_devfuncs_t *dfunc)
{
	// base copy
	VkBufferImageCopy copy = {
		offset, tx->width, tx->height,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1},
		{0, 0, 0}, {tx->width, tx->height, 1},
	};
	int         is_sky = 0;
	int         sky_offset = 0;
	size_t      size = tx->width * tx->height * 4;
	int         copy_count = MIPLEVELS;

	if (strncmp (tx->name, "sky", 3) == 0) {
		if (tx->width == 2 * tx->height) {
			copy.imageExtent.width /= 2;
			// sky layers are interleaved on each row
			sky_offset = tx->width * 4 / 2;
		}
		is_sky = 1;
		copy_count *= 2;
	}

	__auto_type copies = QFV_AllocBufferImageCopy (copy_count, alloca);
	copies->size = 0;

	for (int i = 0; i < miplevels; i++) {
		__auto_type c = &copies->a[copies->size++];
		*c = copy;
		if (is_sky) {
			__auto_type c = &copies->a[copies->size++];
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
								   copies->size, copies->a);
}

static void
transfer_texture (texture_t *tx, VkImage image, qfv_packet_t *packet,
				  byte *palette, qfv_device_t *device)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	byte       *base = packet->stage->data;

	size_t      layer_size = mipsize (tx->width * tx->height * 4);
	byte       *dst = QFV_PacketExtend (packet, layer_size);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = image;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	if (strncmp (tx->name, "sky", 3) == 0) {
		transfer_mips (dst, tx + 1, tx, palette, (vprocess_t) memcpy);
		copy_mips (packet, tx, dst - base, image, 0, MIPLEVELS, dfunc);
	} else if (tx->name[0] == '{') {
		transfer_mip_level (dst, tx + 1, tx, 0, palette, (vprocess_t) memcpy);
		copy_mips (packet, tx, dst - base, image, 0, 1, dfunc);
		QFV_GenerateMipMaps (device, packet->cmd, image, MIPLEVELS,
							 tx->width, tx->height, 1);
	} else {
		transfer_mips (dst, tx + 1, tx, palette, Mod_ClearFullbright);
		copy_mips (packet, tx, dst - base, image, 0, MIPLEVELS, dfunc);
		byte       *glow = QFV_PacketExtend (packet, layer_size);
		transfer_mips (glow, tx + 1, tx, palette, Mod_CalcFullbright);
		copy_mips (packet, tx, glow - base, image, 1, MIPLEVELS, dfunc);
	}

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.image = image;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
}

static void
load_textures (model_t *mod, vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, va (ctx->va_ctx, "brush.load_textures: %s", mod->name));
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	modelctx_t *mctx = mod->data;
	mod_brush_t *brush = &mod->brush;
	VkImage     image = 0;
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

	size_t      memsize = 0;
	for (unsigned i = 0; i < brush->numtextures; i++) {
		texture_t  *tx = brush->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		memsize += QFV_GetImageSize (device, tex->tex->image);
		// just so we have one in the end
		image = tex->tex->image;
	}
	VkDeviceMemory mem;
	mem = QFV_AllocImageMemory (device, image,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								memsize, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 mem, va (ctx->va_ctx, "memory:%s:texture", mod->name));
	mctx->texture_memory = mem;

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device,
													 va (ctx->va_ctx,
														 "brush:%s", mod->name),
													 memsize, ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	size_t      offset = 0;
	for (unsigned i = 0; i < brush->numtextures; i++) {
		texture_t  *tx = brush->textures[i];

		if (!tx) {
			continue;
		}
		qfv_tex_t  *tex = ((vulktex_t *) tx->render)->tex;

		dfunc->vkBindImageMemory (device->dev, tex->image, mem, offset);
		offset += QFV_GetImageSize (device, tex->image);

		VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		tex->view = QFV_CreateImageView (device, tex->image, type,
										 VK_FORMAT_R8G8B8A8_UNORM,
										 VK_IMAGE_ASPECT_COLOR_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW,
							 tex->view,
							 va (ctx->va_ctx, "iview:%s:%s:tex",
								 mod->name, tx->name));

		byte       *palette = vid.palette32;
		if (tx->name[0] == '{') {
			printf ("%s\n", tx->name);
			palette = trans_palette;
		} else if (strncmp (tx->name, "sky", 3) == 0) {
			palette = sky_palette;
		}
		transfer_texture (tx, tex->image, packet, palette, device);
	}
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);
	qfvPopDebug (ctx);
}

void
Vulkan_Mod_ProcessTexture (model_t *mod, texture_t *tx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	if (!tx) {
		modelctx_t *mctx = Hunk_AllocName (0, sizeof (modelctx_t), mod->name);
		mctx->ctx = ctx;
		mod->clear = vulkan_brush_clear;
		mod->data = mctx;

		if (mod->brush.numtextures) {
			load_textures (mod, ctx);
		}
		return;
	}

	vulktex_t    *tex = tx->render;
	tex->tex = (qfv_tex_t *) (tex + 1);
	VkExtent3D extent = { tx->width, tx->height, 1 };

	// Skies are two overlapping layers (one partly transparent), other
	// textures are split into main color and glow color on separate layers
	int layers = 2;
	if (strncmp (tx->name, "sky", 3) == 0) {
		// the sky texture is normally 2 side-by-side squares, but
		// some maps have just a single square
		if (tx->width == 2 * tx->height) {
			extent.width /= 2;
		}
	} else if (tx->name[0] == '{') {
		// trasparent textures don't have glow color
		layers = 1;
	}

	tex->tex->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
									   VK_FORMAT_R8G8B8A8_UNORM,
									   extent, 4, layers,
									   VK_SAMPLE_COUNT_1_BIT,
									   VK_IMAGE_USAGE_TRANSFER_DST_BIT
									   | VK_IMAGE_USAGE_SAMPLED_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE,
						 tex->tex->image,
						 va (ctx->va_ctx, "image:%s:%s:tex", mod->name,
							 tx->name));
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
