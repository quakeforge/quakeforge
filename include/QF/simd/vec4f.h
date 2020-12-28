/*
	QF/simd/vec4f.h

	Vector functions for vec4f_t (ie, float precision)

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_simd_vec4f_h
#define __QF_simd_vec4f_h

#include <immintrin.h>

#include "QF/simd/types.h"

/** 3D vector cross product.
 *
 * The w (4th) component can be any value on input, and is guaranteed to be 0
 * in the result. The result is not affected in any way by either vector's w
 * componemnt
 */
vec4f_t crossf (vec4f_t a, vec4f_t b) __attribute__((const));
vec4f_t
crossf (vec4f_t a, vec4f_t b)
{
	static const vec4i_t A = {1, 2, 0, 3};
	vec4f_t c = a * __builtin_shuffle (b, A) - __builtin_shuffle (a, A) * b;
	return __builtin_shuffle(c, A);
}

/** 4D vector dot product.
 *
 * The w component *IS* significant, but if it is 0 in either vector, then
 * the result will be as for a 3D dot product.
 *
 * Note that the dot product is in all 4 of the return value's elements. This
 * helps optimize vector math as the scalar is already pre-spread. If just the
 * scalar is required, use result[0].
 */
vec4f_t dotf (vec4f_t a, vec4f_t b) __attribute__((const));
vec4f_t
dotf (vec4f_t a, vec4f_t b)
{
	vec4f_t c = a * b;
	c = _mm_hadd_ps (c, c);
	c = _mm_hadd_ps (c, c);
	return c;
}

/** Quaternion product.
 *
 * The vector is interpreted as a quaternion instead of a regular 4D vector.
 * The quaternion may be of any magnitude, so this is more generally useful.
 * than if the quaternion was required to be unit length.
 */
vec4f_t qmulf (vec4f_t a, vec4f_t b) __attribute__((const));
vec4f_t
qmulf (vec4f_t a, vec4f_t b)
{
	// results in [2*as*bs, as*b + bs*a + a x b] ([scalar, vector] notation)
	// doesn't seem to adversly affect precision
	vec4f_t c = crossf (a, b) + a * b[3] + a[3] * b;
	vec4f_t d = dotf (a, b);
	// zero out the vector component of dot product so only the scalar remains
	d = _mm_insert_ps (d, d, 0xf7);
	return c - d;
}

/** Optimized quaterion-vector multiplication for vector rotation.
 *
 * If the vector's w component is not zero, then the result's w component
 * is the cosine of the full rotation angle scaled by the vector's w component.
 * The quaternion is assumed to be unit.
 */
vec4f_t qvmulf (vec4f_t q, vec4f_t v) __attribute__((const));
vec4f_t
qvmulf (vec4f_t q, vec4f_t v)
{
	float s = q[3];
	// zero the scalar of the quaternion. Results in an extra operation, but
	// avoids adding precision issues.
	q = _mm_insert_ps (q, q, 0xf8);
	vec4f_t c = crossf (q, v);	// q.w is 0 so v.w is irrelevant
	vec4f_t qv = dotf (q, v);
	vec4f_t qq = dotf (q, q);

	return (s * s - qq) * v + 2 * (qv * q + s * c);
}

/** Create the quaternion representing the shortest rotation from a to b.
 *
 * Both a and b are assumed to be 3D vectors (w components 0), but a resonable
 * (but incorrect) result will still be produced if either a or b is a 4D
 * vector. The rotation axis will be the same as if both vectors were 3D, but
 * the magnitude of the rotation will be different.
 */
vec4f_t qrotf (vec4f_t a, vec4f_t b) __attribute__((const));
vec4f_t
qrotf (vec4f_t a, vec4f_t b)
{
	vec4f_t ma = _mm_sqrt_ps (dotf (a, a));
	vec4f_t mb = _mm_sqrt_ps (dotf (b, b));
	vec4f_t den = 2 * ma * mb;
	vec4f_t t = mb * a + ma * b;
	vec4f_t mba_mab = _mm_sqrt_ps (dotf (t, t));
	vec4f_t q = crossf (a, b) / mba_mab;
	q[3] = (mba_mab / den)[0];
	return q;
}

vec4f_t loadvec3f (const float v3[3]) __attribute__((pure, access(read_only, 1)));
vec4f_t
loadvec3f (const float v3[3])
{
	vec4f_t v4;

	// this had to be in asm otherwise gcc thinks v4 is only partially
	// initialized, and gcc 10 does not use the zero flags when generating
	// the code, resulting in a memory access to load a 0 into v4[3]
	//
	// The first instruction zeros v4[3] while loading v4[0]
	asm ("\n\
		vinsertps $0x08, %1, %0, %0     \n\
		vinsertps $0x10, %2, %0, %0     \n\
		vinsertps $0x20, %3, %0, %0     \n\
		"
		: "=v"(v4)
		: "m"(v3[0]), "m"(v3[1]), "m"(v3[2]));
	return v4;
}

void storevec3f (float v3[3], vec4f_t v4) __attribute__((access (write_only, 1)));
void storevec3f (float v3[3], vec4f_t v4)
{
	v3[0] = v4[0];
	v3[1] = v4[1];
	v3[2] = v4[2];
}

#endif//__QF_simd_vec4f_h
