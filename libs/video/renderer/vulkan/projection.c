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

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#include "QF/cvar.h"
#include "QF/Vulkan/projection.h"

#include "r_internal.h"

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
QFV_Perspective (mat4f_t proj, float fov, float aspect)
{
	float       f = 1 / tan (fov * M_PI / 360);
	float       neard, fard;

	neard = r_nearclip->value;
	fard = r_farclip->value;

	proj[0] = (vec4f_t) { f / aspect, 0, 0, 0 };
	proj[1] = (vec4f_t) { 0, -f, 0, 0 };
	proj[2] = (vec4f_t) { 0, 0, fard / (neard - fard), -1 };
	proj[3] = (vec4f_t) { 0, 0, (neard * fard) / (neard - fard), 0 };
}
