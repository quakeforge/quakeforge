#ifndef __QF_Vulkan_capture_h
#define __QF_Vulkan_capture_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/qtypes.h"

struct vulkan_ctx_s;
struct tex_s;
typedef void (*capfunc_t) (struct tex_s *screencap, void *data);

typedef struct qfv_capture_frame_s {
	VkBuffer    buffer;
	byte       *data;

	bool        initiated;
	capfunc_t   callback;
	void       *callback_data;
} qfv_capture_frame_t;

typedef struct qfv_capture_frame_set_s
	DARRAY_TYPE (qfv_capture_frame_t) qfv_capture_frame_set_t;

typedef struct qfv_capturectx_s {
	qfv_capture_frame_set_t frames;
	struct qfv_device_s *device;
	VkExtent2D  extent;
	size_t      imgsize;
	size_t      memsize;
	byte       *data;
	VkDeviceMemory memory;
} qfv_capturectx_t;

struct vulkan_ctx_s;

void QFV_Capture_Init (struct vulkan_ctx_s *ctx);
void QFV_Capture_Renew (struct vulkan_ctx_s *ctx);
void QFV_Capture_Shutdown (struct vulkan_ctx_s *ctx);
void QFV_Capture_Screen (struct vulkan_ctx_s *ctx,
						 capfunc_t callback, void *data);

#endif//__QF_Vulkan_capture_h
