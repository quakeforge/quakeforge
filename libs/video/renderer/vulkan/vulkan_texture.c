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
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "r_scrap.h"
#include "vid_vulkan.h"

static int
ilog2 (unsigned x)
{
	unsigned o = x;
	if (x > 0x7fffffff) {
		// avoid overflow
		return 31;
	}
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	int y = 0;
	y |= ((x & 0xffff0000) != 0) << 4;
	y |= ((x & 0xff00ff00) != 0) << 3;
	y |= ((x & 0xf0f0f0f0) != 0) << 2;
	y |= ((x & 0xcccccccc) != 0) << 1;
	y |= ((x & 0xaaaaaaaa) != 0) << 0;
	return y - ((o & (x - 1)) != 0);
}

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

static void
blit_mips (int mips, VkImage image, tex_t *tex,
		   qfv_devfuncs_t *dfunc, VkCommandBuffer cmd)
{
	qfv_pipelinestagepair_t pre_stages = {
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
	};
	qfv_pipelinestagepair_t post_stages = {
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	};
	qfv_pipelinestagepair_t final_stages = {
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	};
	VkImageMemoryBarrier pre_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	VkImageMemoryBarrier post_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	VkImageMemoryBarrier final_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	VkImageBlit blit = {
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{{0, 0, 0}, {tex->width, tex->height, 1}},
		{VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1},
		{{0, 0, 0}, {max (tex->width >> 1, 1), max (tex->height >> 1, 1), 1}},
	};

	while (--mips > 0) {
		dfunc->vkCmdPipelineBarrier (cmd, pre_stages.src, pre_stages.dst, 0,
									 0, 0, 0, 0,
									 1, &pre_barrier);
		dfunc->vkCmdBlitImage (cmd,
							   image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   1, &blit, VK_FILTER_LINEAR);

		dfunc->vkCmdPipelineBarrier (cmd, post_stages.src, post_stages.dst, 0,
									 0, 0, 0, 0,
									 1, &post_barrier);

		blit.srcSubresource.mipLevel++;
		blit.srcOffsets[1].x = blit.dstOffsets[1].x;
		blit.srcOffsets[1].y = blit.dstOffsets[1].y;
		blit.dstSubresource.mipLevel++;
		blit.dstOffsets[1].x = max (blit.dstOffsets[1].x >> 1, 1);
		blit.dstOffsets[1].y = max (blit.dstOffsets[1].y >> 1, 1);
		pre_barrier.subresourceRange.baseMipLevel++;
		post_barrier.subresourceRange.baseMipLevel++;
		final_barrier.subresourceRange.baseMipLevel++;
	}
	dfunc->vkCmdPipelineBarrier (cmd, final_stages.src, final_stages.dst, 0,
								 0, 0, 0, 0,
								 1, &final_barrier);
}

qfv_tex_t *
Vulkan_LoadTex (vulkan_ctx_t *ctx, tex_t *tex, int mip)
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
		mip = ilog2 (max (tex->width, tex->height)) + 1;
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
	qtex->memory = QFV_AllocImageMemory (device, qtex->image,
										 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										 0, 0);
	QFV_BindImageMemory (device, qtex->image, qtex->memory, 0);
	qtex->view = QFV_CreateImageView (device, qtex->image,
									  VK_IMAGE_VIEW_TYPE_2D,
									  VK_FORMAT_R8G8B8A8_UNORM,
									  VK_IMAGE_ASPECT_COLOR_BIT);

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
		blit_mips (mip, qtex->image, tex, dfunc, packet->cmd);
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
	ctx->default_black = Vulkan_LoadTex (ctx, &default_black_tex, 1);
	ctx->default_white = Vulkan_LoadTex (ctx, &default_white_tex, 1);
	ctx->default_magenta = Vulkan_LoadTex (ctx, &default_magenta_tex, 1);
}

void
Vulkan_Texture_Shutdown (vulkan_ctx_t *ctx)
{
	Vulkan_UnloadTex (ctx, ctx->default_black);
	Vulkan_UnloadTex (ctx, ctx->default_white);
	Vulkan_UnloadTex (ctx, ctx->default_magenta);
}
