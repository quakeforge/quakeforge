#ifndef __vkparse_h
#define __vkparse_h

typedef struct parsectx_s {
	struct exprctx_s *ectx;
	struct vulkan_ctx_s *vctx;
	struct plitem_s *properties;
	void       *data;
} parsectx_t;

#include "QF/cexpr.h"
#include "QF/plist.h"
#include "QF/Vulkan/renderpass.h"
#ifdef vkparse_internal
#include "libs/video/renderer/vulkan/vkparse.hinc"
#endif

#define QFV_PROPERTIES "properties"

typedef struct parseres_s {
	const char *name;
	plfield_t  *field;
	size_t      offset;
} parseres_t;

typedef struct handleref_s {
	char       *name;
	uint64_t    handle;
} handleref_t;

void QFV_InitParse (vulkan_ctx_t *ctx);
exprenum_t *QFV_GetEnum (const char *name);

uint64_t QFV_GetHandle (struct hashtab_s *tab, const char *name);
void QFV_AddHandle (struct hashtab_s *tab, const char *name, uint64_t handle);

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist,
								  plitem_t *properties);
VkPipeline QFV_ParsePipeline (vulkan_ctx_t *ctx, plitem_t *plist,
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
int QFV_ParseClearValues (vulkan_ctx_t *ctx, plitem_t *plist,
						  plitem_t *properties);


#endif//__vkparse_h
