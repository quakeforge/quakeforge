#ifndef __QF_Vulkan_render_h
#define __QF_Vulkan_render_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/cexpr.h"
#include "QF/simd/types.h"

typedef struct qfv_reference_s {
	const char *name;
	int         line;
} qfv_reference_t;

typedef struct qfv_imageinfo_s {
	const char *name;
	VkImageCreateFlags flags;
	VkImageType imageType;
	VkFormat    format;
	VkExtent3D  extent;
	uint32_t    mipLevels;
	uint32_t    arrayLayers;
	VkSampleCountFlagBits samples;
	VkImageTiling tiling;
	VkImageUsageFlags usage;
	VkImageLayout initialLayout;
} qfv_imageinfo_t;

typedef struct qfv_imageviewinfo_s {
	const char *name;
	VkImageViewCreateFlags flags;
	//VkImage     image;
	const char *image;
	VkImageViewType viewType;
	VkFormat    format;
	VkComponentMapping components;
	VkImageSubresourceRange subresourceRange;
} qfv_imageviewinfo_t;

typedef struct qfv_dependencymask_s {
	VkPipelineStageFlags stage;
	VkAccessFlags access;
} qfv_dependencymask_t;

typedef struct qfv_dependencyinfo_s {
	const char *name;
	qfv_dependencymask_t src;
	qfv_dependencymask_t dst;
	VkDependencyFlags flags;
} qfv_dependencyinfo_t;

typedef struct qfv_attachmentinfo_s {
	vec4f_t     color;
	const char *name;
	VkAttachmentDescriptionFlags flags;
	VkFormat    format;
	VkSampleCountFlagBits samples;
	VkAttachmentLoadOp loadOp;
	VkAttachmentStoreOp storeOp;
	VkAttachmentLoadOp stencilLoadOp;
	VkAttachmentStoreOp stencilStoreOp;
	VkImageLayout initialLayout;
	VkImageLayout finalLayout;
	VkClearValue clearValue;
} qfv_attachmentinfo_t;

typedef struct qfv_taskinfo_s {
	exprfunc_t *func;
	const exprval_t **params;
	void       *param_data;
} qfv_taskinfo_t;

typedef struct qfv_attachmentrefinfo_s {
	const char *name;
	VkImageLayout layout;
} qfv_attachmentrefinfo_t;

typedef struct qfv_attachmentsetinfo_s {
	uint32_t     num_input;
	qfv_attachmentrefinfo_t *input;
	uint32_t     num_color;
	qfv_attachmentrefinfo_t *color;
	qfv_attachmentrefinfo_t *resolve;
	qfv_attachmentrefinfo_t *depth;
	uint32_t     num_preserve;
	const char **preserve;
} qfv_attachmentsetinfo_t;

typedef struct qfv_pipelineinfo_s {
	vec4f_t     color;
	const char *name;
	qfv_reference_t pipeline;
	uint32_t    num_tasks;
	qfv_taskinfo_t *tasks;
} qfv_pipelineinfo_t;

typedef struct qfv_subpassinfo_s {
	vec4f_t     color;
	const char *name;
	uint32_t    num_dependencies;
	qfv_dependencyinfo_t *dependencies;
	uint32_t    num_attachments;
	qfv_attachmentrefinfo_t *attachments;
	uint32_t    num_pipelines;
	qfv_pipelineinfo_t *pipelines;
} qfv_subpassinfo_t;

typedef struct qfv_framebufferinfo_s {
	qfv_reference_t *attachments;
	uint32_t    num_attachments;
	uint32_t    width;
	uint32_t    height;
	uint32_t    layers;
} qfv_framebufferinfo_t;

typedef struct qfv_renderpassinfo_s {
	const char *name;
	uint32_t    num_attachments;
	qfv_attachmentinfo_t *attachments;
	qfv_framebufferinfo_t framebuffer;
	uint32_t    num_subpasses;
	qfv_subpassinfo_t *subpasses;
} qfv_renderpassinfo_t;

typedef struct qfv_renderinfo_s {
	struct memsuper_s *memsuper;
	uint32_t    num_images;
	qfv_imageinfo_t *images;
	uint32_t    num_views;
	qfv_imageviewinfo_t *views;
	uint32_t    num_renderpasses;
	qfv_renderpassinfo_t *renderpasses;
} qfv_renderinfo_t;

typedef struct qfv_renderctx_s {
	struct hashctx_s *hashctx;
	exprtab_t   task_functions;
} qfv_renderctx_t;

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

typedef struct qfv_subpass_s_ {
	qfv_label_t label;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd;
	uint32_t    pipline_count;
	qfv_pipeline_t *pipelines;
} qfv_subpass_t_;

typedef struct qfv_renderpass_s_ {
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
	qfv_subpass_t_ *subpasses;
} qfv_renderpass_t_;

void QFV_RunRenderPass (qfv_renderpass_t_ *rp, struct vulkan_ctx_s *ctx);
void QFV_LoadRenderPass (struct vulkan_ctx_s *ctx);
void QFV_Render_Init (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_render_h
