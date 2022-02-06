/*
	QF/simd/types.h

	Type definitions for QF SIMD values

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

#ifndef __QF_simd_types_h
#define __QF_simd_types_h

#include <stdint.h>
#include <inttypes.h>

#define VEC_TYPE(t,n,s) \
	typedef t n __attribute__ ((vector_size (s*sizeof (t))))

/** Three element vector type for interfacing with compact data.
 *
 * This cannot be used directly by SIMD code and must be loaded and stored
 * using load_vec3d() and store_vec3d() respectively.
 */
typedef double vec3d_t[3];

VEC_TYPE (double, vec2d_t, 2);
VEC_TYPE (int64_t, vec2l_t, 2);

/** Four element vector type for horizontal (AOS) vector data.
 *
 * This is used for both vectors (3D and 4D) and quaternions. 3D vectors
 * are simply vec4d_t values with 0 in the 4th element.
 *
 * Also usable with vertical (SOA) code, in which case each vec4d_t represents
 * a single component from four vectors, or a single row/column (depending on
 * context) of an Nx4 or 4xN matrix.
 */
VEC_TYPE (double, vec4d_t, 4);

/** Used mostly for __builtin_shuffle.
 */
VEC_TYPE (int64_t, vec4l_t, 4);

/** Three element vector type for interfacing with compact data.
 *
 * This cannot be used directly by SIMD code and must be loaded and stored
 * using load_vec3f() and store_vec3f() respectively.
 */
typedef float vec3f_t[3];

VEC_TYPE (float, vec2f_t, 2);
VEC_TYPE (int, vec2i_t, 2);

/** Four element vector type for horizontal (AOS) vector data.
 *
 * This is used for both vectors (3D and 4D) and quaternions. 3D vectors
 * are simply vec4f_t values with 0 in the 4th element.
 *
 * Also usable with vertical (SOA) code, in which case each vec4f_t represents
 * a single component from four vectors, or a single row/column (depending on
 * context) of an Nx4 or 4xN matrix.
 */
VEC_TYPE (float, vec4f_t, 4);

/** Used mostly for __builtin_shuffle.
 */
VEC_TYPE (int, vec4i_t, 4);

#define VEC2D_FMT "[%.17g, %.17g]"
#define VEC2L_FMT "[%"PRIi64", %"PRIi64"]"
#define VEC4D_FMT "[%.17g, %.17g, %.17g, %.17g]"
#define VEC4L_FMT "[%"PRIi64", %"PRIi64", %"PRIi64", %"PRIi64"]"
#define VEC2F_FMT "[%.9g, %.9g]"
#define VEC2I_FMT "[%d, %d]"
#define VEC4F_FMT "[%.9g, %.9g, %.9g, %.9g]"
#define VEC4I_FMT "[%d, %d, %d, %d]"
#define VEC2_EXP(v) (v)[0], (v)[1]
#define VEC4_EXP(v) (v)[0], (v)[1], (v)[2], (v)[3]

#define MAT4_ROW(m, r) (m)[0][r], (m)[1][r], (m)[2][r], (m)[3][r]

typedef vec4f_t mat4f_t[4];
typedef vec4i_t mat4i_t[4];

typedef struct vspheref_s {
	vec4f_t     center; // w set to 1
	float       radius;
} vspheref_t;

#include <immintrin.h>
#ifndef __SSE__
#define _mm_xor_ps __qf_mm_xor_ps
#define _mm_and_ps __qf_mm_and_ps
GNU89INLINE inline __m128 _mm_xor_ps (__m128 a, __m128 b);
GNU89INLINE inline __m128 _mm_and_ps (__m128 a, __m128 b);
#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
__m128 _mm_xor_ps (__m128 a, __m128 b)
{
	return (__m128) ((vec4i_t) a ^ (vec4i_t) b);
}
#ifndef IMPLEMENT_MAT4F_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
__m128 _mm_and_ps (__m128 a, __m128 b)
{
	return (__m128) ((vec4i_t) a & (vec4i_t) b);
}
#endif

#endif//__QF_simd_types_h
