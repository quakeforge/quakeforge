#ifndef __QF_Vulkan_barrier_h
#define __QF_Vulkan_barrier_h

typedef struct {
	VkPipelineStageFlags srcStages;
	VkPipelineStageFlags dstStages;
	VkImageMemoryBarrier barrier;
} qfv_imagebarrier_t;

//XXX Note: imageBarriers and the enum must be kept in sync
enum {
	qfv_LT_Undefined_to_TransferDst,
	qfv_LT_TransferDst_to_TransferSrc,
	qfv_LT_TransferDst_to_ShaderReadOnly,
	qfv_LT_TransferSrc_to_ShaderReadOnly,
	qfv_LT_ShaderReadOnly_to_TransferDst,
	qfv_LT_Undefined_to_DepthStencil,
	qfv_LT_Undefined_to_Color,
};

extern const qfv_imagebarrier_t imageBarriers[];

#endif//__QF_Vulkan_barrier_h
