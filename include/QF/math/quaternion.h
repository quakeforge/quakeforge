/*
	quaternion.h

	Quaternion functions

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/4/7

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

#ifndef __QF_math_quaternion_h
#define __QF_math_quaternion_h

/** \defgroup mathlib_quaternion Quaternion functions
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

extern const vec_t *const quat_origin;

#define QDotProduct(a,b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] \
						  + (a)[2] * (b)[2] + (a)[3] * (b)[3])
#define QuatSubtract(a,b,c) \
	do { \
		(c)[0] = (a)[0] - (b)[0]; \
		(c)[1] = (a)[1] - (b)[1]; \
		(c)[2] = (a)[2] - (b)[2]; \
		(c)[3] = (a)[3] - (b)[3]; \
	} while (0)
#define QuatNegate(a,b) \
	do { \
		(b)[0] = -(a)[0]; \
		(b)[1] = -(a)[1]; \
		(b)[2] = -(a)[2]; \
		(b)[3] = -(a)[3]; \
	} while (0)
#define QuatConj(a,b) \
	do { \
		(b)[0] = -(a)[0]; \
		(b)[1] = -(a)[1]; \
		(b)[2] = -(a)[2]; \
		(b)[3] = (a)[3]; \
	} while (0)
#define QuatAdd(a,b,c) \
	do { \
		(c)[0] = (a)[0] + (b)[0]; \
		(c)[1] = (a)[1] + (b)[1]; \
		(c)[2] = (a)[2] + (b)[2]; \
		(c)[3] = (a)[3] + (b)[3]; \
	} while (0)
#define QuatCopy(a,b) \
	do { \
		(b)[0] = (a)[0]; \
		(b)[1] = (a)[1]; \
		(b)[2] = (a)[2]; \
		(b)[3] = (a)[3]; \
	} while (0)
#define QuatMultAdd(a,s,b,c) \
	do { \
		(c)[0] = (a)[0] + (s) * (b)[0]; \
		(c)[1] = (a)[1] + (s) * (b)[1]; \
		(c)[2] = (a)[2] + (s) * (b)[2]; \
		(c)[3] = (a)[3] + (s) * (b)[3]; \
	} while (0)
#define QuatMultSub(a,s,b,c) \
	do { \
		(c)[0] = (a)[0] - (s) * (b)[0]; \
		(c)[1] = (a)[1] - (s) * (b)[1]; \
		(c)[2] = (a)[2] - (s) * (b)[2]; \
		(c)[3] = (a)[3] - (s) * (b)[3]; \
	} while (0)
#define QuatLength(a) sqrt(QDotProduct(a, a))

#define QuatScale(a,b,c) \
	do { \
		(c)[0] = (a)[0] * (b); \
		(c)[1] = (a)[1] * (b); \
		(c)[2] = (a)[2] * (b); \
		(c)[3] = (a)[3] * (b); \
	} while (0)

#define QuatCompMult(a,b,c) \
	do { \
		(c)[0] = (a)[0] * (b)[0]; \
		(c)[1] = (a)[1] * (b)[1]; \
		(c)[2] = (a)[2] * (b)[2]; \
		(c)[3] = (a)[3] * (b)[3]; \
	} while (0)
#define QuatCompDiv(a,b,c) \
	do { \
		(c)[0] = (a)[0] / (b)[0]; \
		(c)[1] = (a)[1] / (b)[1]; \
		(c)[2] = (a)[2] / (b)[2]; \
		(c)[3] = (a)[3] / (b)[3]; \
	} while (0)
#define QuatCompCompare(x, op, y) \
	(((x)[0] op (y)[0]) && ((x)[1] op (y)[1]) \
	 && ((x)[2] op (y)[2]) && ((x)[3] op (y)[3]))
#define QuatCompare(x, y) QuatCompCompare (x, ==, y)
#define QuatCompMin(a, b, c) \
	do { \
		(c)[0] = min ((a)[0], (b)[0]); \
		(c)[1] = min ((a)[1], (b)[1]); \
		(c)[2] = min ((a)[2], (b)[2]); \
		(c)[3] = min ((a)[3], (b)[3]); \
	} while (0)
#define QuatCompMax(a, b, c) \
	do { \
		(c)[0] = max ((a)[0], (b)[0]); \
		(c)[1] = max ((a)[1], (b)[1]); \
		(c)[2] = max ((a)[2], (b)[2]); \
		(c)[3] = max ((a)[3], (b)[3]); \
	} while (0)
#define QuatCompBound(a, b, c, d) \
	do { \
		(d)[0] = bound ((a)[0], (b)[0], (c)[0]); \
		(d)[1] = bound ((a)[1], (b)[1], (c)[1]); \
		(d)[2] = bound ((a)[2], (b)[2], (c)[2]); \
		(d)[3] = bound ((a)[3], (b)[3], (c)[3]); \
	} while (0)

#define QuatIsZero(a) (!(a)[0] && !(a)[1] && !(a)[2] && !(a)[3])
#define QuatZero(a) ((a)[3] = (a)[2] = (a)[1] = (a)[0] = 0);
#define QuatSet(a,b,c,d,e) \
	do { \
		(e)[0] = a; \
		(e)[1] = b; \
		(e)[2] = c; \
		(e)[3] = d; \
	} while (0)

#define QuatBlend(q1,q2,b,q) \
	do { \
		(q)[0] = (q1)[0] * (1 - (b)) + (q2)[0] * (b); \
		(q)[1] = (q1)[1] * (1 - (b)) + (q2)[1] * (b); \
		(q)[2] = (q1)[2] * (1 - (b)) + (q2)[2] * (b); \
		(q)[3] = (q1)[3] * (1 - (b)) + (q2)[3] * (b); \
	} while (0)

//For printf etc
#define QuatExpand(q) (q)[0], (q)[1], (q)[2], (q)[3]

void QuatMult (const quat_t q1, const quat_t q2, quat_t out);
void QuatMultVec (const quat_t q, const vec3_t v, vec3_t out);
void QuatInverse (const quat_t in, quat_t out);
void QuatExp (const quat_t a, quat_t b);
void QuatToMatrix (const quat_t q, vec_t *m, int homogenous, int vertical);

///@}

#endif // __QF_math_quaternion_h
