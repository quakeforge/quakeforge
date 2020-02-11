#ifndef __QF_Vulkan_buffer_h
#define __QF_Vulkan_buffer_h

typedef struct qfv_buffer_s {
	struct qfv_device_s *device;
	VkBuffer    buffer;
} qfv_buffer_t;

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

typedef struct qfv_bufferview_s {
	struct qfv_device_s *device;
	VkBufferView view;
	qfv_buffer_t *buffer;
	VkFormat    format;
	VkDeviceSize offset;
	VkDeviceSize size;
} qfv_bufferview_t;

struct qfv_device_s;
qfv_buffer_t *QFV_CreateBuffer (struct qfv_device_s *device,
								VkDeviceSize size,
								VkBufferUsageFlags usage);

struct qfv_memory_s *QFV_AllocBufferMemory (qfv_buffer_t *buffer,
									 VkMemoryPropertyFlags properties,
									 VkDeviceSize size, VkDeviceSize offset);

int QFV_BindBufferMemory (qfv_buffer_t *buffer, struct qfv_memory_s *memory,
						  VkDeviceSize offset);

qfv_bufferbarrierset_t *
QFV_CreateBufferTransitionSet (qfv_buffertransition_t **transitions,
							   int numTransitions);

qfv_bufferview_t *QFV_CreateBufferView (qfv_buffer_t *buffer, VkFormat format,
										VkDeviceSize offset, VkDeviceSize size);

void QFV_DestroyBufferView (qfv_bufferview_t *view);

void QFV_DestroyBuffer (qfv_buffer_t *buffer);

#endif//__QF_Vulkan_buffer_h
