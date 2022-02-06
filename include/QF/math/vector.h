/*
	mathlib.h

	Vector math library

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __QF_math_vector_h
#define __QF_math_vector_h

/** \defgroup mathlib_vector Vector functions
	\ingroup mathlib
*/
///@{

#include "QF/qtypes.h"

extern const vec_t *const vec3_origin;

#define VectorCompUop(b, op, a) \
	do { \
		(b)[0] = op ((a)[0]); \
		(b)[1] = op ((a)[1]); \
		(b)[2] = op ((a)[2]); \
	} while (0)
#define VectorCompOp(c, a, op, b) \
	do { \
		(c)[0] = (a)[0] op (b)[0]; \
		(c)[1] = (a)[1] op (b)[1]; \
		(c)[2] = (a)[2] op (b)[2]; \
	} while (0)
#define VectorCompAdd(c,a,b) VectorCompOp (c, a, +, b)
#define VectorCompSub(c,a,b) VectorCompOp (c, a, -, b)
#define VectorCompMult(c,a,b) VectorCompOp (c, a, *, b)
#define VectorCompDiv(c,a,b) VectorCompOp (c, a, /, b)

#define DotProduct(a,b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
#define VectorSubtract(a,b,c) VectorCompSub (c, a, b)
#define VectorNegate(a,b) VectorCompUop (b, -, a)
#define VectorAdd(a,b,c) VectorCompAdd (c, a, b)
#define VectorCopy(a,b) \
	do { \
		(b)[0] = (a)[0]; \
		(b)[1] = (a)[1]; \
		(b)[2] = (a)[2]; \
	} while (0)
#define VectorMultAdd(a,s,b,c) \
	do { \
		(c)[0] = (a)[0] + (s) * (b)[0]; \
		(c)[1] = (a)[1] + (s) * (b)[1]; \
		(c)[2] = (a)[2] + (s) * (b)[2]; \
	} while (0)
#define VectorMultSub(a,s,b,c) \
	do { \
		(c)[0] = (a)[0] - (s) * (b)[0]; \
		(c)[1] = (a)[1] - (s) * (b)[1]; \
		(c)[2] = (a)[2] - (s) * (b)[2]; \
	} while (0)
#define VectorCompMultAdd(a,b,c,d) \
	do { \
		(d)[0] = (a)[0] + (b)[0] * (c)[0]; \
		(d)[1] = (a)[1] + (b)[1] * (c)[1]; \
		(d)[2] = (a)[2] + (b)[2] * (c)[2]; \
	} while (0)
#define VectorCompMultSub(a,b,c,d) \
	do { \
		(d)[0] = (a)[0] - (b)[0] * (c)[0]; \
		(d)[1] = (a)[1] - (b)[1] * (c)[1]; \
		(d)[2] = (a)[2] - (b)[2] * (c)[2]; \
	} while (0)
#define VectorLength(a) sqrt(DotProduct(a, a))

#define VectorScale(a,b,c) \
	do { \
		(c)[0] = (a)[0] * (b); \
		(c)[1] = (a)[1] * (b); \
		(c)[2] = (a)[2] * (b); \
	} while (0)
/** Shear vector \a b by vector \a a.

	Vector a represents the shear factors XY, XZ, YZ, ie in matrix form:
	[  1  0  0 ] [ b0 ]
	[ a0  1  0 ] [ b1 ]
	[ a1 a2  1 ] [ b2 ]

	The reason for this particular scheme is that is how Mat4Decompose
	calculates the shear from a matrix.

	\note The order of calculations is important for when b and c refer to
	the same vector.
*/
#define VectorShear(a,b,c) \
	do { \
		(c)[2] = (b)[0] * (a)[1] + (b)[1] * (a)[2] + (b)[2]; \
		(c)[1] = (b)[0] * (a)[0] + (b)[1]; \
		(c)[0] = (b)[0]; \
	} while (0)
#define VectorUnshear(a,b,c) \
	do { \
		(c)[2] = (b)[2] - (b)[1] * (a)[2] - (b)[0] * ((a)[1]-(a)[0]*(a)[2]); \
		(c)[1] = (b)[1] - (b)[0] * (a)[0]; \
		(c)[0] = (b)[0]; \
	} while (0)
#define VectorCompCompare(c, m, a, op, b)	\
	do { \
		(c)[0] = m((a)[0] op (b)[0]); \
		(c)[1] = m((a)[1] op (b)[1]); \
		(c)[2] = m((a)[2] op (b)[2]); \
	} while (0)
#define VectorCompCompareAll(x, op, y)	\
	(((x)[0] op (y)[0]) && ((x)[1] op (y)[1]) && ((x)[2] op (y)[2]))
#define VectorCompare(x, y) VectorCompCompareAll (x, ==, y)
#define VectorCompMin(a, b, c) \
	do { \
		(c)[0] = min ((a)[0], (b)[0]); \
		(c)[1] = min ((a)[1], (b)[1]); \
		(c)[2] = min ((a)[2], (b)[2]); \
	} while (0)
#define VectorCompMax(a, b, c) \
	do { \
		(c)[0] = max ((a)[0], (b)[0]); \
		(c)[1] = max ((a)[1], (b)[1]); \
		(c)[2] = max ((a)[2], (b)[2]); \
	} while (0)
#define VectorCompBound(a, b, c, d) \
	do { \
		(d)[0] = bound ((a)[0], (b)[0], (c)[0]); \
		(d)[1] = bound ((a)[1], (b)[1], (c)[1]); \
		(d)[2] = bound ((a)[2], (b)[2], (c)[2]); \
	} while (0)

#define VectorIsZero(a) (!(a)[0] && !(a)[1] && !(a)[2])
#define VectorZero(a) \
	do { \
		(a)[0] = 0; \
		(a)[1] = 0; \
		(a)[2] = 0; \
	} while (0)
#define VectorSet(a,b,c,d) \
	do { \
		(d)[0] = a; \
		(d)[1] = b; \
		(d)[2] = c; \
	} while (0)

#define VectorBlend(v1,v2,b,v) \
	do { \
		(v)[0] = (v1)[0] * (1 - (b)) + (v2)[0] * (b); \
		(v)[1] = (v1)[1] * (1 - (b)) + (v2)[1] * (b); \
		(v)[2] = (v1)[2] * (1 - (b)) + (v2)[2] * (b); \
	} while (0)

//For printf etc
#define VectorExpand(v) (v)[0], (v)[1], (v)[2]
//For scanf etc
#define VectorExpandAddr(v) &(v)[0], &(v)[1], &(v)[2]

/*
 * VectorDistance, the distance between two points.
 * Yes, this is the same as sqrt(VectorSubtract then DotProduct),
 * however that way would involve more vars, this is cheaper.
 */
#define VectorDistance_fast(a, b) \
	((((a)[0] - (b)[0]) * ((a)[0] - (b)[0])) + \
	 (((a)[1] - (b)[1]) * ((a)[1] - (b)[1])) + \
	 (((a)[2] - (b)[2]) * ((a)[2] - (b)[2])))
#define VectorDistance(a, b)	sqrt(VectorDistance_fast(a, b))

vec_t _DotProduct (const vec3_t v1, const vec3_t v2) __attribute__((pure));
void _VectorAdd (const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorCopy (const vec3_t in, vec3_t out);
int _VectorCompare (const vec3_t v1, const vec3_t v2) __attribute__((pure));	// uses EQUAL_EPSILON
vec_t _VectorLength (const vec3_t v) __attribute__((pure));
void _VectorMA (const vec3_t veca, float scale, const vec3_t vecb,
				vec3_t vecc);
void _VectorScale (const vec3_t in, vec_t scale, vec3_t out);
void _VectorSubtract (const vec3_t veca, const vec3_t vecb, vec3_t out);
void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t _VectorNormalize (vec3_t v);			// returns vector length

GNU89INLINE inline float VectorNormalize (vec3_t v);	// returns vector length

#ifndef IMPLEMENT_VectorNormalize
GNU89INLINE inline
#else
VISIBLE
#endif
float
VectorNormalize (vec3_t v)
{
	float length;

	length = DotProduct (v, v);
	if (length) {
		float ilength;

		length = sqrt (length);
		ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

///@}

#endif // __QF_math_vector_h
