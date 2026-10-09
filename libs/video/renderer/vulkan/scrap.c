/*
	scrap.c

	Vulkan scrap manager

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

#include <string.h>

#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/render.h"

#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "r_scrap.h"

struct scrap_s {
	rscrap_t    rscrap;
	VkImage     image;
	VkDeviceMemory memory;
	VkImageView view;
	size_t      bpp;
	qfv_packet_t *packet;
	struct DARRAY_TYPE (scrapbox_t) batch;
	subpic_t   *subpics;
	qfv_device_t *device;
};

scrap_t *
QFV_CreateScrap (qfv_device_t *device, const char *name, int size, int layers,
				 QFFormat format, qfv_stagebuf_t *stage)
{
	qfZoneScoped (true);
	qfv_devfuncs_t *dfunc = device->funcs;
	int         bpp = 0;
	VkFormat    fmt = VK_FORMAT_UNDEFINED;
	dstring_t  *str = dstring_new ();

	switch (format) {
		case tex_l:
		case tex_a:
		case tex_palette:
			bpp = 1;
			fmt = VK_FORMAT_R8_UNORM;
			break;
		case tex_la:
			bpp = 2;
			fmt = VK_FORMAT_R8G8_UNORM;
			break;
		case tex_rgb:
			bpp = 3;
			fmt = VK_FORMAT_R8G8B8_UNORM;
			break;
		case tex_rgba:
			bpp = 4;
			fmt = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case tex_frgba:
			bpp = 16;
			fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
	}

	scrap_t    *scrap = malloc (sizeof (scrap_t));

	*scrap = (scrap_t) {
		.bpp = bpp,
		.device = device,
		.batch = DARRAY_STATIC_INIT (512),
	};

	R_ScrapInit (&scrap->rscrap, size, size);

	// R_ScrapInit rounds sizes up to next power of 2
	size = scrap->rscrap.width;
	VkExtent3D  extent = { size, size, 1 };
	scrap->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D, fmt,
									extent, 1, layers, VK_SAMPLE_COUNT_1_BIT,
									VK_IMAGE_USAGE_TRANSFER_DST_BIT
									| VK_IMAGE_USAGE_STORAGE_BIT
									| VK_IMAGE_USAGE_SAMPLED_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, scrap->image,
						 dsprintf (str, "image:scrap:%s", name));
	scrap->memory = QFV_AllocImageMemory (device, scrap->image,
										  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										  0, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, scrap->memory,
						 dsprintf (str, "memory:scrap:%s", name));
	QFV_BindImageMemory (device, scrap->image, scrap->memory, 0);
	scrap->view = QFV_CreateImageView (device, scrap->image,
									   VK_IMAGE_VIEW_TYPE_2D_ARRAY, fmt,
									   VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, scrap->view,
						 dsprintf (str, "iview:scrap:%s", name));
	dstring_delete (str);

	qfv_packet_t *packet = QFV_PacketAcquire (stage, "scrap.create");
	// no data for the packet
	auto ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.image = scrap->image;
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &ib,
	};
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);
	VkClearColorValue color = {
		.float32 = {0xde/255.0, 0xad/255.0, 0xbe/255.0, 0xef/255.0},
	};
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	dfunc->vkCmdClearColorImage (packet->cmd, scrap->image,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 &color, 1, &range);
	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.image = scrap->image;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);
	QFV_PacketSubmit (packet);
	return scrap;
}

size_t
QFV_ScrapSize (scrap_t *scrap)
{
	return scrap->rscrap.width * scrap->rscrap.height * scrap->bpp;
}

void
QFV_ScrapClear (scrap_t *scrap)
{
	subpic_t   *sp;
	while (scrap->subpics) {
		sp = scrap->subpics;
		scrap->subpics = (subpic_t *) sp->next;
		free (sp);
	}
	R_ScrapClear (&scrap->rscrap);
}

void
QFV_DestroyScrap (scrap_t *scrap)
{
	if (!scrap) {
		return;
	}
	qfv_device_t *device = scrap->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_ScrapClear (scrap);
	R_ScrapDelete (&scrap->rscrap);
	dfunc->vkDestroyImageView (device->dev, scrap->view, 0);
	dfunc->vkDestroyImage (device->dev, scrap->image, 0);
	dfunc->vkFreeMemory (device->dev, scrap->memory, 0);

	DARRAY_CLEAR (&scrap->batch);
	free (scrap);
}

VkImageView
QFV_ScrapImageView (scrap_t *scrap)
{
	return scrap->view;
}

VkImage
QFV_ScrapImage (scrap_t *scrap)
{
	return scrap->image;
}

subpic_t *
QFV_ScrapSubpic (scrap_t *scrap, int width, int height)
{
	auto rect = R_ScrapAlloc (&scrap->rscrap, width, height);
	if (!rect) {
		return nullptr;
	}

	subpic_t *subpic = malloc (sizeof (subpic_t));
	*subpic = (subpic_t) {
		.next = scrap->subpics,
		.scrap = scrap,
		.rect = rect,
		.width = width,
		.height = height,
		.size = 1.0 / scrap->rscrap.width,
	};
	scrap->subpics = subpic;
	return subpic;
}

void
QFV_SubpicDelete (subpic_t *subpic)
{
	scrap_t    *scrap = subpic->scrap;
	scrapbox_t *rect = subpic->rect;
	subpic_t  **sp;

	for (sp = &scrap->subpics; *sp; sp = (subpic_t **) &(*sp)->next) {
		if (*sp == subpic) {
			break;
		}
	}
	if (*sp != subpic) {
		Sys_Error ("QFV_SubpicDelete: broken subpic");
	}
	*sp = (subpic_t *) subpic->next;
	free (subpic);
	R_ScrapFree (&scrap->rscrap, rect);
}

void *
QFV_SubpicBatch (subpic_t *subpic, qfv_stagebuf_t *stage)
{
	qfZoneScoped (true);
	scrap_t    *scrap = subpic->scrap;
	scrapbox_t *rect = subpic->rect;
	byte       *dest;
	size_t      size;

	if (!scrap->packet) {
		scrap->packet = QFV_PacketAcquire (stage, "scrap.subpic");
	}
	size = (subpic->width * subpic->height * scrap->bpp + 3) & ~3;
	if (!(dest = QFV_PacketExtend (scrap->packet, size))) {
		if (scrap->packet->length) {
			QFV_ScrapFlush (scrap);

			scrap->packet = QFV_PacketAcquire (stage, "scrap.subpic");
			dest = QFV_PacketExtend (scrap->packet, size);
		}
		if (!dest) {
			printf ("scrap: could not get space for update\n");
			return 0;
		}
	}

	auto r = *rect;
	r.width = subpic->width;
	r.height = subpic->height;
	DARRAY_APPEND (&scrap->batch, r);
	return dest;
}

void
QFV_ScrapFlush (scrap_t *scrap)
{
	qfZoneScoped (true);
	qfv_device_t *device = scrap->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (!scrap->batch.size) {
		return;
	}

	qfv_packet_t *packet = scrap->packet;
	qfv_stagebuf_t *stage = packet->stage;

	auto ib = imageBarriers[qfv_LT_ShaderReadOnly_to_TransferDst];
	ib.image = scrap->image;
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &ib,
	};
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);

	ib = imageBarriers[qfv_LT_TransferDst_to_TransferDst];
	ib.image = scrap->image;

	size_t      offset = packet->offset;
	__auto_type copy = QFV_AllocBufferImageCopy (128, alloca);
	for (size_t b = 0; b < scrap->batch.size; ) {
		size_t i;
		for (i = 0; b + i < scrap->batch.size && i < 128; i++) {
			auto batch = &scrap->batch.a[b + i];
			size_t      size = batch->width * batch->height * scrap->bpp;
			copy->a[i] = (VkBufferImageCopy) {
				.bufferOffset = offset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.layerCount = 1,
				},
				.imageOffset = {
					.x = batch->x,
					.y = batch->y,
				},
				.imageExtent = {
					.width = batch->width,
					.height = batch->height,
					.depth = 1,
				},
			};

			offset += (size + 3) & ~3;
		}
		dfunc->vkCmdCopyBufferToImage (packet->cmd, stage->buffer, scrap->image,
									   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									   i, copy->a);
		b += i;
		if (b < scrap->batch.size) {
			dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);
		}
	}

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.image = scrap->image;
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &dep);

	scrap->batch.size = 0;

	QFV_PacketSubmit (scrap->packet);
	scrap->packet = 0;
}
