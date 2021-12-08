#ifndef __vid_vulkan_h
#define __vid_vulkan_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/simd/types.h"

typedef struct vulkan_frame_s {
	VkFramebuffer framebuffer;
	VkFence     fence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderDoneSemaphore;
	VkCommandBuffer cmdBuffer;
} vulkan_frame_t;

typedef struct vulkan_frameset_s
	DARRAY_TYPE (vulkan_frame_t) vulkan_frameset_t;

typedef struct qfv_renderpassset_s
	DARRAY_TYPE (struct qfv_renderpass_s *) qfv_renderpassset_t;

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

	struct va_ctx_s *va_ctx;
	struct qfv_instance_s *instance;
	struct qfv_device_s *device;
	struct qfv_swapchain_s *swapchain;
	VkSampleCountFlagBits msaaSamples;	// FIXME not here?
	struct hashlink_s *hashlinks;	//FIXME want per thread
	VkSurfaceKHR surface;	//FIXME surface = window, so "contains" swapchain
	struct plitem_s  *pipelineDef;

	uint32_t    swapImageIndex;

	struct hashtab_s *shaderModules;
	struct hashtab_s *setLayouts;
	struct hashtab_s *pipelineLayouts;
	struct hashtab_s *descriptorPools;
	struct hashtab_s *samplers;
	struct hashtab_s *images;
	struct hashtab_s *imageViews;
	struct hashtab_s *renderpasses;

	struct matrixctx_s *matrix_context;
	struct aliasctx_s *alias_context;
	struct bspctx_s *bsp_context;
	struct drawctx_s *draw_context;
	struct lightingctx_s *lighting_context;
	struct composectx_s *compose_context;

	VkBuffer    quad_buffer;
	VkDeviceMemory quad_memory;

	VkCommandPool cmdpool;
	VkCommandBuffer cmdbuffer;
	VkFence     fence;			// for ctx->cmdbuffer only
	struct qfv_stagebuf_s *staging;
	size_t      curFrame;
	vulkan_frameset_t frames;
	qfv_renderpassset_t renderPasses;

	struct qfv_capture_s *capture;
	void      (*capture_callback) (const byte *data, int width, int height);

	struct qfv_tex_s *default_black;
	struct qfv_tex_s *default_white;
	struct qfv_tex_s *default_magenta;

	VkViewport  viewport;
	VkRect2D    scissor;

#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;
#include "QF/Vulkan/funclist.h"
} vulkan_ctx_t;

#define qfvPushDebug(ctx, x)									\
	do {														\
		if (developer->int_val & SYS_vulkan) {					\
			DARRAY_APPEND(&(ctx)->instance->debug_stack, (x));	\
		}														\
	} while (0)
#define qfvPopDebug(ctx) 										\
	do {														\
		if (developer->int_val & SYS_vulkan) {					\
			__auto_type ds = &(ctx)->instance->debug_stack;		\
			DARRAY_REMOVE_AT(ds, ds->size - 1);					\
		}														\
	} while (0)

#endif//__vid_vulkan_h
