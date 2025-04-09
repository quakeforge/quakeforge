/*
	memory.c

	Vulkan memory functions

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

#include "QF/Vulkan/device.h"
#include "QF/Vulkan/memory.h"

void *
QFV_MapMemory (qfv_device_t *device, VkDeviceMemory object,
			   VkDeviceSize offset, VkDeviceSize size)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	void       *map = 0;

	dfunc->vkMapMemory (dev, object, offset, size, 0, &map);
	return map;
}

void
QFV_FlushMemory (qfv_device_t *device, qfv_mappedmemrangeset_t *flushRanges)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkMappedMemoryRange *ranges = alloca(sizeof (*ranges) * flushRanges->size);

	for (uint32_t i = 0; i < flushRanges->size; i++) {
		ranges[i].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		ranges[i].pNext = 0;
		ranges[i].memory = flushRanges->a[i].object;
		ranges[i].offset = flushRanges->a[i].offset;
		ranges[i].size = flushRanges->a[i].size;
	}
	dfunc->vkFlushMappedMemoryRanges (dev, flushRanges->size, ranges);
}
