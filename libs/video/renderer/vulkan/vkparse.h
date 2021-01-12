#ifndef __vkparse_h
#define __vkparse_h

typedef struct parsectx_s {
	struct exprctx_s *ectx;
	struct vulkan_ctx_s *vctx;
} parsectx_t;

#include "QF/cexpr.h"
#include "QF/qfplist.h"
#include "QF/Vulkan/renderpass.h"
#ifdef vkparse_internal
#include "libs/video/renderer/vulkan/vkparse.hinc"
#endif

typedef struct parseres_s {
	const char *name;
	plfield_t  *field;
	size_t      offset;
} parseres_t;

typedef struct handleref_s {
	char       *name;
	uint64_t    handle;
} handleref_t;

VkShaderModule QFV_GetShaderModule (vulkan_ctx_t *ctx, const char *name);
VkDescriptorPool QFV_GetDescriptorPool (vulkan_ctx_t *ctx, const char *name);
VkDescriptorSetLayout QFV_GetDescriptorSetLayout (vulkan_ctx_t *ctx,
												  const char *name);
VkPipelineLayout QFV_GetPipelineLayout (vulkan_ctx_t *ctx, const char *name);
VkSampler QFV_GetSampler (vulkan_ctx_t *ctx, const char *name);

void QFV_ParseResources (vulkan_ctx_t *ctx, plitem_t *plist);
void QFV_InitParse (vulkan_ctx_t *ctx);
exprenum_t *QFV_GetEnum (const char *name);

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist);
VkPipeline QFV_ParsePipeline (vulkan_ctx_t *ctx, plitem_t *plist);

#endif//__vkparse_h
