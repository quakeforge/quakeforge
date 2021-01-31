#ifndef __QF_Vulkan_staging_h
#define __QF_Vulkan_staging_h

#include "QF/ringbuffer.h"

typedef struct qfv_packet_s {
	struct qfv_stagebuf_s *stage;	///< staging buffer that owns this packet
	VkCommandBuffer cmd;
	VkFence     fence;
	size_t      offset;
	size_t      length;
} qfv_packet_t;

typedef struct qfv_stagebuf_s {
	struct qfv_device_s *device;
	VkBuffer    buffer;
	VkDeviceMemory memory;
	RING_BUFFER(qfv_packet_t, 4) packets;	///< packets for controlling access
	size_t      atom_mask;	///< for flush size rounding
	size_t      size;		///< actual size of the buffer
	size_t      end;		///< effective end of the buffer due to early wrap
	size_t      space_start;///< beginning of available space
	size_t      space_end;	///< end of available space
	void       *data;
} qfv_stagebuf_t;


qfv_stagebuf_t *QFV_CreateStagingBuffer (struct qfv_device_s *device,
										 const char *name, size_t size,
										 VkCommandPool cmdPool);
void QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage);
void QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size);
qfv_packet_t *QFV_PacketAcquire (qfv_stagebuf_t *stage);
void *QFV_PacketExtend (qfv_packet_t *packet, size_t size);
void QFV_PacketSubmit (qfv_packet_t *packet);

#endif//__QF_Vulkan_staging_h
