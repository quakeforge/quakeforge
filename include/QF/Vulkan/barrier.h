#ifndef __QF_Vulkan_barrier_h
#define __QF_Vulkan_barrier_h

typedef struct {
	VkPipelineStageFlags src;
	VkPipelineStageFlags dst;
} qfv_pipelinestagepair_t;

//XXX Note: imageLayoutTransitionBarriers, imageLayoutTransitionStages and
// the enum must be kept in sync
enum {
	qfv_LT_Undefined_to_TransferDst,
	qfv_LT_TransferDst_to_ShaderReadOnly,
	qfv_LT_ShaderReadOnly_to_TransferDst,
	qfv_LT_Undefined_to_DepthStencil,
	qfv_LT_Undefined_to_Color,
};

extern const VkImageMemoryBarrier imageLayoutTransitionBarriers[];
extern const qfv_pipelinestagepair_t imageLayoutTransitionStages[];

#endif//__QF_Vulkan_barrier_h
