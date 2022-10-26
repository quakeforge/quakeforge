/*
	camera.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/scene/camera.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

void
Camera_GetViewMatrix (const camera_t *camera, mat4f_t view)
{
	vec4f_t     rotation = Transform_GetWorldRotation (camera->transform);
	vec4f_t     position = Transform_GetWorldPosition (camera->transform);
	mat4fquat (view, qconjf (rotation));
	// qconjf negates xyz but leaves w alone, which is what we want for
	// inverting the translation (essentially, rotation+position form a
	// dual quaternion, but without the complication of dealing with
	// half-translations)
	view[3] = mvmulf (view, qconjf (position));
}

void Camera_GetProjectionMatrix (const camera_t *camera, mat4f_t proj)
{
	float       v = camera->field_of_view;
	float       a = camera->aspect;
	float       f = camera->far_clip;
	float       n = camera->near_clip;

	proj[0] = (vec4f_t) { v / a, 0, 0, 0 };
	proj[1] = (vec4f_t) { 0, v, 0, 0 };
	proj[2] = (vec4f_t) { 0, 0, f / (f - n), 1 };
	proj[2] = (vec4f_t) { 0, 0, n * f / (n - f), 0 };
}
