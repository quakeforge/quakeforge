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
#include <string.h>

#include "QF/backtrace.h"
#include "QF/dstring.h"
#include "QF/fbsearch.h"

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
	size_t atom_mask = atom - 1;
	size = (size + atom_mask) & ~atom_mask;

	qfv_devfuncs_t *dfunc = device->funcs;
	dstring_t  *str = dstring_new ();

	auto buffer = QFV_CreateBuffer (device, size,
									VK_BUFFER_USAGE_TRANSFER_SRC_BIT
									//FIXME make a param
									| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, buffer,
						 dsprintf (str, "staging:buffer:%s", name));
	auto memory = QFV_AllocBufferMemory (device, buffer,
										 VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
										 size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, memory,
						 dsprintf (str, "staging:memory:%s", name));
	QFV_BindBufferMemory (device, buffer, memory, 0);

	qfv_stagebuf_t *stage = malloc (sizeof (qfv_stagebuf_t));
	*stage = (qfv_stagebuf_t) {
		.device = device,
		.cmdPool = cmdPool,
		.buffer = buffer,
		.memory = memory,
		.atom_mask = atom_mask,
		.free = { [0] = { .offset = 0, .length = size } },
		.num_free = 1,
		.size = size,
		.name = strdup (name),
	};
	dfunc->vkMapMemory (device->dev, memory, 0, size, 0, &stage->data);

	int  count = RB_buffer_size (&stage->packets);
	auto bufferset = QFV_AllocCommandBufferSet (count, alloca);
	QFV_AllocateCommandBuffers (device, cmdPool, 0, bufferset);
	for (int i = 0; i < count; i++) {
		auto packet = &stage->packets.buffer[i];
		*packet = (qfv_packet_t) {
			.stage = stage,
			.cmd = bufferset->a[i],
			.fence = QFV_CreateFence (device, 1),
		};
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 packet->cmd,
							 dsprintf (str, "staging:packet:cmd:%s:%d",
									   name, i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_FENCE,
							 packet->fence,
							 dsprintf (str, "staging:packet:fence:%s:%d",
									   name, i));
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
	auto fences = QFV_AllocFenceSet (count, alloca);
	auto cmdBuf = QFV_AllocCommandBufferSet (count, alloca);
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
	free ((char *) stage->name);
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

static int
stage_space_cmp (const void *_o, const void *_b)
{
	const size_t *offset = _o;
	const qfvs_space_t *b = _b;
	return *offset < b->offset ? -1 : *offset > b->offset ? 1 : 0;
}

static bool
stage_space_touch (const qfvs_space_t *a, const qfvs_space_t *b)
{
	return a->offset + a->length == b->offset;
}

static void
release_space (qfv_stagebuf_t *stage, qfv_packet_t *p)
{
	if (!p->length) {
		printf (ONG "packet never had space allocated" DFL "\n");
		return;
	}
	qfvs_space_t space = {
		.offset = p->offset,
		.length = align (p->length, 16),
	};
	auto f = stage->free[stage->num_free - 1];
	if (stage->num_free == 0 || f.offset + f.length < space.offset) {
		// space being freed is beyond the last one, so simply append it
		if (stage->num_free == QFV_PACKET_COUNT + 1) {
			Sys_Error ("too many free spaces");
		}
		stage->free[stage->num_free++] = space;
		return;
	}
	qfvs_space_t *fs = fbsearch (&space.offset, stage->free, stage->num_free,
								 sizeof (*fs), stage_space_cmp);
	if (!fs) {
		// before first slot
		fs = &stage->free[0];
		if (stage_space_touch (&space, fs)) {
			// merge the slots
			fs->offset = space.offset;
			fs->length += space.length;
			return;
		}
		// need to insert
	} else {
		if (stage_space_touch (fs, &space)) {
			// merge the slots
			fs->length += space.length;
			if (fs - stage->free < stage->num_free - 1
				&& stage_space_touch (fs, fs + 1)) {
				// merge and delete the later entry
				fs->length += fs[1].length;
				int count = --stage->num_free - (fs + 1 - stage->free);
				memmove (fs + 1, fs + 2, sizeof (qfvs_space_t[count]));
			}
			return;
		}
		// need to insert
	}
	// insert a new free space
	if (stage->num_free == QFV_PACKET_COUNT + 1) {
		Sys_Error ("too many free spaces");
	}
	int count = stage->num_free++ - (fs - stage->free);
	memmove (fs + 1, fs, sizeof (qfvs_space_t[count]));
	*fs = space;
}

static void
check_invariants (qfv_stagebuf_t *stage)
{
	if (stage->num_free < 0 || stage->num_free > (int) countof (stage->free)) {
		Sys_Error ("bad num_free: %d/%zd}", stage->num_free,
				   countof (stage->free));
	}
	for (int i = 0; i < stage->num_free; i++) {
		auto f = stage->free[i];
		if (f.offset > stage->size) {
			Sys_Error ("free space outside of stage buffer");
		}
		if (f.offset + f.length > stage->size) {
			Sys_Error ("free space extends behind stage buffer");
		}
		if (f.length == 0) {
			Sys_Error ("free space length is zero");
		}
		if (f.offset != align (f.offset, 16)) {
			Sys_Error ("free space offset misaligned");
		}
		if (f.length != align (f.length, 16)) {
			Sys_Error ("free space length misaligned");
		}
		if (i < stage->num_free - 1) {
			auto n = stage->free[i + 1];
			if (f.offset >= n.offset) {
				Sys_Error ("free space out of order");
			}
			if (f.offset + f.length > n.offset) {
				Sys_Error ("free space overlap");
			}
			if (f.offset + f.length == n.offset) {
				Sys_Error ("free space not merged");
			}
		}
	}
}

static void
consume_space (qfv_stagebuf_t *stage, qfvs_space_t *fs, size_t size)
{
	size = align (size, 16);
	fs->offset += size;
	fs->length -= size;
	if (!fs->length) {
		int count = --stage->num_free - (fs - stage->free);
		memmove (fs, fs + 1, sizeof (qfvs_space_t[count]));
	}
}
static void *
acquire_space (qfv_packet_t *packet, size_t size)
{
	auto stage = packet->stage;
	auto device = stage->device;
	auto dfunc = device->funcs;

	// anticipate being able to allocate
	void *data = (byte *) stage->data + packet->offset + packet->length;

	if (packet->length && packet->length + size <= align (packet->length, 16)) {
		// packet hasn't grown beyond its allocation
		packet->length += size;
		return data;
	}

	check_invariants (stage);

	// clean up after any completed packets
	while (RB_DATA_AVAILABLE (stage->packets) > 1) {
		auto p = RB_PEEK_DATA (stage->packets, 0);
		if (dfunc->vkGetFenceStatus (device->dev, p->fence) != VK_SUCCESS) {
			break;
		}
		release_space (stage, p);
		RB_RELEASE (stage->packets, 1);
	}

	check_invariants (stage);

	if (size > stage->size) {
		// utterly impossible allocation
		return nullptr;
	}
	if (!stage->num_free) {
		return nullptr;
	}
	if (!packet->length) {
		auto fs = &stage->free[0];
		// find the LARGEST slot (to allow for future extensions to the packet)
		for (int i = 0; i < stage->num_free; i++) {
			if (stage->free[i].length > fs->length) {
				fs = &stage->free[i];
			}
		}
		if (fs->length < size) {
			return nullptr;
		}
		packet->offset = fs->offset;
		packet->length = size;

		consume_space (stage, fs, size);
		check_invariants (stage);
		return (byte *) stage->data + packet->offset;
	}
	size_t end = packet->offset + align (packet->length, 16);
	qfvs_space_t *fs = bsearch (&end, stage->free, stage->num_free,
								sizeof (*fs), stage_space_cmp);
	if (!fs) {
		return nullptr;
	}
	if (fs->offset + fs->length >= packet->offset + packet->length + size) {
		// enough space available
		packet->length += size;
		size_t diff = packet->offset + align (packet->length, 16) - fs->offset;
		consume_space (stage, fs, diff);
		check_invariants (stage);
		return data;
	}
	return nullptr;
}

qfv_packet_t *
QFV_PacketAcquire (qfv_stagebuf_t *stage, const char *name)
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
		release_space (stage, packet);
		RB_RELEASE (stage->packets, 1);
	}
	packet = RB_ACQUIRE (stage->packets, 1);

	packet->offset = 0;
	packet->length = 0;
	packet->owner = __builtin_return_address (0);

	int id = packet - stage->packets.buffer;
	dstring_t  *str = dstring_newstr ();
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
						 packet->cmd,
						 dsprintf (str, "staging:packet:cmd:%s:%d:%s",
								   stage->name, id, name));
	dstring_delete (str);

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
					 int width, int height, size_t offset,
					 const qfv_imagebarrier_t *srcBarrier,
					 const qfv_imagebarrier_t *dstBarrier)
{
	if (offset >= packet->length) {
		Sys_Error ("offset outside of packet");
	}
	qfv_devfuncs_t *dfunc = packet->stage->device->funcs;
	qfv_imagebarrier_t ib = *srcBarrier;
	ib.barrier.image = dstImage;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, 1, &ib.barrier);
	VkBufferImageCopy copy_region = {
		.bufferOffset = packet->offset + offset,
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

size_t QFV_PacketOffset (qfv_packet_t *packet, void *ptr)
{
	auto sub = (uintptr_t) ptr;
	auto data = (uintptr_t) packet->stage->data;
	if (sub < data || sub > data + packet->stage->size) {
		Sys_Error ("ptr outside of staging buffer");
	}
	sub -= data;
	if (sub < packet->offset || sub >= packet->offset + packet->length) {
		Sys_Error ("ptr outside of packet");
	}
	return sub - packet->offset;
}

size_t QFV_PacketFullOffset (qfv_packet_t *packet, void *ptr)
{
	auto sub = (uintptr_t) ptr;
	auto data = (uintptr_t) packet->stage->data;
	if (sub < data || sub > data + packet->stage->size) {
		Sys_Error ("ptr outside of staging buffer");
	}
	sub -= data;
	if (sub < packet->offset || sub >= packet->offset + packet->length) {
		Sys_Error ("ptr outside of packet");
	}
	return sub;
}
