/*
	mathlib.c

	math primitives

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>

#define IMPLEMENT_R_Cull
#define IMPLEMENT_VectorNormalize

#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

VISIBLE int nanmask = 255 << 23;
static plane_t _frustum[4];
VISIBLE plane_t   *const frustum = _frustum;
static vec3_t _vec3_origin = { 0, 0, 0 };
VISIBLE const vec_t * const vec3_origin = _vec3_origin;
static vec3_t _quat_origin = { 0, 0, 0 };
VISIBLE const vec_t * const quat_origin = _quat_origin;

#define DEG2RAD(a) (a * (M_PI / 180.0))


static void
ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal)
{
	float		inv_denom, d;
	vec3_t		n;

	inv_denom = 1.0F / DotProduct (normal, normal);

	d = DotProduct (normal, p) * inv_denom;

	VectorScale (normal, inv_denom * d, n);

	VectorSubtract (p, n, dst);
}

// assumes "src" is normalized
static void
PerpendicularVector (vec3_t dst, const vec3_t src)
{
	int			pos, i;
	float		minelem = 1.0F;
	vec3_t		tempvec;

	/* find the smallest magnitude axially aligned vector */
	for (pos = 0, i = 0; i < 3; i++) {
		if (fabs (src[i]) < minelem) {
			pos = i;
			minelem = fabs (src[i]);
		}
	}
	VectorZero (tempvec);
	tempvec[pos] = 1.0F;

	/* project the point onto the plane defined by src */
	ProjectPointOnPlane (dst, tempvec, src);

	/* normalize the result */
	VectorNormalize (dst);
}

#if defined(_WIN32) && !defined(__GNUC__)
# pragma optimize( "", off )
#endif

VISIBLE void
VectorVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	VectorMultSub (right, d, forward, right);
	VectorNormalize (right);
	CrossProduct(right, forward, up);
}

VISIBLE void
RotatePointAroundVector (vec3_t dst, const vec3_t axis, const vec3_t point,
						 float degrees)
{
	float       m[3][3];
	float       im[3][3];
	float       zrot[3][3];
	float       tmpmat[3][3];
	float       rot[3][3];
	int         i;
	vec3_t      vr, vup, vf;

	VectorCopy (axis, vf);

	PerpendicularVector (vr, axis);
	CrossProduct (vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy (im, m, sizeof (im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset (zrot, 0, sizeof (zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos (DEG2RAD (degrees));
	zrot[0][1] = sin (DEG2RAD (degrees));
	zrot[1][0] = -sin (DEG2RAD (degrees));
	zrot[1][1] = cos (DEG2RAD (degrees));

	R_ConcatRotations (m, zrot, tmpmat);
	R_ConcatRotations (tmpmat, im, rot);

	for (i = 0; i < 3; i++) {
		dst[i] = DotProduct (rot[i], point);
	}
}

VISIBLE void
QuatMult (const quat_t q1, const quat_t q2, quat_t out)
{
	vec_t      s;
	vec3_t     v;

	s = q1[0] * q2[0] - DotProduct (q1 + 1, q2 + 1);
	CrossProduct (q1 + 1, q2 + 1, v);
	VectorMultAdd (v, q1[0], q2 + 1, v);
	VectorMultAdd (v, q2[0], q1 + 1, out + 1);
	out[0] = s;
}

VISIBLE void
QuatInverse (const quat_t in, quat_t out)
{
	quat_t      q;
	vec_t       m;

	m = QDotProduct (in, in);	// in * in*
	QuatConj (in, q);
	QuatScale (q, 1 / m, out);
}

VISIBLE void
QuatToMatrix (const quat_t q, vec_t *m, int homogenous, int vertical)
{
	vec_t       aa, ab, ac, ad, bb, bc, bd, cc, cd, dd;
	vec_t       *_m[4] = {
		m + (homogenous ? 0 : 0),
		m + (homogenous ? 4 : 3),
		m + (homogenous ? 8 : 6),
		m + (homogenous ? 12 : 9),
	};

	aa = q[0] * q[0];
	ab = q[0] * q[1];
	ac = q[0] * q[2];
	ad = q[0] * q[3];

	bb = q[1] * q[1];
	bc = q[1] * q[2];
	bd = q[1] * q[3];

	cc = q[2] * q[2];
	cd = q[2] * q[3];

	dd = q[3] * q[3];

	if (vertical) {
		VectorSet (aa + bb - cc - dd, 2 * (bc + ad), 2 * (bd - ac), _m[0]);
		VectorSet (2 * (bc - ad), aa - bb + cc - dd, 2 * (cd + ab), _m[1]);
		VectorSet (2 * (bd + ac), 2 * (cd - ab), aa - bb - cc + dd, _m[2]);
	} else {
		VectorSet (aa + bb - cc - dd, 2 * (bc - ad), 2 * (bd + ac), _m[0]);
		VectorSet (2 * (bc + ad), aa - bb + cc - dd, 2 * (cd - ab), _m[1]);
		VectorSet (2 * (bd - ac), 2 * (cd + ab), aa - bb - cc + dd, _m[2]);
	}
	if (homogenous) {
		_m[0][3] = 0;
		_m[1][3] = 0;
		_m[2][3] = 0;
		VectorZero (_m[3]);
		_m[3][3] = 1;
	}
}

#if defined(_WIN32) && !defined(__GNUC__)
# pragma optimize( "", on )
#endif

VISIBLE float
anglemod (float a)
{
	a = (360.0 / 65536) * ((int) (a * (65536 / 360.0)) & 65535);
	return a;
}

/*
	BOPS_Error

	Split out like this for ASM to call.
*/
void __attribute__ ((noreturn)) BOPS_Error (void);
VISIBLE void __attribute__ ((noreturn))
BOPS_Error (void)
{
	Sys_Error ("BoxOnPlaneSide:  Bad signbits");
}

#ifndef USE_INTEL_ASM

/*
	BoxOnPlaneSide

	Returns 1, 2, or 1 + 2
*/
VISIBLE int
BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, plane_t *p)
{
	float       dist1, dist2;
	int         sides;

#if 0
	// this is done by the BOX_ON_PLANE_SIDE macro before
	// calling this function

	// fast axial cases
	if (p->type < 3) {
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
#endif

	// general case
	switch (p->signbits) {
		case 0:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			break;
		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			break;
		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			break;
		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			break;
		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			break;
		default:
			BOPS_Error ();
	}

#if 0
	int         i;
	vec3_t      corners[2];

	for (i = 0; i < 3; i++) {
		if (plane->normal[i] < 0) {
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		} else {
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist = DotProduct (plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct (plane->normal, corners[1]) - plane->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;
#endif

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
	if (sides == 0)
		Sys_Error ("BoxOnPlaneSide: sides==0");
#endif

	return sides;
}
#endif

/*
	angles is a left(?) handed system: 'pitch yaw roll' with x (pitch) axis to
	the right, y (yaw) axis up and z (roll) axis forward.

	the math in AngleVectors has the entity frame as left handed with x
	(forward) axis forward, y (right) axis to the right and z (up) up. However,
	the world is a right handed system with x to the right, y forward and
	z up.

	pitch =
		cp 0 -sp
		0  1  0
		sp 0  cp

	yaw =
		 cy sy 0
		-sy cy 0
		 0  0  1

	roll =
		1  0  0
		0  cr sr
		0 -sr cr

	final = roll * (pitch * yaw)
	final =
		[forward]
		[-right]	-ve due to left handed to right handed conversion
		[up]
*/
VISIBLE void
AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float       angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI * 2 / 360);
	sy = sin (angle);
	cy = cos (angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp = sin (angle);
	cp = cos (angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr = sin (angle);
	cr = cos (angle);

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
	// need to flip right because it's a left handed system in a right handed
	// world
	right[0] = -1 * (sr * sp * cy + cr * -sy);
	right[1] = -1 * (sr * sp * sy + cr * cy);
	right[2] = -1 * (sr * cp);
	up[0] = (cr * sp * cy + -sr * -sy);
	up[1] = (cr * sp * sy + -sr * cy);
	up[2] = cr * cp;
}

VISIBLE void
AngleQuat (const vec3_t angles, quat_t q)
{
	float       alpha, sr, sp, sy, cr, cp, cy;

	// alpha is half the angle
	alpha = angles[YAW] * (M_PI / 360);
	sy = sin (alpha);
	cy = cos (alpha);
	alpha = angles[PITCH] * (M_PI / 360);
	sp = sin (alpha);
	cp = cos (alpha);
	alpha = angles[ROLL] * (M_PI / 360);
	sr = sin (alpha);
	cr = cos (alpha);

	QuatSet (cy * cp * cr + sy * sp * sr,
			 cy * cp * sr - sy * sp * cr,
			 cy * sp * cr + sy * cp * sr,
			 sy * cp * cr - cy * sp * sr,
			 q);
}

VISIBLE int
_VectorCompare (const vec3_t v1, const vec3_t v2)
{
	int         i;

	for (i = 0; i < 3; i++)
		if (fabs (v1[i] - v2[i]) > EQUAL_EPSILON)
			return 0;

	return 1;
}

VISIBLE void
_VectorMA (const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

VISIBLE vec_t
_DotProduct (const vec3_t v1, const vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

VISIBLE void
_VectorSubtract (const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

VISIBLE void
_VectorAdd (const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

VISIBLE void
_VectorCopy (const vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

VISIBLE void
CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	float v10 = v1[0];
	float v11 = v1[1];
	float v12 = v1[2];
	float v20 = v2[0];
	float v21 = v2[1];
	float v22 = v2[2];

	cross[0] = v11 * v22 - v12 * v21;
	cross[1] = v12 * v20 - v10 * v22;
	cross[2] = v10 * v21 - v11 * v20;
}

VISIBLE vec_t
_VectorLength (const vec3_t v)
{
	float		length;

	length = sqrt (DotProduct (v, v));
	return length;
}

VISIBLE vec_t
_VectorNormalize (vec3_t v)
{
	int         i;
	double      length;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt (length);
	if (length == 0)
		return 0;

	for (i = 0; i < 3; i++)
		v[i] /= length;

	return length;
}

VISIBLE void
_VectorScale (const vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

VISIBLE int
Q_log2 (int val)
{
	int         answer = 0;

	while ((val >>= 1) != 0)
		answer++;
	return answer;
}

VISIBLE void
R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
}

VISIBLE void
R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
		in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
		in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
		in1[2][2] * in2[2][3] + in1[2][3];
}

/*
	FloorDivMod

	Returns mathematically correct (floor-based) quotient and remainder for
	numer and denom, both of which should contain no fractional part. The
	quotient must fit in 32 bits.
*/
VISIBLE void
FloorDivMod (double numer, double denom, int *quotient, int *rem)
{
	double		x;
	int			q, r;

#ifndef PARANOID
	if (denom <= 0.0)
		Sys_Error ("FloorDivMod: bad denominator %f", denom);

//	if ((floor(numer) != numer) || (floor(denom) != denom))
//		Sys_Error ("FloorDivMod: non-integer numer or denom %f %f",
//				   numer, denom);
#endif

	if (numer >= 0.0) {
		x = floor (numer / denom);
		q = (int) x;
		r = (int) floor (numer - (x * denom));
	} else {
		// perform operations with positive values, and fix mod to make
		// floor-based
		x = floor (-numer / denom);
		q = -(int) x;
		r = (int) floor (-numer - (x * denom));
		if (r != 0) {
			q--;
			r = (int) denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}

VISIBLE int
GreatestCommonDivisor (int i1, int i2)
{
	if (i1 > i2) {
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor (i2, i1 % i2);
	} else {
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor (i1, i2 % i1);
	}
}

#ifndef USE_INTEL_ASM
/*
  Invert24To16

  Inverts an 8.24 value to a 16.16 value
*/
VISIBLE fixed16_t
Invert24To16 (fixed16_t val)
{
	if (val < 256)
		return (0xFFFFFFFF);

	return (fixed16_t)
		(((double) 0x10000 * (double) 0x1000000 / (double) val) + 0.5);
}
#endif

void
Mat4Transpose (const mat4_t a, mat4_t b)
{
	vec_t       t;
	int         i, j;

	for (i = 0; i < 3; i++) {
		b[i * 4 + i] = a[i * 4 + i];		// in case b != a
		for (j = i + 1; j < 4; j++) {
			t = a[i * 4 + j];				// in case b == a
			b[i * 4 + j] = a[j * 4 + i];
			b[j * 4 + i] = t;
		}
	}
	b[i * 4 + i] = a[i * 4 + i];		// in case b != a
}

void
Mat4Mult (const mat4_t a, const mat4_t b, mat4_t c)
{
	mat4_t      ta, tb;					// in case c == b or c == a
	int         i, j, k;

	Mat4Transpose (a, ta);				// transpose so we can use dot
	Mat4Copy (b, tb);

	k = 0;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			c[k++] = QDotProduct (ta + 4 * j, tb + 4 * i);
		}
	}
}
