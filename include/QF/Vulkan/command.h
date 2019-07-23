#ifndef __QF_Vulkan_command_h
#define __QF_Vulkan_command_h

typedef struct qfv_cmdpool_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkCommandPool cmdpool;
} qfv_cmdpool_t;

typedef struct qfv_cmdbuffer_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkCommandBuffer buffer;
} qfv_cmdbuffer_t;

typedef struct qfv_cmdbufferset_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkCommandBuffer *buffers;
	int         numBuffers;
} qfv_cmdbufferset_t;

typedef struct qfv_semaphore_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkSemaphore semaphore;
} qfv_semaphore_t;

typedef struct qfv_semaphoreset_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkSemaphore *semaphores;
	VkPipelineStageFlags *stages;
	int         numSemaphores;
} qfv_semaphoreset_t;

typedef struct qfv_fence_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkFence     fence;
} qfv_fence_t;

typedef struct qfv_fenceset_s {
	VkDevice    dev;
	struct qfv_devfuncs_s *funcs;
	VkFence    *fences;
	int         numFences;
} qfv_fenceset_t;

struct qfv_device_s;
qfv_cmdpool_t *QFV_CreateCommandPool (struct qfv_device_s *device,
									  uint32_t queueFamily,
									  int transient, int reset);
int QFV_ResetCommandPool (qfv_cmdpool_t *pool, int release);
qfv_cmdbuffer_t *QFV_AllocateCommandBuffers (qfv_cmdpool_t *pool,
											 int secondary, int count);
qfv_cmdbufferset_t *QFV_CreateCommandBufferSet (qfv_cmdbuffer_t **buffers,
												int numBuffers);
int QFV_BeginCommandBuffer (qfv_cmdbuffer_t *buffer, int oneTime,
							int rpContinue, int simultaneous,
							VkCommandBufferInheritanceInfo *inheritanceInfo);
int QFV_EndCommandBuffer (qfv_cmdbuffer_t *buffer);
int QFV_ResetCommandBuffer (qfv_cmdbuffer_t *buffer, int release);

qfv_semaphore_t *QFV_CreateSemaphore (struct qfv_device_s *device);
qfv_semaphoreset_t *QFV_CreateSemaphoreSet (qfv_semaphore_t **semaphores,
											int numSemaphores);

qfv_fence_t *QFV_CreateFence (struct qfv_device_s *device, int signaled);
qfv_fenceset_t *QFV_CreateFenceSet (qfv_fence_t **fences, int numFences);
int QFV_WaitForFences (qfv_fenceset_t *fences, int all, uint64_t timeout);
int QFV_ResetFences (qfv_fenceset_t *fences);
int QFV_QueueSubmit (struct qfv_device_s *device,
					 qfv_semaphoreset_t *waitSemaphores,
					 qfv_cmdbufferset_t *buffers,
					 qfv_semaphoreset_t *signalSemaphores, qfv_fence_t *fence);


#endif//__QF_Vulkan_command_h
