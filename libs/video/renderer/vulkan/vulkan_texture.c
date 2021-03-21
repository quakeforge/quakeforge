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

#include "QF/alloc.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/mathlib.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "r_scrap.h"
#include "vid_vulkan.h"

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

qfv_tex_t *
Vulkan_LoadTex (vulkan_ctx_t *ctx, tex_t *tex, int mip, const char *name)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	int         bpp = 0;
	VkFormat    format = VK_FORMAT_UNDEFINED;

	switch (tex->format) {
		case tex_l:
		case tex_a:
			format = VK_FORMAT_R8_UNORM;
			bpp = 1;
			break;
		case tex_la:
			format = VK_FORMAT_R8G8_UNORM;
			bpp = 2;
			break;
		case tex_palette:
			if (!tex->palette) {
				return 0;
			}
			format = VK_FORMAT_R8G8B8A8_UNORM;
			bpp = 4;
			break;
		case tex_rgb:
			format = VK_FORMAT_R8G8B8_UNORM;
			bpp = 3;
			break;
		case tex_rgba:
			format = VK_FORMAT_R8G8B8A8_UNORM;
			bpp = 4;
			break;
		case tex_frgba:
			format = VK_FORMAT_R32G32B32A32_SFLOAT;
			bpp = 16;
			break;
	}
	if (format == VK_FORMAT_UNDEFINED) {
		return 0;
	}

	if (mip) {
		mip = QFV_MipLevels (tex->width, tex->height);
	} else {
		mip = 1;
	}

	//qfv_devfuncs_t *dfunc = device->funcs;
	//FIXME this whole thing is ineffiecient, especially for small textures
	qfv_tex_t  *qtex = malloc (sizeof (qfv_tex_t));

	VkExtent3D  extent = { tex->width, tex->height, 1 };
	qtex->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D, format, extent,
								   mip, 1, VK_SAMPLE_COUNT_1_BIT,
								   VK_IMAGE_USAGE_TRANSFER_DST_BIT
								   | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
								   | VK_IMAGE_USAGE_SAMPLED_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, qtex->image,
						 va (ctx->va_ctx, "image:%s", name));
	qtex->memory = QFV_AllocImageMemory (device, qtex->image,
										 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										 0, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, qtex->memory,
						 va (ctx->va_ctx, "memory:%s", name));
	QFV_BindImageMemory (device, qtex->image, qtex->memory, 0);
	qtex->view = QFV_CreateImageView (device, qtex->image,
									  VK_IMAGE_VIEW_TYPE_2D,
									  VK_FORMAT_R8G8B8A8_UNORM,
									  VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, qtex->view,
						 va (ctx->va_ctx, "iview:%s", name));

	size_t      bytes = bpp * tex->width * tex->height;
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	byte       *tex_data = QFV_PacketExtend (packet, bytes);

	if (tex->format == tex_palette) {
		Vulkan_ExpandPalette (tex_data, tex->data, tex->palette,
							  1, tex->width * tex->height);
	} else {
		memcpy (tex_data, tex->data, bytes);
	}

	VkImageMemoryBarrier barrier;
	qfv_pipelinestagepair_t stages;

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_TransferDst];
	barrier = imageLayoutTransitionBarriers[qfv_LT_Undefined_to_TransferDst];
	barrier.image = qtex->image;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
								 0, 0, 0, 0, 0,
								 1, &barrier);
	VkBufferImageCopy copy = {
		packet->offset, 0, 0,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{0, 0, 0}, {tex->width, tex->height, 1},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   qtex->image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy);
	if (mip == 1) {
		stages = imageLayoutTransitionStages[qfv_LT_TransferDst_to_ShaderReadOnly];
		barrier=imageLayoutTransitionBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
		barrier.image = qtex->image;
		dfunc->vkCmdPipelineBarrier (packet->cmd, stages.src, stages.dst,
									 0, 0, 0, 0, 0,
									 1, &barrier);
	} else {
		QFV_GenerateMipMaps (device, packet->cmd, qtex->image,
							 mip, tex->width, tex->height, 1);
	}
	QFV_PacketSubmit (packet);
	return qtex;
}

VkImageView
Vulkan_TexImageView (qfv_tex_t *tex)
{
	return tex->view;
}

void
Vulkan_UnloadTex (vulkan_ctx_t *ctx, qfv_tex_t *tex)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyImageView (device->dev, tex->view, 0);
	dfunc->vkDestroyImage (device->dev, tex->image, 0);
	dfunc->vkFreeMemory (device->dev, tex->memory, 0);
	free (tex);
}

static byte black_data[] = {0, 0, 0, 0};
static byte white_data[] = {255, 255, 255, 255};
static byte magenta_data[] = {255, 0, 255, 255};
static tex_t default_black_tex = {1, 1, tex_rgba, 1, 0, black_data};
static tex_t default_white_tex = {1, 1, tex_rgba, 1, 0, white_data};
static tex_t default_magenta_tex = {1, 1, tex_rgba, 1, 0, magenta_data};

void
Vulkan_Texture_Init (vulkan_ctx_t *ctx)
{
	ctx->default_black = Vulkan_LoadTex (ctx, &default_black_tex, 1,
										 "default_black");
	ctx->default_white = Vulkan_LoadTex (ctx, &default_white_tex, 1,
										 "default_white");
	ctx->default_magenta = Vulkan_LoadTex (ctx, &default_magenta_tex, 1,
										   "default_magenta");
}

void
Vulkan_Texture_Shutdown (vulkan_ctx_t *ctx)
{
	Vulkan_UnloadTex (ctx, ctx->default_black);
	Vulkan_UnloadTex (ctx, ctx->default_white);
	Vulkan_UnloadTex (ctx, ctx->default_magenta);
}
