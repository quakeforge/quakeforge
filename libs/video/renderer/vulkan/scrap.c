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

#include "QF/dstring.h"
#include "QF/render.h"
#include "QF/ui/vrect.h"

#include "QF/Vulkan/qf_vid.h"
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
	vrect_t    *batch;
	vrect_t   **batch_tail;
	vrect_t    *batch_free;
	size_t      batch_count;
	subpic_t   *subpics;
	qfv_device_t *device;
};

scrap_t *
QFV_CreateScrap (qfv_device_t *device, const char *name, int size,
				 QFFormat format, qfv_stagebuf_t *stage)
{
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

	R_ScrapInit (&scrap->rscrap, size, size);

	// R_ScrapInit rounds sizes up to next power of 2
	size = scrap->rscrap.width;
	VkExtent3D  extent = { size, size, 1 };
	scrap->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D, fmt,
									extent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
									VK_IMAGE_USAGE_TRANSFER_DST_BIT
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
									   VK_IMAGE_VIEW_TYPE_2D, fmt,
									   VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, scrap->view,
						 dsprintf (str, "iview:scrap:%s", name));
	dstring_delete (str);
	scrap->bpp = bpp;
	scrap->subpics = 0;
	scrap->device = device;
	scrap->packet = 0;
	scrap->batch = 0;
	scrap->batch_tail = &scrap->batch;
	scrap->batch_free = 0;
	scrap->batch_count = 0;

	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	// no data for the packet
	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = scrap->image;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
	VkClearColorValue color = {
		float32:{0xde/255.0, 0xad/255.0, 0xbe/255.0, 0xef/255.0},
	};
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	dfunc->vkCmdClearColorImage (packet->cmd, scrap->image,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 &color, 1, &range);
	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.image = scrap->image;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
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
	qfv_device_t *device = scrap->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_ScrapClear (scrap);
	R_ScrapDelete (&scrap->rscrap);
	dfunc->vkDestroyImageView (device->dev, scrap->view, 0);
	dfunc->vkDestroyImage (device->dev, scrap->image, 0);
	dfunc->vkFreeMemory (device->dev, scrap->memory, 0);

	while (scrap->batch_free) {
		vrect_t    *b = scrap->batch_free;
		scrap->batch_free = b->next;
		VRect_Delete (b);
	}
	free (scrap);
}

VkImageView
QFV_ScrapImageView (scrap_t *scrap)
{
	return scrap->view;
}

subpic_t *
QFV_ScrapSubpic (scrap_t *scrap, int width, int height)
{
	vrect_t    *rect;
	subpic_t   *subpic;

	rect = R_ScrapAlloc (&scrap->rscrap, width, height);
	if (!rect) {
		return 0;
	}

	subpic = malloc (sizeof (subpic_t));
	*((subpic_t **) &subpic->next) = scrap->subpics;
	scrap->subpics = subpic;
	*((scrap_t **) &subpic->scrap) = scrap;
	*((vrect_t **) &subpic->rect) = rect;
	*((int *) &subpic->width) = width;
	*((int *) &subpic->height) = height;
	*((float *) &subpic->size) = 1.0 / scrap->rscrap.width;
	return subpic;
}

void
QFV_SubpicDelete (subpic_t *subpic)
{
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
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
	scrap_t    *scrap = (scrap_t *) subpic->scrap;
	vrect_t    *rect = (vrect_t *) subpic->rect;
	vrect_t    *batch;
	byte       *dest;
	size_t      size;

	if (!scrap->packet) {
		scrap->packet = QFV_PacketAcquire (stage);
	}
	size = (subpic->width * subpic->height * scrap->bpp + 3) & ~3;
	if (!(dest = QFV_PacketExtend (scrap->packet, size))) {
		if (scrap->packet->length) {
			QFV_ScrapFlush (scrap);

			scrap->packet = QFV_PacketAcquire (stage);
			dest = QFV_PacketExtend (scrap->packet, size);
		}
		if (!dest) {
			printf ("scrap: could not get space for update\n");
			return 0;
		}
	}

	if (scrap->batch_free) {
		batch = scrap->batch_free;
		scrap->batch_free = batch->next;
		batch->x = rect->x;
		batch->y = rect->y;
		batch->width = subpic->width;
		batch->height = subpic->height;
	} else {
		batch = VRect_New (rect->x, rect->y, subpic->width, subpic->height);
	}
	*scrap->batch_tail = batch;
	scrap->batch_tail = &batch->next;
	batch->next = 0;
	scrap->batch_count++;
	return dest;
}

void
QFV_ScrapFlush (scrap_t *scrap)
{
	qfv_device_t *device = scrap->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (!scrap->batch_count) {
		return;
	}

	qfv_packet_t *packet = scrap->packet;
	qfv_stagebuf_t *stage = packet->stage;
	size_t      i;
	__auto_type copy = QFV_AllocBufferImageCopy (128, alloca);
	memset (copy->a, 0, 128 * sizeof (copy->a[0]));

	for (i = 0; i < scrap->batch_count && i < 128; i++) {
		copy->a[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy->a[i].imageSubresource.layerCount = 1;
		copy->a[i].imageExtent.depth = 1;
	}

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_ShaderReadOnly_to_TransferDst];
	ib.barrier.image = scrap->image;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	size_t      offset = packet->offset, size;
	vrect_t    *batch = scrap->batch;
	while (scrap->batch_count) {
		for (i = 0; i < scrap->batch_count && i < 128; i++) {
			size = batch->width * batch->height * scrap->bpp;

			copy->a[i].bufferOffset = offset;
			copy->a[i].imageOffset.x = batch->x;
			copy->a[i].imageOffset.y = batch->y;
			copy->a[i].imageExtent.width = batch->width;
			copy->a[i].imageExtent.height = batch->height;

			offset += (size + 3) & ~3;
			batch = batch->next;
		}
		dfunc->vkCmdCopyBufferToImage (packet->cmd, stage->buffer, scrap->image,
									   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									   i, copy->a);
		scrap->batch_count -= i;
	}

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.image = scrap->image;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	*scrap->batch_tail = scrap->batch_free;
	scrap->batch_free = scrap->batch;
	scrap->batch = 0;
	scrap->batch_tail = &scrap->batch;

	QFV_PacketSubmit (scrap->packet);
	scrap->packet = 0;
}
