#ifndef __QF_Vulkan_pipeline_h
#define __QF_Vulkan_pipeline_h

typedef struct qfv_shadermodule_s {
	struct qfv_device_s *device;
	VkShaderModule module;
} qfv_shadermodule_t;

typedef struct qfv_shaderstageparams_s {
	VkShaderStageFlagBits stageFlags;
	qfv_shadermodule_t *module;
	const char *entryPointName;
	const VkSpecializationInfo *specializationInfo;
} qfv_shaderstageparams_t;

typedef struct qfv_shaderstageparamsset_s {
	uint32_t    numParams;
	qfv_shaderstageparams_t params[];
} qfv_shaderstageparamsset_t;

#define QFV_AllocShaderParamsSet(num, allocator) \
    allocator (field_offset (qfv_shaderstageparamsset_t, params[num]))

typedef struct qfv_vertexinputbindingset_s {
	uint32_t    numBindings;
	VkVertexInputBindingDescription bindings[];
} qfv_vertexinputbindingset_t;

#define QFV_AllocVertexInputBindingSet(num, allocator) \
    allocator (field_offset (qfv_vertexinputbindingset_t, bindings[num]))

typedef struct qfv_vertexinputattributeset_s {
	uint32_t    numAttributes;
	VkVertexInputAttributeDescription attributes[];
} qfv_vertexinputattributeset_t;

#define QFV_AllocVertexInputAttributeSet(num, allocator) \
    allocator (field_offset (qfv_vertexinputattributeset_t, bindings[num]))

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

typedef struct qfv_viewportset_s {
	uint32_t    numViewports;
	VkViewport  viewports[];
} qfv_viewportset_t;

#define QFV_AllocViewportSet(num, allocator) \
    allocator (field_offset (qfv_viewportset_t, viewports[num]))

typedef struct qfv_scissorsset_s {
	uint32_t    numScissors;
	VkRect2D    scissors[];
} qfv_scissorsset_t;

#define QFV_AllocScissorsSet(num, allocator) \
    allocator (field_offset (qfv_scissorsset_t, scissors[num]))

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

typedef struct qfv_blendattachmentset_s {
	uint32_t    numAttachments;
	VkPipelineColorBlendAttachmentState attachments[];
} qfv_blendattachmentset_t;

#define QFV_AllocBlendAttachmentSet(num, allocator) \
    allocator (field_offset (qfv_blendattachmentset_t, attachments[num]))

typedef struct qfv_pipelineblend_s {
	VkBool32    logicOpEnable;
	VkLogicOp   logicOp;
	qfv_blendattachmentset_t *blendAttachments;
	float       blendConstants[4];
} qfv_pipelineblend_t;

typedef struct qfv_dynamicstateset_s {
	uint32_t    numStates;
	VkDynamicState states[];
} qfv_dynamicstateset_t;

#define QFV_AllocDynamicStateSet(num, allocator) \
    allocator (field_offset (qfv_dynamicstateset_t, states[num]))

typedef struct qfv_pushconstantrangeset_s {
	uint32_t    numRanges;
	VkPushConstantRange ranges[];
} qfv_pushconstantrangeset_t;

#define QFV_AllocPushConstantRangeSet(num, allocator) \
    allocator (field_offset (qfv_pushconstantrangeset_t, ranges[num]))

typedef struct qfv_pipelinelayout_s {
	struct qfv_device_s *device;
	VkPipelineLayout  layout;
} qfv_pipelinelayout_t;

typedef struct qfv_pipeline_s {
	struct qfv_device_s *device;
	VkPipeline  pipeline;
} qfv_pipeline_t;

typedef struct qfv_pipelineset_s {
	uint32_t    numPipelines;
	qfv_pipeline_t *pipelines[];
} qfv_pipelineset_t;

#define QFV_AllocPipelineSet(num, allocator) \
    allocator (field_offset (qfv_pipelineset_t, pipelines[num]))

typedef struct qfv_graphicspipelinecreateinfo_s {
	VkPipelineCreateFlags flags;
	qfv_shaderstageparamsset_t *stages;
	qfv_vertexinputstate_t *vertexState;
	qfv_pipelineinputassembly_t *inputAssemblyState;
	qfv_pipelinetessellation_t *tessellationState;
	qfv_viewportinfo_t *viewportState;
	qfv_pipelinerasterization_t *rasterizationState;
	qfv_pipelinemultisample_t *multisampleState;
	qfv_pipelinedepthandstencil_t *depthStencilState;
	qfv_pipelineblend_t *colorBlendState;
	qfv_dynamicstateset_t *dynamicState;
	qfv_pipelinelayout_t *layout;
	struct qfv_renderpass_s *renderPass;
	uint32_t    subpass;
	qfv_pipeline_t *basePipeline;
	int32_t     basePipelineIndex;
} qfv_graphicspipelinecreateinfo_t;

typedef struct qfv_graphicspipelinecreateinfoset_s {
	uint32_t    numPipelines;
	qfv_graphicspipelinecreateinfo_t *pipelines[];
} qfv_graphicspipelinecreateinfoset_t;

#define QFV_AllocGraphicsPipelineCreateInfoSet(num, allocator) \
    allocator (field_offset (qfv_graphicspipelinecreateinfoset_t, \
							 pipelines[num]))

typedef struct qfv_computepipelinecreateinfo_s {
	VkPipelineCreateFlags flags;
	qfv_shaderstageparams_t *stage;
	qfv_pipelinelayout_t *layout;
	qfv_pipeline_t *basePipeline;
	int32_t     basePipelineIndex;
} qfv_computepipelinecreateinfo_t;

typedef struct qfv_computepipelinecreateinfoset_s {
	uint32_t    numPipelines;
	qfv_computepipelinecreateinfo_t *pipelines[];
} qfv_computepipelinecreateinfoset_t;

#define QFV_AllocComputePipelineCreateInfoSet(num, allocator) \
    allocator (field_offset (qfv_computepipelinecreateinfoset_t, \
							 pipelines[num]))

typedef struct qfv_pipelinecache_s {
	struct qfv_device_s *device;
	VkPipelineCache cache;
} qfv_pipelinecache_t;

typedef struct qfv_pipelinecacheset_s {
	uint32_t    numCaches;
	qfv_pipelinecache_t *caches[];
} qfv_pipelinecacheset_t;

#define QFV_AllocPipelineCacheSet(num, allocator) \
    allocator (field_offset (qfv_pipelinecacheset_t, caches[num]))

qfv_shadermodule_t *QFV_CreateShaderModule (struct qfv_device_s *device,
											size_t size, const uint32_t *code);
void QFV_DestroyShaderModule (qfv_shadermodule_t *module);

struct dstring_s;
qfv_pipelinecache_t *QFV_CreatePipelineCache (struct qfv_device_s *device,
											  struct dstring_s *cacheData);
struct dstring_s *QFV_GetPipelineCacheData (qfv_pipelinecache_t *cache);
void QFV_MergePipelineCaches (qfv_pipelinecache_t *targetCache,
							  qfv_pipelinecacheset_t *sourceCaches);
void QFV_DestroyPipelineCache (qfv_pipelinecache_t *cache);

struct qfv_descriptorsetlayoutset_s;
qfv_pipelinelayout_t *
QFV_CreatePipelineLayout (struct qfv_device_s *device,
						  struct qfv_descriptorsetlayoutset_s *layouts,
						  qfv_pushconstantrangeset_t *pushConstants);
void QFV_DestroyPipelineLayout (qfv_pipelinelayout_t *layout);
qfv_pipelineset_t *
QFV_CreateGraphicsPipelines (struct qfv_device_s *device,
							 qfv_pipelinecache_t *cache,
							 qfv_graphicspipelinecreateinfoset_t *gpciSet);
qfv_pipelineset_t *
QFV_CreateComputePipelines (struct qfv_device_s *device,
							qfv_pipelinecache_t *cache,
							qfv_computepipelinecreateinfoset_t *cpciSet);
void QFV_DestroyPipeline (qfv_pipeline_t *pipeline);

#endif//__QF_Vulkan_pipeline_h
