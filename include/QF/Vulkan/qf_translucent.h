#ifndef __QF_Vulkan_qf_translucent_h
#define __QF_Vulkan_qf_translucent_h

#include "QF/darray.h"

#include "QF/simd/types.h"

#include "QF/Vulkan/command.h"

typedef struct qfv_transfrag_s {
	vec4f_t     color;
	float       depth;
	int32_t     next;
} qfv_transfrag_t;

typedef struct qfv_transtate_s {
	int32_t     numFragments;
	int32_t     maxFragments;
} qfv_transtate_t;

typedef enum {
	QFV_translucentClear,
	QFV_translucentBlend,

	QFV_translucentNumPasses
} QFV_TranslucentSubpass;

typedef struct translucentframe_s {
	VkDescriptorSet descriptors;
	VkImage     heads;
	VkBuffer    state;
	qfv_cmdbufferset_t cmdSet;
} translucentframe_t;

typedef struct translucentframeset_s
	DARRAY_TYPE (translucentframe_t) translucentframeset_t;

typedef struct translucentctx_s {
	translucentframeset_t frames;

	struct qfv_resource_s *resources;

	int         maxFragments;
	VkDescriptorPool pool;
	VkDescriptorSetLayout setLayout;
} translucentctx_t;

struct vulkan_ctx_s;
struct qfv_renderframe_s;

void Vulkan_Translucent_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Translucent_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Translucent_Draw (struct qfv_renderframe_s *rFrame);
VkDescriptorSet Vulkan_Translucent_Descriptors (struct vulkan_ctx_s *ctx,
												int frame)__attribute__((pure));
void Vulkan_Translucent_CreateBuffers (struct vulkan_ctx_s *ctx,
									   VkExtent2D extent);
void Vulkan_Translucent_CreateRenderPasses (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_translucent_h
