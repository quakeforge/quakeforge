/*
	image.c

	Vulkan image functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2020      Bill Currie <bill@taniwha.org>

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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/memory.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

VkImage
QFV_CreateImage (qfv_device_t *device, int cubemap,
				 VkImageType type,
				 VkFormat format,
				 VkExtent3D size,
				 uint32_t num_mipmaps,
				 uint32_t num_layers,
				 VkSampleCountFlagBits samples,
				 VkImageUsageFlags usage_scenarios,
				 int initialized)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkImageCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0,
		cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
		type, format, size, num_mipmaps,
		cubemap ? 6 * num_layers : num_layers,
		samples,
		initialized ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
		usage_scenarios,
		VK_SHARING_MODE_EXCLUSIVE,
		0, 0,
		initialized ? VK_IMAGE_LAYOUT_PREINITIALIZED
					: VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VkImage image;
	dfunc->vkCreateImage (dev, &createInfo, 0, &image);
	return image;
}

VkDeviceMemory
QFV_AllocImageMemory (qfv_device_t *device,
					  VkImage image, VkMemoryPropertyFlags properties,
					  VkDeviceSize size, VkDeviceSize offset)
{
	VkDevice    dev = device->dev;
	qfv_physdev_t *physdev = device->physDev;
	VkPhysicalDeviceMemoryProperties *memprops = &physdev->memory_properties;
    qfv_devfuncs_t *dfunc = device->funcs;

	VkMemoryRequirements requirements;
	dfunc->vkGetImageMemoryRequirements (dev, image, &requirements);

	size = max (size, offset + requirements.size);
	VkDeviceMemory object = 0;

	for (uint32_t type = 0; type < memprops->memoryTypeCount; type++) {
		if ((requirements.memoryTypeBits & (1 << type))
			&& ((memprops->memoryTypes[type].propertyFlags & properties)
				== properties)) {
			VkMemoryAllocateInfo allocate_info = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0,
				size, type
			};

			VkResult res = dfunc->vkAllocateMemory (dev, &allocate_info,
													0, &object);
			if (res == VK_SUCCESS) {
				break;
			}
		}
	}

	return object;
}

int
QFV_BindImageMemory (qfv_device_t *device,
					 VkImage image, VkDeviceMemory object, VkDeviceSize offset)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkResult res = dfunc->vkBindImageMemory (dev, image, object, offset);
	return res == VK_SUCCESS;
}

qfv_imagebarrierset_t *
QFV_CreateImageTransitionSet (qfv_imagetransition_t *transitions,
							   int numTransitions)
{
	qfv_imagebarrierset_t *barrierset;
    barrierset = DARRAY_ALLOCFIXED (*barrierset, numTransitions, malloc);

	for (int i = 0; i < numTransitions; i++) {
		barrierset->a[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierset->a[i].pNext = 0;
		barrierset->a[i].srcAccessMask = transitions[i].srcAccess;
		barrierset->a[i].dstAccessMask = transitions[i].dstAccess;
		barrierset->a[i].oldLayout = transitions[i].oldLayout;
		barrierset->a[i].newLayout = transitions[i].newLayout;
		barrierset->a[i].srcQueueFamilyIndex = transitions[i].srcQueueFamily;
		barrierset->a[i].dstQueueFamilyIndex = transitions[i].dstQueueFamily;
		barrierset->a[i].image = transitions[i].image;
		barrierset->a[i].subresourceRange.aspectMask = transitions[i].aspect;
		barrierset->a[i].subresourceRange.baseMipLevel = 0;
		barrierset->a[i].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrierset->a[i].subresourceRange.baseArrayLayer = 0;
		barrierset->a[i].subresourceRange.layerCount
			= VK_REMAINING_ARRAY_LAYERS;
	}
	return barrierset;
}

VkImageView
QFV_CreateImageView (qfv_device_t *device, VkImage image,
					 VkImageViewType type, VkFormat format,
					 VkImageAspectFlags aspect)
{
	VkDevice    dev = device->dev;
    qfv_devfuncs_t *dfunc = device->funcs;

	VkImageViewCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0,
		0,
		image, type, format,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{
			aspect,
			0, VK_REMAINING_MIP_LEVELS,
			0, VK_REMAINING_ARRAY_LAYERS,
		}
	};

	VkImageView view;
	dfunc->vkCreateImageView (dev, &createInfo, 0, &view);
	return view;
}
