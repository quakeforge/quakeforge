#ifndef __QF_Vulkan_render_h
#define __QF_Vulkan_render_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/cexpr.h"
#include "QF/simd/types.h"

#ifndef __QFCC__
#include "QF/darray.h"
#include "QF/Vulkan/command.h"
#endif

typedef struct qfv_output_s {
	VkExtent2D  extent;
	VkImage     image;		// only if owned
	VkImageView view;
	VkFormat    format;
	uint32_t    frames;
	VkImageView *view_list;	// per frame
	VkImageLayout finalLayout;
} qfv_output_t;

typedef struct qfv_reference_s {
	const char *name;
	int         line;
} qfv_reference_t;

typedef struct qfv_descriptorsetlayoutinfo_s {
	const char *name;
	VkDescriptorSetLayoutCreateFlags flags;
	uint32_t    num_bindings;
	VkDescriptorSetLayoutBinding *bindings;
	VkDescriptorSetLayout setLayout;
} qfv_descriptorsetlayoutinfo_t;

typedef enum qfv_type_t {
	qfv_float,
	qfv_int,
	qfv_uint,
	qfv_vec3,
	qfv_vec4,
	qfv_mat4,
} qfv_type_t;

typedef struct qfv_pushconstantinfo_s {
	const char *name;
	int         line;
	qfv_type_t  type;
	uint32_t    offset;
	uint32_t    size;
} qfv_pushconstantinfo_t;

typedef struct qfv_pushconstantrangeinfo_s {
	VkShaderStageFlags stageFlags;
	uint32_t    num_pushconstants;
	qfv_pushconstantinfo_t *pushconstants;
} qfv_pushconstantrangeinfo_t;

typedef struct qfv_layoutinfo_s {
	const char *name;
	uint32_t    num_sets;
	qfv_reference_t *sets;
	uint32_t    num_pushconstantranges;
	qfv_pushconstantrangeinfo_t *pushconstantranges;
	VkPipelineLayout layout;
} qfv_layoutinfo_t;

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
	struct qfv_resobj_s *object;
} qfv_imageinfo_t;

typedef struct qfv_imageviewinfo_s {
	const char *name;
	VkImageViewCreateFlags flags;
	qfv_reference_t image;
	VkImageViewType viewType;
	VkFormat    format;
	VkComponentMapping components;
	VkImageSubresourceRange subresourceRange;
	struct qfv_resobj_s *object;
} qfv_imageviewinfo_t;

typedef struct qfv_bufferinfo_s {
	const char *name;
	VkBufferCreateFlags flags;
	VkDeviceSize size;
	VkBufferUsageFlags usage;
	VkSharingMode sharingMode;
} qfv_bufferinfo_t;

typedef struct qfv_bufferviewinfo_s {
	const char *name;
	VkBufferViewCreateFlags flags;
	qfv_reference_t buffer;
	VkFormat    format;
	VkDeviceSize offset;
	VkDeviceSize range;
} qfv_bufferviewinfo_t;

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
	const char *name;
	int         line;
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
	qfv_reference_t view;
	const char *external;
} qfv_attachmentinfo_t;

typedef struct qfv_taskinfo_s {
	exprfunc_t *func;
	const exprval_t **params;
	void       *param_data;
} qfv_taskinfo_t;

typedef struct qfv_attachmentrefinfo_s {
	const char *name;
	int         line;
	VkImageLayout layout;
	VkPipelineColorBlendAttachmentState blend;
} qfv_attachmentrefinfo_t;

typedef struct qfv_attachmentsetinfo_s {
	uint32_t     num_input;
	qfv_attachmentrefinfo_t *input;
	uint32_t     num_color;
	qfv_attachmentrefinfo_t *color;
	qfv_attachmentrefinfo_t *resolve;
	qfv_attachmentrefinfo_t *depth;
	uint32_t     num_preserve;
	qfv_reference_t *preserve;
} qfv_attachmentsetinfo_t;

typedef struct qfv_pipelineinfo_s {
	vec4f_t     color;
	const char *name;
	bool        disabled;
	uint32_t    num_tasks;
	qfv_taskinfo_t *tasks;

	VkPipelineCreateFlags flags;
	VkPipelineShaderStageCreateInfo *compute_stage;
	uint32_t    dispatch[3];

	uint32_t    num_graph_stages;
	const VkPipelineShaderStageCreateInfo *graph_stages;
	const VkPipelineVertexInputStateCreateInfo *vertexInput;
	const VkPipelineInputAssemblyStateCreateInfo *inputAssembly;
	const VkPipelineTessellationStateCreateInfo *tessellation;
	const VkPipelineViewportStateCreateInfo *viewport;
	const VkPipelineRasterizationStateCreateInfo *rasterization;
	const VkPipelineMultisampleStateCreateInfo *multisample;
	const VkPipelineDepthStencilStateCreateInfo *depthStencil;
	const VkPipelineColorBlendStateCreateInfo *colorBlend;
	const VkPipelineDynamicStateCreateInfo *dynamic;
	qfv_reference_t layout;
} qfv_pipelineinfo_t;

typedef struct qfv_subpassinfo_s {
	vec4f_t     color;
	const char *name;
	uint32_t    num_dependencies;
	qfv_dependencyinfo_t *dependencies;
	qfv_attachmentsetinfo_t *attachments;
	uint32_t    num_pipelines;
	qfv_pipelineinfo_t *pipelines;
	qfv_pipelineinfo_t *base_pipeline;
} qfv_subpassinfo_t;

typedef struct qfv_framebufferinfo_s {
	qfv_attachmentinfo_t *attachments;
	uint32_t    num_attachments;
	uint32_t    width;
	uint32_t    height;
	uint32_t    layers;
} qfv_framebufferinfo_t;

typedef struct qfv_renderpassinfo_s {
	vec4f_t     color;
	const char *name;
	void       *pNext;
	qfv_framebufferinfo_t framebuffer;
	uint32_t    num_subpasses;
	qfv_subpassinfo_t *subpasses;
	qfv_reference_t output;
} qfv_renderpassinfo_t;

typedef struct qfv_computeinfo_s {
	vec4f_t     color;
	const char *name;

	uint32_t    num_pipelines;
	qfv_pipelineinfo_t *pipelines;
} qfv_computeinfo_t;

typedef struct qfv_renderinfo_s {
	vec4f_t     color;
	const char *name;

	uint32_t    num_renderpasses;
	qfv_renderpassinfo_t *renderpasses;
} qfv_renderinfo_t;

typedef struct qfv_processinfo_s {
	vec4f_t     color;
	const char *name;

	uint32_t    num_tasks;
	qfv_taskinfo_t *tasks;
} qfv_processinfo_t;

typedef struct qfv_stepinfo_s {
	vec4f_t     color;
	const char *name;

	uint32_t     num_dependencies;
	qfv_reference_t *dependencies;

	qfv_renderinfo_t *render;
	qfv_computeinfo_t *compute;
	qfv_processinfo_t *process;
} qfv_stepinfo_t;

typedef struct qfv_jobinfo_s {
	struct memsuper_s *memsuper;

	struct plitem_s *plitem;
	uint32_t     num_steps;
	qfv_stepinfo_t *steps;

	uint32_t    num_images;
	uint32_t    num_imageviews;
	qfv_imageinfo_t *images;
	qfv_imageviewinfo_t *imageviews;
	uint32_t    num_buffers;
	uint32_t    num_bufferviews;
	qfv_imageinfo_t *buffers;
	qfv_imageviewinfo_t *bufferviews;

	uint32_t    num_dslayouts;
	qfv_descriptorsetlayoutinfo_t *dslayouts;

	qfv_taskinfo_t *newscene_tasks;
	uint32_t    newscene_num_tasks;
	uint32_t    init_num_tasks;
	qfv_taskinfo_t *init_tasks;
} qfv_jobinfo_t;

typedef struct qfv_samplercreateinfo_s {
	const char *name;
	VkSamplerCreateFlags flags;
	VkFilter    magFilter;
	VkFilter    minFilter;
	VkSamplerMipmapMode mipmapMode;
	VkSamplerAddressMode addressModeU;
	VkSamplerAddressMode addressModeV;
	VkSamplerAddressMode addressModeW;
	float       mipLodBias;
	VkBool32    anisotropyEnable;
	float       maxAnisotropy;
	VkBool32    compareEnable;
	VkCompareOp compareOp;
	float       minLod;
	float       maxLod;
	VkBorderColor borderColor;
	VkBool32    unnormalizedCoordinates;
	VkSampler   sampler;
} qfv_samplercreateinfo_t;

typedef struct qfv_samplerinfo_s {
	struct memsuper_s *memsuper;

	struct plitem_s *plitem;
	qfv_samplercreateinfo_t *samplers;
	uint32_t    num_samplers;
} qfv_samplerinfo_t;

#ifndef __QFCC__
typedef struct qfv_time_s {
	int64_t     cur_time;
	int64_t     min_time;
	int64_t     max_time;
} qfv_time_t;

typedef struct qfv_label_s {
	vec4f_t     color;
	uint32_t    color32;
	uint32_t    name_len;
	const char *name;
} qfv_label_t;

typedef struct qfv_pipeline_s {
	qfv_label_t label;
	bool        disabled;
	VkPipelineBindPoint bindPoint;
	vec4u_t     dispatch;
	VkPipeline  pipeline;
	VkPipelineLayout layout;

	VkViewport  viewport;
	VkRect2D    scissor;
	uint32_t    num_indices;
	uint32_t   *ds_indices;

	uint32_t    task_count;
	qfv_taskinfo_t *tasks;
} qfv_pipeline_t;

typedef struct qfv_subpass_s {
	qfv_label_t label;
	VkCommandBufferInheritanceInfo inherit;
	VkCommandBufferBeginInfo beginInfo;
	uint32_t    pipeline_count;
	qfv_pipeline_t *pipelines;
} qfv_subpass_t;

typedef struct qfv_framebuffer_s {
	uint32_t    width;
	uint32_t    height;
	uint32_t    layers;
	uint32_t    num_attachments;
	VkImageView *views;
} qfv_framebuffer_t;

typedef struct qfv_renderpass_s {
	struct vulkan_ctx_s *vulkan_ctx;
	qfv_label_t label;		// for debugging

	VkRenderPassBeginInfo beginInfo;
	VkSubpassContents subpassContents;

	uint32_t    subpass_count;
	qfv_subpass_t *subpasses;

	qfv_framebuffer_t framebuffer;
	qfv_framebufferinfo_t *framebufferinfo;
	VkImageView output;
	qfv_reference_t outputref;

	struct qfv_resource_s *resources;
} qfv_renderpass_t;

typedef struct qfv_render_s {
	qfv_label_t label;
	qfv_renderpass_t *active;
	qfv_renderpass_t *renderpasses;
	uint32_t    num_renderpasses;
	qfv_output_t output;
} qfv_render_t;

typedef struct qfv_compute_s {
	qfv_label_t label;
	qfv_pipeline_t *pipelines;
	uint32_t    pipeline_count;
} qfv_compute_t;

typedef struct qfv_process_s {
	qfv_label_t label;
	qfv_taskinfo_t *tasks;
	uint32_t    task_count;
} qfv_process_t;

typedef struct qfv_step_s {
	qfv_label_t label;
	qfv_render_t *render;
	qfv_compute_t *compute;
	qfv_process_t *process;
	qfv_time_t time;
} qfv_step_t;

typedef void (*qfv_initfunc_f) (exprctx_t *ectx);
typedef struct qfv_initfuncset_s
	DARRAY_TYPE (qfv_initfunc_f) qfv_initfuncset_t;

typedef struct qfv_job_s {
	qfv_label_t label;

	uint32_t    num_renderpasses;
	uint32_t    num_pipelines;
	uint32_t    num_layouts;
	uint32_t    num_steps;
	VkRenderPass *renderpasses;
	VkPipeline *pipelines;
	VkPipelineLayout *layouts;
	qfv_step_t *steps;
	qfv_cmdbufferset_t commands;
	uint32_t    num_dsmanagers;
	struct qfv_dsmanager_s **dsmanager;
	qfv_time_t time;

	qfv_taskinfo_t *newscene_tasks;
	qfv_taskinfo_t *init_tasks;
	uint32_t    newscene_task_count;
	uint32_t    init_task_count;
	qfv_initfuncset_t startup_funcs;
	qfv_initfuncset_t shutdown_funcs;
	qfv_initfuncset_t clearstate_funcs;
} qfv_job_t;

typedef struct qfv_renderframe_s {
	VkFence     fence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderDoneSemaphore;
	qfv_cmdpoolmgr_t cmdpool;
	qftVkCtx_t *qftVkCtx;
} qfv_renderframe_t;

typedef struct qfv_renderframeset_s
	DARRAY_TYPE (qfv_renderframe_t) qfv_renderframeset_t;
typedef struct qfv_attachmentinfoset_s
	DARRAY_TYPE (qfv_attachmentinfo_t *) qfv_attachmentinfoset_t;

typedef struct qfv_renderctx_s {
	struct hashctx_s *hashctx;
	exprtab_t   task_functions;
	qfv_attachmentinfoset_t external_attachments;
	qfv_jobinfo_t *jobinfo;
	qfv_samplerinfo_t *samplerinfo;
	qfv_job_t  *job;
	qfv_renderframeset_t frames;
	int64_t     size_time;
	struct qfv_renderdebug_s *debug;
} qfv_renderctx_t;

typedef struct qfv_taskctx_s {
	struct vulkan_ctx_s *ctx;
	qfv_renderframe_t *frame;
	qfv_pipeline_t *pipeline;
	qfv_renderpass_t *renderpass;
	VkCommandBuffer cmd;
	void       *data;
} qfv_taskctx_t;

VkCommandBuffer QFV_GetCmdBuffer (struct vulkan_ctx_s *ctx, bool secondary);
void QFV_AppendCmdBuffer (struct vulkan_ctx_s *ctx, VkCommandBuffer cmd);

void QFV_RunRenderPassCmd (VkCommandBuffer cmd, struct vulkan_ctx_s *ctx,
						   qfv_renderpass_t *renderpass,
						   void *data);
void QFV_RunRenderPass (struct vulkan_ctx_s *ctx, qfv_renderpass_t *renderpass,
						uint32_t width, uint32_t height, void *data);
void QFV_RunRenderJob (struct vulkan_ctx_s *ctx);
void QFV_LoadRenderInfo (struct vulkan_ctx_s *ctx, struct plitem_s *item);
void QFV_LoadSamplerInfo (struct vulkan_ctx_s *ctx, struct plitem_s *item);
void QFV_BuildRender (struct vulkan_ctx_s *ctx);
void QFV_Render_Init (struct vulkan_ctx_s *ctx);
void QFV_Render_Run_Init (struct vulkan_ctx_s *ctx);
void QFV_Render_Run_Startup (struct vulkan_ctx_s *ctx);
void QFV_Render_Run_ClearState (struct vulkan_ctx_s *ctx);
void QFV_Render_Shutdown (struct vulkan_ctx_s *ctx);
void QFV_Render_AddTasks (struct vulkan_ctx_s *ctx, exprsym_t *task_sys);
void QFV_Render_AddStartup (struct vulkan_ctx_s *ctx, qfv_initfunc_f func);
void QFV_Render_AddShutdown (struct vulkan_ctx_s *ctx, qfv_initfunc_f func);
void QFV_Render_AddClearState (struct vulkan_ctx_s *ctx, qfv_initfunc_f func);
void QFV_Render_AddAttachments (struct vulkan_ctx_s *ctx,
								uint32_t num_attachments,
		                        qfv_attachmentinfo_t **attachments);

void QFV_DestroyFramebuffer (struct vulkan_ctx_s *ctx, qfv_renderpass_t *rp);
void QFV_CreateFramebuffer (struct vulkan_ctx_s *ctx, qfv_renderpass_t *rp,
							VkExtent2D extent);

struct qfv_dsmanager_s *
QFV_Render_DSManager (struct vulkan_ctx_s *ctx,
					  const char *setName) __attribute__((pure));
VkSampler QFV_Render_Sampler (struct vulkan_ctx_s *ctx, const char *name);

qfv_step_t *QFV_GetStep (const exprval_t *param, qfv_job_t *job);
qfv_step_t *QFV_FindStep (const char *step, qfv_job_t *job) __attribute__((pure));
struct qfv_resobj_s *QFV_FindResource (const char *name, qfv_renderpass_t *rp) __attribute__((pure));

struct scene_s;
void QFV_Render_NewScene (struct scene_s *scene, struct vulkan_ctx_s *ctx);

struct imui_ctx_s;
void QFV_Render_UI (struct vulkan_ctx_s *ctx, struct imui_ctx_s *imui_ctx);
void QFV_Render_Menu (struct vulkan_ctx_s *ctx, struct imui_ctx_s *imui_ctx);
void QFV_Render_UI_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QFCC__

#endif//__QF_Vulkan_render_h
