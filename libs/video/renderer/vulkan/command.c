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
#include "QF/Vulkan/buffer.h"//FIXME should QFV_CmdPipelineBarrier be here?
#include "QF/Vulkan/image.h"//FIXME should QFV_CmdPipelineBarrier be here?
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

qfv_cmdpool_t *
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
	qfv_cmdpool_t *cmdpool = malloc (sizeof (*cmdpool));
	dfunc->vkCreateCommandPool (dev, &createInfo, 0, &cmdpool->cmdpool);
	cmdpool->device = device;
	return cmdpool;
}

int
QFV_ResetCommandPool (qfv_cmdpool_t *pool, int release)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandPool cmdpool = pool->cmdpool;
	VkCommandPoolResetFlags release_flag = 0;

	if (release) {
		release_flag = VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;
	}

	return dfunc->vkResetCommandPool (dev, cmdpool, release_flag) == VK_SUCCESS;
}

void
QFV_DestroyCommandPool (qfv_cmdpool_t *pool)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyCommandPool (dev, pool->cmdpool, 0);
	free (pool);
}

qfv_cmdbuffer_t *
QFV_AllocateCommandBuffers (qfv_cmdpool_t *pool, int secondary, int count)
{
	qfv_device_t *device = pool->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	if (secondary) {
		level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	}
	VkCommandBufferAllocateInfo allocInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0,
		pool->cmdpool, level, count
	};
	qfv_cmdbuffer_t *cmdbuffers = malloc (count * sizeof (*cmdbuffers));
	VkCommandBuffer *buffers = alloca (count * sizeof (*buffers));
	dfunc->vkAllocateCommandBuffers (dev, &allocInfo, buffers);
	for (int i = 0; i < count; i++) {
		cmdbuffers[i].device = device;
		cmdbuffers[i].cmdpool = pool->cmdpool;
		cmdbuffers[i].buffer = buffers[i];
	}
	return cmdbuffers;
}

qfv_cmdbufferset_t *
QFV_CreateCommandBufferSet (qfv_cmdbuffer_t **buffers, int numBuffers)
{
	qfv_device_t *device = buffers[0]->device;
	qfv_cmdbufferset_t *bufferset = malloc (sizeof (*bufferset)
											+ sizeof (VkCommandBuffer)
											  * numBuffers);

	bufferset->device = device;
	bufferset->buffers = (VkCommandBuffer *) (bufferset + 1);
	bufferset->numBuffers = numBuffers;

	for (int i = 0; i < numBuffers; i++) {
		bufferset->buffers[i] = buffers[i]->buffer;
	}
	return bufferset;
}

void QFV_FreeCommandBuffers (qfv_cmdbuffer_t *buffer, int count)
{
	qfv_device_t *device = buffer->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandBuffer *buffers = alloca (sizeof (*buffers) * count);

	for (int i = 0; i < count; i++) {
		buffers[i] = buffer[i].buffer;
	}

	dfunc->vkFreeCommandBuffers (dev, buffer->cmdpool, count, buffers);
	free (buffer);
}

void
QFV_DestroyCommandBufferSet (qfv_cmdbufferset_t *buffers)
{
	free (buffers);
}

int
QFV_BeginCommandBuffer (qfv_cmdbuffer_t *buffer, int oneTime, int rpContinue,
						int simultaneous,
						VkCommandBufferInheritanceInfo *inheritanceInfo)
{
	qfv_device_t *device = buffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandBuffer buff = buffer->buffer;
	VkCommandBufferUsageFlags usage = 0;

	if (oneTime) {
		usage |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	if (rpContinue) {
		usage |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	}
	if (simultaneous) {
		usage |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	}

	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		usage,
		0
	};

	return dfunc->vkBeginCommandBuffer (buff, &beginInfo) == VK_SUCCESS;
}

int
QFV_EndCommandBuffer (qfv_cmdbuffer_t *buffer)
{
	qfv_device_t *device = buffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandBuffer buff = buffer->buffer;

	return dfunc->vkEndCommandBuffer (buff) == VK_SUCCESS;
}

int
QFV_ResetCommandBuffer (qfv_cmdbuffer_t *buffer, int release)
{
	qfv_device_t *device = buffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandBuffer buff = buffer->buffer;
	VkCommandBufferResetFlags release_flag = 0;

	if (release) {
		release_flag = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
	}

	return dfunc->vkResetCommandBuffer (buff, release_flag) == VK_SUCCESS;
}

qfv_semaphore_t *
QFV_CreateSemaphore (qfv_device_t *device)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkSemaphoreCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0,
		0
	};

	qfv_semaphore_t *semaphore = malloc (sizeof (*semaphore));
	semaphore->device = device;

	dfunc->vkCreateSemaphore (dev, &createInfo, 0, &semaphore->semaphore);
	return semaphore;
}

qfv_semaphoreset_t *
QFV_CreateSemaphoreSet (qfv_semaphore_t **semaphores, int numSemaphores)
{
	qfv_device_t *device = semaphores[0]->device;
	qfv_semaphoreset_t *semaphoreset;
	semaphoreset = calloc (1, sizeof (*semaphoreset)
						   + sizeof (VkSemaphore) * numSemaphores
						   + sizeof (VkPipelineStageFlags) * numSemaphores);

	semaphoreset->device = device;
	semaphoreset->semaphores = (VkSemaphore *) (semaphoreset + 1);
	semaphoreset->stages = (VkPipelineStageFlags *)
							&semaphoreset->semaphores[numSemaphores];
	semaphoreset->numSemaphores = numSemaphores;

	for (int i = 0; i < numSemaphores; i++) {
		semaphoreset->semaphores[i] = semaphores[i]->semaphore;
	}
	return semaphoreset;
}

void
QFV_DestroySemaphore (qfv_semaphore_t *semaphore)
{
	qfv_device_t *device = semaphore->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroySemaphore (dev, semaphore->semaphore, 0);
	free (semaphore);
}

void
QFV_DestroySemaphoreSet (qfv_semaphoreset_t *semaphores)
{
	free (semaphores);
}

qfv_fence_t *
QFV_CreateFence (qfv_device_t *device, int signaled)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFenceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 0,
		signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};

	qfv_fence_t *fence = malloc (sizeof (*fence));
	fence->device = device;

	dfunc->vkCreateFence (dev, &createInfo, 0, &fence->fence);
	return fence;
}

qfv_fenceset_t *
QFV_CreateFenceSet (qfv_fence_t **fences, int numFences)
{
	qfv_device_t *device = fences[0]->device;
	qfv_fenceset_t *fenceset = malloc (sizeof (*fenceset)
									   + sizeof (VkFence) * numFences);

	fenceset->device = device;
	fenceset->fences = (VkFence *) (fenceset + 1);
	fenceset->numFences = numFences;

	for (int i = 0; i < numFences; i++) {
		fenceset->fences[i] = fences[i]->fence;
	}
	return fenceset;
}

void
QFV_DestroyFence (qfv_fence_t *fence)
{
	qfv_device_t *device = fence->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyFence (dev, fence->fence, 0);
	free (fence);
}

void
QFV_DestroyFenceSet (qfv_fenceset_t *fences)
{
	free (fences);
}

int
QFV_WaitForFences (qfv_fenceset_t *fences, int all, uint64_t timeout)
{
	qfv_device_t *device = fences->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkResult res = dfunc->vkWaitForFences (dev, fences->numFences,
										   fences->fences, all, timeout);
	return res == VK_SUCCESS;
}

int
QFV_ResetFences (qfv_fenceset_t *fences)
{
	qfv_device_t *device = fences->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	return dfunc->vkResetFences (dev, fences->numFences,
								 fences->fences) == VK_SUCCESS;
}

int
QFV_QueueSubmit (qfv_queue_t *queue, qfv_semaphoreset_t *waitSemaphores,
				 qfv_cmdbufferset_t *buffers,
				 qfv_semaphoreset_t *signalSemaphores, qfv_fence_t *fence)
{
	qfv_device_t *device = queue->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		waitSemaphores->numSemaphores,
		waitSemaphores->semaphores, waitSemaphores->stages,
		buffers->numBuffers, buffers->buffers,
		signalSemaphores->numSemaphores,
		signalSemaphores->semaphores
	};
	//FIXME multi-batch
	return dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo,
								 fence->fence) == VK_SUCCESS;
}

int
QFV_QueueWaitIdle (qfv_queue_t *queue)
{
	qfv_device_t *device = queue->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	return dfunc->vkQueueWaitIdle (queue->queue) == VK_SUCCESS;
}

void
QFV_CmdPipelineBarrier (qfv_cmdbuffer_t *cmdBuffer,
						VkPipelineStageFlags srcStageMask,
						VkPipelineStageFlags dstStageMask,
						VkDependencyFlags dependencyFlags,
						struct qfv_memorybarrierset_s *memBarrierSet,
						qfv_bufferbarrierset_t *buffBarrierSet,
						qfv_imagebarrierset_t *imgBarrierSet)
{
	qfv_device_t *device = cmdBuffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    numMemBarriers = 0;
	VkMemoryBarrier *memBarriers = 0;
	uint32_t    numBuffBarriers = 0;
	VkBufferMemoryBarrier *buffBarriers = 0;
	uint32_t    numImgBarriers = 0;
	VkImageMemoryBarrier *imgBarriers = 0;

	if (buffBarrierSet) {
		numBuffBarriers = buffBarrierSet->numBarriers;
		buffBarriers = buffBarrierSet->barriers;
	}
	if (imgBarrierSet) {
		numImgBarriers = imgBarrierSet->numBarriers;
		imgBarriers = imgBarrierSet->barriers;
	}
	dfunc->vkCmdPipelineBarrier (cmdBuffer->buffer,
								  srcStageMask, dstStageMask, dependencyFlags,
								  numMemBarriers, memBarriers,
								  numBuffBarriers, buffBarriers,
								  numImgBarriers, imgBarriers);
}

void
QFV_CmdCopyBuffer (qfv_cmdbuffer_t *cmdBuffer,
				   struct qfv_buffer_s *src, struct qfv_buffer_s *dst,
				   VkBufferCopy *regions, uint32_t numRegions)
{
	qfv_device_t *device = cmdBuffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkCmdCopyBuffer (cmdBuffer->buffer, src->buffer, dst->buffer,
							numRegions, regions);
}

void
QFV_CmdCopyBufferToImage (qfv_cmdbuffer_t *cmdBuffer,
						  struct qfv_buffer_s *src,
						  struct qfv_image_s *dst,
						  VkImageLayout layout,
						  VkBufferImageCopy *regions, uint32_t numRegions)
{
	qfv_device_t *device = cmdBuffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkCmdCopyBufferToImage (cmdBuffer->buffer, src->buffer, dst->image,
								   layout, numRegions, regions);
}

void
QFV_CmdCopyImageToBuffer (qfv_cmdbuffer_t *cmdBuffer,
						  struct qfv_image_s *src,
						  VkImageLayout layout,
						  struct qfv_buffer_s *dst,
						  VkBufferImageCopy *regions, uint32_t numRegions)
{
	qfv_device_t *device = cmdBuffer->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkCmdCopyImageToBuffer (cmdBuffer->buffer, src->image, layout,
								   dst->buffer, numRegions, regions);
}
