#ifndef __QF_Vulkan_projection_h
#define __QF_Vulkan_projection_h

#include "QF/simd/types.h"

void QFV_Orthographic (mat4f_t proj, float xmin, float xmax,
					   float ymin, float ymax, float znear, float zfar);
// fov_x and fov_y are tan(fov/2) for x and y respectively
void QFV_PerspectiveTan (mat4f_t proj, float fov_x, float fov_y);
void QFV_PerspectiveCos (mat4f_t proj, float fov);

#endif//__QF_Vulkan_projection_h
