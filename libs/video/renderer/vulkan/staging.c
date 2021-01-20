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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/alloc.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

#include "vid_vulkan.h"

qfv_stagebuf_t *
QFV_CreateStagingBuffer (qfv_device_t *device, size_t size, int num_packets,
						 VkCommandPool cmdPool)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	qfv_stagebuf_t *stage = malloc (sizeof (qfv_stagebuf_t)
									+ num_packets * sizeof (qfv_packet_t));
	stage->device = device;
	stage->buffer = QFV_CreateBuffer (device, size,
									  VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	stage->memory = QFV_AllocBufferMemory (device, stage->buffer,
										   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
										   size, 0);
	stage->packet = (qfv_packet_t *) (stage + 1);
	stage->num_packets = num_packets;
	stage->next_packet = 0;
	stage->size = size;
	stage->end = size;
	stage->head = 0;
	stage->tail = 0;

	dfunc->vkMapMemory (device->dev, stage->memory, 0, size, 0, &stage->data);
	QFV_BindBufferMemory (device, stage->buffer, stage->memory, 0);

	__auto_type bufferset = QFV_AllocCommandBufferSet (num_packets, alloca);
	QFV_AllocateCommandBuffers (device, cmdPool, 0, bufferset);
	for (int i = 0; i < num_packets; i++) {
		stage->packet[i].stage = stage;
		stage->packet[i].cmd = bufferset->a[i];
		stage->packet[i].fence = QFV_CreateFence (device, 1);
		stage->packet[i].offset = 0;
		stage->packet[i].length = 0;
	}
	return stage;
}

void
QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	__auto_type fences = QFV_AllocFenceSet (stage->num_packets, alloca);
	for (size_t i = 0; i < stage->num_packets; i++) {
		fences->a[i] = stage->packet[i].fence;
	}
	dfunc->vkWaitForFences (device->dev, fences->size, fences->a, VK_TRUE,
							~0ull);
	for (size_t i = 0; i < stage->num_packets; i++) {
		dfunc->vkDestroyFence (device->dev, stage->packet[i].fence, 0);
	}

	dfunc->vkUnmapMemory (device->dev, stage->memory);
	dfunc->vkFreeMemory (device->dev, stage->memory, 0);
	dfunc->vkDestroyBuffer (device->dev, stage->buffer, 0);
	free (stage);
}

void
QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	size_t atom = device->physDev->properties.limits.nonCoherentAtomSize;
	offset &= ~(atom - 1);
	size = (size + atom - 1) & ~ (atom - 1);
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		stage->memory, offset, size
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
}

static void
advance_tail (qfv_stagebuf_t *stage, qfv_packet_t *packet)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfv_packet_t *start = packet;

	while (1) {
		if ((size_t) (++packet - stage->packet) >= stage->num_packets) {
			packet = stage->packet;
		}
		if (packet != start
			&& (dfunc->vkGetFenceStatus (device->dev, packet->fence)
				== VK_SUCCESS)) {
			if (packet->length == 0) {
				continue;
			}
			if ((stage->tail == stage->end && packet->offset == 0)
				|| stage->tail == packet->offset) {
				stage->tail = packet->offset + packet->length;
				packet->length = 0;
				if (stage->tail >= stage->end) {
					stage->end = stage->size;
					stage->tail = stage->size;
				}
			}
			continue;
		}
		// Packets are always aquired in sequence and thus the first busy
		// packet after the start packet marks the end of available space.
		// Alternatively, there is only one packet and we've looped around
		// back to the start packet. Must ensure the tail is updated
		if (stage->tail >= stage->end && packet->offset == 0) {
			stage->end = stage->size;
			stage->tail = packet->offset;
		}
		break;
	}
}

qfv_packet_t *
QFV_PacketAcquire (qfv_stagebuf_t *stage)
{
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_packet_t *packet = &stage->packet[stage->next_packet++];
	stage->next_packet %= stage->num_packets;

	dfunc->vkWaitForFences (device->dev, 1, &packet->fence, VK_TRUE, ~0ull);

	advance_tail (stage, packet);
	if (stage->head == stage->size) {
		stage->head = 0;
	}
	packet->offset = stage->head;
	packet->length = 0;

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
	qfv_stagebuf_t *stage = packet->stage;
	if (!size || size > stage->size) {
		return 0;
	}

	//FIXME extra logic may be needed to wait wait for space to become
	//available when the requested size should fit but can't due to in-flight
	//packets
	advance_tail (stage, packet);

	size_t      head = stage->head;
	size_t      end = stage->end;
	if (head + size > stage->end) {
		if (packet->length) {
			// packets cannot wrap around the buffer (must use separate
			// packets)
			return 0;
		}
		if (stage->tail == 0) {
			// the beginning of the the staging buffer is occupied
			return 0;
		}
		packet->offset = 0;
		head = 0;
		end = stage->head;
	}
	if (head < stage->tail && head + size > stage->tail) {
		// not enough room for the sub-packet
		return 0;
	}
	void       *data = (byte *) stage->data + head;
	stage->end = end;
	stage->head = head + size;
	packet->length += size;
	return data;
}

void
QFV_PacketSubmit (qfv_packet_t *packet)
{
	qfv_stagebuf_t *stage = packet->stage;
	qfv_device_t *device = stage->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (!packet->length) {
		// XXX at this stage, this looks the same as below, I think a queue
		// completing is the only way to set a fence (other than creation),
		// so submit the (hopefully) empty command buffer so the fence becomes
		// set, but without waiting on or triggering any semaphores.
		dfunc->vkEndCommandBuffer (packet->cmd);
		VkSubmitInfo submitInfo = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
			0, 0, 0,
			1, &packet->cmd,
			0, 0,
		};
		dfunc->vkQueueSubmit (device->queue.queue, 1, &submitInfo,
							  packet->fence);
		return;
	}

	QFV_FlushStagingBuffer (stage, packet->offset, packet->length);

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
