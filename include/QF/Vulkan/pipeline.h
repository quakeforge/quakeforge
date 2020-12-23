#ifndef __QF_Vulkan_pipeline_h
#define __QF_Vulkan_pipeline_h

#include "QF/darray.h"

typedef struct qfv_pipelineshaderstateset_s
	DARRAY_TYPE (VkPipelineShaderStageCreateInfo) qfv_pipelineshaderstateset_s;

#define QFV_AllocPipelineShaderStageSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_pipelineshaderstateset_s, num, allocator)

typedef struct qfv_vertexinputbindingset_s
	DARRAY_TYPE (VkVertexInputBindingDescription)  qfv_vertexinputbindingset_t;

#define QFV_AllocVertexInputBindingSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_vertexinputbindingset_t, num, allocator)

typedef struct qfv_vertexinputattributeset_s
	DARRAY_TYPE (VkVertexInputAttributeDescription)
		qfv_vertexinputattributeset_t;

#define QFV_AllocVertexInputAttributeSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_vertexinputattributeset_t, num, allocator)

typedef struct qfv_vertexinputstate_s {
	qfv_vertexinputbindingset_t *bindings;
	qfv_vertexinputattributeset_t *attributes;
} qfv_vertexinputstate_t;

typedef struct qfv_pipelineinputassembly_s {
	VkPrimitiveTopology topology;
	VkBool32    primativeRestartEnable;
} qfv_pipelineinputassembly_t;

typedef struct qfv_pipelinetessellation_s {
	uint32_t    patchControlPoints;
} qfv_pipelinetessellation_t;

typedef struct qfv_viewportset_s DARRAY_TYPE (VkViewport) qfv_viewportset_t;

#define QFV_AllocViewportSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_viewportset_t, num, allocator)

typedef struct qfv_scissorsset_s DARRAY_TYPE (VkRect2D) qfv_scissorsset_t;

#define QFV_AllocScissorsSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_scissorsset_t, num, allocator)

typedef struct qfv_viewportinfo_s {
	qfv_viewportset_t *viewportset;
	qfv_scissorsset_t *scissorsset;
} qfv_viewportinfo_t;

typedef struct qfv_pipelinerasterization_s {
	VkBool32    depthClampEnable;
	VkBool32    rasterizerDiscardEnable;
	VkPolygonMode polygonMode;
	VkCullModeFlags cullMode;
	VkFrontFace frontFace;
	VkBool32    depthBiasEnable;
	float       depthBiasConstantFactor;
	float       depthBiasClamp;
	float       depthBiasSlopeFactor;
	float       lineWidth;
} qfv_pipelinerasterization_t;

typedef struct qfv_pipelinemultisample_s {
	VkSampleCountFlagBits rasterizationSamples;
	VkBool32    sampleShadingEnable;
	float       minSampleShading;
	const VkSampleMask *sampleMask;
	VkBool32    alphaToCoverageEnable;
	VkBool32    alphaToOneEnable;
} qfv_pipelinemultisample_t;

typedef struct qfv_pipelinedepthandstencil_s {
	VkBool32    depthTestEnable;
	VkBool32    depthWriteEnable;
	VkCompareOp depthCompareOp;
	VkBool32    depthBoundsTestEnable;
	VkBool32    stencilTestEnable;
	VkStencilOpState front;
	VkStencilOpState back;
	float       minDepthBounds;
	float       maxDepthBounds;
} qfv_pipelinedepthandstencil_t;

typedef struct qfv_blendattachmentset_s
	DARRAY_TYPE (VkPipelineColorBlendAttachmentState) qfv_blendattachmentset_t;

#define QFV_AllocBlendAttachmentSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_blendattachmentset_t, num, allocator)

typedef struct qfv_dynamicstateset_s
	DARRAY_TYPE (VkDynamicState) qfv_dynamicstateset_t;

#define QFV_AllocDynamicStateSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_dynamicstateset_t, num, allocator)

typedef struct qfv_pushconstantrangeset_s
	DARRAY_TYPE (VkPushConstantRange) qfv_pushconstantrangeset_t;

#define QFV_AllocPushConstantRangeSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_pushconstantrangeset_t, num, allocator)

typedef struct qfv_pipelineset_s DARRAY_TYPE (VkPipeline) qfv_pipelineset_t;

#define QFV_AllocPipelineSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_pipelineset_t, num, allocator)

typedef struct qfv_graphicspipelinecreateinfoset_s
	DARRAY_TYPE (VkGraphicsPipelineCreateInfo)
		qfv_graphicspipelinecreateinfoset_t;

#define QFV_AllocGraphicsPipelineCreateInfoSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_graphicspipelinecreateinfoset_t, num, allocator)

typedef struct qfv_computepipelinecreateinfoset_s
	DARRAY_TYPE (VkComputePipelineCreateInfo)
		qfv_computepipelinecreateinfoset_t;

#define QFV_AllocComputePipelineCreateInfoSet(num, allocator) \
    DARRAY_ALLOCFIXED (qfv_computepipelinecreateinfoset_t, num, allocator)

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

struct qfv_descriptorsetlayoutset_s;
VkPipelineLayout
QFV_CreatePipelineLayout (struct qfv_device_s *device,
						  struct qfv_descriptorsetlayoutset_s *layouts,
						  qfv_pushconstantrangeset_t *pushConstants);
qfv_pipelineset_t *
QFV_CreateGraphicsPipelines (struct qfv_device_s *device,
							 VkPipelineCache cache,
							 qfv_graphicspipelinecreateinfoset_t *gpciSet);
qfv_pipelineset_t *
QFV_CreateComputePipelines (struct qfv_device_s *device,
							VkPipelineCache cache,
							qfv_computepipelinecreateinfoset_t *cpciSet);

#endif//__QF_Vulkan_pipeline_h
