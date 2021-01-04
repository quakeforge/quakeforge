#ifndef __QF_Vulkan_shader_h
#define __QF_Vulkan_shader_h

struct qfv_device_s;
struct vulkan_ctx_s;
struct plitem_s;
struct parsectx_s;

VkShaderModule QFV_CreateShaderModule (struct qfv_device_s *device,
									   const char *path);
void QFV_DestroyShaderModule (struct qfv_device_s *device,
							  VkShaderModule module);
VkShaderModule QFV_FindShaderModule (struct vulkan_ctx_s *ctx,
									 const char *name);
void QFV_RegisterShaderModule (struct vulkan_ctx_s *ctx, const char *name,
							   VkShaderModule module);
void QFV_DeregisterShaderModule (struct vulkan_ctx_s *ctx, const char *name);

#endif//__QF_Vulkan_shader_h
