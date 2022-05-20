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
#include <math.h>

#include "QF/simd/types.h"

GNU89INLINE inline vec4f_t vabs4f (vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t vsqrt4f (vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t vceil4f (vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t vfloor4f (vec4f_t v) __attribute__((const));
GNU89INLINE inline vec4f_t vtrunc4f (vec4f_t v) __attribute__((const));
/** 3D vector cross product.
 *
 * The w (4th) component can be any value on input, and is guaranteed to be 0
 * in the result. The result is not affected in any way by either vector's w
 * componemnt
 */
GNU89INLINE inline vec4f_t crossf (vec4f_t a, vec4f_t b) __attribute__((const));
/** 4D vector dot product.
 *
 * The w component *IS* significant, but if it is 0 in either vector, then
 * the result will be as for a 3D dot product.
 *
 * Note that the dot product is in all 4 of the return value's elements. This
 * helps optimize vector math as the scalar is already pre-spread. If just the
 * scalar is required, use result[0].
 */
GNU89INLINE inline vec4f_t dotf (vec4f_t a, vec4f_t b) __attribute__((const));
/** Quaternion product.
 *
 * The vector is interpreted as a quaternion instead of a regular 4D vector.
 * The quaternion may be of any magnitude, so this is more generally useful.
 * than if the quaternion was required to be unit length.
 */
GNU89INLINE inline vec4f_t qmulf (vec4f_t a, vec4f_t b) __attribute__((const));
/** Optimized quaterion-vector multiplication for vector rotation.
 *
 * \note This is the inverse of vqmulf
 *
 * If the vector's w component is not zero, then the result's w component
 * is the cosine of the full rotation angle scaled by the vector's w component.
 * The quaternion is assumed to be unit.
 */
GNU89INLINE inline vec4f_t qvmulf (vec4f_t q, vec4f_t v) __attribute__((const));
/** Optimized vector-quaterion multiplication for vector rotation.
 *
 * \note This is the inverse of qvmulf
 *
 * If the vector's w component is not zero, then the result's w component
 * is the cosine of the full rotation angle scaled by the vector's w component.
 * The quaternion is assumed to be unit.
 */
GNU89INLINE inline vec4f_t vqmulf (vec4f_t v, vec4f_t q) __attribute__((const));
/** Create the quaternion representing the shortest rotation from a to b.
 *
 * Both a and b are assumed to be 3D vectors (w components 0), but a resonable
 * (but incorrect) result will still be produced if either a or b is a 4D
 * vector. The rotation axis will be the same as if both vectors were 3D, but
 * the magnitude of the rotation will be different.
 */
GNU89INLINE inline vec4f_t qrotf (vec4f_t a, vec4f_t b) __attribute__((const));
/** Return the conjugate of the quaternion.
 *
 * That is, [-x, -y, -z, w].
 */
GNU89INLINE inline vec4f_t qconjf (vec4f_t q) __attribute__((const));
GNU89INLINE inline vec4f_t qexpf (vec4f_t q) __attribute__((const));
GNU89INLINE inline vec4f_t loadvec3f (const float *v3) __attribute__((pure));
GNU89INLINE inline void storevec3f (float *v3, vec4f_t v4);
GNU89INLINE inline vec4f_t normalf (vec4f_t v) __attribute__((pure));
GNU89INLINE inline vec4f_t magnitudef (vec4f_t v) __attribute__((pure));
GNU89INLINE inline vec4f_t magnitude3f (vec4f_t v) __attribute__((pure));

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vabs4f (vec4f_t v)
{
	const uint32_t  nan = ~0u >> 1;
	const vec4i_t   abs = { nan, nan, nan, nan };
	return (vec4f_t) ((vec4i_t) v & abs);
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vsqrt4f (vec4f_t v)
{
#ifndef __SSE__
	vec4f_t     r = { sqrtf (v[0]), sqrtf (v[1]), sqrtf (v[2]), sqrtf (v[3]) };
	return r;
#else
	return _mm_sqrt_ps (v);
#endif
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vceil4f (vec4f_t v)
{
#ifndef __SSE4_1__
	return (vec4f_t) {
		ceilf (v[0]),
		ceilf (v[1]),
		ceilf (v[2]),
		ceilf (v[3])
	};
#else
	return _mm_ceil_ps (v);
#endif
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vfloor4f (vec4f_t v)
{
#ifndef __SSE4_1__
	return (vec4f_t) {
		floorf (v[0]),
		floorf (v[1]),
		floorf (v[2]),
		floorf (v[3])
	};
#else
	return _mm_floor_ps (v);
#endif
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vtrunc4f (vec4f_t v)
{
#ifndef __SSE4_1__
	return (vec4f_t) {
		truncf (v[0]),
		truncf (v[1]),
		truncf (v[2]),
		truncf (v[3])
	};
#else
	return _mm_round_ps (v, _MM_FROUND_TRUNC);
#endif
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
crossf (vec4f_t a, vec4f_t b)
{
	vec4f_t c = a * (vec4f_t) {b[1], b[2], b[0], b[3]}
			  - b * (vec4f_t) {a[1], a[2], a[0], a[3]};
	return (vec4f_t) {c[1], c[2], c[0], c[3]};
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
dotf (vec4f_t a, vec4f_t b)
{
	vec4f_t c = a * b;
	c += (vec4f_t) { c[3], c[0], c[1], c[2] };
	c += (vec4f_t) { c[2], c[3], c[0], c[1] };
	return c;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
qmulf (vec4f_t a, vec4f_t b)
{
	// results in [2*as*bs, as*b + bs*a + a x b] ([scalar, vector] notation)
	// doesn't seem to adversly affect precision
	vec4f_t c = crossf (a, b) + a * b[3] + a[3] * b;
	vec4f_t d = dotf (a, b);
	// zero out the vector component of dot product so only the scalar remains
	d = (vec4f_t) { 0, 0, 0, d[3] };
	return c - d;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
qvmulf (vec4f_t q, vec4f_t v)
{
	float s = q[3];
	// zero the scalar of the quaternion. Results in an extra operation, but
	// avoids adding precision issues.
#ifndef __SSE4_1__
	q[3] = 0;
#else
	q = _mm_insert_ps (q, q, 0xf8);
#endif
	vec4f_t c = crossf (q, v);
	vec4f_t qv = dotf (q, v);	// q.w is 0 so v.w is irrelevant
	vec4f_t qq = dotf (q, q);

	return (s * s - qq) * v + 2 * (qv * q + s * c);
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
vqmulf (vec4f_t v, vec4f_t q)
{
	float s = q[3];
	// zero the scalar of the quaternion. Results in an extra operation, but
	// avoids adding precision issues.
#ifndef __SSE4_1__
	q[3] = 0;
#else
	q = _mm_insert_ps (q, q, 0xf8);
#endif
	vec4f_t c = crossf (q, v);
	vec4f_t qv = dotf (q, v);	// q.w is 0 so v.w is irrelevant
	vec4f_t qq = dotf (q, q);

	return (s * s - qq) * v + 2 * (qv * q - s * c);
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
qrotf (vec4f_t a, vec4f_t b)
{
	vec4f_t ma = vsqrt4f (dotf (a, a));
	vec4f_t mb = vsqrt4f (dotf (b, b));
	vec4f_t den = 2 * ma * mb;
	vec4f_t t = mb * a + ma * b;
	vec4f_t mba_mab = vsqrt4f (dotf (t, t));
	vec4f_t q = crossf (a, b) / mba_mab;
	q[3] = (mba_mab / den)[0];
	return q;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
qconjf (vec4f_t q)
{
	return (vec4f_t) { -q[0], -q[1], -q[2], q[3] };
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
qexpf (vec4f_t q)
{
	vec4f_t     th = magnitude3f (q);
	float       r = expf (q[3]);
	if (!th[0]) {
		return (vec4f_t) { 0, 0, 0, r };
	}
	float       c = cosf (th[0]);
	float       s = sinf (th[0]);
	vec4f_t     n = (r * s) * (q / th);
	n[3] = r * c;
	return n;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
loadvec3f (const float *v3)
{
	vec4f_t v4;

	v4 = (vec4f_t) { v3[0], v3[1], v3[2], 0 };
	return v4;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
storevec3f (float *v3, vec4f_t v4)
{
	v3[0] = v4[0];
	v3[1] = v4[1];
	v3[2] = v4[2];
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
normalf (vec4f_t v)
{
	return v / vsqrt4f (dotf (v, v));
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
magnitudef (vec4f_t v)
{
	return vsqrt4f (dotf (v, v));
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4f_t
magnitude3f (vec4f_t v)
{
	v[3] = 0;
	return vsqrt4f (dotf (v, v));
}

vec4f_t __attribute__((pure))
BarycentricCoords_vf (const vec4f_t **points, int num_points, vec4f_t p);

vspheref_t __attribute__((pure))
CircumSphere_vf (const vec4f_t *points, int num_points);

vspheref_t SmallestEnclosingBall_vf (const vec4f_t *points, int num_points);

#endif//__QF_simd_vec4f_h
