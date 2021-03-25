#ifndef __QF_Vulkan_capture_h
#define __QF_Vulkan_capture_h

#include "QF/darray.h"
#include "QF/qtypes.h"

typedef struct qfv_capture_image_s {
	VkImage     image;
	VkImageLayout layout;
	VkCommandBuffer cmd;
	byte       *data;
} qfv_capture_image_t;

typedef struct qfv_capture_image_set_s
	DARRAY_TYPE (qfv_capture_image_t) qfv_capture_image_set_t;

#define QFV_AllocCaptureImageSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_capture_image_set_t, num, allocator)

typedef struct qfv_capture_s {
	struct qfv_device_s *device;

	int         canBlit;
	VkExtent2D  extent;
	qfv_capture_image_set_t *image_set;
	size_t      memsize;
	VkDeviceMemory memory;
} qfv_capture_t;

struct qfv_swapchain_s;

qfv_capture_t *QFV_CreateCapture (struct qfv_device_s *device, int numframes,
								  struct qfv_swapchain_s *swapchain,
								  VkCommandPool cmdPool);
void QFV_RenewCapture (qfv_capture_t *capture,
					   struct qfv_swapchain_s *swapchain);
void QFV_DestroyCapture (qfv_capture_t *capture);

VkCommandBuffer QFV_CaptureImage (qfv_capture_t *capture, VkImage scImage,
								  int frame);
const byte *QFV_CaptureData (qfv_capture_t *capture, int frame) __attribute__((pure));

#endif//__QF_Vulkan_capture_h
