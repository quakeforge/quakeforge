#ifndef __QF_Vulkan_buffer_h
#define __QF_Vulkan_buffer_h

typedef struct qfv_buffer_s {
	struct qfv_device_s *device;
	VkBuffer    buffer;
} qfv_buffer_t;

typedef struct qfv_memory_s {
	struct qfv_device_s *device;
	VkDeviceMemory object;
} qfv_memory_t;

typedef struct qfv_buffertransition_s {
	qfv_buffer_t *buffer;
	VkAccessFlags srcAccess;
	VkAccessFlags dstAccess;
	uint32_t      srcQueueFamily;
	uint32_t      dstQueueFamily;
	VkDeviceSize  offset;
	VkDeviceSize  size;
} qfv_buffertransition_t;

typedef struct qfv_bufferbarrierset_s {
	struct qfv_device_s *device;
	uint32_t    numBarriers;
	VkBufferMemoryBarrier *barriers;
} qfv_bufferbarrierset_t;

struct qfv_device_s;
qfv_buffer_t *QFV_CreateBuffer (struct qfv_device_s *device,
								VkDeviceSize size,
								VkBufferUsageFlags usage);

qfv_memory_t *QFV_AllocMemory (qfv_buffer_t *buffer,
							   VkMemoryPropertyFlags properties,
							   VkDeviceSize size, VkDeviceSize offset);

int QFV_BindBufferMemory (qfv_buffer_t *buffer, qfv_memory_t *memory,
						  VkDeviceSize offset);

qfv_bufferbarrierset_t *
QFV_CreateBufferTransitionSet (qfv_buffertransition_t **transitions,
							   int numTransitions);


#endif//__QF_Vulkan_buffer_h
