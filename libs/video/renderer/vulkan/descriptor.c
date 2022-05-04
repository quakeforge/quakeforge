/*
	descriptor.c

	Vulkan descriptor functions

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

#include "QF/hash.h"

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"

VkSampler
QFV_CreateSampler (qfv_device_t *device,
				   VkFilter magFilter, VkFilter minFilter,
                   VkSamplerMipmapMode mipmapMode,
                   VkSamplerAddressMode addressModeU,
                   VkSamplerAddressMode addressModeV,
                   VkSamplerAddressMode addressModeW,
                   float mipLodBias,
                   VkBool32 anisotryEnable, float maxAnisotropy,
                   VkBool32 compareEnable, VkCompareOp compareOp,
                   float minLod, float maxLod,
                   VkBorderColor borderColor,
                   VkBool32 unnormalizedCoordinates)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSamplerCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, 0,
		0,
		magFilter, minFilter, mipmapMode,
		addressModeU, addressModeV, addressModeW,
		mipLodBias,
		anisotryEnable, maxAnisotropy,
		compareEnable, compareOp,
		minLod, maxLod,
		borderColor, unnormalizedCoordinates,
	};

	VkSampler sampler;
	dfunc->vkCreateSampler (dev, &createInfo, 0, &sampler);
	return sampler;
}

VkDescriptorSetLayout
QFV_CreateDescriptorSetLayout (qfv_device_t *device,
                               qfv_bindingset_t *bindingset)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 0,
		0,
		bindingset->size, bindingset->a,
	};

	VkDescriptorSetLayout layout;
	dfunc->vkCreateDescriptorSetLayout (dev, &createInfo, 0, &layout);
	return layout;
}

// There are currently only 13 descriptor types, so 16 should be plenty
static VkDescriptorPoolSize poolsize_pool[16];
static VkDescriptorPoolSize *poolsize_next;

static uintptr_t
poolsize_gethash (const void *ele, void *unused)
{
	const VkDescriptorPoolSize *poolsize = ele;
	return poolsize->type;
}

static int
poolsize_compmare (const void *ele1, const void *ele2, void *unused)
{
	const VkDescriptorPoolSize *poolsize1 = ele1;
	const VkDescriptorPoolSize *poolsize2 = ele2;
	return poolsize1->type == poolsize2->type;
}

//FIXME not thread-safe
VkDescriptorPool
QFV_CreateDescriptorPool (qfv_device_t *device,
						  VkDescriptorPoolCreateFlags flags, uint32_t maxSets,
                          qfv_bindingset_t *bindings)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	static hashtab_t *poolsizes;

	if (!poolsizes) {
		poolsizes = Hash_NewTable (16, 0, 0, 0, 0);//FIXME threads
		Hash_SetHashCompare (poolsizes, poolsize_gethash, poolsize_compmare);
	} else {
		Hash_FlushTable (poolsizes);
	}
	poolsize_next = poolsize_pool;

	VkDescriptorPoolSize *ps;
	for (size_t i = 0; i < bindings->size; i++) {
		VkDescriptorPoolSize test = { bindings->a[i].descriptorType, 0 };
		ps = Hash_FindElement (poolsizes, &test);
		if (!ps) {
			ps = poolsize_next++;
			if ((size_t) (poolsize_next - poolsize_pool)
				> sizeof (poolsize_pool) / sizeof (poolsize_pool[0])) {
				Sys_Error ("Too many descriptor types");
			}
			Hash_AddElement (poolsizes, ps);
		}
		//XXX is descriptorCount correct?
		//FIXME assumes only one layout is used with this pool
		ps->descriptorCount += bindings->a[i].descriptorCount * maxSets;
	}

	VkDescriptorPoolCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 0,
		flags, maxSets, poolsize_next - poolsize_pool, poolsize_pool,
	};

	VkDescriptorPool pool;
	dfunc->vkCreateDescriptorPool (dev, &createInfo, 0, &pool);
	return pool;
}

qfv_descriptorsets_t *
QFV_AllocateDescriptorSet (qfv_device_t *device,
						   VkDescriptorPool pool,
                           qfv_descriptorsetlayoutset_t *layouts)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0,
		pool, layouts->size, layouts->a,
	};

	__auto_type descriptorsets
		= QFV_AllocDescriptorSets (layouts->size, malloc);
	dfunc->vkAllocateDescriptorSets (dev, &allocateInfo, descriptorsets->a);
	return descriptorsets;
}
