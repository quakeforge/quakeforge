#ifndef __QF_Vulkan_qf_texture_h
#define __QF_Vulkan_qf_texture_h

#include "QF/image.h"
#include "QF/Vulkan/qf_vid.h"

typedef struct qfv_tex_s {
	VkDeviceMemory memory;
	VkImage     image;
	VkImageView view;
} qfv_tex_t;

typedef struct texturectx_s {
	VkDescriptorPool pool;
	VkDescriptorSetLayout setLayout;
} texturectx_t;

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
void Vulkan_UnloadTex (struct vulkan_ctx_s *ctx, qfv_tex_t *tex);
void Vulkan_Texture_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Texture_Shutdown (struct vulkan_ctx_s *ctx);
VkDescriptorSet Vulkan_CreateCombinedImageSampler (struct vulkan_ctx_s *ctx,
												   VkImageView view,
												   VkSampler sampler);
VkDescriptorSet Vulkan_CreateTextureDescriptor (struct vulkan_ctx_s *ctx,
												qfv_tex_t *tex,
												VkSampler sampler);
void Vulkan_FreeTexture (struct vulkan_ctx_s *ctx, VkDescriptorSet texture);

#endif//__QF_Vulkan_qf_texture_h
