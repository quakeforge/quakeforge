/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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
#include "QF/qtypes.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"

qfv_cmdpoolmgr_t *
QFV_CmdPoolManager_Init (qfv_cmdpoolmgr_t *manager, qfv_device_t *device,
						 const char *name)
{
	*manager = (qfv_cmdpoolmgr_t) {
		.primary = DARRAY_STATIC_INIT (16),
		.secondary = DARRAY_STATIC_INIT (16),
		.device = device,
	};
	auto dfunc = device->funcs;

	VkCommandPoolCreateInfo poolCInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = device->queue.queueFamily,
	};
	dfunc->vkCreateCommandPool (device->dev, &poolCInfo, 0, &manager->pool);
	return manager;
}

qfv_cmdpoolmgr_t *
QFV_CmdPoolManager_New (qfv_device_t *device, const char *name)
{
	return QFV_CmdPoolManager_Init (malloc (sizeof (qfv_cmdpoolmgr_t)), device,
									name);
}

void
QFV_CmdPoolManager_Shutdown (qfv_cmdpoolmgr_t *manager)
{
	auto device = manager->device;
	auto dfunc = device->funcs;
	dfunc->vkDestroyCommandPool (device->dev, manager->pool, 0);
	DARRAY_CLEAR (&manager->primary);
	DARRAY_CLEAR (&manager->secondary);
}

void
QFV_CmdPoolManager_Delete (qfv_cmdpoolmgr_t *manager)
{
	QFV_CmdPoolManager_Shutdown (manager);
	free (manager);
}

void
QFV_CmdPoolManager_Reset (qfv_cmdpoolmgr_t *manager)
{
	auto device = manager->device;
	auto dfunc = device->funcs;
	dfunc->vkResetCommandPool (device->dev, manager->pool, 0);
	manager->active_primary = 0;
	manager->active_secondary = 0;
}

VkCommandBuffer
QFV_CmdPoolManager_CmdBuffer (qfv_cmdpoolmgr_t *manager, bool secondary)
{
	auto device = manager->device;
	auto dfunc = device->funcs;

	if (secondary) {
		if (manager->active_secondary < manager->secondary.size) {
			return manager->secondary.a[manager->active_secondary++];
		}
	} else {
		if (manager->active_primary < manager->primary.size) {
			return manager->primary.a[manager->active_primary++];
		}
	}
	VkCommandBufferAllocateInfo cinfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = manager->pool,
		.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
						   : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd;
	dfunc->vkAllocateCommandBuffers (device->dev, &cinfo, &cmd);
	if (secondary) {
		DARRAY_APPEND (&manager->secondary, cmd);
		manager->active_secondary++;
	} else {
		DARRAY_APPEND (&manager->primary, cmd);
		manager->active_primary++;
	}
	return cmd;
}

VkCommandPool
QFV_CreateCommandPool (qfv_device_t *device, uint32_t queueFamily,
					   int transient, int reset)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    flags = 0;
	if (transient) {
		flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}
	if (reset) {
		flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	}
	VkCommandPoolCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0,
		flags,
		queueFamily
	};
	VkCommandPool pool;
	dfunc->vkCreateCommandPool (dev, &createInfo, 0, &pool);
	return pool;
}

int
QFV_AllocateCommandBuffers (qfv_device_t *device, VkCommandPool pool,
							int secondary, qfv_cmdbufferset_t *bufferset)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	if (secondary) {
		level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	}
	VkCommandBufferAllocateInfo allocInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0,
		pool, level, bufferset->size
	};
	int ret = dfunc->vkAllocateCommandBuffers (dev, &allocInfo, bufferset->a);
	return ret == VK_SUCCESS;
}

VkSemaphore
QFV_CreateSemaphore (qfv_device_t *device)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSemaphoreCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0,
		0
	};

	VkSemaphore semaphore;
	dfunc->vkCreateSemaphore (dev, &createInfo, 0, &semaphore);
	return semaphore;
}

VkFence
QFV_CreateFence (qfv_device_t *device, int signaled)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFenceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 0,
		signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};

	VkFence fence;
	dfunc->vkCreateFence (dev, &createInfo, 0, &fence);
	return fence;
}

int
QFV_QueueSubmit (qfv_queue_t *queue, qfv_semaphoreset_t *waitSemaphores,
				 VkPipelineStageFlags *stages,
				 qfv_cmdbufferset_t *buffers,
				 qfv_semaphoreset_t *signalSemaphores, VkFence fence)
{
	qfv_device_t *device = queue->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		waitSemaphores->size, waitSemaphores->a, stages,
		buffers->size, buffers->a,
		signalSemaphores->size, signalSemaphores->a
	};
	//FIXME multi-batch
	return dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo, fence)
			== VK_SUCCESS;
}

int
QFV_QueueWaitIdle (qfv_queue_t *queue)
{
	qfv_device_t *device = queue->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	return dfunc->vkQueueWaitIdle (queue->queue) == VK_SUCCESS;
}

void
QFV_PushConstants (qfv_device_t *device, VkCommandBuffer cmd,
				   VkPipelineLayout layout, uint32_t numPC,
				   const qfv_push_constants_t *constants)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	for (uint32_t i = 0; i < numPC; i++) {
		dfunc->vkCmdPushConstants (cmd, layout, constants[i].stageFlags,
								   constants[i].offset, constants[i].size,
								   constants[i].data);
	}
}
