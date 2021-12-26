/*
	matrix3.h

	3x3 Matrix functions

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/8/18

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

#ifndef __QF_math_matrix3_h
#define __QF_math_matrix3_h

/** \defgroup mathlib_matrix3 3x3 matrix functions
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

#define Mat3Copy(a, b) \
	do { \
		VectorCopy ((a) + 0, (b) + 0); \
		VectorCopy ((a) + 3, (b) + 3); \
		VectorCopy ((a) + 6, (b) + 6); \
	} while (0)
#define Mat3Add(a, b, c) \
	do { \
		VectorAdd ((a) + 0, (b) + 0, (c) + 0); \
		VectorAdd ((a) + 3, (b) + 3, (c) + 3); \
		VectorAdd ((a) + 6, (b) + 6, (c) + 6); \
	} while (0)
#define Mat3Subtract(a, b, c) \
	do { \
		VectorSubtract ((a) + 0, (b) + 0, (c) + 0); \
		VectorSubtract ((a) + 3, (b) + 3, (c) + 3); \
		VectorSubtract ((a) + 6, (b) + 6, (c) + 6); \
	} while (0)
#define Mat3Scale(a, b, c) \
	do { \
		VectorScale ((a) + 0, (b), (c) + 0); \
		VectorScale ((a) + 3, (b), (c) + 3); \
		VectorScale ((a) + 6, (b), (c) + 6); \
	} while (0)
#define Mat3CompMult(a, b, c) \
	do { \
		VectorCompMult ((a) + 0, (b) + 0, (c) + 0); \
		VectorCompMult ((a) + 3, (b) + 3, (c) + 3); \
		VectorCompMult ((a) + 6, (b) + 6, (c) + 6); \
	} while (0)
#define Mat3Zero(a) \
	memset ((a), 0, 9 * sizeof (a)[0])
#define Mat3Identity(a) \
	do { \
		Mat3Zero (a); \
		(a)[8] = (a)[4] = (a)[0] = 1; \
	} while (0)
#define Mat3Expand(a) \
	VectorExpand ((a) + 0), \
	VectorExpand ((a) + 3), \
	VectorExpand ((a) + 6)
#define Mat3Blend(m1,m2,b,m) \
	do { \
		VectorBlend ((m1) + 0, (m2) + 0, (b), (m) + 0); \
		VectorBlend ((m1) + 3, (m2) + 3, (b), (m) + 3); \
		VectorBlend ((m1) + 6, (m2) + 6, (b), (m) + 6); \
	} while (0)
#define Mat3MultAdd(a,s,b,c) \
	do { \
		VectorMultAdd ((a) + 0, s, (b) + 0, (c) + 0); \
		VectorMultAdd ((a) + 3, s, (b) + 3, (c) + 3); \
		VectorMultAdd ((a) + 6, s, (b) + 6, (c) + 6); \
	} while (0)
#define Mat3Trace(a) ((a)[0] + (a)[4] + (a)[8])
#define Mat4toMat3(a, b) \
	do { \
		VectorCopy ((a) + 0, (b) + 0); \
		VectorCopy ((a) + 4, (b) + 3); \
		VectorCopy ((a) + 8, (b) + 6); \
	} while (0)

void Mat3Init (const quat_t rot, const vec3_t scale, mat3_t mat);
void Mat3Transpose (const mat3_t a, mat3_t b);
vec_t Mat3Determinant (const mat3_t m) __attribute__((pure));
int Mat3Inverse (const mat3_t a, mat3_t b);
void Mat3Mult (const mat3_t a, const mat3_t b, mat3_t c);
void Mat3MultVec (const mat3_t a, const vec3_t b, vec3_t c);
void Mat3SymEigen (const mat3_t m, vec3_t e);
/** Decompose a 3x3 column major matrix into its component transformations.

	This gives the matrix's rotation as a quaternion, shear (XY, XZ, YZ),
	and scale. Using the following sequence will give the same result as
	multiplying \a v by \a mat.

	QuatMultVec (rot, v, v);
	VectorShear (shear, v, v);
	VectorCompMult (scale, v, v);
*/
int Mat3Decompose (const mat3_t mat, quat_t rot, vec3_t shear, vec3_t scale);

///@}

#endif // __QF_math_matrix3_h
