/*
	staging.c

	Vulkan staging buffer manager

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

#include "QF/backtrace.h"
#include "QF/dstring.h"

#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

qfv_stagebuf_t *
QFV_CreateStagingBuffer (qfv_device_t *device, const char *name, size_t size,
						 VkCommandPool cmdPool)
{
	size_t atom = device->physDev->p.properties.limits.nonCoherentAtomSize;
	qfv_devfuncs_t *dfunc = device->funcs;
	dstring_t  *str = dstring_new ();

	qfv_stagebuf_t *stage = calloc (1, sizeof (qfv_stagebuf_t));
	stage->atom_mask = atom - 1;
	size = (size + stage->atom_mask) & ~stage->atom_mask;
	stage->device = device;
	stage->cmdPool = cmdPool;
	stage->buffer = QFV_CreateBuffer (device, size,
									  VK_BUFFER_USAGE_TRANSFER_SRC_BIT
									  //FIXME make a param
									  | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, stage->buffer,
						 dsprintf (str, "staging:buffer:%s", name));
	stage->memory = QFV_AllocBufferMemory (device, stage->buffer,
										   VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
										   size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, stage->memory,
						 dsprintf (str, "staging:memory:%s", name));
	stage->size = size;
	stage->end = size;

	dfunc->vkMapMemory (device->dev, stage->memory, 0, size, 0, &stage->data);
	QFV_BindBufferMemory (device, stage->buffer, stage->memory, 0);

	int         count = RB_buffer_size (&stage->packets);

	__auto_type bufferset = QFV_AllocCommandBufferSet (count, alloca);
	QFV_AllocateCommandBuffers (device, cmdPool, 0, bufferset);
	for (int i = 0; i < count; i++) {
		qfv_packet_t *packet = &stage->packets.buffer[i];
		packet->stage = stage;
		packet->cmd = bufferset->a[i];
		packet->fence = QFV_CreateFence (device, 1);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 packet->cmd,
							 dsprintf (str, "staging:packet:cmd:%s:%d",
									   name, i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_FENCE,
							 packet->fence,
							 dsprintf (str, "staging:packet:fence:%s:%d",
									   name, i));
		packet->offset = 0;
		packet->length = 0;
	}
	dstring_delete (str);
	return stage;
}

void
QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage)
{
	if (!stage) {
		return;
	}
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	int         count = RB_buffer_size (&stage->packets);
	__auto_type fences = QFV_AllocFenceSet (count, alloca);
	__auto_type cmdBuf = QFV_AllocCommandBufferSet (count, alloca);
	for (int i = 0; i < count; i++) {
		fences->a[i] = stage->packets.buffer[i].fence;
		cmdBuf->a[i] = stage->packets.buffer[i].cmd;
#if 0
		auto stat = dfunc->vkGetFenceStatus (device->dev, fences->a[i]);
		if (stat != VK_SUCCESS) {
			dstring_t  *str = dstring_newstr ();
			auto packet = &stage->packets.buffer[i];
			BT_pcInfo (str, (intptr_t) packet->owner);
			Sys_Printf ("QFV_DestroyStagingBuffer: %d live packet in %p:%s\n",
						stat, stage, str->str);
			dstring_delete (str);
		}
#endif
	}
	dfunc->vkWaitForFences (device->dev, fences->size, fences->a, VK_TRUE,
							5000000000ull);
	for (int i = 0; i < count; i++) {
		dfunc->vkDestroyFence (device->dev, fences->a[i], 0);
	}
	dfunc->vkFreeCommandBuffers (device->dev, stage->cmdPool,
								 cmdBuf->size, cmdBuf->a);

	dfunc->vkUnmapMemory (device->dev, stage->memory);
	dfunc->vkDestroyBuffer (device->dev, stage->buffer, 0);
	dfunc->vkFreeMemory (device->dev, stage->memory, 0);
	free (stage);
}

void
QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	offset &= ~stage->atom_mask;
	size = (size + stage->atom_mask) & ~stage->atom_mask;
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		stage->memory, offset, size
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
}

static size_t
align (size_t x, size_t a)
{
	return (x + a - 1) & ~(a - 1);
}

static void
release_space (qfv_stagebuf_t *stage, size_t offset, size_t length)
{
	if (stage->space_end != offset
		&& offset != 0
		&& stage->space_end != stage->end) {
		Sys_Error ("staging: out of sequence packet release");
	}
	if (stage->space_end == stage->end) {
		stage->space_end = 0;
		stage->end = stage->size;
	}
	stage->space_end += align (length, 16);
}

static void *
acquire_space (qfv_packet_t *packet, size_t size)
{
	qfv_stagebuf_t *stage = packet->stage;
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	// clean up after any completed packets
	while (RB_DATA_AVAILABLE (stage->packets) > 1) {
		qfv_packet_t *p = RB_PEEK_DATA (stage->packets, 0);
		if (dfunc->vkGetFenceStatus (device->dev, p->fence) != VK_SUCCESS) {
			break;
		}
		release_space (stage, p->offset, p->length);
		RB_RELEASE (stage->packets, 1);
	}

	if (size > stage->size) {
		// utterly impossible allocation
		return 0;
	}

	// if the staging buffer has been freed up and no data is assigned to the
	// single existing packet, then ensure the packet starts at the beginning
	// of the staging buffer in order to maximize the space available to it
	// (some of the tests are redundant since if any space is assigned to a
	// packet, the buffer cannot be fully freed up)
	if (stage->space_end == stage->space_start
		&& RB_DATA_AVAILABLE (stage->packets) == 1
		&& packet->length == 0) {
		stage->space_end = 0;
		stage->space_start = 0;
		packet->offset = 0;
	}
	if (stage->space_start >= stage->space_end) {
		// all the space to the actual end of the buffer is free
		if (stage->space_start + size <= stage->size) {
			void       *data = (byte *) stage->data + stage->space_start;
			stage->space_start += size;
			return data;
		}
		// doesn't fit at the end of the buffer, try the beginning but only
		// if the packet can be moved (no spaced has been allocated to it yet)
		if (packet->length > 0) {
			// can't move it
			return 0;
		}
		// mark the unused end of the buffer such that it gets reclaimed
		// properly when the preceeding packet is freed
		stage->end = stage->space_start;
		stage->space_start = 0;
		packet->offset = 0;
	}
	while (stage->space_start + size > stage->space_end
		   && RB_DATA_AVAILABLE (stage->packets) > 1) {
		packet = RB_PEEK_DATA (stage->packets, 0);
		dfunc->vkWaitForFences (device->dev, 1, &packet->fence, VK_TRUE,
								~0ull);
		release_space (stage, packet->offset, packet->length);
		RB_RELEASE (stage->packets, 1);
	}
	if (stage->space_start + size > stage->space_end) {
		return 0;
	}
	void       *data = (byte *) stage->data + stage->space_start;
	stage->space_start += size;
	return data;
}

qfv_packet_t *
QFV_PacketAcquire (qfv_stagebuf_t *stage)
{
	qfZoneNamed (zone, true);
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfv_packet_t *packet = 0;
	if (!RB_SPACE_AVAILABLE (stage->packets)) {
		// need to wait for a packet to become available
		packet = RB_PEEK_DATA (stage->packets, 0);
		auto start = Sys_LongTime ();
		qfMessageL ("waiting on fence");
		dfunc->vkWaitForFences (device->dev, 1, &packet->fence, VK_TRUE,
								~0ull);
		qfMessageL ("got fence");
		auto end = Sys_LongTime ();
		if (end - start > 500) {
			dstring_t  *str = dstring_newstr ();
			BT_pcInfo (str, (intptr_t) packet->owner);
			Sys_Printf ("QFV_PacketAcquire: long acquire %'d for %p:%s\n",
						(int) (end - start), stage, str->str);
			dstring_delete (str);
		}
		release_space (stage, packet->offset, packet->length);
		RB_RELEASE (stage->packets, 1);
	}
	packet = RB_ACQUIRE (stage->packets, 1);

	stage->space_start = align (stage->space_start, 16);
	packet->offset = stage->space_start;
	packet->length = 0;
	packet->owner = __builtin_return_address (0);

	dfunc->vkResetFences (device->dev, 1, &packet->fence);
	dfunc->vkResetCommandBuffer (packet->cmd, 0);
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, 0,
	};
	dfunc->vkBeginCommandBuffer (packet->cmd, &beginInfo);

	return packet;
}

void *
QFV_PacketExtend (qfv_packet_t *packet, size_t size)
{
	qfZoneNamed (zone, true);
	void       *data = acquire_space (packet, size);
	if (data) {
		packet->length += size;
	}
	return data;
}

void
QFV_PacketSubmit (qfv_packet_t *packet)
{
	qfZoneNamed (zone, true);
	qfv_stagebuf_t *stage = packet->stage;
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (packet->length) {
		QFV_FlushStagingBuffer (stage, packet->offset, packet->length);
	}

	dfunc->vkEndCommandBuffer (packet->cmd);
	//XXX it may become necessary to pass in semaphores etc (maybe add to
	//packet?)
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		0, 0, 0,
		1, &packet->cmd,
		0, 0,
	};
	// The fence was reset when the packet was acquired
	dfunc->vkQueueSubmit (device->queue.queue, 1, &submitInfo, packet->fence);
}

VkResult
QFV_PacketWait (qfv_packet_t *packet)
{
	auto stage = packet->stage;
	auto device = stage->device;
	auto dfunc = device->funcs;
	VkResult res = dfunc->vkWaitForFences (device->dev, 1, &packet->fence,
										   VK_TRUE, ~0ull);
	if (res != VK_SUCCESS) {
		printf ("QFV_PacketWait: %d\n", res);
	}
	return res;
}

void
QFV_PacketCopyBuffer (qfv_packet_t *packet,
					  VkBuffer dstBuffer, VkDeviceSize offset,
					  const qfv_bufferbarrier_t *srcBarrier,
					  const qfv_bufferbarrier_t *dstBarrier)
{
	qfv_devfuncs_t *dfunc = packet->stage->device->funcs;
	qfv_bufferbarrier_t bb = *srcBarrier;
	bb.barrier.buffer = dstBuffer;
	bb.barrier.offset = offset;
	bb.barrier.size = packet->length;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	VkBufferCopy copy_region = {
		.srcOffset = packet->offset,
		.dstOffset = offset,
		.size = packet->length,
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, packet->stage->buffer, dstBuffer,
							1, &copy_region);
	bb = *dstBarrier;
	bb.barrier.buffer = dstBuffer;
	bb.barrier.offset = offset;
	bb.barrier.size = packet->length;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
}

void
QFV_PacketScatterBuffer (qfv_packet_t *packet, VkBuffer dstBuffer,
						 uint32_t count, qfv_scatter_t *scatter,
						 const qfv_bufferbarrier_t *srcBarrier,
						 const qfv_bufferbarrier_t *dstBarrier)
{
	qfv_devfuncs_t *dfunc = packet->stage->device->funcs;
	qfv_bufferbarrier_t bb = *srcBarrier;
	VkBufferCopy copy_regions[count];
	VkBufferMemoryBarrier barriers[count] = {};//FIXME arm gcc sees as uninit
	for (uint32_t i = 0; i < count; i++) {
		barriers[i] = bb.barrier;
		barriers[i].buffer = dstBuffer;
		barriers[i].offset = scatter[i].dstOffset;
		barriers[i].size = scatter[i].length;

		copy_regions[i] = (VkBufferCopy) {
			.srcOffset = packet->offset + scatter[i].srcOffset,
			.dstOffset = scatter[i].dstOffset,
			.size = scatter[i].length,
		};
	}
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, count, barriers, 0, 0);
	dfunc->vkCmdCopyBuffer (packet->cmd, packet->stage->buffer, dstBuffer,
							count, copy_regions);
	bb = *dstBarrier;
	for (uint32_t i = 0; i < count; i++) {
		barriers[i] = bb.barrier;
		barriers[i].buffer = dstBuffer;
		barriers[i].offset = scatter[i].dstOffset;
		barriers[i].size = scatter[i].length;
	}
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, count, barriers, 0, 0);
}

void
QFV_PacketCopyImage (qfv_packet_t *packet, VkImage dstImage,
					 int width, int height,
					 const qfv_imagebarrier_t *srcBarrier,
					 const qfv_imagebarrier_t *dstBarrier)
{
	qfv_devfuncs_t *dfunc = packet->stage->device->funcs;
	qfv_imagebarrier_t ib = *srcBarrier;
	ib.barrier.image = dstImage;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, 1, &ib.barrier);
	VkBufferImageCopy copy_region = {
		.bufferOffset = packet->offset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{0, 0, 0}, {width, height, 1},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   dstImage,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy_region);
	if (dstBarrier) {
		ib = *dstBarrier;
		ib.barrier.image = dstImage;
		ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
									 0, 0, 0, 0, 0, 1, &ib.barrier);
	}
}
