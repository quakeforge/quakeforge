#ifndef __QF_Vulkan_pipeline_h
#define __QF_Vulkan_pipeline_h

#include "QF/darray.h"

typedef struct qfv_pipelinecacheset_s
	DARRAY_TYPE (VkPipelineCache) qfv_pipelinecacheset_t;

#define QFV_AllocPipelineCacheSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_pipelinecacheset_t, num, allocator)

struct dstring_s;
VkPipelineCache QFV_CreatePipelineCache (struct qfv_device_s *device,
										 struct dstring_s *cacheData);
struct dstring_s *QFV_GetPipelineCacheData (struct qfv_device_s *device,
											VkPipelineCache cache);
void QFV_MergePipelineCaches (struct qfv_device_s *device,
							  VkPipelineCache targetCache,
							  qfv_pipelinecacheset_t *sourceCaches);

#endif//__QF_Vulkan_pipeline_h
