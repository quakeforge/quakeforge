#ifndef __QF_Vulkan_memory_h
#define __QF_Vulkan_memory_h

#include "QF/darray.h"

typedef struct qfv_mappedmemrange_s {
	VkDeviceMemory object;
	VkDeviceSize offset;
	VkDeviceSize size;
} qfv_mappedmemrange_t;

typedef struct qfv_mappedmemrangeset_s
	DARRAY_TYPE (qfv_mappedmemrange_t) qfv_mappedmemrangeset_t;

struct qfv_device_s;

void *QFV_MapMemory (struct qfv_device_s *device, VkDeviceMemory object,
					 VkDeviceSize offset, VkDeviceSize size);
void QFV_FlushMemory (struct qfv_device_s *device,
					  qfv_mappedmemrangeset_t *ranges);

#endif//__QF_Vulkan_memory_h
