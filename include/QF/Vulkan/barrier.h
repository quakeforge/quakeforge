#ifndef __QF_Vulkan_barrier_h
#define __QF_Vulkan_barrier_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

typedef struct qfv_imagebarrier_s {
	VkPipelineStageFlags srcStages;
	VkPipelineStageFlags dstStages;
	VkImageMemoryBarrier barrier;
} qfv_imagebarrier_t;

typedef struct qfv_bufferbarrier_s {
	VkPipelineStageFlags srcStages;
	VkPipelineStageFlags dstStages;
	VkBufferMemoryBarrier barrier;
} qfv_bufferbarrier_t;

// image layout transitions
enum {
	qfv_LT_Undefined_to_TransferDst,
	qfv_LT_Undefined_to_General,
	qfv_LT_TransferDst_to_TransferSrc,
	qfv_LT_TransferDst_to_General,
	qfv_LT_TransferDst_to_ShaderReadOnly,
	qfv_LT_TransferSrc_to_ShaderReadOnly,
	qfv_LT_ShaderReadOnly_to_TransferDst,
	qfv_LT_Undefined_to_DepthStencil,
	qfv_LT_Undefined_to_Color,
};

// buffer barriers
enum {
	qfv_BB_Unknown_to_TransferWrite,
	qfv_BB_TransferWrite_to_VertexAttrRead,
	qfv_BB_TransferWrite_to_IndexRead,
	qfv_BB_TransferWrite_to_UniformRead,
	qfv_BB_TransferWrite_to_ShaderRW,
	qfv_BB_ShaderRW_to_ShaderRO,
	qfv_BB_ShaderRW_to_ShaderRO_VA,
	qfv_BB_ShaderRO_to_ShaderWrite,
	qfv_BB_ShaderRO_VA_to_ShaderWrite,
	qfv_BB_ShaderWrite_to_ShaderRO,
	qfv_BB_ShaderWrite_to_ShaderRW,
};

extern const qfv_imagebarrier_t imageBarriers[];
extern const qfv_bufferbarrier_t bufferBarriers[];

#endif//__QF_Vulkan_barrier_h
