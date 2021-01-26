#ifndef __vid_vulkan_h
#define __vid_vulkan_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"

typedef struct vulkan_renderpass_s {
	VkRenderPass renderpass;
	struct qfv_imageresource_s *colorImage;
	struct qfv_imageresource_s *depthImage;
} vulkan_renderpass_t;

typedef struct vulkan_framebuffer_s {
	VkFramebuffer framebuffer;
	VkFence     fence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderDoneSemaphore;
	VkCommandBuffer cmdBuffer;

	struct qfv_cmdbufferset_s *subCommand;
} vulkan_framebuffer_t;

typedef struct vulkan_matrices_s {
	VkBuffer    buffer_2d;
	VkBuffer    buffer_3d;
	VkDeviceMemory memory;
	float      *projection_2d;
	float      *projection_3d;
	float      *view_3d;
	float      *sky_3d;
} vulkan_matrices_t;

typedef struct vulkan_framebufferset_s
	DARRAY_TYPE (vulkan_framebuffer_t) vulkan_framebufferset_t;

typedef struct vulkan_ctx_s {
	void        (*load_vulkan) (struct vulkan_ctx_s *ctx);
	void        (*unload_vulkan) (struct vulkan_ctx_s *ctx);

	const char **required_extensions;
	struct vulkan_presentation_s *presentation;
	int (*get_presentation_support) (struct vulkan_ctx_s *ctx,
									 VkPhysicalDevice physicalDevice,
									 uint32_t queueFamilyIndex);
	void        (*choose_visual) (struct vulkan_ctx_s *ctx);
	void        (*create_window) (struct vulkan_ctx_s *ctx);
	VkSurfaceKHR (*create_surface) (struct vulkan_ctx_s *ctx);

	struct qfv_instance_s *instance;
	struct qfv_device_s *device;
	struct qfv_swapchain_s *swapchain;
	VkSampleCountFlagBits msaaSamples;	// FIXME not here?
	struct hashlink_s *hashlinks;	//FIXME want per thread
	VkSurfaceKHR surface;	//FIXME surface = window, so "contains" swapchain
	struct plitem_s  *pipelineDef;
	struct hashtab_s *shaderModules;
	struct hashtab_s *setLayouts;
	struct hashtab_s *pipelineLayouts;
	struct hashtab_s *descriptorPools;
	struct hashtab_s *samplers;

	struct aliasctx_s *alias_context;
	struct bspctx_s *bsp_context;
	struct drawctx_s *draw_context;

	VkCommandPool cmdpool;
	VkCommandBuffer cmdbuffer;
	VkFence     fence;			// for ctx->cmdbuffer only
	vulkan_renderpass_t renderpass;
	struct qfv_stagebuf_s *staging;
	VkPipeline  pipeline;
	size_t      curFrame;
	vulkan_framebufferset_t framebuffers;

	struct qfv_tex_s *default_black;
	struct qfv_tex_s *default_white;
	struct qfv_tex_s *default_magenta;

	// projection and view matrices (model is push constant)
	vulkan_matrices_t matrices;

#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;
#include "QF/Vulkan/funclist.h"
} vulkan_ctx_t;

#endif//__vid_vulkan_h
