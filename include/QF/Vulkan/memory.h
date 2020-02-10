#ifndef __QF_Vulkan_memory_h
#define __QF_Vulkan_memory_h

typedef struct qfv_memory_s {
	struct qfv_device_s *device;
	VkDeviceMemory object;
} qfv_memory_t;

typedef struct qfv_mappedmemrange_s {
	qfv_memory_t *memory;
	VkDeviceSize offset;
	VkDeviceSize size;
} qfv_mappedmemrange_t;

void *QFV_MapMemory (qfv_memory_t *memory,
					 VkDeviceSize offset, VkDeviceSize size);
void QFV_UnmapMemory (qfv_memory_t *memory);
void QFV_FlushMemory (qfv_mappedmemrange_t *ranges, uint32_t numRanges);

#endif//__QF_Vulkan_memory_h
