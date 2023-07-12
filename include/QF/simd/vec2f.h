/*
	QF/simd/vec2f.h

	Vector functions for vec2f_t (ie, float precision)

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>
	Copyright (C) 2022  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_simd_vec2f_h
#define __QF_simd_vec2f_h

#ifdef __aarch64__
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif
#include <math.h>

#include "QF/simd/types.h"

GNU89INLINE inline vec2f_t vabs2f (vec2f_t v) __attribute__((const));
GNU89INLINE inline vec2f_t vsqrt2f (vec2f_t v) __attribute__((const));
GNU89INLINE inline vec2f_t vceil2f (vec2f_t v) __attribute__((const));
GNU89INLINE inline vec2f_t vfloor2f (vec2f_t v) __attribute__((const));
GNU89INLINE inline vec2f_t vtrunc2f (vec2f_t v) __attribute__((const));
/** 2D vector dot product.
 */
GNU89INLINE inline vec2f_t dot2f (vec2f_t a, vec2f_t b) __attribute__((const));
GNU89INLINE inline vec2f_t cmulf (vec2f_t a, vec2f_t b) __attribute__((const));
GNU89INLINE inline vec2f_t normal2f (vec2f_t v) __attribute__((pure));
GNU89INLINE inline vec2f_t magnitude2f (vec2f_t v) __attribute__((pure));

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
vabs2f (vec2f_t v)
{
	const uint32_t  nan = ~0u >> 1;
	const vec2i_t   abs = { nan, nan };
	return (vec2f_t) ((vec2i_t) v & abs);
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
vsqrt2f (vec2f_t v)
{
#ifdef __aarch64__
	return vsqrt_f32 (v);
#else
	vec4f_t     t = { v[0], v[1], 0, 0 };
	t = _mm_sqrt_ps (t);
	return (vec2f_t) { t[0], t[1] };
#endif
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
vceil2f (vec2f_t v)
{
#ifndef __SSE4_1__
	return (vec2f_t) {
		ceilf (v[0]),
		ceilf (v[1]),
	};
#else
	vec4f_t     t = { v[0], v[1], 0, 0 };
	t = _mm_ceil_ps (t);
	return (vec2f_t) { t[0], t[1] };
#endif
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
vfloor2f (vec2f_t v)
{
#ifndef __SSE4_1__
	return (vec2f_t) {
		floorf (v[0]),
		floorf (v[1]),
	};
#else
	vec4f_t     t = { v[0], v[1], 0, 0 };
	t = _mm_floor_ps (t);
	return (vec2f_t) { t[0], t[1] };
#endif
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
vtrunc2f (vec2f_t v)
{
#ifndef __SSE4_1__
	return (vec2f_t) {
		truncf (v[0]),
		truncf (v[1]),
	};
#else
	vec4f_t     t = { v[0], v[1], 0, 0 };
	t = _mm_round_ps (t, _MM_FROUND_TRUNC);
	return (vec2f_t) { t[0], t[1] };
#endif
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
dot2f (vec2f_t a, vec2f_t b)
{
	vec2f_t c = a * b;
	return (vec2f_t) { c[0] + c[1], c[0] + c[1] };
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
cmulf (vec2f_t a, vec2f_t b)
{
	vec2f_t     c1 = a * b[0];
	vec2f_t     c2 = a * b[1];
	return (vec2f_t) { c1[0] - c2[1], c1[1] + c2[0] };
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
normal2f (vec2f_t v)
{
	return v / vsqrt2f (dot2f (v, v));
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2f_t
magnitude2f (vec2f_t v)
{
	return vsqrt2f (dot2f (v, v));
}

#endif//__QF_simd_vec2f_h
