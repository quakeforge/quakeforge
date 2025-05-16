/*
	blend_palette.c

	Matrix(motor) palette to blend palette conversion

	Copyright (C) 2012-2025 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/11

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/scene/entity.h"

#include "r_internal.h"
#include "mod_internal.h"


static inline void
cofactor_matrix (mat4f_t cf, const mat4f_t m)
{
	cf[0] = crossf (m[1], m[2]);
	cf[1] = crossf (m[2], m[0]);
	cf[2] = crossf (m[0], m[1]);
	vec4i_t pos = dotf (m[0], cf[0]) >= 0;
	vec4i_t neg = ~pos;
	cf[3] = (vec4f_t)((pos & (vec4i_t)((vec4f_t) { 1, 1, 1, 1}))
					+ (neg & (vec4i_t)((vec4f_t) {-1,-1,-1,-1})));
}

VISIBLE mat4f_t *
Mod_BlendPalette (qfm_blend_t *blend_palette, uint32_t palette_size,
				  qfm_motor_t *motors, uint32_t num_motors, size_t extra)
{
	size_t size = 2 * palette_size * sizeof (mat4f_t) + extra;
	mat4f_t *frame = Hunk_TempAlloc (0, size);
	for (uint32_t i = 0; i < num_motors; i++) {
		auto out = &frame[i * 2];
		qfm_motor_to_mat (out[0], motors[i]);
		cofactor_matrix (out[1], out[0]);
	}
	for (int i = 0; i < 1; i++) {
		auto out = &frame[(num_motors + i) * 2];
		mat4fidentity (out[0]);
		mat4fidentity (out[1]);
		out[1][3] = (vec4f_t) { 1, 1, 1, 1 };
	}
	for (uint32_t i = num_motors + 1; i < palette_size; i++) {
		auto blend = &blend_palette[i];
		auto in = &frame[blend->indices[0] * 2];
		auto out = &frame[i * 2];

		float w = blend->weights[0] / 255.0;
		QuatScale (in[0], w, out[0]);
		for (int j = 1; j < 4 && blend->weights[j]; j++) {
			in = &frame[blend->indices[j] * 2];
			w = blend->weights[j] / 255.0;
			QuatMultAdd (out[0], w, in[0], out[0]);
		}
		cofactor_matrix (out[1], out[0]);
	}
	return frame;
}
