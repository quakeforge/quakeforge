/*
	QF/simd/vec4i.h

	Vector functions for vec4i_t (ie, int)

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

#ifndef __QF_simd_vec4i_h
#define __QF_simd_vec4i_h

#include <immintrin.h>
#include <math.h>

#include "QF/simd/types.h"

GNU89INLINE inline vec4i_t vabs4i (vec4i_t v) __attribute__((const));
GNU89INLINE inline int any4i (vec4i_t v) __attribute__((const));
GNU89INLINE inline int all4i (vec4i_t v) __attribute__((const));
GNU89INLINE inline int none4i (vec4i_t v) __attribute__((const));
GNU89INLINE inline vec4i_t loadvec3i (const int *v3) __attribute__((pure));
GNU89INLINE inline vec4i_t loadvec3i1 (const int *v3) __attribute__((pure));
GNU89INLINE inline void storevec3i (int *v3, vec4i_t v4);

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4i_t
vabs4i (vec4i_t v)
{
	const uint32_t  nan = ~0u >> 1;
	const vec4i_t   abs = { nan, nan, nan, nan };
	return (vec4i_t) ((vec4i_t) v & abs);
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
any4i (vec4i_t v)
{
#ifndef __SSE4_1__
	vec4i_t     t = (v != (vec4i_t) {});
	return (t[0] + t[1] + t[2] + t[3]) != 0;
#else
	return !__builtin_ia32_ptestz128 ((__v2di)v, (__v2di)v);
#endif
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
all4i (vec4i_t v)
{
	vec4i_t     t = (v == (vec4i_t) {});
#ifndef __SSE4_1__
	return (t[0] + t[1] + t[2] + t[3]) == 0;
#else
	return __builtin_ia32_ptestz128 ((__v2di)t, (__v2di)t);
#endif
}

#ifndef IMPLEMENT_VEC2I_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
none4i (vec4i_t v)
{
#ifndef __SSE4_1__
	vec4i_t     t = (v != (vec4i_t) {});
	return (t[0] + t[1] + t[2] + t[3]) == 0;
#else
	return __builtin_ia32_ptestz128 ((__v2di)v, (__v2di)v);
#endif
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4i_t
loadvec3i (const int *v3)
{
	vec4i_t v4 = { v3[0], v3[1], v3[2], 0 };
	return v4;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4i_t
loadvec3i1 (const int *v3)
{
	vec4i_t v4 = { v3[0], v3[1], v3[2], 1 };
	return v4;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
storevec3i (int *v3, vec4i_t v4)
{
	v3[0] = v4[0];
	v3[1] = v4[1];
	v3[2] = v4[2];
}

#endif//__QF_simd_vec4i_h
