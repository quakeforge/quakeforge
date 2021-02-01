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
#include "QF/Vulkan/staging.h"

#include "qfalloca.h"
#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static vulktex_t vulkan_notexture = { };

static void vulkan_brush_clear (model_t *mod, void *data)
{
	modelctx_t *mctx = data;
	vulkan_ctx_t *ctx = mctx->ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (int i = 0; i < mod->numtextures; i++) {
		texture_t  *tx = mod->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		dfunc->vkDestroyImage (device->dev, tex->tex->image, 0);
		dfunc->vkDestroyImageView (device->dev, tex->tex->view, 0);
		if (tex->glow) {
			dfunc->vkDestroyImage (device->dev, tex->glow->image, 0);
			dfunc->vkDestroyImageView (device->dev, tex->glow->view, 0);
		}
	}
	dfunc->vkFreeMemory (device->dev, mctx->texture_memory, 0);
}

static size_t
get_image_size (VkImage image, qfv_device_t *device)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	size_t      size;
	size_t      align;

	VkMemoryRequirements requirements;
	dfunc->vkGetImageMemoryRequirements (device->dev, image, &requirements);
	size = requirements.size;
	align = requirements.alignment - 1;
	size = (size + align) & ~(align);
	return size;
}

static void
transfer_mips (byte *dst, const void *_src, const texture_t *tx, byte *palette)
{
	const byte *src = _src;
	unsigned    width = tx->width;
	unsigned    height = tx->height;
	unsigned    count, offset;

	for (int i = 0; i < MIPLEVELS; i++) {
		// mip offsets are relative to the texture pointer rather than the
		// end of the texture struct
		offset = tx->offsets[i] - sizeof (texture_t);
		count = width * height;
		Vulkan_ExpandPalette (dst, src + offset, palette, 2, count);
		dst += count * 4;
		width >>= 1;
		height >>= 1;
	}
}

static void
copy_mips (qfv_packet_t *packet, texture_t *tx, qfv_tex_t *tex,
			qfv_devfuncs_t *dfunc)
{
	// base copy
	VkBufferImageCopy copy = {
		tex->offset, tx->width, tx->height,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{0, 0, 0}, {tx->width, tx->height, 1},
	};
	int         is_sky = 0;
	int         sky_offset = 0;
	size_t      size = tx->width * tx->height * 4;
	int         copy_count = MIPLEVELS;

	if (strncmp (tx->name, "sky", 3) == 0) {
		if (tx->width == 2 * tx->height) {
			copy.imageExtent.width /= 2;
			sky_offset = tx->width * 4 / 2;
		}
		is_sky = 1;
		copy_count *= 2;
	}

	__auto_type copies = QFV_AllocBufferImageCopy (copy_count, alloca);
	copies->size = 0;

	for (int i = 0; i < MIPLEVELS; i++) {
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
								   tex->image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   copies->size, copies->a);
}

static void
load_textures (model_t *mod, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	modelctx_t *mctx = mod->data;
	VkImage     image = 0;
	byte       *buffer;

	size_t      image_count = 0;
	size_t      copy_count = 0;
	size_t      memsize = 0;
	for (int i = 0; i < mod->numtextures; i++) {
		texture_t  *tx = mod->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		tex->tex->offset = memsize;
		memsize += get_image_size (tex->tex->image, device);
		image_count++;
		copy_count += MIPLEVELS;
		if (strncmp (tx->name, "sky", 3) == 0) {
			copy_count += MIPLEVELS;
		}
		// just so we have one in the end
		image = tex->tex->image;
		if (tex->glow) {
			copy_count += MIPLEVELS;
			tex->glow->offset = memsize;
			memsize += get_image_size (tex->glow->image, device);
			image_count++;
		}
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
	buffer = QFV_PacketExtend (packet, memsize);

	for (int i = 0; i < mod->numtextures; i++) {
		texture_t  *tx = mod->textures[i];
		byte       *palette = vid.palette32;
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;

		dfunc->vkBindImageMemory (device->dev, tex->tex->image, mem,
								  tex->tex->offset);
		VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
		if (strncmp (tx->name, "sky", 3) == 0) {
			palette = alloca (256 * 4);
			memcpy (palette, vid.palette32, 256 * 4);
			// sky's black is transparent
			// this hits both layers, but so long as the screen is cleared
			// to black, no one should notice :)
			palette[3] = 0;
			type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		}
		tex->tex->view = QFV_CreateImageView (device, tex->tex->image,
											  type, VK_FORMAT_R8G8B8A8_UNORM,
											  VK_IMAGE_ASPECT_COLOR_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW,
							 tex->tex->view,
							 va (ctx->va_ctx, "iview:%s:%s:tex",
								 mod->name, tx->name));
		transfer_mips (buffer + tex->tex->offset, tx + 1, tx, palette);
		if (tex->glow) {
			dfunc->vkBindImageMemory (device->dev, tex->glow->image, mem,
									  tex->glow->offset);
			// skys are unlit so never have a glow texture thus glow
			// textures are always simple 2D
			tex->glow->view = QFV_CreateImageView (device, tex->tex->image,
												   VK_IMAGE_VIEW_TYPE_2D,
												   VK_FORMAT_R8G8B8A8_UNORM,
												   VK_IMAGE_ASPECT_COLOR_BIT);
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW,
								 tex->glow->view,
								 va (ctx->va_ctx, "iview:%s:%s:glow",
									 mod->name, tx->name));
			transfer_mips (buffer + tex->glow->offset, tex->glow->memory, tx,
						   palette);
		}
	}

	// base barrier
	VkImageMemoryBarrier barrier;
	barrier = imageLayoutTransitionBarriers[qfv_LT_Undefined_to_TransferDst];
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	__auto_type barriers = QFV_AllocImageBarrierSet (image_count, malloc);
	barriers->size = 0;
	for (int i = 0; i < mod->numtextures; i++) {
		texture_t  *tx = mod->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		__auto_type b = &barriers->a[barriers->size++];
		*b = barrier;
		b->image = tex->tex->image;
		if (tex->glow) {
			b = &barriers->a[barriers->size++];
			*b = barrier;
			b->image = tex->glow->image;
		}
	}
	qfv_pipelinestagepair_t stages;

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_TransferDst];
	dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
								 0, 0, 0, 0, 0,
								 barriers->size, barriers->a);
	for (int i = 0, j = 0; i < mod->numtextures; i++) {
		texture_t  *tx = mod->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		__auto_type b = &barriers->a[j++];
		b->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		copy_mips (packet, tx, tex->tex, dfunc);
		if (tex->glow) {
			b = &barriers->a[j++];
			b->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			b->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			copy_mips (packet, tx, tex->glow, dfunc);
		}
	}

	stages=imageLayoutTransitionStages[qfv_LT_TransferDst_to_ShaderReadOnly];
	dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
								 0, 0, 0, 0, 0,
								 barriers->size, barriers->a);
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);
}

void
Vulkan_Mod_ProcessTexture (model_t *mod, texture_t *tx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	if (!tx) {
		modelctx_t *mctx = Hunk_AllocName (sizeof (modelctx_t), mod->name);
		mctx->ctx = ctx;
		mod->clear = vulkan_brush_clear;
		mod->data = mctx;

		r_notexture_mip->render = &vulkan_notexture;
		load_textures (mod, ctx);
		return;
	}

	vulktex_t    *tex = tx->render;
	tex->texture = tx;
	tex->tex = (qfv_tex_t *) (tex + 1);
	VkExtent3D extent = { tx->width, tx->height, 1 };

	int layers = 1;
	if (strncmp (tx->name, "sky", 3) == 0) {
		layers = 2;
		// the sky texture is normally 2 side-by-side squares, but
		// some maps have just a single square
		if (tx->width == 2 * tx->height) {
			extent.width /= 2;
		}
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
	if (layers > 1) {
		// skys are unlit, so no fullbrights
		return;
	}

	const char *name = va (ctx->va_ctx, "fb_%s", tx->name);
	int         size = (tx->width * tx->height * 85) / 64;
	int         fullbright_mark = Hunk_LowMark ();
	byte       *pixels = Hunk_AllocName (size, name);

	if (!Mod_CalcFullbright ((byte *) (tx + 1), pixels, size)) {
		Hunk_FreeToLowMark (fullbright_mark);
		return;
	}
	tex->glow = tex->tex + 1;
	tex->glow->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
										VK_FORMAT_R8G8B8A8_UNORM,
										extent, 4, 1,
										VK_SAMPLE_COUNT_1_BIT,
										VK_IMAGE_USAGE_TRANSFER_DST_BIT
										| VK_IMAGE_USAGE_SAMPLED_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE,
						 tex->glow->image,
						 va (ctx->va_ctx, "image:%s:%s:glow", mod->name,
							 tx->name));
	// store the pointer to the fullbright data: memory will never be set to
	// actual device memory because all of the textures will be loaded in one
	// big buffer
	tex->glow->memory = (VkDeviceMemory) pixels;
}

void
Vulkan_Mod_LoadLighting (model_t *mod, bsp_t *bsp, vulkan_ctx_t *ctx)
{
	mod_lightmap_bytes = 3;
	if (!bsp->lightdatasize) {
		mod->lightdata = NULL;
		return;
	}

	byte        d;
	byte       *in, *out, *data;
	size_t      i;
	int         ver;
	QFile      *lit_file;

	mod->lightdata = 0;
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
					Sys_MaskPrintf (SYS_DEV, "%s loaded", litfilename->str);
					mod->lightdata = data + 8;
				} else {
					Sys_MaskPrintf (SYS_DEV,
									"Unknown .lit file version (%d)\n", ver);
				}
			} else {
				Sys_MaskPrintf (SYS_DEV, "Corrupt .lit file (old version?)\n");
			}
		}
		dstring_delete (litfilename);
	}
	if (mod->lightdata || !bsp->lightdatasize) {
		return;
	}
	// LordHavoc: oh well, expand the white lighting data
	mod->lightdata = Hunk_AllocName (bsp->lightdatasize * 3, mod->name);
	in = bsp->lightdata;
	out = mod->lightdata;

	for (i = 0; i < bsp->lightdatasize ; i++) {
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}
