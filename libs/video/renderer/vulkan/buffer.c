/*
	buffer.c

	Vulkan buffer functions

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
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/memory.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

VkBuffer
QFV_CreateBuffer (qfv_device_t *device, VkDeviceSize size,
				  VkBufferUsageFlags usage)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkBufferCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 0,
		0,
		size, usage, VK_SHARING_MODE_EXCLUSIVE,
		0, 0
	};
	VkBuffer buffer;
	dfunc->vkCreateBuffer (dev, &createInfo, 0, &buffer);
	return buffer;
}

VkDeviceMemory
QFV_AllocBufferMemory (qfv_device_t *device,
					   VkBuffer buffer, VkMemoryPropertyFlags properties,
					   VkDeviceSize size, VkDeviceSize offset)
{
	VkDevice    dev = device->dev;
	qfv_physdev_t *physdev = device->physDev;
	VkPhysicalDeviceMemoryProperties *memprops = &physdev->memory_properties;
    qfv_devfuncs_t *dfunc = device->funcs;

	VkMemoryRequirements requirements;
	dfunc->vkGetBufferMemoryRequirements (dev, buffer, &requirements);

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
QFV_BindBufferMemory (qfv_device_t *device,
					  VkBuffer buffer, VkDeviceMemory object,
					  VkDeviceSize offset)
{
	VkDevice    dev = device->dev;
    qfv_devfuncs_t *dfunc = device->funcs;
	VkResult res = dfunc->vkBindBufferMemory (dev, buffer, object, offset);
	return res == VK_SUCCESS;
}

qfv_bufferbarrierset_t *
QFV_CreateBufferTransitions (qfv_buffertransition_t *transitions,
							 int numTransitions)
{
	qfv_bufferbarrierset_t *barrierset;
	barrierset = DARRAY_ALLOCFIXED (*barrierset, numTransitions, malloc);

	for (int i = 0; i < numTransitions; i++) {
		barrierset->a[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrierset->a[i].pNext = 0;
		barrierset->a[i].srcAccessMask = transitions[i].srcAccess;
		barrierset->a[i].dstAccessMask = transitions[i].dstAccess;
		barrierset->a[i].srcQueueFamilyIndex = transitions[i].srcQueueFamily;
		barrierset->a[i].dstQueueFamilyIndex = transitions[i].dstQueueFamily;
		barrierset->a[i].buffer = transitions[i].buffer;
		barrierset->a[i].offset = transitions[i].offset;
		barrierset->a[i].size = transitions[i].size;
	}
	return barrierset;
}

VkBufferView
QFV_CreateBufferView (qfv_device_t *device, VkBuffer buffer, VkFormat format,
					  VkDeviceSize offset, VkDeviceSize size)
{
	VkDevice    dev = device->dev;
    qfv_devfuncs_t *dfunc = device->funcs;

	VkBufferViewCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, 0,
		0,
		buffer, format, offset, size,
	};

	VkBufferView view;
	dfunc->vkCreateBufferView (dev, &createInfo, 0, &view);
	return view;
}
