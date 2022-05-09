#ifndef __QF_Vulkan_renderpass_h
#define __QF_Vulkan_renderpass_h

#include "QF/darray.h"
#include "QF/simd/types.h"

typedef struct qfv_attachmentdescription_s
	DARRAY_TYPE (VkAttachmentDescription) qfv_attachmentdescription_t;

#define QFV_AllocAttachmentDescription(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_attachmentdescription_t, num, allocator)

typedef struct qfv_attachmentreference_s
	DARRAY_TYPE (VkAttachmentReference) qfv_attachmentreference_t;

#define QFV_AllocAttachmentReference(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_attachmentreference_t, num, allocator)

typedef struct qfv_subpassparametersset_s
	DARRAY_TYPE (VkSubpassDescription) qfv_subpassparametersset_t;

#define QFV_AllocSubpassParametersSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_subpassparametersset_t, num, allocator)

typedef struct qfv_subpassdependency_s
	DARRAY_TYPE (VkSubpassDependency) qfv_subpassdependency_t;

#define QFV_AllocSubpassDependencies(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_subpassdependency_t, num, allocator)

typedef struct qfv_framebufferset_s
	DARRAY_TYPE (VkFramebuffer) qfv_framebufferset_t;

#define QFV_AllocFrameBuffers(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_framebufferset_t, num, allocator)

struct qfv_device_s;
struct qfv_imageviewset_s;
VkRenderPass
QFV_CreateRenderPass (struct qfv_device_s *device,
					  qfv_attachmentdescription_t *attachments,
					  qfv_subpassparametersset_t *subpasses,
					  qfv_subpassdependency_t *dependencies);

VkFramebuffer
QFV_CreateFramebuffer (struct qfv_device_s *device,
					   VkRenderPass renderPass,
					   struct qfv_imageviewset_s *attachments,
					   VkExtent2D, uint32_t layers);

typedef struct qfv_subpass_s {
	vec4f_t     color;
	const char *name;
} qfv_subpass_t;

typedef struct qfv_renderframe_s {
	struct vulkan_ctx_s *vulkan_ctx;
	struct qfv_renderpass_s *renderpass;
	VkSubpassContents subpassContents;
	int subpassCount;
	qfv_subpass_t *subpassInfo;
	struct qfv_cmdbufferset_s *subpassCmdSets;
} qfv_renderframe_t;

typedef struct qfv_renderframeset_s
	DARRAY_TYPE (qfv_renderframe_t) qfv_renderframeset_t;

typedef struct clearvalueset_s
	DARRAY_TYPE (VkClearValue) clearvalueset_t;

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

	qfv_renderframeset_t frames;

	void      (*draw) (qfv_renderframe_t *rFrame);
} qfv_renderpass_t;

#endif//__QF_Vulkan_renderpass_h
