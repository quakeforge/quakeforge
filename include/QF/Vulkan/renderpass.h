#ifndef __QF_Vulkan_renderpass_h
#define __QF_Vulkan_renderpass_h

typedef struct qfv_attachmentdescription_s {
	uint32_t    numAttachments;
	VkAttachmentDescription attachments[];
} qfv_attachmentdescription_t;

#define QFV_AllocAttachmentDescription(num, allocator) \
	allocator (field_offset (qfv_attachmentdescription_t, attachments[num]))

typedef struct qfv_attachmentreference_s {
	uint32_t    numReferences;
	VkAttachmentReference references[];
} qfv_attachmentreference_t;

#define QFV_AllocAttachmentReference(num, allocator) \
	allocator (field_offset (qfv_attachmentreference_t, references[num]))

typedef struct qfv_subpassparameters_s {
	VkPipelineBindPoint pipelineBindPoint;
	qfv_attachmentreference_t *inputAttachments;
	qfv_attachmentreference_t *colorAttachments;
	qfv_attachmentreference_t *resolveAttachments;
	VkAttachmentReference *depthStencilAttachment;
	uint32_t    numPreserve;
	uint32_t   *preserveAttachments;
} qfv_subpassparameters_t;

typedef struct qfv_subpassparametersset_s {
	uint32_t    numSubpasses;
	qfv_subpassparameters_t subpasses[];
} qfv_subpassparametersset_t;

#define QFV_AllocSubpassParametersSet(num, allocator) \
	allocator (field_offset (qfv_subpassparametersset_t, subpasses[num]))

typedef struct qfv_subpassdependency_s {
	uint32_t    numDependencies;
	VkSubpassDependency dependencies[];
} qfv_subpassdependency_t;

#define QFV_AllocSubpassDependencies(num, allocator) \
	allocator (field_offset (qfv_subpassdependency_t, dependencies[num]))

typedef struct qfv_renderpass_s {
	struct qfv_device_s *device;
	VkRenderPass renderPass;
} qfv_renderpass_t;

typedef struct qfv_framebuffer_s {
	struct qfv_device_s *device;
	VkFramebuffer framebuffer;
} qfv_framebuffer_t;

qfv_renderpass_t *
QFV_CreateRenderPass (struct qfv_device_s *device,
					  qfv_attachmentdescription_t *attachments,
					  qfv_subpassparametersset_t *subpasses,
					  qfv_subpassdependency_t *dependencies);

struct qfv_imageview_s;
qfv_framebuffer_t *
QFV_CreateFramebuffer (qfv_renderpass_t *renderPass,
					   uint32_t numAttachments,
					   VkImageView *attachments,
					   uint32_t width, uint32_t height, uint32_t layers);

void QFV_DestroyFramebuffer (qfv_framebuffer_t *framebuffer);
void QFV_DestroyRenderPass (qfv_renderpass_t *renderPass);

#endif//__QF_Vulkan_renderpass_h
