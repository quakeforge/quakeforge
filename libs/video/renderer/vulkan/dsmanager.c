/*
	dsmanager.c

	Vulkan descriptor set manager

	Copyright (C) 2023      Bill Currie <bill@taniwha.org>

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

#include "QF/hash.h"
#include "QF/va.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"

#include "vid_vulkan.h"

qfv_dsmanager_t *
QFV_DSManager_Create (const qfv_descriptorsetlayoutinfo_t *setLayoutInfo,
					  uint32_t maxSets, vulkan_ctx_t *ctx)
{
	VkDescriptorPoolCreateFlags poolFlags = setLayoutInfo->flags
		& VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	size_t      size = sizeof (qfv_dsmanager_t);
	size += sizeof (VkDescriptorPoolSize[setLayoutInfo->num_bindings]);
	qfv_dsmanager_t *setManager = malloc (size);
	auto poolSizes = (VkDescriptorPoolSize *) &setManager[1];
	*setManager = (qfv_dsmanager_t) {
		.name = setLayoutInfo->name,
		.device = ctx->device,
		.poolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = poolFlags,
			.maxSets = maxSets,
			.poolSizeCount = setLayoutInfo->num_bindings,
			.pPoolSizes = poolSizes,
		},
		.freePools = DARRAY_STATIC_INIT (4),
		.usedPools = DARRAY_STATIC_INIT (4),
		.freeSets = DARRAY_STATIC_INIT (4),
	};
	for (uint32_t i = 0; i < setLayoutInfo->num_bindings; i++) {
		auto binding = setLayoutInfo->bindings[i];
		poolSizes[i] = (VkDescriptorPoolSize) {
			.type = binding.descriptorType,
			.descriptorCount = maxSets * binding.descriptorCount,
		};
	};

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = setManager->device->funcs;

	VkDescriptorSetLayoutCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = setLayoutInfo->flags,
		.bindingCount = setLayoutInfo->num_bindings,
		.pBindings = setLayoutInfo->bindings,
	};
	dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0,
										&setManager->layout);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						 setManager->layout,
						 va (ctx->va_ctx, "descriptorSetLayout:%s",
							 setLayoutInfo->name));

	return setManager;
}

void
QFV_DSManager_Destroy (qfv_dsmanager_t *setManager)
{
	if (!setManager) {
		return;
	}
	VkDevice dev = setManager->device->dev;
	qfv_devfuncs_t *dfunc = setManager->device->funcs;

	for (size_t i = 0; i < setManager->freePools.size; i++) {
		dfunc->vkDestroyDescriptorPool (dev, setManager->freePools.a[i], 0);
	}
	for (size_t i = 0; i < setManager->usedPools.size; i++) {
		dfunc->vkDestroyDescriptorPool (dev, setManager->usedPools.a[i], 0);
	}
	if (setManager->activePool) {
		dfunc->vkDestroyDescriptorPool (dev, setManager->activePool, 0);
	}
	DARRAY_CLEAR (&setManager->freePools);
	DARRAY_CLEAR (&setManager->usedPools);
	DARRAY_CLEAR (&setManager->freeSets);

	dfunc->vkDestroyDescriptorSetLayout (dev, setManager->layout, 0);
	free (setManager);
}

VkDescriptorSet
QFV_DSManager_AllocSet (qfv_dsmanager_t *setManager)
{
	if (setManager->freeSets.size) {
		uint32_t    ind = --setManager->freeSets.size;
		return setManager->freeSets.a[ind];
	}
	VkDevice dev = setManager->device->dev;
	qfv_devfuncs_t *dfunc = setManager->device->funcs;
	VkResult    res;
retry:
	if (setManager->activePool) {
		VkDescriptorSet set;
		VkDescriptorSetAllocateInfo aInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = setManager->activePool,
			.descriptorSetCount = 1,
			.pSetLayouts = &setManager->layout,
		};
		res = dfunc->vkAllocateDescriptorSets (dev, &aInfo, &set);
		if (res == VK_SUCCESS) {
			return set;
		}
		if (res != VK_ERROR_OUT_OF_POOL_MEMORY) {
			Sys_Error ("failed to allocate descriptor set: %d", res);
		}
		DARRAY_APPEND (&setManager->usedPools, setManager->activePool);
	}
	if (setManager->freePools.size) {
		uint32_t    ind = --setManager->freePools.size;
		setManager->activePool = setManager->freePools.a[ind];
		goto retry;
	}

	res = dfunc->vkCreateDescriptorPool (dev, &setManager->poolCreateInfo, 0,
										 &setManager->activePool);
	if (res != VK_SUCCESS) {
		Sys_Error ("failed to create descriptor set pool: %d", res);
	}
	goto retry;
}

void
QFV_DSManager_FreeSet (qfv_dsmanager_t *setManager, VkDescriptorSet set)
{
	DARRAY_APPEND (&setManager->freeSets, set);
}
