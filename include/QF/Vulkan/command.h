#ifndef __QF_Vulkan_command_h
#define __QF_Vulkan_command_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"

typedef struct qfv_cmdbufferset_s
	DARRAY_TYPE (VkCommandBuffer) qfv_cmdbufferset_t;

#define QFV_AllocCommandBufferSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_cmdbufferset_t, num, allocator)

typedef struct qfv_semaphoreset_s
	DARRAY_TYPE (VkSemaphore) qfv_semaphoreset_t;

typedef struct qfv_fenceset_s
	DARRAY_TYPE (VkFence) qfv_fenceset_t;

#define QFV_AllocFenceSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_fenceset_t, num, allocator)

typedef struct qfv_bufferimagecopy_s
	DARRAY_TYPE (VkBufferImageCopy) qfv_bufferimagecopy_t;

#define QFV_AllocBufferImageCopy(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_bufferimagecopy_t, num, allocator)

struct qfv_queue_s;
struct qfv_device_s;
VkCommandPool QFV_CreateCommandPool (struct qfv_device_s *device,
									  uint32_t queueFamily,
									  int transient, int reset);
/** Allocate bufferset->size command buffers
 */
int QFV_AllocateCommandBuffers (struct qfv_device_s *device,
								VkCommandPool pool, int secondary,
								qfv_cmdbufferset_t *bufferset);

VkSemaphore QFV_CreateSemaphore (struct qfv_device_s *device);
VkFence QFV_CreateFence (struct qfv_device_s *device, int signaled);
int QFV_QueueSubmit (struct qfv_queue_s *queue,
					 qfv_semaphoreset_t *waitSemaphores,
					 VkPipelineStageFlags *stages,
					 qfv_cmdbufferset_t *buffers,
					 qfv_semaphoreset_t *signalSemaphores, VkFence fence);
int QFV_QueueWaitIdle (struct qfv_queue_s *queue);

typedef struct qfv_push_constants_s {
	VkShaderStageFlags stageFlags;
	uint32_t    offset;
	uint32_t    size;
	const void *data;
} qfv_push_constants_t;

void QFV_PushConstants (struct qfv_device_s *device, VkCommandBuffer cmd,
						VkPipelineLayout layout, uint32_t numPC,
						const qfv_push_constants_t *constants);

#endif//__QF_Vulkan_command_h
