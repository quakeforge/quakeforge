/*
	shader.c

	Vulkan shader manager

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

#include "QF/alloc.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

#include "vid_vulkan.h"

qfv_stagebuf_t *
QFV_CreateStagingBuffer (qfv_device_t *device, size_t size)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	qfv_stagebuf_t *stage = malloc (sizeof (qfv_stagebuf_t));
	stage->device = device;
	stage->buffer = QFV_CreateBuffer (device, size,
									  VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	stage->memory = QFV_AllocBufferMemory (device, stage->buffer,
										   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
										   size, 0);
	QFV_BindBufferMemory (device, stage->buffer, stage->memory, 0);
	dfunc->vkMapMemory (device->dev, stage->memory, 0, size, 0, &stage->data);
	return stage;
}

void
QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkUnmapMemory (device->dev, stage->memory);
	dfunc->vkFreeMemory (device->dev, stage->memory, 0);
	dfunc->vkDestroyBuffer (device->dev, stage->buffer, 0);
	free (stage);
}

void
QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		stage->memory, offset, size
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
}
