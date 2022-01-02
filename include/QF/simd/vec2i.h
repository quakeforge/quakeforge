/*
	QF/simd/vec2i.h

	Vector functions for vec2i_t (ie, int)

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

#ifndef __QF_simd_vec2i_h
#define __QF_simd_vec2i_h

#include <immintrin.h>
#include <math.h>

#include "QF/simd/types.h"

GNU89INLINE inline vec2i_t vabs2i (vec2i_t v) __attribute__((const));
GNU89INLINE inline int any2i (vec2i_t v) __attribute__((const));
GNU89INLINE inline int all2i (vec2i_t v) __attribute__((const));
GNU89INLINE inline int none2i (vec2i_t v) __attribute__((const));

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec2i_t
vabs2i (vec2i_t v)
{
	const uint32_t  nan = ~0u >> 1;
	const vec2i_t   abs = { nan, nan };
	return (vec2i_t) ((vec2i_t) v & abs);
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
any2i (vec2i_t v)
{
	vec2i_t     t = _m_pcmpeqd (v, (vec2i_t) {0, 0});
	return _mm_hadd_pi32 (t, t)[0] > -2;
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
all2i (vec2i_t v)
{
	vec2i_t     t = _m_pcmpeqd (v, (vec2i_t) {0, 0});
	return _mm_hadd_pi32 (t, t)[0] == 0;
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
none2i (vec2i_t v)
{
	vec2i_t     t = _m_pcmpeqd (v, (vec2i_t) {0, 0});
	return _mm_hadd_pi32 (t, t)[0] == -2;
}

#endif//__QF_simd_vec2i_h
