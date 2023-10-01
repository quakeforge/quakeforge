#ifndef __QF_Vulkan_projection_h
#define __QF_Vulkan_projection_h

#include "QF/simd/types.h"

void QFV_Orthographic (mat4f_t proj, float xmin, float xmax,
					   float ymin, float ymax, float znear, float zfar);
void QFV_OrthographicV (mat4f_t proj, vec4f_t mins, vec4f_t maxs);
// fov_x and fov_y are tan(fov/2) for x and y respectively
void QFV_PerspectiveTan (mat4f_t proj, float fov_x, float fov_y,
						 float nearclip);
void QFV_InversePerspectiveTan (mat4f_t proj, float fov_x, float fov_y,
								float nearclip);
void QFV_PerspectiveTanFar (mat4f_t proj, float fov_x, float fov_y,
							float nearclip, float farclip);
void QFV_InversePerspectiveTanFar (mat4f_t proj, float fov_x, float fov_y,
								   float nearclip, float farclip);
void QFV_PerspectiveCos (mat4f_t proj, float fov, float nearclip);

extern const mat4f_t qfv_z_up;
extern const mat4f_t qfv_box_rotations[6];

#endif//__QF_Vulkan_projection_h
