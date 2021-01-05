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

#endif//__QF_Vulkan_shader_h
