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

	$Id$
*/

#ifndef __mathlib_h
#define __mathlib_h

#include <math.h>
#include "QF/qtypes.h"

#ifndef M_PI
# define M_PI	    3.14159265358979323846  // matches value in gcc v2 math.h
#endif

extern int		nanmask;
extern const vec_t * const vec3_origin;

#define EQUAL_EPSILON 0.001
#define RINT(x) (floor ((x) + 0.5))

#define IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

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
		(c)[0]=(a)[0]+(b)[0]; \
		(c)[1]=(a)[1]+(b)[1]; \
		(c)[2]=(a)[2]+(b)[2]; \
	} while (0)
#define VectorCopy(a,b) \
	do { \
		(b)[0]=(a)[0]; \
		(b)[1]=(a)[1]; \
		(b)[2]=(a)[2]; \
	} while (0)
#define VectorMult(a,b,c) \
	do { \
		(c)[0]=(a)[0]*(b)[0]; \
		(c)[1]=(a)[1]*(b)[1]; \
		(c)[2]=(a)[2]*(b)[2]; \
	} while (0)
#define VectorMultAdd(a,s,b,c) \
	do { \
		(c)[0]=(a)[0]+(s)*(b)[0]; \
		(c)[1]=(a)[1]+(s)*(b)[1]; \
		(c)[2]=(a)[2]+(s)*(b)[2]; \
	} while (0)
#define VectorMultSub(a,s,b,c) \
	do { \
		(c)[0]=(a)[0]-(s)*(b)[0]; \
		(c)[1]=(a)[1]-(s)*(b)[1]; \
		(c)[2]=(a)[2]-(s)*(b)[2]; \
	} while (0)
#define VectorLength(a) sqrt(DotProduct(a, a))

#define VectorScale(a,b,c) \
	do { \
		(c)[0]=(a)[0]*(b); \
		(c)[1]=(a)[1]*(b); \
		(c)[2]=(a)[2]*(b); \
	} while (0)
#define VectorCompare(x, y) \
	(((x)[0] == (y)[0]) && ((x)[1] == (y)[1]) && ((x)[2] == (y)[2]))

#define VectorIsZero(a) ((a)[0] == 0 && (a)[1] == 0 && (a)[2] == 0)
#define VectorZero(a) ((a)[2] = (a)[1] = (a)[0] = 0);

#define VectorBlend(v1,v2,b,v) \
	do { \
		(v)[0] = (v1)[0] * (1 - (b)) + (v2)[0] * (b); \
		(v)[1] = (v1)[1] * (1 - (b)) + (v2)[1] * (b); \
		(v)[2] = (v1)[2] * (1 - (b)) + (v2)[2] * (b); \
	} while (0)

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
void VectorInverse (vec3_t v);
int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16(fixed16_t val);
fixed16_t Mul16_30(fixed16_t multiplier, fixed16_t multiplicand);
int GreatestCommonDivisor (int i1, int i2);

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right,
				   vec3_t up);
void VectorVectors (const vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs,
					struct mplane_s *plane);
float anglemod (float a);

void RotatePointAroundVector (vec3_t dst, const vec3_t axis,
							  const vec3_t point, float degrees);

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

#define PlaneDist(point,plane)  ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)


extern mplane_t * const frustum;
extern inline qboolean R_CullBox (const vec3_t mins, const vec3_t maxs);
extern inline qboolean R_CullSphere (const vec3_t origin, const float radius);
extern inline float VectorNormalize (vec3_t v);	// returns vector length
#ifndef IMPLEMENT_R_Cull
extern inline
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

#endif // __mathlib_h
