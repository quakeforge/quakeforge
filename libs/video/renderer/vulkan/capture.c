/*
	capture.c

	Vulkan frame capture support

	Copyright (C) 1996-1997 Id Software, Inc.
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
#include <stdlib.h>

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/swapchain.h"

#include "vid_vulkan.h"

qfv_capture_t *
QFV_CreateCapture (qfv_device_t *device, int numframes,
				   qfv_swapchain_t *swapchain, VkCommandPool cmdPool)
{
	qfv_instfuncs_t *ifunc = device->physDev->instance->funcs;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkFormat    format = VK_FORMAT_R8G8B8A8_UNORM;
	int         canBlit = 1;

	VkFormatProperties format_props;
	ifunc->vkGetPhysicalDeviceFormatProperties (device->physDev->dev,
												swapchain->format,
												&format_props);
	if (!(swapchain->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
		Sys_Printf ("Swapchain does not support reading. FIXME\n");
		return 0;
	}
	if (!(format_props.optimalTilingFeatures
		  & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
		Sys_MaskPrintf (SYS_vulkan,
						"Device does not support blitting from optimal tiled "
						"images.\n");
		canBlit = 0;
	}
	ifunc->vkGetPhysicalDeviceFormatProperties (device->physDev->dev, format,
												&format_props);
	if (!(format_props.linearTilingFeatures
		  & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
		Sys_MaskPrintf (SYS_vulkan,
						"Device does not support blitting from optimal tiled "
						"images.\n");
		canBlit = 0;
	}

	qfv_capture_t *capture = malloc (sizeof (qfv_capture_t));
	capture->device = device;
	capture->canBlit = canBlit;
	capture->extent = swapchain->extent;
	capture->image_set = QFV_AllocCaptureImageSet (numframes, malloc);

	__auto_type cmdset = QFV_AllocCommandBufferSet (numframes, alloca);
	QFV_AllocateCommandBuffers (device, cmdPool, 1, cmdset);

	VkImageCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { swapchain->extent.width, swapchain->extent.height, 1 },
		.arrayLayers = 1,
		.mipLevels = 1,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	};

	for (int i = 0; i < numframes; i++) {
		__auto_type image = &capture->image_set->a[i];
		dfunc->vkCreateImage (device->dev, &createInfo, 0, &image->image);
		image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		image->cmd = cmdset->a[i];
	}
	size_t      image_size = QFV_GetImageSize (device,
											   capture->image_set->a[0].image);
	capture->memsize = numframes * image_size;
	capture->memory = QFV_AllocImageMemory (device,
											capture->image_set->a[0].image,
											VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
											capture->memsize, 0);
	byte       *data;
	dfunc->vkMapMemory (device->dev, capture->memory, 0, capture->memsize, 0,
						(void **) &data);

	for (int i = 0; i < numframes; i++) {
		__auto_type image = &capture->image_set->a[i];
		image->data = data + i * image_size;
		dfunc->vkBindImageMemory (device->dev, image->image, capture->memory,
								  image->data - data);
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
		dfunc->vkDestroyImage (device->dev, image->image, 0);
	}
	dfunc->vkUnmapMemory (device->dev, capture->memory);
	dfunc->vkFreeMemory (device->dev, capture->memory, 0);
	free (capture->image_set);
	free (capture);
}

static void
blit_image (qfv_capture_t *capture, qfv_devfuncs_t *dfunc,
			VkImage scImage, qfv_capture_image_t *image)
{
	VkImageBlit blit = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ { }, { capture->extent.width, capture->extent.height, 1 } },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ { }, { capture->extent.width, capture->extent.height, 1 } },
	};
	dfunc->vkCmdBlitImage (image->cmd,
						   scImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   1, &blit, VK_FILTER_NEAREST);
}

static void
copy_image (qfv_capture_t *capture, qfv_devfuncs_t *dfunc,
			VkImage scImage, qfv_capture_image_t *image)
{
	VkImageCopy copy = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { },
		{ capture->extent.width, capture->extent.height, 1 },
	};
	dfunc->vkCmdCopyImage (image->cmd,
						   scImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &copy);
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

	VkImageMemoryBarrier start_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = image->layout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = image->image,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
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
	VkImageMemoryBarrier end_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			.image = image->image,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
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
								 0, 0, 0, 0, 0,
								 2, start_barriers);

	if (capture->canBlit) {
		blit_image (capture, dfunc, scImage, image);
	} else {
		copy_image (capture, dfunc, scImage, image);
	}

	dfunc->vkCmdPipelineBarrier (image->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0, 0, 0,
								 2, end_barriers);
	image->layout = VK_IMAGE_LAYOUT_GENERAL;

	dfunc->vkEndCommandBuffer (image->cmd);

	return image->cmd;
}

const byte *
QFV_CaptureData (qfv_capture_t *capture, int frame)
{
	__auto_type image = &capture->image_set->a[frame];
	return image->data;
}
