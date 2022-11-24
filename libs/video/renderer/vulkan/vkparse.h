#ifndef __vkparse_h
#define __vkparse_h

typedef struct parsectx_s {
	struct exprctx_s *ectx;
	struct vulkan_ctx_s *vctx;
	struct plitem_s *properties;
	void       *data;
} parsectx_t;

typedef struct scriptctx_s {
	struct vulkan_ctx_s *vctx;
	struct hashctx_s *hashctx;

	struct plitem_s  *pipelineDef;
	struct hashtab_s *shaderModules;
	struct hashtab_s *setLayouts;
	struct hashtab_s *pipelineLayouts;
	struct hashtab_s *descriptorPools;
	struct hashtab_s *samplers;
	struct hashtab_s *images;
	struct hashtab_s *imageViews;
	struct hashtab_s *renderpasses;

	qfv_output_t output;
} scriptctx_t;

void Vulkan_Init_Cvars (void);
void Vulkan_Script_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Script_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Script_SetOutput (struct vulkan_ctx_s *ctx, qfv_output_t *output);

#include "QF/cexpr.h"
#include "QF/plist.h"

#define QFV_PROPERTIES "properties"

exprenum_t *QFV_GetEnum (const char *name);

uint64_t QFV_GetHandle (struct hashtab_s *tab, const char *name);
void QFV_AddHandle (struct hashtab_s *tab, const char *name, uint64_t handle);

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist,
								  plitem_t *properties);
VkPipeline QFV_ParseComputePipeline (vulkan_ctx_t *ctx, plitem_t *plist,
									 plitem_t *properties);
VkPipeline QFV_ParseGraphicsPipeline (vulkan_ctx_t *ctx, plitem_t *plist,
									  plitem_t *properties);
VkDescriptorPool QFV_ParseDescriptorPool (vulkan_ctx_t *ctx, plitem_t *plist,
										  plitem_t *properties);
VkDescriptorSetLayout QFV_ParseDescriptorSetLayout (vulkan_ctx_t *ctx,
													plitem_t *plist,
													plitem_t *properties);
VkPipelineLayout QFV_ParsePipelineLayout (vulkan_ctx_t *ctx, plitem_t *plist,
										  plitem_t *properties);
VkSampler QFV_ParseSampler (vulkan_ctx_t *ctx, plitem_t *plist,
							plitem_t *properties);
VkImage QFV_ParseImage (vulkan_ctx_t *ctx, plitem_t *plist,
						plitem_t *properties);
VkImageView QFV_ParseImageView (vulkan_ctx_t *ctx, plitem_t *plist,
								plitem_t *properties);
struct qfv_imageset_s *QFV_ParseImageSet (vulkan_ctx_t *ctx, plitem_t *plist,
										  plitem_t *properties);
struct qfv_imageviewset_s *QFV_ParseImageViewSet (vulkan_ctx_t *ctx,
												  plitem_t *plist,
												  plitem_t *properties);
VkFramebuffer QFV_ParseFramebuffer (vulkan_ctx_t *ctx, plitem_t *plist,
									plitem_t *properties);
struct clearvalueset_s *QFV_ParseClearValues (vulkan_ctx_t *ctx,
											  plitem_t *plist,
											  plitem_t *properties);

struct qfv_subpassset_s *QFV_ParseSubpasses (vulkan_ctx_t *ctx,
											 plitem_t *plist,
											 plitem_t *properties);
int QFV_ParseRGBA (vulkan_ctx_t *ctx, float *rgba, plitem_t *plist,
				   plitem_t *properties);
#endif//__vkparse_h
