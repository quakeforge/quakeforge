/*
	descriptor.c

	Vulkan descriptor functions

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
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

qfv_sampler_t *
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

	qfv_sampler_t *sampler = malloc (sizeof (*sampler));
	sampler->device = device;
	dfunc->vkCreateSampler (dev, &createInfo, 0, &sampler->sampler);
	return sampler;
}

qfv_bindingset_t *
QFV_CreateBindingSet (int numBindings)
{
	qfv_bindingset_t *bindingset;
	bindingset = malloc (field_offset (qfv_bindingset_t,
									   bindings[numBindings]));
	bindingset->numBindings = numBindings;
	return bindingset;
}

void
QFV_DestroyBindingSet (qfv_bindingset_t *bindingset)
{
	free (bindingset);
}

qfv_descriptorsetlayout_t *
QFV_CreateDescriptorSetLayout (qfv_device_t *device,
                               qfv_bindingset_t *bindingset)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 0,
		0,
		bindingset->numBindings, bindingset->bindings,
	};

	qfv_descriptorsetlayout_t *layout = malloc (sizeof (layout));
	layout->device = device;
	dfunc->vkCreateDescriptorSetLayout (dev, &createInfo, 0, &layout->layout);
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
qfv_descriptorpool_t *
QFV_CreateDescriptorPool (qfv_device_t *device,
						  VkDescriptorPoolCreateFlags flags, uint32_t maxSets,
                          qfv_bindingset_t *bindings)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	static hashtab_t *poolsizes;

	if (!poolsizes) {
		poolsizes = Hash_NewTable (16, 0, 0, 0);
		Hash_SetHashCompare (poolsizes, poolsize_gethash, poolsize_compmare);
	} else {
		Hash_FlushTable (poolsizes);
	}
	poolsize_next = poolsize_pool;

	VkDescriptorPoolSize *ps;
	for (int i = 0; i < bindings->numBindings; i++) {
		VkDescriptorPoolSize test = { bindings->bindings[i].descriptorType, 0 };
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
		ps->descriptorCount += bindings->bindings[i].descriptorCount * maxSets;
	}

	VkDescriptorPoolCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 0,
		flags, maxSets, ps - poolsize_pool, poolsize_pool,
	};

	qfv_descriptorpool_t *descriptorpool = malloc (sizeof (descriptorpool));
	descriptorpool->device = device;
	dfunc->vkCreateDescriptorPool (dev, &createInfo, 0, &descriptorpool->pool);
	return descriptorpool;
}

qfv_descriptorset_t *
QFV_AllocateDescriptorSet (qfv_descriptorpool_t *pool,
                           qfv_descriptorsetlayout_t *layout)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkDescriptorSetAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0,
		pool->pool, 1, &layout->layout,
	};

	qfv_descriptorset_t *descriptorset = malloc (sizeof (*descriptorset));
	descriptorset->device = device;
	descriptorset->pool = pool;
	dfunc->vkAllocateDescriptorSets (dev, &allocateInfo, &descriptorset->set);
	return descriptorset;
}

void
QFV_UpdateDescriptorSets (qfv_device_t *device,
						  uint32_t numImageInfos,
						  qfv_imagedescriptorinfo_t *imageInfos,
						  uint32_t numBufferInfos,
						  qfv_bufferdescriptorinfo_t *bufferInfos,
						  uint32_t numTexelBufferInfos,
						  qfv_texelbufferdescriptorinfo_t *texelbufferInfos,
						  uint32_t numCopyInfos,
						  qfv_copydescriptorinfo_t *copyInfos)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	uint32_t numWrite = 0;
	numWrite += numImageInfos;
	numWrite += numBufferInfos;
	numWrite += numTexelBufferInfos;
	VkWriteDescriptorSet *writeSets = alloca (numWrite * sizeof (*writeSets));
	VkWriteDescriptorSet *writeSet = writeSets;
	for (uint32_t i = 0; i < numImageInfos; i++, writeSet++) {
		writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet->pNext = 0;
		writeSet->dstSet = imageInfos[i].descriptorset->set;
		writeSet->dstBinding = imageInfos[i].binding;
		writeSet->dstArrayElement = imageInfos[i].arrayElement;
		writeSet->descriptorCount = imageInfos[i].numInfo;
		writeSet->descriptorType = imageInfos[i].type;
		writeSet->pImageInfo = imageInfos[i].infos;
		writeSet->pBufferInfo = 0;
		writeSet->pTexelBufferView = 0;
	}
	for (uint32_t i = 0; i < numBufferInfos; i++, writeSet++) {
		writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet->pNext = 0;
		writeSet->dstSet = bufferInfos[i].descriptorset->set;
		writeSet->dstBinding = bufferInfos[i].binding;
		writeSet->dstArrayElement = bufferInfos[i].arrayElement;
		writeSet->descriptorCount = bufferInfos[i].numInfo;
		writeSet->descriptorType = bufferInfos[i].type;
		writeSet->pImageInfo = 0;
		writeSet->pBufferInfo = bufferInfos[i].infos;
		writeSet->pTexelBufferView = 0;
	}
	for (uint32_t i = 0; i < numTexelBufferInfos; i++, writeSet++) {
		writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet->pNext = 0;
		writeSet->dstSet = bufferInfos[i].descriptorset->set;
		writeSet->dstBinding = texelbufferInfos[i].binding;
		writeSet->dstArrayElement = texelbufferInfos[i].arrayElement;
		writeSet->descriptorCount = texelbufferInfos[i].numInfo;
		writeSet->descriptorType = texelbufferInfos[i].type;
		writeSet->pImageInfo = 0;
		writeSet->pBufferInfo = 0;
		writeSet->pTexelBufferView = texelbufferInfos[i].infos;
	}
	VkCopyDescriptorSet *copySets = alloca (numWrite * sizeof (*copySets));
	VkCopyDescriptorSet *copySet = copySets;
	for (uint32_t i = 0; i < numCopyInfos; i++, copySet++) {
		copySet->sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
		copySet->pNext = 0;
		copySet->srcSet = copyInfos[i].srcSet->set;
		copySet->srcBinding = copyInfos[i].srcBinding;
		copySet->srcArrayElement = copyInfos[i].srcArrayElement;
		copySet->dstSet = copyInfos[i].dstSet->set;
		copySet->dstBinding = copyInfos[i].dstBinding;
		copySet->dstArrayElement = copyInfos[i].dstArrayElement;
		copySet->descriptorCount = copyInfos[i].descriptorCount;
	}
	dfunc->vkUpdateDescriptorSets (dev, numWrite, writeSets,
								   numCopyInfos, copySets);
}

void
QFV_FreeDescriptorSet (qfv_descriptorset_t *set)
{
	qfv_device_t *device = set->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkDescriptorPool pool = set->pool->pool;

	dfunc->vkFreeDescriptorSets (dev, pool, 1, &set->set);
}

void
QFV_ResetDescriptorPool (qfv_descriptorpool_t *pool)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkResetDescriptorPool (dev, pool->pool, 0);
}

void
QFV_DestroyDescriptorPool (qfv_descriptorpool_t *pool)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyDescriptorPool (dev, pool->pool, 0);
}

void
QFV_DestroyDescriptorSetLayout (qfv_descriptorsetlayout_t *layout)
{
	qfv_device_t *device = layout->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyDescriptorSetLayout (dev, layout->layout, 0);
}

void
QFV_DestroySampler (qfv_sampler_t *sampler)
{
	qfv_device_t *device = sampler->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroySampler (dev, sampler->sampler, 0);
}
