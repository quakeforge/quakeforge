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

#include "QF/simd/types.h"

GNU89INLINE inline void maddf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline void msubf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline void mmulf (mat4f_t c, const mat4f_t a, const mat4f_t b);
GNU89INLINE inline vec4f_t mvmulf (const mat4f_t m, vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t m3vmulf (const mat4f_t m, vec4f_t v) __attribute__((const));
GNU89INLINE inline void mat4fidentity (mat4f_t m);
GNU89INLINE inline void mat4ftranspose (mat4f_t t, const mat4f_t m);
GNU89INLINE inline void mat4fquat (mat4f_t m, vec4f_t q);

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
	mat4f_t     t;
	t[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2] + a[3] * b[0][3];
	t[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2] + a[3] * b[1][3];
	t[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2] + a[3] * b[2][3];
	t[3] = a[0] * b[3][0] + a[1] * b[3][1] + a[2] * b[3][2] + a[3] * b[3][3];

	c[0] = t[0];
	c[1] = t[1];
	c[2] = t[2];
	c[3] = t[3];
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
	w[3] = v[3];
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

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
mat4ftranspose (mat4f_t t, const mat4f_t m)
{
	vec4f_t     a = m[0];
	vec4f_t     b = m[1];
	vec4f_t     c = m[2];
	vec4f_t     d = m[3];
	t[0] = (vec4f_t) { a[0], b[0], c[0], d[0] };
	t[1] = (vec4f_t) { a[1], b[1], c[1], d[1] };
	t[2] = (vec4f_t) { a[2], b[2], c[2], d[2] };
	t[3] = (vec4f_t) { a[3], b[3], c[3], d[3] };
}

#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
mat4fquat (mat4f_t m, vec4f_t q)
{
	vec4f_t xq = q[0] * q;
	vec4f_t yq = q[1] * q;
	vec4f_t zq = q[2] * q;
	vec4f_t wq = q[3] * q;

#define shuff103(v) (vec4f_t) {v[1], v[0], v[3], v[2]}
#define shuff230(v) (vec4f_t) {v[2], v[3], v[0], v[1]}
#define shuff321(v) (vec4f_t) {v[3], v[2], v[1], v[0]}
#define p (0)
#define m (1u << 31)
	static const vec4i_t mpm = { m, p, m, 0 };
	static const vec4i_t pmm = { p, m, m, 0 };
	static const vec4i_t mmp = { m, m, p, 0 };
	static const vec4i_t mask = { ~0u, ~0u, ~0u, 0 };
#undef p
#undef m
	{
		vec4f_t a = xq;
		vec4f_t b = (vec4f_t) ((vec4i_t) shuff103 (yq) ^ mpm);
		vec4f_t c = (vec4f_t) ((vec4i_t) shuff230 (zq) ^ pmm);
		vec4f_t d = (vec4f_t) ((vec4i_t) shuff321 (wq) ^ mmp);
		// column: ww + xx - yy - zz // 2xy + 2wz // 2zx - 2wy // 0
		m[0] = (vec4f_t) ((vec4i_t) (a + b - c - d) & mask);
	}
	{
		vec4f_t a = (vec4f_t) ((vec4i_t) shuff103 (xq) ^ mpm);
		vec4f_t b = yq;
		vec4f_t c = (vec4f_t) ((vec4i_t) shuff321 (zq) ^ mmp);
		vec4f_t d = (vec4f_t) ((vec4i_t) shuff230 (wq) ^ pmm);
		// column: 2xy - 2wz // ww - xx + yy - zz // 2yz + 2wx // 0
		m[1] = (vec4f_t) ((vec4i_t) (b + c - a - d) & mask);
	}
	{
		vec4f_t a = (vec4f_t) ((vec4i_t) shuff230 (xq) ^ pmm);
		vec4f_t b = (vec4f_t) ((vec4i_t) shuff321 (yq) ^ mmp);
		vec4f_t c = zq;
		vec4f_t d = (vec4f_t) ((vec4i_t) shuff103 (wq) ^ mpm);
		// column: 2xz + 2wy // 2yz - 2wx // ww - xx - yy + zz // 0
		m[2] = (vec4f_t) ((vec4i_t) (a - b + c - d) & mask);
	}
	m[3] = (vec4f_t) { 0, 0, 0, 1 };
}

#endif//__QF_simd_mat4f_h
