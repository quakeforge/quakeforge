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

static VkResult
vkWaitForFences (VkDevice device, uint32_t fenceCount, const VkFence *fences,
				 VkBool32 waitAll, uint64_t timeout)
{
	for (uint32_t i = 0; i < fenceCount; i++) {
		int        *f = (int *)fences[i];
		*f = 0;
	}
	return VK_SUCCESS;
}

static VkResult
vkResetFences (VkDevice device, uint32_t fenceCount, const VkFence *fences)
{
	for (uint32_t i = 0; i < fenceCount; i++) {
		int        *f = (int *)fences[i];
		*f = 0;
	}
	return VK_SUCCESS;
}

static VkResult
vkGetFenceStatus (VkDevice device, VkFence fence)
{
	int        *f = (int *)fence;
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
	int        *f = (int *)fence;
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
qfv_physdev_t physDev;
qfv_device_t device = {
	physDev:&physDev,
	funcs:&dfuncs,
};

static void __attribute__ ((format (printf, 2, 3), noreturn))
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
	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (&device, 1024, 4, 0);

	if (stage->num_packets != 4) {
		error ("stage has incorrect packet count: %zd\n", stage->num_packets);
	}
	if (stage->next_packet != 0) {
		error ("stage has incorrect next_packet: %zd\n", stage->next_packet);
	}
	if (stage->size != 1024) {
		error ("stage has incorrect size: %zd\n", stage->size);
	}
	if (stage->end != stage->size) {
		error ("stage has incorrect end: %zd\n", stage->end);
	}
	if (stage->head || stage->tail != stage->head) {
		error ("stage ring buffer not initialized: h:%zd t:%zd\n",
			   stage->head, stage->tail);
	}
	if (!stage->data || stage->data != stage_memory) {
		error ("stage memory not mapped: d:%p, m:%p\n",
			   stage->data, stage_memory);
	}

	for (size_t i = 0; i < stage->num_packets; i++) {
		qfv_packet_t *p = &stage->packet[i];
		if (p->stage != stage) {
			error ("packet[%zd] stage not set: ps:%p s:%p\n", i,
				   p->stage, stage);
		}
		if (!p->cmd) {
			error ("packet[%zd] has no command buffer\n", i);
		}
		if (!p->fence) {
			error ("packet[%zd] has no fence\n", i);
		}
		for (size_t j = 0; j < i; j++) {
			qfv_packet_t *q = &stage->packet[j];
			if (q->cmd == p->cmd || q->fence == p->fence) {
				error ("packet[%zd] has dup fence or cmd buf\n", i);
			}
		}
		if (vkGetFenceStatus (device.dev, p->fence) != VK_SUCCESS) {
			error ("packet[%zd].fence is not signaled\n", i);
		}
		if (p->offset || p->length) {
			error ("packet[%zd] size/length not initialized: o:%zd l:%zd\n",
				   i, p->offset, p->length);
		}
	}

	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	if (vkGetFenceStatus (device.dev, packet->fence) != VK_NOT_READY) {
		error ("packet.fence is signaled\n");
	}

	void *data;
	size_t old_head, old_tail;

	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet, 0);
	if (data) {
		error ("0 byte extend did not return null\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("0 byte extend moved head or tail\n");
	}

	data = QFV_PacketExtend (packet, 2048);
	if (data) {
		error ("2048 byte extend did not return null\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("2048 byte extend moved head or tail\n");
	}

	data = QFV_PacketExtend (packet, 1024);
	if (!data) {
		error ("1024 byte extend failed\n");
	}
	if (stage->head == old_head) {
		error ("1024 byte extend did not move head\n");
	}
	if (stage->tail != old_tail) {
		error ("1024 byte extend moved tail\n");
	}
	if (stage->head > stage->size) {
		error ("stage head out of bounds: %zd\n", stage->head);
	}
	if (packet->offset != old_head || packet->length != 1024) {
		error ("packet offset/size incorrect: p:%zd,%zd h:%zd\n",
			   packet->offset, packet->length, old_head);
	}
	if (stage->head < packet->offset + packet->length) {
		error ("stage head before end of packet: %zd, pe:%zd\n",
			   stage->head, packet->offset + packet->length);
	}

	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet, 16);
	if (data) {
		error ("16 byte extend in full stage did not return null\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("16 byte extend moved head or tail\n");
	}
	QFV_PacketSubmit (packet);
	if (vkGetFenceStatus (device.dev, packet->fence) != VK_SUCCESS) {
		error ("packet.fence is not signaled\n");
	}

	if (stage->head != 1024 || stage->tail != 0) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}

	qfv_packet_t *packet2 = QFV_PacketAcquire (stage);
	if (!packet2 || packet2 == packet) {
		error ("did not get new packet: n:%p o:%p\n", packet2, packet);
	}
	packet = packet2;
	if (packet->offset != 0 || stage->head != 0 || stage->tail != 0) {
		error ("new packet did not wrap: p:%zd h:%zd t:%zd\n",
			   packet2->offset, stage->head, stage->tail);
	}

	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet, 768);
	if (!data) {
		error ("768 byte extend failed\n");
	}
	if (stage->head == old_head) {
		error ("768 byte extend did not move head\n");
	}
	if (stage->tail != 0) {
		error ("768 byte extend dit not wrap tail: %zd\n", stage->tail);
	}
	if (stage->head > stage->size) {
		error ("stage head out of bounds: %zd\n", stage->head);
	}
	if (packet->offset != old_head || packet->length != 768) {
		error ("packet offset/size incorrect: p:%zd,%zd h:%zd\n",
			   packet->offset, packet->length, old_head);
	}
	if (stage->head < packet->offset + packet->length) {
		error ("stage head before end of packet: %zd, pe:%zd\n",
			   stage->head, packet->offset + packet->length);
	}

	// test attempting to wrap the packet (without needed space)
	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet, 512);
	if (data) {
		error ("512 byte extend in partially full stage succeeded\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("512 byte extend moved head or tail\n");
	}
	QFV_PacketSubmit (packet);

	if (stage->head != 768 || stage->tail != 0) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}

	packet = QFV_PacketAcquire (stage);

	// test wrapping a new packet
	data = QFV_PacketExtend (packet, 512);
	if (!data) {
		error ("512 byte extend failed\n");
	}
	if (packet->offset != 0 || packet->length != 512) {
		error ("packet offset/size incorrect: p:%zd,%zd\n",
			   packet->offset, packet->length);
	}
	if (stage->head != 512 || stage->tail != stage->end || stage->end !=768) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}
	data = QFV_PacketExtend (packet, 512);
	if (!data) {
		error ("second 512 byte extend failed\n");
	}
	if (packet->offset != 0 || packet->length != 1024) {
		error ("packet offset/size incorrect: p:%zd,%zd\n",
			   packet->offset, packet->length);
	}
	if (stage->head != 1024 || stage->tail != 0 || stage->end != 1024) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (stage);
	data = QFV_PacketExtend (packet, 512);
	if (!data) {
		error ("512 byte extend failed\n");
	}
	if (packet->offset != 0 || packet->length != 512) {
		error ("packet offset/size incorrect: p:%zd,%zd\n",
			   packet->offset, packet->length);
	}
	QFV_PacketSubmit (packet);

	if (stage->head != 512 || stage->tail != 0 || stage->end != 1024) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}

	packet = QFV_PacketAcquire (stage);
	data = QFV_PacketExtend (packet, 256);
	if (!data) {
		error ("256 byte extend failed\n");
	}
	if (packet->offset != 512 || packet->length != 256) {
		error ("packet offset/size incorrect: p:%zd,%zd\n",
			   packet->offset, packet->length);
	}
	if (stage->head != 768 || stage->tail != 512 || stage->end != 1024) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}
	// don't submit yet. Normally, it would be an error, but the test harness
	// needs to keep the packet on hand for the following tests to work
	packet2 = QFV_PacketAcquire (stage);
	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet2, 768);
	if (data) {
		error ("768 byte extend did not return null\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("768 byte extend moved head or tail\n");
	}

	//should wrap
	data = QFV_PacketExtend (packet2, 512);
	if (!data) {
		error ("512 byte extend failed\n");
	}
	if (packet2->offset != 0 || packet2->length != 512) {
		error ("packet offset/size incorrect: p:%zd,%zd\n",
			   packet2->offset, packet2->length);
	}
	if (stage->head != 512 || stage->tail != 512 || stage->end != 768) {
		error ("stage head or tail not as expected: h: %zd t:%zd\n",
			   stage->head, stage->tail);
	}

	//submit the first packet
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (stage);
	old_head = stage->head;
	old_tail = stage->tail;
	data = QFV_PacketExtend (packet, 768);
	if (data) {
		error ("768 byte extend did not return null\n");
	}
	if (stage->head != old_head || stage->tail != old_tail) {
		error ("768 byte extend moved head or tail\n");
	}
	return 0;
}
