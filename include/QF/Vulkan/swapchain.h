#ifndef __QF_Vulkan_swapchain_h
#define __QF_Vulkan_swapchain_h

typedef struct qfv_swapchain_s {
	struct qfv_device_s *device;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	int32_t     numImages;
	VkImage    *images;
} qfv_swapchain_t;

struct vulkan_ctx_s;
qfv_swapchain_t *QFV_CreateSwapchain (struct vulkan_ctx_s *ctx,
									  VkSwapchainKHR old_swapchain);
void QFV_DestroySwapchain (qfv_swapchain_t *swapchain);

#endif//__QF_Vulkan_swapchain_h
