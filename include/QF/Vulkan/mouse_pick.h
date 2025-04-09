#ifndef __QF_Vulkan_mouse_pick_h
#define __QF_Vulkan_mouse_pick_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/qtypes.h"

#define mousepick_size 5

struct vulkan_ctx_s;
typedef void (*mousepickfunc_t) (const uint32_t *entid, void *data);

typedef struct qfv_mousepick_frame_s {
	VkBuffer    entid_buffer;
	uint32_t   *entid_data;

	bool        initiated;
	mousepickfunc_t callback;
	void       *callback_data;
	uint32_t    x, y;
	VkOffset3D  offset;
	VkExtent3D  extent;
	VkImage     entid_image;
} qfv_mousepick_frame_t;

typedef struct qfv_mousepick_frame_set_s
	DARRAY_TYPE (qfv_mousepick_frame_t) qfv_mousepick_frame_set_t;

typedef struct qfv_mousepickctx_s {
	qfv_mousepick_frame_set_t frames;
	struct qfv_resobj_s *entid_res;
	struct qfv_resource_s *resources;
} qfv_mousepickctx_t;

struct vulkan_ctx_s;

void QFV_MousePick_Init (struct vulkan_ctx_s *ctx);
void QFV_MousePick_Shutdown (struct vulkan_ctx_s *ctx);
void QFV_MousePick_Read (struct vulkan_ctx_s *ctx, uint32_t x, uint32_t y,
						 mousepickfunc_t callback, void *data);

#endif//__QF_Vulkan_mouse_pick_h
