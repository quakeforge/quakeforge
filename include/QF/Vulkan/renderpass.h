#ifndef __QF_Vulkan_renderpass_h
#define __QF_Vulkan_renderpass_h

#include "QF/darray.h"

typedef struct qfv_attachmentdescription_s
	DARRAY_TYPE (VkAttachmentDescription) qfv_attachmentdescription_t;

#define QFV_AllocAttachmentDescription(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_attachmentdescription_t, num, allocator)

typedef struct qfv_attachmentreference_s
	DARRAY_TYPE (VkAttachmentReference) qfv_attachmentreference_t;

#define QFV_AllocAttachmentReference(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_attachmentreference_t, num, allocator)

typedef struct qfv_subpassparameters_s {
	VkPipelineBindPoint pipelineBindPoint;
	qfv_attachmentreference_t *inputAttachments;
	qfv_attachmentreference_t *colorAttachments;
	qfv_attachmentreference_t *resolveAttachments;
	VkAttachmentReference *depthStencilAttachment;
	uint32_t    numPreserve;
	uint32_t   *preserveAttachments;
} qfv_subpassparameters_t;

typedef struct qfv_subpassparametersset_s
	DARRAY_TYPE (qfv_subpassparameters_t) qfv_subpassparametersset_t;

#define QFV_AllocSubpassParametersSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_subpassparametersset_t, num, allocator)

typedef struct qfv_subpassdependency_s
	DARRAY_TYPE (VkSubpassDependency) qfv_subpassdependency_t;

#define QFV_AllocSubpassDependencies(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_subpassdependency_t, num, allocator)

struct qfv_device_s;
VkRenderPass
QFV_CreateRenderPass (struct qfv_device_s *device,
					  qfv_attachmentdescription_t *attachments,
					  qfv_subpassparametersset_t *subpasses,
					  qfv_subpassdependency_t *dependencies);

VkFramebuffer
QFV_CreateFramebuffer (struct qfv_device_s *device,
					   VkRenderPass renderPass,
					   uint32_t numAttachments,
					   VkImageView *attachments,
					   uint32_t width, uint32_t height, uint32_t layers);

#endif//__QF_Vulkan_renderpass_h
