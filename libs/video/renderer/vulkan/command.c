/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
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

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"

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
