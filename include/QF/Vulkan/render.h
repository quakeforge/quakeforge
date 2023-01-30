#ifndef __QF_Vulkan_render_h
#define __QF_Vulkan_render_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/simd/types.h"

typedef struct qfv_label_s {
	vec4f_t     color;
	const char *name;
} qfv_label_t;

typedef struct qfv_bar_s {
	VkBuffer   *buffers;
	VkDeviceSize *offsets;
	uint32_t    firstBinding;
	uint32_t    bindingCount;
} qfv_bar_t;

typedef struct qfv_pipeline_s {
	qfv_label_t label;
	VkPipelineBindPoint bindPoint;
	VkPipeline  pipeline;
	VkPipelineLayout layout;
	VkViewport  viewport;
	VkRect2D    scissor;
	struct qfv_push_constants_s *push_constants;
	uint32_t    num_push_constants;
	uint32_t    num_descriptor_sets;
	uint32_t    first_descriptor_set;
	VkDescriptorSet *descriptor_sets;
} qfv_pipeline_t;

typedef struct qfv_subpass_s {
	qfv_label_t label;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd;
	uint32_t    pipline_count;
	qfv_pipeline_t *pipelines;
} qfv_subpass_t;

typedef struct qfv_renderpass_s {
	struct vulkan_ctx_s *vulkan_ctx;
	qfv_label_t label;		// for debugging

	VkCommandBuffer cmd;
	VkRenderPassBeginInfo beginInfo;
	VkSubpassContents subpassContents;

	//struct qfv_imageset_s *attachment_images;
	//struct qfv_imageviewset_s *attachment_views;
	//VkDeviceMemory attachmentMemory;
	//size_t attachmentMemory_size;
	//qfv_output_t output;

	uint32_t    subpass_count;
	qfv_subpass_t *subpasses;
} qfv_renderpass_t;

void QFV_RunRenderPass (qfv_renderpass_t *rp, struct vulkan_ctx_s *ctx)m

#endif//__QF_Vulkan_render_h
