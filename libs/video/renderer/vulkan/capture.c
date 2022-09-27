/*
	capture.c

	Vulkan frame capture support

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

#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/swapchain.h"

qfv_capture_t *
QFV_CreateCapture (qfv_device_t *device, int numframes,
				   qfv_swapchain_t *swapchain, VkCommandPool cmdPool)
{
	qfv_instfuncs_t *ifunc = device->physDev->instance->funcs;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFormatProperties format_props;
	ifunc->vkGetPhysicalDeviceFormatProperties (device->physDev->dev,
												swapchain->format,
												&format_props);
	if (!(swapchain->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
		Sys_Printf ("Swapchain does not support reading. FIXME\n");
		return 0;
	}

	qfv_capture_t *capture = malloc (sizeof (qfv_capture_t));
	capture->device = device;
	capture->extent = swapchain->extent;
	capture->image_set = QFV_AllocCaptureImageSet (numframes, malloc);

	__auto_type cmdset = QFV_AllocCommandBufferSet (numframes, alloca);
	QFV_AllocateCommandBuffers (device, cmdPool, 1, cmdset);

	//FIXME assumes the swapchain is 32bpp
	capture->imgsize = swapchain->extent.width * swapchain->extent.height * 4;

	for (int i = 0; i < numframes; i++) {
		__auto_type image = &capture->image_set->a[i];
		image->buffer = QFV_CreateBuffer (device, capture->imgsize,
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	}
	VkMemoryRequirements req;
	dfunc->vkGetBufferMemoryRequirements (device->dev,
										  capture->image_set->a[0].buffer,
										  &req);
	capture->imgsize = QFV_NextOffset (capture->imgsize, &req);
	capture->memsize = numframes * capture->imgsize;
	capture->memory = QFV_AllocBufferMemory (device,
											 capture->image_set->a[0].buffer,
											 VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
											 capture->memsize, 0);
	dfunc->vkMapMemory (device->dev, capture->memory, 0, capture->memsize, 0,
						(void **) &capture->data);

	for (int i = 0; i < numframes; i++) {
		__auto_type image = &capture->image_set->a[i];
		image->cmd = cmdset->a[i];
		image->data = capture->data + i * capture->imgsize;
		QFV_BindBufferMemory (device, image->buffer, capture->memory,
							  i * capture->imgsize);
	}
	return capture;
}

void
QFV_DestroyCapture (qfv_capture_t *capture)
{
	qfv_device_t *device = capture->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < capture->image_set->size; i++) {
		__auto_type image = &capture->image_set->a[i];
		dfunc->vkDestroyBuffer (device->dev, image->buffer, 0);
	}
	dfunc->vkUnmapMemory (device->dev, capture->memory);
	dfunc->vkFreeMemory (device->dev, capture->memory, 0);
	free (capture->image_set);
	free (capture);
}

static void
copy_image (qfv_capture_t *capture, qfv_devfuncs_t *dfunc,
			VkImage scImage, qfv_capture_image_t *image)
{
	VkBufferImageCopy copy = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageOffset = { },
		.imageExtent = { capture->extent.width, capture->extent.height, 1 },
	};
	dfunc->vkCmdCopyImageToBuffer (image->cmd, scImage,
								   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								   image->buffer, 1, &copy);
}

VkCommandBuffer
QFV_CaptureImage (qfv_capture_t *capture, VkImage scImage, int frame)
{
	qfv_device_t *device = capture->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type image = &capture->image_set->a[frame];

	dfunc->vkResetCommandBuffer (image->cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		0, 0, 0, 0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (image->cmd, &beginInfo);

	VkBufferMemoryBarrier start_buffer_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.buffer = image->buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	VkImageMemoryBarrier start_image_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = scImage,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
	};
	VkBufferMemoryBarrier end_buffer_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.buffer = image->buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	VkImageMemoryBarrier end_image_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = scImage,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
	};

	dfunc->vkCmdPipelineBarrier (image->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0,
								 1, start_buffer_barriers,
								 1, start_image_barriers);

	copy_image (capture, dfunc, scImage, image);

	dfunc->vkCmdPipelineBarrier (image->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0,
								 1, end_buffer_barriers,
								 1, end_image_barriers);

	dfunc->vkEndCommandBuffer (image->cmd);

	return image->cmd;
}

const byte *
QFV_CaptureData (qfv_capture_t *capture, int frame)
{
	qfv_device_t *device = capture->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type image = &capture->image_set->a[frame];
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = capture->memory,
		.offset = image->data - capture->data,
		.size = capture->imgsize,
	};
	dfunc->vkInvalidateMappedMemoryRanges (device->dev, 1, &range);
	return image->data;
}
