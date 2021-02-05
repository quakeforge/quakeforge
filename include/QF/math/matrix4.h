/*
	matrix4.h

	4x4 Matrix functions

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/30

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

#ifndef __QF_math_matrix4_h
#define __QF_math_matrix4_h

/** \defgroup mathlib_matrix4 4x4 matrix functions
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

#define Mat4Copy(a, b) \
	do { \
		QuatCopy ((a) + 0, (b) + 0); \
		QuatCopy ((a) + 4, (b) + 4); \
		QuatCopy ((a) + 8, (b) + 8); \
		QuatCopy ((a) + 12, (b) + 12); \
	} while (0)
#define Mat4Add(a, b, c) \
	do { \
		QuatAdd ((a) + 0, (b) + 0, (c) + 0); \
		QuatAdd ((a) + 4, (b) + 4, (c) + 4); \
		QuatAdd ((a) + 8, (b) + 8, (c) + 8); \
		QuatAdd ((a) + 12, (b) + 12, (c) + 12); \
	} while (0)
#define Mat4Subtract(a, b, c) \
	do { \
		QuatSubtract ((a) + 0, (b) + 0, (c) + 0); \
		QuatSubtract ((a) + 4, (b) + 4, (c) + 4); \
		QuatSubtract ((a) + 8, (b) + 8, (c) + 8); \
		QuatSubtract ((a) + 12, (b) + 12, (c) + 12); \
	} while (0)
#define Mat4Scale(a, b, c) \
	do { \
		QuatScale ((a) + 0, (b), (c) + 0); \
		QuatScale ((a) + 4, (b), (c) + 4); \
		QuatScale ((a) + 8, (b), (c) + 8); \
		QuatScale ((a) + 12, (b), (c) + 12); \
	} while (0)
#define Mat4CompMult(a, b, c) \
	do { \
		QuatCompMult ((a) + 0, (b) + 0, (c) + 0); \
		QuatCompMult ((a) + 4, (b) + 4, (c) + 4); \
		QuatCompMult ((a) + 8, (b) + 8, (c) + 8); \
		QuatCompMult ((a) + 12, (b) + 12, (c) + 12); \
	} while (0)
#define Mat4Zero(a) \
	memset ((a), 0, 16 * sizeof (a)[0])
#define Mat4Identity(a) \
	do { \
		Mat4Zero (a); \
		(a)[15] = (a)[10] = (a)[5] = (a)[0] = 1; \
	} while (0)
#define Mat4Expand(a) \
	QuatExpand ((a) + 0), \
	QuatExpand ((a) + 4), \
	QuatExpand ((a) + 8), \
	QuatExpand ((a) + 12)
#define Mat4Blend(m1,m2,b,m) \
	do { \
		QuatBlend ((m1) + 0, (m2) + 0, (b), (m) + 0); \
		QuatBlend ((m1) + 4, (m2) + 4, (b), (m) + 4); \
		QuatBlend ((m1) + 8, (m2) + 8, (b), (m) + 8); \
		QuatBlend ((m1) + 12, (m2) + 12, (b), (m) + 12); \
	} while (0)
#define Mat4MultAdd(a,s,b,c) \
	do { \
		QuatMultAdd ((a) + 0, s, (b) + 0, (c) + 0); \
		QuatMultAdd ((a) + 4, s, (b) + 4, (c) + 4); \
		QuatMultAdd ((a) + 8, s, (b) + 8, (c) + 8); \
		QuatMultAdd ((a) + 12, s, (b) + 12, (c) + 12); \
	} while (0)
#define Mat4Trace(a) ((a)[0] + (a)[5] + (a)[10] + (a)[15])

#define Mat3toMat4(a, b) \
	do { \
		VectorCopy ((a) + 0, (b) + 0); (b)[3] = 0; \
		VectorCopy ((a) + 3, (b) + 4); (b)[7] = 0; \
		VectorCopy ((a) + 6, (b) + 8); (b)[11] = 0; \
		QuatZero ((b) + 12); \
	} while (0)

void Mat4Init (const quat_t rot, const vec3_t scale, const vec3_t trans,
			   mat4_t mat);
void Mat4Transpose (const mat4_t a, mat4_t b);
int Mat4Inverse (const mat4_t a, mat4_t b);
void Mat4Mult (const mat4_t a, const mat4_t b, mat4_t c);
void Mat4MultVec (const mat4_t a, const vec3_t b, vec3_t c);
void Mat4as3MultVec (const mat4_t a, const vec3_t b, vec3_t c);
/** Decompose a 4x4 column major matrix into its component transformations.

	This gives the matrix's rotation as a quaternion, shear (XY, XZ, YZ),
	scale, and translation. Using the following sequence will give the
	same result as multiplying \a v by \a mat.

	QuatMultVec (rot, v, v);
	VectorShear (shear, v, v);
	VectorCompMult (scale, v, v);
	VectorAdd (trans, v, v);
*/
int Mat4Decompose (const mat4_t mat, quat_t rot, vec3_t shear, vec3_t scale,
				   vec3_t trans);

///@}

#endif // __QF_math_matrix4_h
