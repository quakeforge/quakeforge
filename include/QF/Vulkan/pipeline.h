#ifndef __QF_Vulkan_pipeline_h
#define __QF_Vulkan_pipeline_h

#include "QF/darray.h"

typedef struct qfv_pipelinecacheset_s
	DARRAY_TYPE (VkPipelineCache) qfv_pipelinecacheset_t;

#define QFV_AllocPipelineCacheSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_pipelinecacheset_t, num, allocator)

typedef struct qfv_device_s qfv_device_t;
typedef struct dstring_s dstring_t;
VkPipelineCache QFV_CreatePipelineCache (qfv_device_t *device,
										 dstring_t *cacheData);
bool QFV_GetPipelineCacheData (qfv_device_t *device, VkPipelineCache cache,
							   dstring_t *cacheData);
void QFV_MergePipelineCaches (qfv_device_t *device,
							  VkPipelineCache targetCache,
							  qfv_pipelinecacheset_t *sourceCaches);

#endif//__QF_Vulkan_pipeline_h
