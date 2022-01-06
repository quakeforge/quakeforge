/*
	QF/simd/vec4d.h

	Vector functions for vec4d_t (ie, double precision)

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

#ifndef __QF_simd_vec4d_h
#define __QF_simd_vec4d_h

#include <immintrin.h>

#include "QF/simd/types.h"

GNU89INLINE inline vec4d_t vsqrt4d (vec4d_t v) __attribute__((const));
GNU89INLINE inline vec4d_t vceil4d (vec4d_t v) __attribute__((const));
GNU89INLINE inline vec4d_t vfloor4d (vec4d_t v) __attribute__((const));
GNU89INLINE inline vec4d_t vtrunc4d (vec4d_t v) __attribute__((const));
/** 3D vector cross product.
 *
 * The w (4th) component can be any value on input, and is guaranteed to be 0
 * in the result. The result is not affected in any way by either vector's w
 * componemnt
 */
GNU89INLINE inline vec4d_t crossd (vec4d_t a, vec4d_t b) __attribute__((const));
/** 4D vector dot product.
 *
 * The w component *IS* significant, but if it is 0 in either vector, then
 * the result will be as for a 3D dot product.
 *
 * Note that the dot product is in all 4 of the return value's elements. This
 * helps optimize vector math as the scalar is already pre-spread. If just the
 * scalar is required, use result[0].
 */
GNU89INLINE inline vec4d_t dotd (vec4d_t a, vec4d_t b) __attribute__((const));
/** Quaternion product.
 *
 * The vector is interpreted as a quaternion instead of a regular 4D vector.
 * The quaternion may be of any magnitude, so this is more generally useful.
 * than if the quaternion was required to be unit length.
 */
GNU89INLINE inline vec4d_t qmuld (vec4d_t a, vec4d_t b) __attribute__((const));
/** Optimized quaterion-vector multiplication for vector rotation.
 *
 * \note This is the inverse of vqmulf
 *
 * If the vector's w component is not zero, then the result's w component
 * is the cosine of the full rotation angle scaled by the vector's w component.
 * The quaternion is assumed to be unit. If the quaternion is not unit, the
 * vector will be scaled by the square of the quaternion's magnitude.
 */
GNU89INLINE inline vec4d_t qvmuld (vec4d_t q, vec4d_t v) __attribute__((const));
/** Optimized vector-quaterion multiplication for vector rotation.
 *
 * \note This is the inverse of qvmulf
 *
 * If the vector's w component is not zero, then the result's w component
 * is the cosine of the full rotation angle scaled by the vector's w component.
 * The quaternion is assumed to be unit. If the quaternion is not unit, the
 * vector will be scaled by the square of the quaternion's magnitude.
 */
GNU89INLINE inline vec4d_t vqmuld (vec4d_t v, vec4d_t q) __attribute__((const));
/** Create the quaternion representing the shortest rotation from a to b.
 *
 * Both a and b are assumed to be 3D vectors (w components 0), but a resonable
 * (but incorrect) result will still be produced if either a or b is a 4D
 * vector. The rotation axis will be the same as if both vectors were 3D, but
 * the magnitude of the rotation will be different.
 */
GNU89INLINE inline vec4d_t qrotd (vec4d_t a, vec4d_t b) __attribute__((const));
/** Return the conjugate of the quaternion.
 *
 * That is, [-x, -y, -z, w].
 */
GNU89INLINE inline vec4d_t qconjd (vec4d_t q) __attribute__((const));
GNU89INLINE inline vec4d_t loadvec3d (const double v3[]) __attribute__((pure));
GNU89INLINE inline void storevec3d (double v3[3], vec4d_t v4);
GNU89INLINE inline vec4l_t loadvec3l (const int64_t *v3) __attribute__((pure));
GNU89INLINE inline vec4l_t loadvec3l1 (const int64_t *v3) __attribute__((pure));
GNU89INLINE inline void storevec3l (int64_t *v3, vec4l_t v4);

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
vsqrt4d (vec4d_t v)
{
	return _mm256_sqrt_pd (v);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
vceil4d (vec4d_t v)
{
	return _mm256_ceil_pd (v);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
vfloor4d (vec4d_t v)
{
	return _mm256_floor_pd (v);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
vtrunc4d (vec4d_t v)
{
	return _mm256_round_pd (v, _MM_FROUND_TRUNC);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
crossd (vec4d_t a, vec4d_t b)
{
	static const vec4l_t A = {1, 2, 0, 3};
	vec4d_t c = a * __builtin_shuffle (b, A);
	vec4d_t d = __builtin_shuffle (a, A) * b;
	c = c - d;
	return __builtin_shuffle(c, A);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
dotd (vec4d_t a, vec4d_t b)
{
	vec4d_t c = a * b;
	c = _mm256_hadd_pd (c, c);
	static const vec4l_t A = {2, 3, 0, 1};
	c += __builtin_shuffle(c, A);
	return c;
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
qmuld (vec4d_t a, vec4d_t b)
{
	// results in [2*as*bs, as*b + bs*a + a x b] ([scalar, vector] notation)
	// doesn't seem to adversly affect precision
	vec4d_t c = crossd (a, b) + a * b[3] + a[3] * b;
	vec4d_t d = dotd (a, b);
	// zero out the vector component of dot product so only the scalar remains
	d = (vec4d_t) { 0, 0, 0, d[3] };
	return c - d;
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
qvmuld (vec4d_t q, vec4d_t v)
//             ^^^        ^^^
{
	double s = q[3];
	// zero the scalar of the quaternion. Results in an extra operation, but
	// avoids adding precision issues.
	vec4d_t z = {};
	q = _mm256_blend_pd (q, z, 0x08);
	vec4d_t c = crossd (q, v);
	vec4d_t qv = dotd (q, v);	// q.w is 0 so v.w is irrelevant
	vec4d_t qq = dotd (q, q);

	//                                   vvv
	return (s * s - qq) * v + 2 * (qv * q + s * c);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
vqmuld (vec4d_t v, vec4d_t q)
//             ^^^        ^^^
{
	double s = q[3];
	// zero the scalar of the quaternion. Results in an extra operation, but
	// avoids adding precision issues.
	vec4d_t z = {};
	q = _mm256_blend_pd (q, z, 0x08);
	vec4d_t c = crossd (q, v);
	vec4d_t qv = dotd (q, v);	// q.w is 0 so v.w is irrelevant
	vec4d_t qq = dotd (q, q);

	//                                   vvv
	return (s * s - qq) * v + 2 * (qv * q - s * c);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
qrotd (vec4d_t a, vec4d_t b)
{
	vec4d_t ma = vsqrt4d (dotd (a, a));
	vec4d_t mb = vsqrt4d (dotd (b, b));
	vec4d_t den = 2 * ma * mb;
	vec4d_t t = mb * a + ma * b;
	vec4d_t mba_mab = vsqrt4d (dotd (t, t));
	vec4d_t q = crossd (a, b) / mba_mab;
	q[3] = (mba_mab / den)[0];
	return q;
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
qconjd (vec4d_t q)
{
	const uint64_t sign = UINT64_C(1) << 63;
	const vec4l_t neg = { sign, sign, sign, 0 };
	return _mm256_xor_pd (q, (__m256d) neg);
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4d_t
loadvec3d (const double v3[])
{
	vec4d_t v4 = {};

	v4[0] = v3[0];
	v4[1] = v3[1];
	v4[2] = v3[2];
	return v4;
}

#ifndef IMPLEMENT_VEC4D_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
storevec3d (double v3[3], vec4d_t v4)
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
vec4l_t
loadvec3l (const int64_t *v3)
{
	vec4l_t v4 = { v3[0], v3[1], v3[2], 0 };
	return v4;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
vec4l_t
loadvec3l1 (const int64_t *v3)
{
	vec4l_t v4 = { v3[0], v3[1], v3[2], 1 };
	return v4;
}

#ifndef IMPLEMENT_VEC4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
storevec3l (int64_t *v3, vec4l_t v4)
{
	v3[0] = v4[0];
	v3[1] = v4[1];
	v3[2] = v4[2];
}

#endif//__QF_simd_vec4d_h
