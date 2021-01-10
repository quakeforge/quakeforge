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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "compat.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/staging.h"

#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"

//FIXME move into a context struct
VkImage         conchars_image;
VkDeviceMemory  conchars_memory;
VkImageView     conchars_view;
VkSampler       conchars_sampler;

VkPipeline		twod;

static qpic_t *
pic_data (const char *name, int w, int h, const byte *data)
{
	qpic_t     *pic;

	pic = malloc (field_offset (qpic_t, data[w * h]));
	pic->width = w;
	pic->height = h;
	memcpy (pic->data, data, pic->width * pic->height);
	return pic;
}

qpic_t *
Vulkan_Draw_MakePic (int width, int height, const byte *data,
					 vulkan_ctx_t *ctx)
{
	return pic_data (0, width, height, data);
}

void
Vulkan_Draw_DestroyPic (qpic_t *pic, vulkan_ctx_t *ctx)
{
}

qpic_t *
Vulkan_Draw_PicFromWad (const char *name, vulkan_ctx_t *ctx)
{
	return pic_data (0, 1, 1, (const byte *)"");
}

qpic_t *
Vulkan_Draw_CachePic (const char *path, qboolean alpha, vulkan_ctx_t *ctx)
{
	return pic_data (0, 1, 1, (const byte *)"");
}

void
Vulkan_Draw_UncachePic (const char *path, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_TextBox (int x, int y, int width, int lines, byte alpha,
					 vulkan_ctx_t *ctx)
{
}

static void
Vulkan_Draw_Shutdown (void *data)
{
	vulkan_ctx_t *ctx = data;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyPipeline (device->dev, twod, 0);
	dfunc->vkDestroyImageView (device->dev, conchars_view, 0);
	dfunc->vkFreeMemory (device->dev, conchars_memory, 0);
	dfunc->vkDestroyImage (device->dev, conchars_image, 0);
}

void
Vulkan_Draw_Init (vulkan_ctx_t *ctx)
{
	Sys_RegisterShutdown (Vulkan_Draw_Shutdown, ctx);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandBuffer cmd = ctx->cmdbuffer;

	qpic_t     *charspic = Draw_Font8x8Pic ();
	VkExtent3D  extent = { charspic->width, charspic->height, 1 };
	conchars_image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
									  VK_FORMAT_A1R5G5B5_UNORM_PACK16,
									  extent, 1, 1,
									  VK_SAMPLE_COUNT_1_BIT,
									  VK_IMAGE_USAGE_TRANSFER_DST_BIT
									  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
									  | VK_IMAGE_USAGE_SAMPLED_BIT);
	conchars_memory = QFV_AllocImageMemory (device, conchars_image,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											0, 0);
	QFV_BindImageMemory (device, conchars_image, conchars_memory, 0);
	conchars_view = QFV_CreateImageView (device, conchars_image,
										 VK_IMAGE_VIEW_TYPE_2D,
										 VK_FORMAT_A1R5G5B5_UNORM_PACK16,
										 VK_IMAGE_ASPECT_COLOR_BIT);
	conchars_sampler = Hash_Find (ctx->samplers, "quakepic");

	uint16_t *chars_data = ctx->staging[0]->data;
	size_t size = charspic->width * charspic->height;
	for (size_t i = 0; i < size; i++) {
		// convert 0xff = transparent, 0xfe = white to 0x0000 and 0xffff
		// for a1r5g5b5
		uint16_t    pix = (charspic->data[i] & 1) - 1;
		chars_data[i] = ~pix;
	}
	QFV_FlushStagingBuffer (ctx->staging[0], 0, size);
	VkImageMemoryBarrier barrier;
	qfv_pipelinestagepair_t stages;
	dfunc->vkWaitForFences (device->dev, 1, &ctx->fence, VK_TRUE, ~0ull);
	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, 0,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_TransferDst];
	barrier=imageLayoutTransitionBarriers[qfv_LT_Undefined_to_TransferDst];
	barrier.image = conchars_image;
	dfunc->vkCmdPipelineBarrier (cmd, stages.src, stages.dst, 0,
								 0, 0,
								 0, 0,
								 1, &barrier);

	VkBufferImageCopy region = {
		0, 0, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ charspic->width, charspic->height, 1 },
	};
	dfunc->vkCmdCopyBufferToImage (cmd,
								   ctx->staging[0]->buffer, conchars_image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &region);

	stages = imageLayoutTransitionStages[qfv_LT_TransferDst_to_ShaderReadOnly];
	barrier=imageLayoutTransitionBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	barrier.image = conchars_image;
	dfunc->vkCmdPipelineBarrier (cmd, stages.src, stages.dst, 0,
								 0, 0,
								 0, 0,
								 1, &barrier);
	dfunc->vkEndCommandBuffer (cmd);

	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		0, 0, 0,
		1, &cmd,
		0, 0,
	};
	dfunc->vkResetFences (device->dev, 1, &ctx->fence);
	dfunc->vkQueueSubmit (device->queue.queue, 1, &submitInfo, ctx->fence);

	twod = Vulkan_CreatePipeline (ctx, "twod");

	handleref_t *h;
	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (ctx->framebuffers.size, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		h = Hash_Find (ctx->setLayouts, "twod");
		layouts->a[i] = (VkDescriptorSetLayout) h->handle;
	}
	h = Hash_Find (ctx->descriptorPools, "twod");
	__auto_type pool = (VkDescriptorPool) h->handle;
	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	for (size_t i = 0; i < ctx->framebuffers.size; i++) {
		ctx->framebuffers.a[i].twodDescriptors = sets->a[i];
	}
}

void
Vulkan_Draw_Character (int x, int y, unsigned int chr, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_String (int x, int y, const char *str, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_nString (int x, int y, const char *str, int count,
					 vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_AltString (int x, int y, const char *str, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_CrosshairAt (int ch, int x, int y, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_Crosshair (vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_Pic (int x, int y, qpic_t *pic, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_Picf (float x, float y, qpic_t *pic, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_SubPic (int x, int y, qpic_t *pic,
					int srcx, int srcy, int width, int height,
					vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_ConsoleBackground (int lines, byte alpha, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_TileClear (int x, int y, int w, int h, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_Fill (int x, int y, int w, int h, int c, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_FadeScreen (vulkan_ctx_t *ctx)
{
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
Vulkan_FlushText (vulkan_ctx_t *ctx)
{
}

void
Vulkan_Draw_BlendScreen (quat_t color, vulkan_ctx_t *ctx)
{
}
