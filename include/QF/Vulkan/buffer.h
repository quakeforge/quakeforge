#ifndef __QF_Vulkan_buffer_h
#define __QF_Vulkan_buffer_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"

typedef struct qfv_buffertransition_s {
	VkBuffer    buffer;
	VkAccessFlags srcAccess;
	VkAccessFlags dstAccess;
	uint32_t    srcQueueFamily;
	uint32_t    dstQueueFamily;
	VkDeviceSize offset;
	VkDeviceSize size;
} qfv_buffertransition_t;

typedef struct qfv_buffertransitionset_s
	DARRAY_TYPE (qfv_buffertransition_t) qfv_buffertransitionset_t;
typedef struct qfv_bufferbarrierset_s
	DARRAY_TYPE (VkBufferMemoryBarrier) qfv_bufferbarrierset_t;

typedef struct qfv_bufferset_s
	DARRAY_TYPE (VkBuffer) qfv_bufferset_t;
#define QFV_AllocBufferSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_bufferset_t, num, allocator)

struct qfv_device_s;
VkBuffer QFV_CreateBuffer (struct qfv_device_s *device,
						   VkDeviceSize size,
						   VkBufferUsageFlags usage);

VkDeviceMemory QFV_AllocBufferMemory (struct qfv_device_s *device,
									  VkBuffer buffer,
									  VkMemoryPropertyFlags properties,
									  VkDeviceSize size, VkDeviceSize offset);

int QFV_BindBufferMemory (struct qfv_device_s *device,
						  VkBuffer buffer, VkDeviceMemory object,
						  VkDeviceSize offset);

qfv_bufferbarrierset_t *
QFV_CreateBufferTransitions (qfv_buffertransition_t *transitions,
							 int numTransitions);

VkBufferView QFV_CreateBufferView (struct qfv_device_s *device,
								   VkBuffer buffer, VkFormat format,
								   VkDeviceSize offset, VkDeviceSize size);

VkDeviceSize QFV_NextOffset (VkDeviceSize current,
							 const VkMemoryRequirements *requirements)
	__attribute__((pure));

#endif//__QF_Vulkan_buffer_h
