#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

void *stage_memory;

static VkResult
vkMapMemory (VkDevice device, VkDeviceMemory memory,
			 VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
			 void **data)
{
	char       *buf = calloc (1, size);
	stage_memory = buf;
	*data = buf;
	return VK_SUCCESS;
}

static VkResult
vkBindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
					VkDeviceSize offset)
{
	return VK_SUCCESS;
}

static VkResult
vkCreateBuffer (VkDevice device, const VkBufferCreateInfo *cinfo,
				const VkAllocationCallbacks *allocator, VkBuffer *buffer)
{
	*buffer = 0;
	return VK_SUCCESS;
}

static void
vkGetBufferMemoryRequirements (VkDevice device, VkBuffer buffer,
							   VkMemoryRequirements *requirements)
{
	memset (requirements, 0, sizeof (*requirements));
}

static VkResult
vkAllocateCommandBuffers (VkDevice device,
						  const VkCommandBufferAllocateInfo *info,
						  VkCommandBuffer *buffers)
{
	static size_t cmdBuffer = 0;

	for (uint32_t i = 0; i < info->commandBufferCount; i++) {
		buffers[i] = (VkCommandBuffer) ++cmdBuffer;
	}
	return VK_SUCCESS;
}

static VkResult
vkCreateFence (VkDevice device, const VkFenceCreateInfo *info,
			   const VkAllocationCallbacks *allocator, VkFence *fence)
{
	int       *f = malloc (sizeof (int));
	*f = info->flags & VK_FENCE_CREATE_SIGNALED_BIT ? 1 : 0;
	*(int **)fence = f;
	return VK_SUCCESS;
}

int wait_count = 0;

static VkResult
vkWaitForFences (VkDevice device, uint32_t fenceCount, const VkFence *fences,
				 VkBool32 waitAll, uint64_t timeout)
{
	for (uint32_t i = 0; i < fenceCount; i++) {
		int        *f = (int *) (intptr_t) fences[i];
		if (*f) {
			wait_count++;
		}
		*f = 0;
	}
	return VK_SUCCESS;
}

static VkResult
vkResetFences (VkDevice device, uint32_t fenceCount, const VkFence *fences)
{
	for (uint32_t i = 0; i < fenceCount; i++) {
		int        *f = (int *) (intptr_t) fences[i];
		*f = 0;
	}
	return VK_SUCCESS;
}

static VkResult
vkGetFenceStatus (VkDevice device, VkFence fence)
{
	int        *f = (int *) (intptr_t) fence;
	return *f ? VK_SUCCESS : VK_NOT_READY;
}

static VkResult
vkResetCommandBuffer (VkCommandBuffer buffer, VkCommandBufferResetFlags flags)
{
	return VK_SUCCESS;
}

static VkResult
vkBeginCommandBuffer (VkCommandBuffer buffer,
					  const VkCommandBufferBeginInfo *info)
{
	return VK_SUCCESS;
}

static VkResult
vkEndCommandBuffer (VkCommandBuffer buffer)
{
	return VK_SUCCESS;
}

static VkResult
vkFlushMappedMemoryRanges (VkDevice device, uint32_t count,
						   const VkMappedMemoryRange *ranges)
{
	return VK_SUCCESS;
}

static VkResult
vkQueueSubmit (VkQueue queue, uint32_t count, const VkSubmitInfo *submits,
			   VkFence fence)
{
	int        *f = (int *) (intptr_t) fence;
	*f = 1;
	return VK_SUCCESS;
}

qfv_devfuncs_t dfuncs = {
	vkCreateBuffer:vkCreateBuffer,
	vkGetBufferMemoryRequirements:vkGetBufferMemoryRequirements,
	vkMapMemory:vkMapMemory,
	vkBindBufferMemory:vkBindBufferMemory,
	vkAllocateCommandBuffers:vkAllocateCommandBuffers,
	vkCreateFence:vkCreateFence,
	vkWaitForFences:vkWaitForFences,
	vkResetFences:vkResetFences,
	vkGetFenceStatus:vkGetFenceStatus,
	vkResetCommandBuffer:vkResetCommandBuffer,
	vkBeginCommandBuffer:vkBeginCommandBuffer,
	vkEndCommandBuffer:vkEndCommandBuffer,
	vkFlushMappedMemoryRanges:vkFlushMappedMemoryRanges,
	vkQueueSubmit:vkQueueSubmit,
};
qfv_physdev_t physDev = {
	properties:{
		limits:{
			nonCoherentAtomSize:256,
		},
	},
};
qfv_device_t device = {
	physDev:&physDev,
	funcs:&dfuncs,
};

static void __attribute__ ((format (PRINTF, 2, 3), noreturn))
_error (int line, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	fprintf (stderr, "%d: ", line);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
	exit(1);
}
#define error(fmt...) _error(__LINE__, fmt)

int
main (void)
{
	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (&device, "", 1024, 0);

	if (stage->size != 1024) {
		error ("stage has incorrect size: %zd", stage->size);
	}
	if (stage->end != stage->size) {
		error ("stage has incorrect end: %zd", stage->end);
	}
	if (!stage->data || stage->data != stage_memory) {
		error ("stage memory not mapped: d:%p, m:%p",
			   stage->data, stage_memory);
	}

	for (size_t i = 0; i < RB_buffer_size (&stage->packets); i++) {
		qfv_packet_t *p = &stage->packets.buffer[i];
		if (p->stage != stage) {
			error ("packet[%zd] stage not set: ps:%p s:%p", i,
				   p->stage, stage);
		}
		if (!p->cmd) {
			error ("packet[%zd] has no command buffer", i);
		}
		if (!p->fence) {
			error ("packet[%zd] has no fence", i);
		}
		if (vkGetFenceStatus (device.dev, p->fence) != VK_SUCCESS) {
			error ("packet[%zd].fence is not signaled", i);
		}
		if (p->offset || p->length) {
			error ("packet[%zd] size/length not initialized: o:%zd l:%zd",
				   i, p->offset, p->length);
		}
	}

	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	if (!packet) {
		error ("could not get a packet");
	}
	if (RB_DATA_AVAILABLE (stage->packets) != 1) {
		error ("in flight packet count incorrect");
	}
	if (vkGetFenceStatus (device.dev, packet->fence) != VK_NOT_READY) {
		error ("packet.fence is signaled");
	}
	if (QFV_PacketExtend (packet, 2048)) {
		error ("2048 byte request did not return null");
	}
	if (!QFV_PacketExtend (packet, 1024)) {
		error ("1024 byte request returned null");
	}
	if (QFV_PacketExtend (packet, 1)) {
		error ("1 byte request did not return null");
	}

	return 0;
}
