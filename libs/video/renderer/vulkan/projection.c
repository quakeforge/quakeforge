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
	float d = zfar - znear;
	float w = xmax - xmin;
	float h = ymax - ymin;
	float m = xmax + xmin;
	float c = ymax + ymin;
	float f = zfar;

	proj[0] = (vec4f_t) {  2/w,    0,    0, 0 };
	proj[1] = (vec4f_t) {    0,  2/h,    0, 0 };
	proj[2] = (vec4f_t) {    0,    0, -1/d, 0 };
	proj[3] = (vec4f_t) { -m/w, -c/h,  f/d, 1 };
}

void
QFV_OrthographicV (mat4f_t proj, vec4f_t mins, vec4f_t maxs)
{
	QFV_Orthographic (proj, mins[0], maxs[0], mins[1], maxs[1],
					  mins[2], maxs[2]);
}

void
QFV_PerspectiveTan (mat4f_t proj, float fov_x, float fov_y, float nearclip)
{
	float       n = nearclip;
	float       fx = fov_x;
	float       fy = fov_y;

	proj[0] = (vec4f_t) { 1/fx,   0, 0, 0 };
	proj[1] = (vec4f_t) {   0, 1/fy, 0, 0 };
	proj[2] = (vec4f_t) {   0,   0,  0, 1 };
	proj[3] = (vec4f_t) {   0,   0,  n, 0 };
}

void
QFV_InversePerspectiveTan (mat4f_t proj, float fov_x, float fov_y,
						   float nearclip)
{
	float       n = r_nearclip;
	float       fx = fov_x;
	float       fy = fov_y;
	proj[0] = (vec4f_t) { fx,  0, 0,  0  };
	proj[1] = (vec4f_t) {  0, fy, 0,  0  };
	proj[2] = (vec4f_t) {  0,  0, 0, 1/n };
	proj[3] = (vec4f_t) {  0,  0, 1,  0  };
}

void
QFV_PerspectiveTanFar (mat4f_t proj, float fov_x, float fov_y,
					   float nearclip, float farclip)
{
	float       n = nearclip;
	float       f = farclip;
	float       fx = fov_x;
	float       fy = fov_y;

	proj[0] = (vec4f_t) { 1/fx,  0,     0,     0 };
	proj[1] = (vec4f_t) {   0, 1/fy,    0,     0 };
	proj[2] = (vec4f_t) {   0,   0, -n/(n-f),  1 };
	proj[3] = (vec4f_t) {   0,   0, n*f/(n-f), 0 };
}

void
QFV_InversePerspectiveTanFar (mat4f_t proj, float fov_x, float fov_y,
							  float nearclip, float farclip)
{
	float       n = r_nearclip;
	float       f = farclip;
	float       fx = fov_x;
	float       fy = fov_y;
	proj[0] = (vec4f_t) { fx,  0,  0,      0      };
	proj[1] = (vec4f_t) {  0, fy,  0,      0      };
	proj[2] = (vec4f_t) {  0,  0,  0, (f-n)/(n*f) };
	proj[3] = (vec4f_t) {  0,  0,  1,     1/f     };
}

void
QFV_PerspectiveCos (mat4f_t proj, float fov, float nearclip)
{
	// square first for auto-abs (no support for > 180 degree fov)
	fov = fov * fov;
	float       t = sqrt ((1 - fov) / fov);
	QFV_PerspectiveTan (proj, t, t, nearclip);
}
