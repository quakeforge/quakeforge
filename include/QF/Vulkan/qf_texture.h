#ifndef __QF_Vulkan_qf_texture_h
#define __QF_Vulkan_qf_texture_h

#include "QF/ecs.h"
#include "QF/image.h"
#include "QF/Vulkan/qf_vid.h"

typedef struct ecs_registry_s ecs_registry_t;
typedef struct vulkan_ctx_s vulkan_ctx_t;
typedef struct qfv_textureinfo_s qfv_textureinfo_t;
typedef struct hashtab_s hashtab_t;
typedef struct hashctx_s hashctx_t;

typedef struct qfv_tex_s {
	struct qfv_resource_s *resource;
	VkImage     image;
	VkImageView view;
} qfv_tex_t;

typedef struct texturectx_s {
	struct qfv_dsmanager_s *dsmanager;
	struct qfv_resource_s *tex_resource;

	ecs_registry_t *reg;
	uint32_t    comp_base;
	hashtab_t  *textures;
	hashctx_t  *hashctx;

	VkSampler   default_sampler;
} texturectx_t;

VkImageView QFV_Tex_View (vulkan_ctx_t *ctx, uint32_t texid) __attribute__((pure));
VkSampler QFV_Tex_Sampler (vulkan_ctx_t *ctx, uint32_t texid) __attribute__((pure));
uint32_t QFV_CreateTexture (vulkan_ctx_t *ctx, VkImage image, VkImageView view,
							VkSampler sampler, const char *name);
uint32_t QFV_LoadTexinfo (vulkan_ctx_t *ctx, qfv_textureinfo_t *texinfo,
						  const char *name);
uint32_t QFV_TexFindTexture (vulkan_ctx_t *ctx, const char *name);
uint32_t QFV_TexGetSkinId (vulkan_ctx_t *ctx, uint32_t id) __attribute__((pure));
void QFV_TexSetSkinId (vulkan_ctx_t *ctx, uint32_t id, uint32_t skinid);
VkDescriptorSet QFV_GetTexture (vulkan_ctx_t *ctx, uint32_t texid) __attribute__((pure));
bool QFV_TexIsCubemap (vulkan_ctx_t *ctx, uint32_t texid) __attribute__((pure));

void Vulkan_ExpandPalette (byte *dst, const byte *src, const byte *palette,
						   int alpha, int count);
qfv_tex_t *Vulkan_LoadTex (struct vulkan_ctx_s *ctx, tex_t *tex, int mip,
						   const char *name);
qfv_tex_t *Vulkan_LoadTexArray (struct vulkan_ctx_s *ctx, tex_t *tex,
								int layers, int mip, const char *name);
qfv_tex_t *Vulkan_LoadEnvMap (struct vulkan_ctx_s *ctx, tex_t *tex,
							  const char *name);
qfv_tex_t *Vulkan_LoadEnvSides (struct vulkan_ctx_s *ctx, tex_t **tex,
								const char *name);
VkImageView Vulkan_TexImageView (qfv_tex_t *tex) __attribute__((pure));
void Vulkan_UpdateTex (struct vulkan_ctx_s *ctx, qfv_tex_t *tex, tex_t *src,
					   int x, int  y, int layer, int mip, bool vert);
void Vulkan_UnloadTex (struct vulkan_ctx_s *ctx, qfv_tex_t *tex);
void Vulkan_Texture_Init (struct vulkan_ctx_s *ctx);
VkDescriptorSet Vulkan_CreateCombinedImageSampler (struct vulkan_ctx_s *ctx,
												   VkImageView view,
												   VkSampler sampler);
VkDescriptorSet Vulkan_CreateTextureDescriptor (struct vulkan_ctx_s *ctx,
												qfv_tex_t *tex,
												VkSampler sampler);
void Vulkan_FreeTexture (struct vulkan_ctx_s *ctx, VkDescriptorSet texture);

#endif//__QF_Vulkan_qf_texture_h
