#ifndef __QF_Vulkan_renderpass_h
#define __QF_Vulkan_renderpass_h

#include "QF/darray.h"
#include "QF/simd/types.h"

typedef struct qfv_framebufferset_s
	DARRAY_TYPE (VkFramebuffer) qfv_framebufferset_t;

#define QFV_AllocFrameBuffers(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_framebufferset_t, num, allocator)

typedef struct qfv_subpass_s {
	vec4f_t     color;
	const char *name;
} qfv_subpass_t;

typedef struct qfv_subpassset_s
	DARRAY_TYPE (qfv_subpass_t) qfv_subpassset_t;

typedef struct qfv_renderframe_s {
	struct vulkan_ctx_s *vulkan_ctx;
	struct qfv_renderpass_s *renderpass;
	VkSubpassContents subpassContents;
	int         subpassCount;
	qfv_subpass_t *subpassInfo;
	struct qfv_cmdbufferset_s *subpassCmdSets;
} qfv_renderframe_t;

typedef struct qfv_renderframeset_s
	DARRAY_TYPE (qfv_renderframe_t) qfv_renderframeset_t;

typedef struct clearvalueset_s
	DARRAY_TYPE (VkClearValue) clearvalueset_t;

typedef void (*qfv_draw_t) (qfv_renderframe_t *rFrame);

typedef struct qfv_renderpass_s {
	vec4f_t     color;		// for debugging
	const char *name;		// for debugging
	struct plitem_s  *renderpassDef;
	VkRenderPass renderpass;
	clearvalueset_t *clearValues;
	struct qfv_imageset_s *attachment_images;
	struct qfv_imageviewset_s *attachment_views;
	VkDeviceMemory attachmentMemory;

	qfv_framebufferset_t *framebuffers;
	VkViewport  viewport;
	VkRect2D    scissor;
	int         order;
	int         primary_commands;
	size_t      subpassCount;
	qfv_subpassset_t *subpass_info;
	qfv_renderframeset_t frames;

	qfv_draw_t  draw;
} qfv_renderpass_t;

struct qfv_output_s;
qfv_renderpass_t *Vulkan_CreateRenderPass (struct vulkan_ctx_s *ctx,
										   const char *name,
										   struct qfv_output_s *output,
										   qfv_draw_t draw);
void Vulkan_DestroyRenderPass (struct vulkan_ctx_s *ctx,
							   qfv_renderpass_t *renderpass);
void Vulkan_CreateAttachments (struct vulkan_ctx_s *ctx,
							   qfv_renderpass_t *renderpass);

#endif//__QF_Vulkan_renderpass_h
