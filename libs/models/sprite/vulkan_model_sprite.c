/*
	vulkan_model_sprite.c

	Sprite model mesh processing for Vulkan

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/12/13

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

#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_sprite.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static void
vulkan_sprite_clear (model_t *m, void *data)
{
	vulkan_ctx_t *ctx = data;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	msprite_t  *msprite = m->cache.data;
	auto sprite = (qfv_sprite_t *) ((byte *) msprite + msprite->skin.data);

	Vulkan_Sprite_FreeDescriptors (ctx, sprite);

	dfunc->vkDestroyBuffer (device->dev, sprite->verts, 0);
	dfunc->vkDestroyImageView (device->dev, sprite->view, 0);
	dfunc->vkDestroyImage (device->dev, sprite->image, 0);
	dfunc->vkFreeMemory (device->dev, sprite->memory, 0);
}

void
Vulkan_Mod_SpriteLoadFrames (mod_sprite_ctx_t *sprite_ctx, vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, va (ctx->va_ctx, "sprite.load_frames: %s",
						   sprite_ctx->mod->name));

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	model_t    *mod = sprite_ctx->mod;
	dsprite_t  *dsprite = sprite_ctx->dsprite;
	mod->clear = vulkan_sprite_clear;
	mod->data = ctx;

	qfv_sprite_t *sprite = Hunk_AllocName (0, sizeof (*sprite), mod->name);
	int         mipLevels = QFV_MipLevels (dsprite->width, dsprite->height);
	VkExtent3D  extent = { dsprite->width, dsprite->height, 1 };
	sprite->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
									 VK_FORMAT_R8G8B8A8_UNORM, extent,
									 mipLevels, sprite_ctx->numframes,
									 VK_SAMPLE_COUNT_1_BIT,
									 VK_IMAGE_USAGE_SAMPLED_BIT
									 | VK_IMAGE_USAGE_TRANSFER_DST_BIT
									 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, sprite->image,
						 va (ctx->va_ctx, "image:%s", mod->name));

	int         numverts = 4 * sprite_ctx->numframes;
	sprite->verts = QFV_CreateBuffer (device, numverts * sizeof (spritevrt_t),
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT
									  | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, sprite->verts,
						 va (ctx->va_ctx, "buffer:sprite:vertex:%s",
							 mod->name));

	VkMemoryRequirements ireq;
	dfunc->vkGetImageMemoryRequirements (device->dev, sprite->image, &ireq);
	VkMemoryRequirements vreq;
	dfunc->vkGetBufferMemoryRequirements (device->dev, sprite->verts, &vreq);
	size_t      size = QFV_NextOffset (vreq.size, &ireq) + ireq.size;

	sprite->memory = QFV_AllocBufferMemory (device, sprite->verts,
										 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										 size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, sprite->memory,
						 va (ctx->va_ctx, "memory:sprite:%s",
							 sprite_ctx->mod->name));

	QFV_BindBufferMemory (device, sprite->verts, sprite->memory, 0);
	QFV_BindImageMemory (device, sprite->image, sprite->memory,
						 QFV_NextOffset (vreq.size, &ireq));
	sprite->view = QFV_CreateImageView (device, sprite->image,
										VK_IMAGE_VIEW_TYPE_2D_ARRAY,
										VK_FORMAT_R8G8B8A8_UNORM,
										VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, sprite->view,
						 va (ctx->va_ctx, "view:sprite:%s",
							 sprite_ctx->mod->name));

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device,
													 va (ctx->va_ctx,
														 "sprite:%s",
														 sprite_ctx->mod->name),
													 size, ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	spritevrt_t *verts = QFV_PacketExtend (packet,
										   numverts * sizeof (spritevrt_t));
	int         texsize = 4 * dsprite->width * dsprite->height;
	byte       *pixels = QFV_PacketExtend (packet,
										   sprite_ctx->numframes * texsize);

	for (int i = 0; i < sprite_ctx->numframes; i++) {
		auto dframe = sprite_ctx->dframes[i];
		mspriteframe_t f;
		Mod_LoadSpriteFrame (&f, dframe);
		verts[i * 4 + 0] = (spritevrt_t) { f.left, f.up, 0, 0 };
		verts[i * 4 + 1] = (spritevrt_t) { f.right, f.up, 1, 0 };
		verts[i * 4 + 2] = (spritevrt_t) { f.left, f.down, 0, 1 };
		verts[i * 4 + 3] = (spritevrt_t) { f.right, f.down, 1, 1 };
		Vulkan_ExpandPalette (pixels + i * texsize, (const byte *)(dframe + 1),
							  vid.palette32, 2, texsize / 4);
		sprite_ctx->frames[i]->data = i;
	}

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	bb.barrier.buffer = sprite->verts;
	bb.barrier.size = numverts * sizeof (spritevrt_t);
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	VkBufferCopy copy_region[] = {
		{ packet->offset, 0, numverts * sizeof (spritevrt_t) },
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							sprite->verts, 1, &copy_region[0]);
	bb = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb.barrier.buffer = sprite->verts;
	bb.barrier.size = numverts * sizeof (spritevrt_t);
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = sprite->image;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	VkBufferImageCopy copy = {
		packet->offset + numverts * sizeof (spritevrt_t), 0, 0,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, sprite_ctx->numframes},
		{0, 0, 0}, {dsprite->width, dsprite->height, 1},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   sprite->image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy);
	QFV_GenerateMipMaps (device, packet->cmd, sprite->image,
						 mipLevels, dsprite->width, dsprite->height,
						 sprite_ctx->numframes);

	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);

	Vulkan_Sprite_DescriptorSet (ctx, sprite);

	sprite_ctx->sprite->skin.data = (byte *)sprite - (byte *)sprite_ctx->sprite;

	qfvPopDebug (ctx);
}
