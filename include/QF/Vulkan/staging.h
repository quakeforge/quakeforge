#ifndef __QF_Vulkan_staging_h
#define __QF_Vulkan_staging_h

#include "QF/ringbuffer.h"

typedef struct dstring_s dstring_t;

typedef struct qfv_packet_s {
	struct qfv_stagebuf_s *stage;	///< staging buffer that owns this packet
	VkCommandBuffer cmd;
	VkFence     fence;
	size_t      offset;
	size_t      length;
	void       *owner;
} qfv_packet_t;

typedef struct qfvs_space_s {
	size_t      offset;
	size_t      length;
} qfvs_space_t;

#define QFV_PACKET_COUNT 32

typedef struct qfv_stagebuf_s {
	struct qfv_device_s *device;
	VkCommandPool cmdPool;
	VkBuffer    buffer;
	VkDeviceMemory memory;
	/// packets for controlling access
	RING_BUFFER(qfv_packet_t, QFV_PACKET_COUNT) packets;
	qfvs_space_t free[QFV_PACKET_COUNT + 1];
	int         num_free;	///< number of free spaces
	size_t      atom_mask;	///< for flush size rounding
	size_t      size;		///< actual size of the buffer
	dstring_t  *dstr;
	void       *data;
	const char *name;
} qfv_stagebuf_t;

typedef struct qfv_scatter_s {
	VkDeviceSize srcOffset;
	VkDeviceSize dstOffset;
	VkDeviceSize length;
} qfv_scatter_t;

typedef struct qfv_offset_s {
	uint32_t    x;
	uint32_t    y;
	uint32_t    z;
	uint32_t    layer;
	uint32_t    mip;
} qfv_offset_t;

typedef struct qfv_extent_s {
	uint32_t    width;
	uint32_t    height;
	uint32_t    depth;
	uint32_t    layers;
	uint32_t    row_length;
} qfv_extent_t;

qfv_stagebuf_t *QFV_CreateStagingBuffer (struct qfv_device_s *device,
										 const char *name, size_t size,
										 VkCommandPool cmdPool);
void QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage);
void QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size);
qfv_packet_t *QFV_PacketAcquire (qfv_stagebuf_t *stage, const char *name);
void *QFV_PacketExtend (qfv_packet_t *packet, size_t size);
void QFV_PacketSubmit (qfv_packet_t *packet);
VkResult QFV_PacketWait (qfv_packet_t *packet);
void QFV_PacketCopyBuffer (qfv_packet_t *packet,
						   VkBuffer dstBuffer, VkDeviceSize offset,
						   const VkBufferMemoryBarrier2 *srcBarrier,
						   const VkBufferMemoryBarrier2 *dstBarrier);
void QFV_PacketScatterBuffer (qfv_packet_t *packet, VkBuffer dstBuffer,
							  uint32_t count, qfv_scatter_t *scatter,
							  const VkBufferMemoryBarrier2 *srcBarrier,
							  const VkBufferMemoryBarrier2 *dstBarrier);
void QFV_PacketCopyImage (qfv_packet_t *packet, VkImage dstImage,
						  qfv_offset_t imgOffset, qfv_extent_t imgExtent,
						  size_t offset,
						  const VkImageMemoryBarrier2 *srcBarrier,
						  const VkImageMemoryBarrier2 *dstBarrier);
size_t QFV_PacketOffset (qfv_packet_t *packet, void *ptr);
size_t QFV_PacketFullOffset (qfv_packet_t *packet, void *ptr);

#endif//__QF_Vulkan_staging_h
