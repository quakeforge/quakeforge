/*
	camera.h

	Scene camera data

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/02/18

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_scene_camera_h
#define __QF_scene_camera_h

#include "QF/qtypes.h"
#include "QF/simd/mat4f.h"

/** \defgroup scene_camera Camera data
	\ingroup scene
*/
///@{

typedef struct camera_s {
	struct scene_s *scene;	///< owning scene
	struct framebuffer_s *framebuffer;
	int32_t     id;			///< id in scene
	int32_t     transform;
	float       field_of_view;
	float       aspect;
	float       near_clip;
	float       far_clip;
} camera_t;

void Camera_GetViewMatrix (const camera_t *camera, mat4f_t mat);
void Camera_GetProjectionMatrix (const camera_t *camera, mat4f_t mat);

///@}

#endif//__QF_scene_camera_h
