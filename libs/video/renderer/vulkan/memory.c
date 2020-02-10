/*
	memory.c

	Vulkan memory functions

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
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/memory.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

void *
QFV_MapMemory (qfv_memory_t *memory, VkDeviceSize offset, VkDeviceSize size)
{
	qfv_device_t *device = memory->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	void       *map = 0;

	dfunc->vkMapMemory (dev, memory->object, offset, size, 0, &map);
	return map;
}

void
QFV_UnmapMemory (qfv_memory_t *memory)
{
	qfv_device_t *device = memory->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkUnmapMemory (dev, memory->object);
}

void
QFV_FlushMemory (qfv_mappedmemrange_t *flushRanges, uint32_t numFlushRanges)
{
	qfv_device_t *device = flushRanges[0].memory->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkMappedMemoryRange *ranges = alloca(sizeof (*ranges) * numFlushRanges);

	for (uint32_t i = 0; i < numFlushRanges; i++) {
		ranges[i].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		ranges[i].pNext = 0;
		ranges[i].memory = flushRanges[i].memory->object;
		ranges[i].offset = flushRanges[i].offset;
		ranges[i].size = flushRanges[i].size;
	}
	dfunc->vkFlushMappedMemoryRanges (dev, numFlushRanges, ranges);
}
