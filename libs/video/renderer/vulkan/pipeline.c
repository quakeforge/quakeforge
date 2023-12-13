/*
	pipeline.c

	Vulkan pipeline functions

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

#include "QF/dstring.h"

#include "QF/Vulkan/device.h"
#include "QF/Vulkan/pipeline.h"

VkPipelineCache
QFV_CreatePipelineCache (qfv_device_t *device, dstring_t *cacheData)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkPipelineCacheCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, 0, 0,
		cacheData->size, cacheData->str,
	};

	VkPipelineCache cache;
	dfunc->vkCreatePipelineCache (dev, &createInfo, 0, &cache);
	return cache;
}

dstring_t *
QFV_GetPipelineCacheData (qfv_device_t *device, VkPipelineCache cache)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	dstring_t  *cacheData = dstring_new ();

	dfunc->vkGetPipelineCacheData (dev, cache, &cacheData->size, 0);
	dstring_adjust (cacheData);
	dfunc->vkGetPipelineCacheData (dev, cache,
								   &cacheData->size, cacheData->str);
	return cacheData;
}

void
QFV_MergePipelineCaches (qfv_device_t *device,
						 VkPipelineCache targetCache,
						 qfv_pipelinecacheset_t *sourceCaches)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkMergePipelineCaches (dev, targetCache,
								  sourceCaches->size, sourceCaches->a);
}
