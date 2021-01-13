#ifndef __QF_Vulkan_staging_h
#define __QF_Vulkan_staging_h

typedef struct qfv_stagebuf_s {
	struct qfv_device_s *device;
	VkBuffer    buffer;
	VkDeviceMemory memory;
	size_t      size;
	size_t      offset;		///< for batching building
	void       *data;
} qfv_stagebuf_t;


qfv_stagebuf_t *QFV_CreateStagingBuffer (struct qfv_device_s *device,
										 size_t size);
void QFV_DestroyStagingBuffer (qfv_stagebuf_t *stage);
void QFV_FlushStagingBuffer (qfv_stagebuf_t *stage, size_t offset, size_t size);
void *QFV_ClaimStagingBuffer (qfv_stagebuf_t *stage, size_t size);

#endif//__QF_Vulkan_staging_h
