#ifndef __QF_Vulkan_memory_h
#define __QF_Vulkan_memory_h

typedef struct qfv_memory_s {
	struct qfv_device_s *device;
	VkDeviceMemory object;
} qfv_memory_t;

#endif//__QF_Vulkan_memory_h
