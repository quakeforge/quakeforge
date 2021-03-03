/*
	QF/simd/mat4f.h

	Matrix functions for mat4f_t (ie, float precision)

	Copyright (C) 2021  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_simd_mat4f_h
#define __QF_simd_mat4f_h

#include <immintrin.h>

#include "QF/simd/types.h"

GNU89INLINE inline void maddf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline void msubf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline void mmulf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline vec4f_t mvmulf (const mat4f_t m, vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t m3vmulf (const mat4f_t m, vec4f_t v) __attribute__((const));
GNU89INLINE inline void mat4fidentity (mat4f_t m);

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
maddf (mat4f_t c, const mat4f_t a, const mat4f_t b)
{
	c[0] = a[0] + b[0];
	c[1] = a[1] + b[1];
	c[2] = a[2] + b[2];
	c[3] = a[3] + b[3];
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
msubf (mat4f_t c, const mat4f_t a, const mat4f_t b)
{
	c[0] = a[0] - b[0];
	c[1] = a[1] - b[1];
	c[2] = a[2] - b[2];
	c[3] = a[3] - b[3];
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
mmulf (mat4f_t c, const mat4f_t a, const mat4f_t b)
{
	c[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2] + a[3] * b[0][3];
	c[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2] + a[3] * b[1][3];
	c[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2] + a[3] * b[2][3];
	c[3] = a[0] * b[3][0] + a[1] * b[3][1] + a[2] * b[3][2] + a[3] * b[3][3];
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
mvmulf (const mat4f_t m, vec4f_t v)
{
	return m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3] * v[3];
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
m3vmulf (const mat4f_t m, vec4f_t v)
{
	vec4f_t     w;
	w = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
	w[3] = 1;
	return w;
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
mat4fidentity (mat4f_t m)
{
	m[0] = (vec4f_t) { 1, 0, 0, 0 };
	m[1] = (vec4f_t) { 0, 1, 0, 0 };
	m[2] = (vec4f_t) { 0, 0, 1, 0 };
	m[3] = (vec4f_t) { 0, 0, 0, 1 };
}

#endif//__QF_simd_mat4f_h
