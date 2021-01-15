#ifndef __QF_Vulkan_staging_h
#define __QF_Vulkan_staging_h

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
	qfv_packet_t *packet;	///< array of packets for controlling access
	size_t      num_packets;///< number of packets in array
	size_t      next_packet;///< index of the next packet to be used
	size_t      size;		///< actual size of the buffer
	size_t      end;		///< effective end of the buffer due to early wrap
	size_t      head;
	size_t      tail;
	void       *data;
} qfv_stagebuf_t;


qfv_stagebuf_t *QFV_CreateStagingBuffer (struct qfv_device_s *device,
										 size_t size, int num_packets,
										 VkCommandPool cmdPool);
void QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage);
void QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size);
qfv_packet_t *QFV_PacketAcquire (qfv_stagebuf_t *stage);
void *QFV_PacketExtend (qfv_packet_t *packet, size_t size);
void QFV_PacketSubmit (qfv_packet_t *packet);

#endif//__QF_Vulkan_staging_h
