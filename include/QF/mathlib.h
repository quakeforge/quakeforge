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

#ifndef __mathlib_h
#define __mathlib_h

/** \defgroup mathlib Vector and matrix functions
	\ingroup utils
*/
//@{

#include <math.h>
#include "QF/qtypes.h"

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef bound
# define bound(a,b,c) (max(a, min(b, c)))
#endif

#ifndef M_PI
# define M_PI	    3.14159265358979323846  // matches value in gcc v2 math.h
#endif

extern int		nanmask;
extern const vec_t *const vec3_origin;
extern const vec_t *const quat_origin;

#define EQUAL_EPSILON 0.001
#define RINT(x) (floor ((x) + 0.5))

#define IS_NAN(x) (((*(int *) (char *) &x) & nanmask) == nanmask)

#define DotProduct(a,b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
#define VectorSubtract(a,b,c) \
	do { \
		(c)[0] = (a)[0] - (b)[0]; \
		(c)[1] = (a)[1] - (b)[1]; \
		(c)[2] = (a)[2] - (b)[2]; \
	} while (0)
#define VectorNegate(a,b) \
	do { \
		(b)[0] = -(a)[0]; \
		(b)[1] = -(a)[1]; \
		(b)[2] = -(a)[2]; \
	} while (0)
#define VectorAdd(a,b,c) \
	do { \
		(c)[0] = (a)[0] + (b)[0]; \
		(c)[1] = (a)[1] + (b)[1]; \
		(c)[2] = (a)[2] + (b)[2]; \
	} while (0)
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
#define VectorLength(a) sqrt(DotProduct(a, a))

#define VectorScale(a,b,c) \
	do { \
		(c)[0] = (a)[0] * (b); \
		(c)[1] = (a)[1] * (b); \
		(c)[2] = (a)[2] * (b); \
	} while (0)
#define VectorCompMult(a,b,c) \
	do { \
		(c)[0] = (a)[0] * (b)[0]; \
		(c)[1] = (a)[1] * (b)[1]; \
		(c)[2] = (a)[2] * (b)[2]; \
	} while (0)
#define VectorCompCompare(x, op, y)	\
	(((x)[0] op (y)[0]) && ((x)[1] op (y)[1]) && ((x)[2] op (y)[2]))
#define VectorCompare(x, y) VectorCompCompare (x, ==, y)
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
#define VectorZero(a) ((a)[2] = (a)[1] = (a)[0] = 0);
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
		(b)[0] = (a)[0]; \
		(b)[1] = -(a)[1]; \
		(b)[2] = -(a)[2]; \
		(b)[3] = -(a)[3]; \
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
	memset (a, 0, 16 * sizeof a[0])
#define Mat4Identity(a) \
	do { \
		Mat4Zero (a); \
		a[15] = a[10] = a[5] = a[0] = 1; \
	} while (0)

#define qfrandom(MAX) ((float) MAX * (rand() * (1.0 / (RAND_MAX + 1.0))))

// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2

vec_t _DotProduct (const vec3_t v1, const vec3_t v2);
void _VectorAdd (const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorCopy (const vec3_t in, vec3_t out);
int _VectorCompare (const vec3_t v1, const vec3_t v2);	// uses EQUAL_EPSILON
vec_t _VectorLength (const vec3_t v);
void _VectorMA (const vec3_t veca, float scale, const vec3_t vecb,
				vec3_t vecc);
void _VectorScale (const vec3_t in, vec_t scale, vec3_t out);
void _VectorSubtract (const vec3_t veca, const vec3_t vecb, vec3_t out);
void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t _VectorNormalize (vec3_t v);			// returns vector length
int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16(fixed16_t val);
fixed16_t Mul16_30(fixed16_t multiplier, fixed16_t multiplicand);
int GreatestCommonDivisor (int i1, int i2);

/**	Convert quake angles to basis vectors.

	The basis vectors form a left handed system (although the world is
	right handed). When all angles are 0, \a forward points along the world
	X axis, \a right along the <em>negative</em> Y axis, and \a up along
	the Z axis.

	Rotation is done by:
	-# Rotating YAW degrees counterclockwise around the local Z axis
	-# Rotating PITCH degrees clockwise around the new local negative Y axis
	   (or counterclockwise around the new local Y axis).
	-# Rotating ROLL degrees counterclockwise around the local X axis

	Thus when used for the player from the first person perspective,
	positive YAW turns to the left, positive PITCH looks down, and positive
	ROLL leans to the right.

	\f[
		YAW=\begin{array}{ccc}
		c_{y} & s_{y} & 0\\
		-s_{y} & c_{y} & 0\\
		0 & 0 & 1
		\end{array}
	\f]
	\f[
		PITCH=\begin{array}{ccc}
		c_{p} & 0 & -s_{p}\\
		0 & 1 & 0\\
		s_{p} & 0 & c_{p}
		\end{array}
	\f]
	\f[
		ROLL=\begin{array}{ccc}
		1 & 0 & 0\\
		0 & c_{r} & -s_{r}\\
		0 & s_{r} & c_{r}
		\end{array}
	\f]
	\f[
		ROLL\,(PITCH\,YAW)=\begin{array}{c}
		forward\\
		-right\\
		up
		\end{array}
	\f]

	\param angles	The rotation angles.
	\param forward	The vector pointing forward.
	\param right    The vector pointing to the right.
	\param up		The vector pointing up.
*/
void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right,
				   vec3_t up);
void AngleQuat (const vec3_t angles, quat_t q);
void VectorVectors (const vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs,
					struct plane_s *plane);
float anglemod (float a);

void RotatePointAroundVector (vec3_t dst, const vec3_t axis,
							  const vec3_t point, float degrees);

void QuatMult (const quat_t q1, const quat_t q2, quat_t out);
void QuatMultVec (const quat_t q, const vec3_t v, vec3_t out);
void QuatInverse (const quat_t in, quat_t out);
void QuatExp (const quat_t a, quat_t b);
void QuatToMatrix (const quat_t q, vec_t *m, int homogenous, int vertical);

void Mat4Transpose (const mat4_t a, mat4_t b);
void Mat4Mult (const mat4_t a, const mat4_t b, mat4_t c);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)				\
	(((p)->type < 3)?									\
	(													\
		((p)->dist <= (emins)[(p)->type])?				\
		1												\
		:												\
		(												\
			((p)->dist >= (emaxs)[(p)->type])?			\
			2											\
			:											\
			3											\
		)												\
	)													\
	:													\
	BoxOnPlaneSide( (emins), (emaxs), (p)))

#define PlaneDist(point,plane) \
	((plane)->type < 3 ? (point)[(plane)->type] \
	 				   : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) \
	(PlaneDist (point, plane) - (plane)->dist)

#define PlaneFlip(sp, dp) \
	do {											\
		(dp)->dist = -(sp)->dist;					\
		VectorNegate ((sp)->normal, (dp)->normal);	\
	} while (0)

int16_t FloatToHalf (float x);
float HalfToFloat (int16_t x);
extern plane_t * const frustum;
extern inline qboolean R_CullBox (const vec3_t mins, const vec3_t maxs);
extern inline qboolean R_CullSphere (const vec3_t origin, const float radius);
extern inline float VectorNormalize (vec3_t v);	// returns vector length
#ifndef IMPLEMENT_R_Cull
extern inline
#else
VISIBLE
#endif
qboolean
R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int		i;

	for (i=0 ; i < 4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

#ifndef IMPLEMENT_R_Cull
extern inline
#else
VISIBLE
#endif
qboolean
R_CullSphere (const vec3_t origin, const float radius)
{
	int		i;
	float	r;

	for (i = 0; i < 4; i++)
	{
		r = DotProduct (origin, frustum[i].normal) - frustum[i].dist;
		if (r <= -radius)
			return true;
	}
	return false;
}

#ifndef IMPLEMENT_VectorNormalize
extern inline
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

//@}

#endif // __mathlib_h
