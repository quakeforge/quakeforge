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
static quat_t _quat_origin = { 0, 0, 0, 0 };
VISIBLE const vec_t * const quat_origin = _quat_origin;

#define DEG2RAD(a) (a * (M_PI / 180.0))

#define FMANTBITS 23
#define FMANTMASK ((1 << FMANTBITS) - 1)
#define FEXPBITS 8
#define FEXPMASK ((1 << FEXPBITS) - 1)
#define FBIAS (1 << (FEXPBITS - 1))
#define FEXPMAX ((1 << FEXPBITS) - 1)

#define HMANTBITS 10
#define HMANTMASK ((1 << HMANTBITS) - 1)
#define HEXPBITS 5
#define HEXPMASK ((1 << HEXPBITS) - 1)
#define HBIAS (1 << (HEXPBITS - 1))
#define HEXPMAX ((1 << HEXPBITS) - 1)

int16_t
FloatToHalf (float x)
{
	union {
		float       f;
		uint32_t    u;
	} uf;
	unsigned    sign;
	int         exp;
	unsigned    mant;
	int16_t     half;

	uf.f = x;
	sign = (uf.u >> (FEXPBITS + FMANTBITS)) & 1;
	exp = ((uf.u >> FMANTBITS) & FEXPMASK) - FBIAS + HBIAS;
	mant = (uf.u & FMANTMASK) >> (FMANTBITS - HMANTBITS);
	if (exp <= 0) {
		mant |= 1 << HMANTBITS;
		mant >>= min (1 - exp, HMANTBITS + 1);
		exp = 0;
	} else if (exp >= HEXPMAX) {
		mant = 0;
		exp = HEXPMAX;
	}
	half = (sign << (HEXPBITS + HMANTBITS)) | (exp << HMANTBITS) | mant;
	return half;
}

float
HalfToFloat (int16_t x)
{
	union {
		float       f;
		uint32_t    u;
	} uf;
	unsigned    sign;
	int         exp;
	unsigned    mant;

	sign = (x >> (HEXPBITS + HMANTBITS)) & 1;
	exp = ((x >> HMANTBITS) & HEXPMASK);
	mant = (x & HMANTMASK) << (FMANTBITS - HMANTBITS);

	if (exp == 0) {
		if (mant) {
			while (mant < (1 << FMANTBITS)) {
				mant <<= 1;
				exp--;
			}
			mant &= (1 << FMANTBITS) - 1;
			exp += FBIAS - HBIAS + 1;
		}
	} else if (exp == HEXPMAX) {
		exp = FEXPMAX;
	} else {
		exp += FBIAS - HBIAS;
	}
	uf.u = (sign << (FEXPBITS + FMANTBITS)) | (exp << FMANTBITS) | mant;
	return uf.f;
}

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

	s = q1[3] * q2[3] - DotProduct (q1, q2);
	CrossProduct (q1, q2, v);
	VectorMultAdd (v, q1[3], q2, v);
	VectorMultAdd (v, q2[3], q1, out);
	out[3] = s;
}

VISIBLE void
QuatMultVec (const quat_t q, const vec3_t v, vec3_t out)
{
	vec3_t     tv;
	vec_t      dqv, dqq;
	vec_t      s;

	s = q[3];
	CrossProduct (q, v, tv);
	dqv = DotProduct (q, v);
	dqq = DotProduct (q, q);
	VectorScale (tv, s, tv);
	VectorMultAdd (tv, dqv, q, tv);
	VectorScale (tv, 2, tv);
	VectorMultAdd (tv, s * s - dqq, v, out);
}

VISIBLE void
QuatRotation(const vec3_t a, const vec3_t b, quat_t out)
{
	vec_t       ma, mb;
	vec_t       den, mba_mab;
	vec3_t      t;

	ma = VectorLength(a);
	mb = VectorLength(b);
	den = 2 * ma * mb;
	VectorScale (a, mb, t);
	VectorMultAdd(t, ma, b, t);
	mba_mab = VectorLength(t);
	CrossProduct (a, b, t);
	VectorScale(t, 1 / mba_mab, out);
	out[3] = mba_mab / den;
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
QuatExp (const quat_t a, quat_t b)
{
	vec3_t      n;
	vec_t       th;
	vec_t       r;
	vec_t       c, s;

	VectorCopy (a, n);
	th = VectorNormalize (n);
	r = expf (a[3]);
	c = cosf (th);
	s = sinf (th);
	VectorScale (n, r * s, b);
	b[3] = r * c;
}

VISIBLE void
QuatToMatrix (const quat_t q, vec_t *m, int homogenous, int vertical)
{
	vec_t       xx, xy, xz, xw, yy, yz, yw, zz, zw;
	vec_t       *_m[4] = {
		m + (homogenous ? 0 : 0),
		m + (homogenous ? 4 : 3),
		m + (homogenous ? 8 : 6),
		m + (homogenous ? 12 : 9),
	};

	xx = 2 * q[0] * q[0];
	xy = 2 * q[0] * q[1];
	xz = 2 * q[0] * q[2];
	xw = 2 * q[0] * q[3];

	yy = 2 * q[1] * q[1];
	yz = 2 * q[1] * q[2];
	yw = 2 * q[1] * q[3];

	zz = 2 * q[2] * q[2];
	zw = 2 * q[2] * q[3];

	if (vertical) {
		VectorSet (1.0f - yy - zz, xy + zw, xz - yw, _m[0]);
		VectorSet (xy - zw, 1.0f - xx - zz, yz + xw, _m[1]);
		VectorSet (xz + yw, yz - xw, 1.0f - xx - yy, _m[2]);
	} else {
		VectorSet (1.0f - yy - zz, xy - zw, xz + yw, _m[0]);
		VectorSet (xy + zw, 1.0f - xx - zz, yz - xw, _m[1]);
		VectorSet (xz - yw, yz + xw, 1.0f - xx - yy, _m[2]);
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

	QuatSet (cy * cp * sr - sy * sp * cr,	// x
			 cy * sp * cr + sy * cp * sr,	// y
			 sy * cp * cr - cy * sp * sr,	// z
			 cy * cp * cr + sy * sp * sr,	// w
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
Mat3Init (const quat_t rot, const vec3_t scale, mat3_t mat)
{
	QuatToMatrix (rot, mat, 0, 1);
	VectorScale (mat + 0, scale[0], mat + 0);
	VectorScale (mat + 3, scale[1], mat + 3);
	VectorScale (mat + 6, scale[2], mat + 6);
}

void
Mat3Transpose (const mat3_t a, mat3_t b)
{
	vec_t       t;
	int         i, j;

	for (i = 0; i < 2; i++) {
		b[i * 3 + i] = a[i * 3 + i];		// in case b != a
		for (j = i + 1; j < 3; j++) {
			t = a[i * 3 + j];				// in case b == a
			b[i * 3 + j] = a[j * 3 + i];
			b[j * 3 + i] = t;
		}
	}
	b[i * 3 + i] = a[i * 3 + i];		// in case b != a
}

vec_t
Mat3Determinant (const mat3_t m)
{
	vec3_t      t;
	CrossProduct (m + 3, m + 6, t);
	return DotProduct (m + 0, t);
}

typedef vec_t mat2_t[2 * 2];

static void
Mat3Sub2 (const mat3_t m3, mat2_t m2, int i, int j)
{
	int         si, sj, di, dj;

	for (di = 0; di < 2; di++) {
		for (dj = 0; dj < 2; dj++) {
			si = di + ((di >= i) ? 1 : 0);
			sj = dj + ((dj >= j) ? 1 : 0);
			m2[di * 2 + dj] = m3[si * 3 + sj];
		}
	}
}

static vec_t
Mat2Det (const mat2_t m)
{
	return m[0] * m[3] - m[1] * m[2];
}

int
Mat3Inverse (const mat3_t a, mat3_t b)
{
	mat3_t      tb;
	mat2_t      m2;
	vec_t      *m = b;
	int         i, j;
	vec_t       det;
	vec_t       sign[2] = { 1, -1};

	det = Mat3Determinant (a);
	if (det * det < 1e-6) {
		Mat3Identity (b);
		return 0;
	}
	if (b == a)
		m = tb;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			Mat3Sub2 (a, m2, i, j);
			m[j * 3 + i] = sign[(i + j) & 1] * Mat2Det (m2) / det;
		}
	}
	if (m != b)
		Mat3Copy (m, b);
	return 1;
}

void Mat3Mult (const mat3_t a, const mat3_t b, mat3_t c)
{
	mat3_t      ta, tb;					// in case c == b or c == a
	int         i, j, k;

	Mat3Transpose (a, ta);				// transpose so we can use dot
	Mat3Copy (b, tb);

	k = 0;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			c[k++] = DotProduct (ta + 3 * j, tb + 3 * i);
		}
	}
}

void Mat3MultVec (const mat3_t a, const vec3_t b, vec3_t c)
{
	int         i;
	vec3_t      tb;

	VectorCopy (b, tb);
	for (i = 0; i < 3; i++)
		c[i] = a[i + 0] * tb[0] + a[i + 3] * b[1] + a[i + 6] * b[2];
}

#define sqr(x) ((x) * (x))
void Mat3SymEigen (const mat3_t m, vec3_t e)
{
	vec_t       p, q, r;
	vec_t       phi;
	mat3_t      B;

	p = sqr (m[1]) + sqr (m[2]) + sqr (m[5]);
	if (p < 1e-6) {
		e[0] = m[0];
		e[1] = m[4];
		e[2] = m[8];
		return;
	}
	q = Mat3Trace (m) / 3;
	p = sqr (m[0] - q) + sqr (m[4] - q) + sqr (m[8] - q) + 2 * p;
	p = sqrt (p);
	Mat3Zero (B);
	B[0] = B[4] = B[8] = q;
	Mat3Subtract (m, B, B);
	Mat3Scale (B, 1.0 / p, B);
	r = Mat3Determinant (B) / 2;
	if (r >= 1)
		phi = 0;
	else if (r <= -1)
		phi = M_PI / 3;
	else
		phi = acos (r) / 3;

	e[0] = q + 2 * p * cos (phi);
	e[2] = q + 2 * p * cos (phi + M_PI * 2 / 3);
	e[1] = 3 * q - e[0] - e[2];
}

void
Mat4Init (const quat_t rot, const vec3_t scale, const vec3_t trans, mat4_t mat)
{
	QuatToMatrix (rot, mat, 1, 1);
	VectorScale (mat + 0, scale[0], mat + 0);
	VectorScale (mat + 4, scale[1], mat + 4);
	VectorScale (mat + 8, scale[2], mat + 8);
	VectorCopy (trans, mat + 12);
}

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

static void
Mat4Sub3 (const mat4_t m4, mat3_t m3, int i, int j)
{
	int         si, sj, di, dj;

	for (di = 0; di < 3; di++) {
		for (dj = 0; dj < 3; dj++) {
			si = di + ((di >= i) ? 1 : 0);
			sj = dj + ((dj >= j) ? 1 : 0);
			m3[di * 3 + dj] = m4[si * 4 + sj];
		}
	}
}

static vec_t
Mat4Det (const mat4_t m)
{
	mat3_t      t;
	int         i;
	vec_t       res = 0, det, s = 1;

	for (i = 0; i < 4; i++, s = -s) {
		Mat4Sub3 (m, t, 0, i);
		det = Mat3Determinant (t);
		res += m[i] * det * s;
	}
	return res;
}

int
Mat4Inverse (const mat4_t a, mat4_t b)
{
	mat4_t      tb;
	mat3_t      m3;
	vec_t      *m = b;
	int         i, j;
	vec_t       det;
	vec_t       sign[2] = { 1, -1};

	det = Mat4Det (a);
	if (det * det < 1e-6) {
		Mat4Identity (b);
		return 0;
	}
	if (b == a)
		m = tb;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			Mat4Sub3 (a, m3, i, j);
			m[j * 4 + i] = sign[(i + j) & 1] * Mat3Determinant (m3) / det;
		}
	}
	if (m != b)
		Mat4Copy (m, b);
	return 1;
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

void
Mat4MultVec (const mat4_t a, const vec3_t b, vec3_t c)
{
	int         i;
	vec3_t      tb;

	VectorCopy (b, tb);
	for (i = 0; i < 3; i++)
		c[i] = a[i + 0] * tb[0] + a[i + 4] * b[1] + a[i + 8] * b[2] + a[i +12];
}

void
Mat4as3MultVec (const mat4_t a, const vec3_t b, vec3_t c)
{
	int         i;
	vec3_t      tb;

	VectorCopy (b, tb);
	for (i = 0; i < 3; i++)
		c[i] = a[i + 0] * tb[0] + a[i + 4] * b[1] + a[i + 8] * b[2];
}

int
Mat3Decompose (const mat3_t mat, quat_t rot, vec3_t shear, vec3_t scale)
{
	vec3_t      row[3], shr, scl;
	vec_t       l, t;
	int         i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			row[j][i] = mat[i * 3 + j];

	l = DotProduct (row[0], row[0]);
	if (l < 1e-5)
		return 0;
	scl[0] = sqrt (l);
	VectorScale (row[0], 1/scl[0], row[0]);
	shr[0] = DotProduct (row[0], row[1]);

	VectorMultSub (row[1], shr[0], row[0], row[1]);
	l = DotProduct (row[1], row[1]);
	if (l < 1e-5)
		return 0;
	scl[1] = sqrt (l);
	shr[0] /= scl[1];
	VectorScale (row[1], 1/scl[1], row[1]);
	shr[1] = DotProduct (row[0], row[2]);

	VectorMultSub (row[2], shr[1], row[0], row[2]);
	shr[2] = DotProduct (row[1], row[2]);
	VectorMultSub (row[2], shr[2], row[1], row[2]);
	l = DotProduct (row[2], row[2]);
	if (l < 1e-5)
		return 0;
	scl[2] = sqrt (l);
	shr[1] /= scl[2];
	shr[2] /= scl[2];
	VectorScale (row[2], 1/scl[2], row[2]);
	if (scale)
		VectorCopy (scl, scale);
	if (shear)
		VectorCopy (shr, shear);
	if (!rot)
		return 1;

	t = 1 + row[0][0] + row[1][1] + row[2][2];
	if (t >= 1e-5) {
		vec_t       s = sqrt (t) * 2;
		rot[0] = (row[2][1] - row[1][2]) / s;
		rot[1] = (row[0][2] - row[2][0]) / s;
		rot[2] = (row[1][0] - row[0][1]) / s;
		rot[3] = s / 4;
	} else {
		if (row[0][0] > row[1][1] && row[0][0] > row[2][2]) {
			vec_t       s = sqrt (1 + row[0][0] - row[1][1] - row[2][2]) * 2;
			rot[0] = s / 4;
			rot[1] = (row[1][0] + row[0][1]) / s;
			rot[2] = (row[0][2] + row[2][0]) / s;
			rot[3] = (row[2][1] - row[1][2]) / s;
		} else if (row[1][1] > row[2][2]) {
			vec_t       s = sqrt (1 + row[1][1] - row[0][0] - row[2][2]) * 2;
			rot[0] = (row[1][0] + row[0][1]) / s;
			rot[1] = s / 4;
			rot[2] = (row[2][1] + row[1][2]) / s;
			rot[3] = (row[0][2] - row[2][0]) / s;
		} else {
			vec_t       s = sqrt (1 + row[2][2] - row[0][0] - row[1][1]) * 2;
			rot[0] = (row[0][2] + row[2][0]) / s;
			rot[1] = (row[2][1] + row[1][2]) / s;
			rot[2] = s / 4;
			rot[3] = (row[1][0] - row[0][1]) / s;
		}
	}
	return 1;
}

int
Mat4Decompose (const mat4_t mat, quat_t rot, vec3_t shear, vec3_t scale,
			   vec3_t trans)
{
	mat3_t      m3;

	if (trans)
		VectorCopy (mat + 12, trans);
	Mat4toMat3 (mat, m3);
	return Mat3Decompose (m3, rot, shear, scale);
}

void
BarycentricCoords (const vec_t **points, int num_points, const vec3_t p,
				   vec_t *lambda)
{
	vec3_t      a, b, c, x, ab, bc, ca, n;
	vec_t       div;
	if (num_points > 4)
		Sys_Error ("Don't know how to compute the barycentric coordinates "
				   "for %d points", num_points);
	switch (num_points) {
		case 1:
			lambda[0] = 1;
			return;
		case 2:
			VectorSubtract (p, points[0], x);
			VectorSubtract (points[1], points[0], a);
			lambda[1] = DotProduct (x, a) / DotProduct (a, a);
			lambda[0] = 1 - lambda[1];
			return;
		case 3:
			VectorSubtract (p, points[0], x);
			VectorSubtract (points[1], points[0], a);
			VectorSubtract (points[2], points[0], b);
			CrossProduct (a, b, ab);
			div = DotProduct (ab, ab);
			CrossProduct (x, b, n);
			lambda[1] = DotProduct (n, ab) / div;
			CrossProduct (a, x, n);
			lambda[2] = DotProduct (n, ab) / div;
			lambda[0] = 1 - lambda[1] - lambda[2];
			return;
		case 4:
			VectorSubtract (p, points[0], x);
			VectorSubtract (points[1], points[0], a);
			VectorSubtract (points[2], points[0], b);
			VectorSubtract (points[3], points[0], c);
			CrossProduct (a, b, ab);
			CrossProduct (b, c, bc);
			CrossProduct (c, a, ca);
			div = DotProduct (a, bc);
			lambda[1] = DotProduct (x, bc) / div;
			lambda[2] = DotProduct (x, ca) / div;
			lambda[3] = DotProduct (x, ab) / div;
			lambda[0] = 1 - lambda[1] - lambda[2] - lambda[3];
			return;
	}
	Sys_Error ("Not enough points to project or enclose the point");
}

static int
circum_circle (const vec_t **points, int num_points, sphere_t *sphere)
{
	vec3_t      a, c, b;
	vec3_t      bc, ca, ab;
	vec_t       aa, bb, cc;
	vec_t       div;
	vec_t       alpha, beta, gamma;

	switch (num_points) {
		case 1:
			VectorCopy (points[0], sphere->center);
			return 1;
		case 2:
			VectorBlend (points[0], points[1], 0.5, sphere->center);
			return 1;
		case 3:
			VectorSubtract (points[0], points[1], a);
			VectorSubtract (points[0], points[2], b);
			VectorSubtract (points[1], points[2], c);
			aa = DotProduct (a, a);
			bb = DotProduct (b, b);
			cc = DotProduct (c, c);
			div = DotProduct (a, c);
			div = 2 * (aa * cc - div * div);
			if (fabs (div) < EQUAL_EPSILON) {
				// degenerate
				return 0;
			}
			alpha = cc * DotProduct (a, b) / div;
			beta = -bb * DotProduct (a, c) / div;
			gamma = aa * DotProduct (b, c) / div;
			VectorScale (points[0], alpha, sphere->center);
			VectorMultAdd (sphere->center, beta, points[1], sphere->center);
			VectorMultAdd (sphere->center, gamma, points[2], sphere->center);
			return 1;
		case 4:
			VectorSubtract (points[1], points[0], a);
			VectorSubtract (points[2], points[0], b);
			VectorSubtract (points[3], points[0], c);
			CrossProduct (b, c, bc);
			CrossProduct (c, a, ca);
			CrossProduct (a, b, ab);
			div = 2 * DotProduct (a, bc);
			if (fabs (div) < EQUAL_EPSILON) {
				// degenerate
				return 0;
			}
			aa = DotProduct (a, a) / div;
			bb = DotProduct (b, b) / div;
			cc = DotProduct (c, c) / div;
			VectorScale (bc, aa, sphere->center);
			VectorMultAdd (sphere->center, bb, ca, sphere->center);
			VectorMultAdd (sphere->center, cc, ab, sphere->center);
			VectorAdd (sphere->center, points[0], sphere->center);
			return 1;
	}
	return 0;
}

int
CircumSphere (const vec3_t points[], int num_points, sphere_t *sphere)
{
	const vec_t *p[] = {points[0], points[1], points[2], points[3]};

	if (num_points > 4)
		return 0;
	sphere->radius = 0;
	if (num_points) {
		if (circum_circle (p, num_points, sphere)) {
			if (num_points > 1)
				sphere->radius = VectorDistance (sphere->center, points[0]);
			return 1;
		}
		return 0;
	}
	VectorZero (sphere->center);
	return 1;
}

static void
closest_affine_point (const vec_t **points, int num_points, const vec3_t x,
					  vec3_t closest)
{
	vec3_t      a, b, n, d;
	vec_t       l;

	switch (num_points) {
		default:
		case 1:
			VectorCopy (points[0], closest);
			break;
		case 2:
			VectorSubtract (points[1], points[0], n);
			VectorSubtract (x, points[0], d);
			l = DotProduct (d, n) / DotProduct (n, n);
			VectorMultAdd (points[0], l, n, closest);
			break;
		case 3:
			VectorSubtract (points[1], points[0], a);
			VectorSubtract (points[2], points[0], b);
			CrossProduct (a, b, n);
			VectorSubtract (points[0], x, d);
			l = DotProduct (d, n) / DotProduct (n, n);
			VectorMultAdd (x, l, n, closest);
			break;
	}
}

static int
test_support_points(const vec_t **points, int *num_points, const vec3_t center)
{
	int         in_affine = 0;
	int         in_convex = 0;
	vec3_t      v, d, n, a, b;
	vec_t       nn, dd, vv, dn;

	switch (*num_points) {
		case 1:
			in_affine = VectorCompare (points[0], center);
			// the convex hull and affine hull for a single point are the same
			in_convex = in_affine;
			break;
		case 2:
			VectorSubtract (points[1], points[0], v);
			VectorSubtract (center, points[0], d);
			CrossProduct (v, d, n);
			nn = DotProduct (n, n);
			dd = DotProduct (d, d);
			vv = DotProduct (v, v);
			in_affine = nn < 1e-8 * vv * dd;
			break;
		case 3:
			VectorSubtract (points[1], points[0], a);
			VectorSubtract (points[2], points[0], b);
			VectorSubtract (center, points[0], d);
			CrossProduct (a, b, n);
			dn = DotProduct (d, n);
			dd = DotProduct (d, d);
			nn = DotProduct (n, n);
			in_affine = dn * dn < 1e-8 * dd * nn;
			break;
		case 4:
			in_affine = 1;
			break;
		default:
			Sys_Error ("Invalid number of points (%d) in test_support_points",
					   *num_points);
	}

	// if in_convex is not true while in_affine is, then need to test as
	// there is more than one dimension for the affine hull (a single support
	// point is never dropped as it cannot be redundant)
	if (in_affine && !in_convex) {
		vec_t       lambda[4];
		int         dropped = 0;
		int         count = *num_points;

		BarycentricCoords (points, count, center, lambda);

		for (int i = 0; i < count; i++) {
			points[i - dropped] = points[i];
			if (lambda[i] < -1e-4) {
				dropped++;
				(*num_points)--;
			}
		}
		in_convex = !dropped;
		if (dropped) {
			for (int i = count - dropped; i < count; i++) {
				points[i] = 0;
			}
		}
	}
	return in_convex;
}

sphere_t
SmallestEnclosingBall (const vec3_t points[], int num_points)
{
	sphere_t    sphere;
	const vec_t *best;
	const vec_t *support[4];
	int         num_support;
	vec_t       dist, best_dist;
	int         i;
	int         iters = 0;

	if (num_points < 1) {
		VectorZero (sphere.center);
		sphere.radius = 0;
		return sphere;
	}

	for (i = 0; i < 4; i++)
		support[i] = 0;

	VectorCopy (points[0], sphere.center);
	best_dist = dist = 0;
	best = points[0];
	for (i = 1; i < num_points; i++) {
		dist = VectorDistance_fast (points[i], sphere.center);
		if (dist > best_dist) {
			best_dist = dist;
			best = points[i];
		}
	}
	num_support = 1;
	support[0] = best;
	sphere.radius = best_dist;	// note: radius squared until the end

	while (!test_support_points (support, &num_support, sphere.center)) {
		vec3_t      affine;
		vec3_t      center_to_affine, center_to_point;
		vec_t       affine_dist, point_proj, point_dist, bound;
		vec_t       scale = 1;
		int         i;

		if (iters++ > 10)
			Sys_Error ("stuck SEB");
		best = 0;

		closest_affine_point (support, num_support, sphere.center, affine);
		VectorSubtract (affine, sphere.center, center_to_affine);
		affine_dist = DotProduct (center_to_affine, center_to_affine);
		for (i = 0; i < num_points; i++) {
			if (points[i] == support[0] || points[i] == support[1]
				|| points[i] == support[2])
				continue;
			VectorSubtract (points[i], sphere.center, center_to_point);
			point_proj = DotProduct (center_to_affine, center_to_point);
			if (affine_dist - point_proj <= 0
				|| ((affine_dist - point_proj) * (affine_dist - point_proj)
					< 1e-8 * sphere.radius * affine_dist))
				continue;
			point_dist = DotProduct (center_to_point, center_to_point);
			bound = sphere.radius - point_dist;
			bound /= 2 * (affine_dist - point_proj);
			if (bound < scale) {
				best = points[i];
				scale = bound;
			}
		}
		VectorMultAdd (sphere.center, scale, center_to_affine, sphere.center);
		sphere.radius = VectorDistance_fast (sphere.center, support[0]);
		if (best) {
			support[num_support++] = best;
		}
	}
	best_dist = 0;
	for (i = 0; i < num_points; i++) {
		dist = VectorDistance_fast (sphere.center, points[i]);
		if (dist > best_dist)
			best_dist = dist;
	}
	sphere.radius = sqrt (best_dist);
	return sphere;
}
