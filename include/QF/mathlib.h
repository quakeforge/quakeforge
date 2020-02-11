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
///@{

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

#define EQUAL_EPSILON 0.001
#define RINT(x) (floor ((x) + 0.5))

#define IS_NAN(x) (((*(int *) (char *) &x) & nanmask) == nanmask)

#include "QF/math/vector.h"
#include "QF/math/quaternion.h"
#include "QF/math/dual.h"
#include "QF/math/matrix3.h"
#include "QF/math/matrix4.h"
#include "QF/math/half.h"

#define qfrandom(MAX) ((float) MAX * (rand() * (1.0 / (RAND_MAX + 1.0))))

// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2

int Q_log2(int val) __attribute__((const));

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16(fixed16_t val) __attribute__((const));
fixed16_t Mul16_30(fixed16_t multiplier, fixed16_t multiplicand);
int GreatestCommonDivisor (int i1, int i2) __attribute__((const));

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
					struct plane_s *plane) __attribute__((pure));
float anglemod (float a) __attribute__((const));

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

extern plane_t * const frustum;
GNU89INLINE inline qboolean R_CullBox (const vec3_t mins, const vec3_t maxs) __attribute__((pure));
GNU89INLINE inline qboolean R_CullSphere (const vec3_t origin, const float radius);

#ifndef IMPLEMENT_R_Cull
GNU89INLINE inline
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
GNU89INLINE inline
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

sphere_t SmallestEnclosingBall (const vec3_t points[], int num_points);
int CircumSphere (const vec3_t points[], int num_points, sphere_t *sphere);
void BarycentricCoords (const vec_t **points, int num_points, const vec3_t p,
		                vec_t *lambda);

///@}

#endif // __mathlib_h
