/*
	QF/simd/vec2d.h

	Vector functions for vec2d_t (ie, double precision)

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

#ifndef __QF_simd_vec2d_h
#define __QF_simd_vec2d_h

#ifdef __aarch64__
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif

#include "QF/simd/types.h"

GNU89INLINE inline vec2d_t vsqrt2d (vec2d_t v) __attribute__((const));
GNU89INLINE inline vec2d_t vceil2d (vec2d_t v) __attribute__((const));
GNU89INLINE inline vec2d_t vfloor2d (vec2d_t v) __attribute__((const));
GNU89INLINE inline vec2d_t vtrunc2d (vec2d_t v) __attribute__((const));
/** 2D vector dot product.
 */
GNU89INLINE inline vec2d_t dot2d (vec2d_t a, vec2d_t b) __attribute__((const));
GNU89INLINE inline vec2d_t cmuld (vec2d_t a, vec2d_t b) __attribute__((const));

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vsqrt2d (vec2d_t v)
{
#ifdef __aarch64__
	return vsqrtq_f64 (v);
#else
	return _mm_sqrt_pd (v);
#endif
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vceil2d (vec2d_t v)
{
#ifndef __SSE4_1__
	return (vec2d_t) {
		ceil (v[0]),
		ceil (v[1]),
	};
#else
	return _mm_ceil_pd (v);
#endif
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vfloor2d (vec2d_t v)
{
#ifndef __SSE4_1__
	return (vec2d_t) {
		floor (v[0]),
		floor (v[1]),
	};
#else
	return _mm_floor_pd (v);
#endif
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vtrunc2d (vec2d_t v)
{
#ifndef __SSE4_1__
	return (vec2d_t) {
		trunc (v[0]),
		trunc (v[1]),
	};
#else
	return _mm_round_pd (v, _MM_FROUND_TRUNC);
#endif
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
dot2d (vec2d_t a, vec2d_t b)
{
	vec2d_t c = a * b;
	// gcc-11 does a good job with hadd
	c = (vec2d_t) { c[0] + c[1], c[0] + c[1] };
	return c;
}

#ifndef IMPLEMENT_VEC2F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
cmuld (vec2d_t a, vec2d_t b)
{
	vec2d_t     c1 = a * b[0];
	vec2d_t     c2 = a * b[1];
#ifndef __SSE3__
	return (vec2d_t) { c1[0] - c2[1], c1[1] + c2[0] };
#else
	return _mm_addsub_pd (c1, (vec2d_t) { c2[1], c2[0] });
#endif
}

#endif//__QF_simd_vec2d_h
