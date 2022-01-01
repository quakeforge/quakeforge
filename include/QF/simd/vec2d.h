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

#include <immintrin.h>

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
	return _mm_sqrt_pd (v);
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vceil2d (vec2d_t v)
{
	return _mm_ceil_pd (v);
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vfloor2d (vec2d_t v)
{
	return _mm_floor_pd (v);
}

#ifndef IMPLEMENT_VEC2D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2d_t
vtrunc2d (vec2d_t v)
{
	return _mm_round_pd (v, _MM_FROUND_TRUNC);
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
	c = _mm_hadd_pd (c, c);
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
	vec2d_t     c = a * b[0];
	c = _mm_addsub_pd (c, (vec2d_t) { c[1], c[0] });
	return c;
}

#endif//__QF_simd_vec2d_h
