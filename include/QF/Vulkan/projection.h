#ifndef __QF_Vulkan_projection_h
#define __QF_Vulkan_projection_h

#include "QF/simd/types.h"

void QFV_Orthographic (mat4f_t proj, float xmin, float xmax,
					   float ymin, float ymax, float znear, float zfar);
void QFV_Perspective (mat4f_t proj, float fov, float aspect);

#endif//__QF_Vulkan_projection_h
