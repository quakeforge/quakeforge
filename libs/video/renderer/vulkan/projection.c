/*
	proejct.c

	Vulkan projection matrices

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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

#include <math.h>

#include "QF/cvar.h"
#include "QF/Vulkan/projection.h"

#include "r_internal.h"

//FIXME? The box rotations (in particular top/bottom) for vulkan are not
//compatible with the other renderers, so need a local version
const mat4f_t qfv_box_rotations[] = {
	[BOX_FRONT] = {
		{ 1, 0, 0, 0},
		{ 0, 1, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_RIGHT] = {
		{ 0,-1, 0, 0},
		{ 1, 0, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_BEHIND] = {
		{-1, 0, 0, 0},
		{ 0,-1, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_LEFT] = {
		{ 0, 1, 0, 0},
		{-1, 0, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_BOTTOM] = {
		{ 0, 0, 1, 0},
		{ 0, 1, 0, 0},
		{-1, 0, 0, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_TOP] = {
		{ 0, 0,-1, 0},
		{ 0, 1, 0, 0},
		{ 1, 0, 0, 0},
		{ 0, 0, 0, 1}
	},
};

// Quake's world is z-up, x-forward, y-left, but Vulkan's world is
// z-forward, x-right, y-down.
const mat4f_t qfv_z_up = {
	{ 0, 0, 1, 0},
	{-1, 0, 0, 0},
	{ 0,-1, 0, 0},
	{ 0, 0, 0, 1},
};

void
QFV_Orthographic (mat4f_t proj, float xmin, float xmax, float ymin, float ymax,
				  float znear, float zfar)
{
	proj[0] = (vec4f_t) {
		2 / (xmax - xmin),
		0,
		0,
		0
	};
	proj[1] = (vec4f_t) {
		0,
		2 / (ymax - ymin),
		0,
		0
	};
	proj[2] = (vec4f_t) {
		0,
		0,
		1 / (znear - zfar),
		0
	};
	proj[3] = (vec4f_t) {
		-(xmax + xmin) / (xmax - xmin),
		-(ymax + ymin) / (ymax - ymin),
		znear / (znear - zfar),
		1,
	};
}

void
QFV_PerspectiveTan (mat4f_t proj, float fov_x, float fov_y)
{
	float       neard;

	neard = r_nearclip;

	proj[0] = (vec4f_t) { 1 / fov_x, 0, 0, 0 };
	proj[1] = (vec4f_t) { 0, 1 / fov_y, 0, 0 };
	proj[2] = (vec4f_t) { 0, 0, 0, 1 };
	proj[3] = (vec4f_t) { 0, 0, neard, 0 };
}

void
QFV_PerspectiveCos (mat4f_t proj, float fov)
{
	// square first for auto-abs (no support for > 180 degree fov)
	fov = fov * fov;
	float       t = sqrt ((1 - fov) / fov);
	QFV_PerspectiveTan (proj, t, t);
}
