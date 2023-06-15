#ifndef __vid_vulkan_h
#define __vid_vulkan_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/simd/types.h"

typedef struct vulkan_frame_s {
	VkFence     fence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderDoneSemaphore;
	VkCommandBuffer cmdBuffer;
} vulkan_frame_t;

typedef struct vulkan_frameset_s
	DARRAY_TYPE (vulkan_frame_t) vulkan_frameset_t;

typedef struct qfv_renderpassset_s
	DARRAY_TYPE (struct qfv_orenderpass_s *) qfv_renderpassset_t;

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
	VkSurfaceKHR surface;	//FIXME surface = window, so "contains" swapchain

	uint32_t    swapImageIndex;

	struct scriptctx_s *script_context;
	struct qfv_renderctx_s *render_context;
	struct texturectx_s *texture_context;
	struct matrixctx_s *matrix_context;
	struct translucentctx_s *translucent_context;
	struct aliasctx_s *alias_context;
	struct bspctx_s *bsp_context;
	struct iqmctx_s *iqm_context;
	struct scenectx_s *scene_context;
	struct palettectx_s *palette_context;
	struct particlectx_s *particle_context;
	struct spritectx_s *sprite_context;
	struct drawctx_s *draw_context;
	struct lightingctx_s *lighting_context;
	struct composectx_s *compose_context;
	struct outputctx_s *output_context;

	VkCommandPool cmdpool;
	struct qfv_stagebuf_s *staging;
	uint32_t    curFrame;
	vulkan_frameset_t frames;
	qfv_renderpassset_t renderPasses;
	struct qfv_orenderpass_s *output_renderpass;

	struct qfv_capture_s *capture;
	void      (*capture_callback) (const byte *data, int width, int height);
	// make a queue?
	void       *capture_complete;// really capfunc_t
	void       *capture_complete_data;

	struct qfv_tex_s *default_black;
	struct qfv_tex_s *default_white;
	struct qfv_tex_s *default_magenta;
	struct qfv_tex_s *default_magenta_array;

	// size of window
	int         window_width;
	int         window_height;
	int         twod_scale;

#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;
#include "QF/Vulkan/funclist.h"
} vulkan_ctx_t;

#define qfvPushDebug(ctx, x)									\
	do {														\
		if (developer & SYS_vulkan) {					\
			DARRAY_APPEND(&(ctx)->instance->debug_stack, (x));	\
		}														\
	} while (0)
#define qfvPopDebug(ctx) 										\
	do {														\
		if (developer & SYS_vulkan) {					\
			__auto_type ds = &(ctx)->instance->debug_stack;		\
			DARRAY_REMOVE_AT(ds, ds->size - 1);					\
		}														\
	} while (0)

#endif//__vid_vulkan_h
