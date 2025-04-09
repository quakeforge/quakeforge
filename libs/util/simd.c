/*
	simd.c

	SIMD math support

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qfalloca.h"

#include <math.h>

#define IMPLEMENT_VEC2F_Funcs
#define IMPLEMENT_VEC2D_Funcs
#define IMPLEMENT_VEC2I_Funcs
#define IMPLEMENT_VEC4F_Funcs
#define IMPLEMENT_VEC4D_Funcs
#define IMPLEMENT_VEC4I_Funcs
#define IMPLEMENT_MAT4F_Funcs

#include "QF/mathlib.h"
#include "QF/simd/vec2d.h"
#include "QF/simd/vec2f.h"
#include "QF/simd/vec2i.h"
#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/vec4i.h"
#include "QF/simd/mat4f.h"
#include "QF/set.h"
#include "QF/sys.h"

vec4f_t
BarycentricCoords_vf (const vec4f_t **points, int num_points, const vec4f_t p)
{
	vec4f_t     a, b, c, x, l = {}, ab, bc, ca, d;
	if (num_points > 4)
		Sys_Error ("Don't know how to compute the barycentric coordinates "
				   "for %d points", num_points);
	switch (num_points) {
		case 1:
			l[0] = 1;
			return l;
		case 2:
			x = p - *points[0];
			a = *points[1] - *points[0];
			d = dotf (x, a) / dotf (a, a);
			l[1] = d[0];
			l[0] = 1 - d[0];
			return l;
		case 3:
			x = p - *points[0];
			a = *points[1] - *points[0];
			b = *points[2] - *points[0];
			ab = crossf (a, b);
			d = dotf (ab, ab);
			l[1] = dotf (crossf (x, b), ab)[0];
			l[2] = dotf (crossf (a, x), ab)[0];
			l[0] = d[0] - l[1] - l[2];
			return l / d;
		case 4:
			x = p - *points[0];
			a = *points[1] - *points[0];
			b = *points[2] - *points[0];
			c = *points[3] - *points[0];
			ab = crossf (a, b);
			bc = crossf (b, c);
			ca = crossf (c, a);
			d = dotf (a, bc);
			l[1] = (dotf (x, bc) / d)[0];
			l[2] = (dotf (x, ca) / d)[0];
			l[3] = (dotf (x, ab) / d)[0];
			l[0] = 1 - l[1] - l[2] - l[3];
			return l;
	}
	Sys_Error ("Not enough points to project or enclose the point");
}

static int
circum_circle (const vec4f_t points[], int num_points, vec4f_t *center)
{
	vec4f_t     a, c, b;
	vec4f_t     bc, ca, ab;
	vec4f_t     aa, bb, cc;
	vec4f_t     div;
	vec4f_t     alpha, beta, gamma;

	switch (num_points) {
		case 1:
			*center = points[0];
			return 1;
		case 2:
			*center = (points[0] + points[1]) / 2;
			return 1;
		case 3:
			a = points[0] - points[1];
			b = points[0] - points[2];
			c = points[1] - points[2];
			aa = dotf (a, a);
			bb = dotf (b, b);
			cc = dotf (c, c);
			div = dotf (a, c);
			div = 2 * (aa * cc - div * div);
			if (fabs (div[0]) < EQUAL_EPSILON) {
				// degenerate
				return 0;
			}
			alpha = cc * dotf (a, b) / div;
			beta = -bb * dotf (a, c) / div;
			gamma = aa * dotf (b, c) / div;
			*center = alpha * points[0] + beta * points[1] + gamma * points[2];
			return 1;
		case 4:
			a = points[1] - points[0];
			b = points[2] - points[0];
			c = points[3] - points[0];
			bc = crossf (b, c);
			ca = crossf (c, a);
			ab = crossf (a, b);
			div = 2 * dotf (a, bc);
			if (fabs (div[0]) < EQUAL_EPSILON) {
				// degenerate
				return 0;
			}
			aa = dotf (a, a) / div;
			bb = dotf (b, b) / div;
			cc = dotf (c, c) / div;
			*center = bc * aa + bb * ca + cc * ab + points[0];
			return 1;
	}
	return 0;
}

vspheref_t
CircumSphere_vf (const vec4f_t *points, int num_points)
{
	vspheref_t  sphere = {};
	if (num_points > 0 && num_points <= 4) {
		if (circum_circle (points, num_points, &sphere.center)) {
			vec4f_t     d = sphere.center - points[0];
			sphere.radius = sqrt(dotf (d, d)[0]);
		} else {
			// degenerate
			sphere.radius = -1;
		}
	}
	return sphere;
}

static vec4f_t
closest_affine_point (const vec4f_t **points, int num_points, const vec4f_t x)
{
	vec4f_t     closest = {};
	vec4f_t     a, b, n, d;
	vec4f_t     l;

	switch (num_points) {
		default:
		case 1:
			closest = *points[0];
			break;
		case 2:
			n = *points[1] - *points[0];
			d = x - *points[0];
			l = dotf (d, n) / dotf (n, n);
			closest = *points[0] + l * n;
			break;
		case 3:
			a = *points[1] - *points[0];
			b = *points[2] - *points[0];
			n = crossf (a, b);
			d = *points[0] - x;
			l = dotf (d, n) / dotf (n, n);
			closest = x + l * n;
			break;
	}
	return closest;
}

static int
test_support_points(const vec4f_t **points, int *num_points, vec4f_t center)
{
	vec4i_t     cmp;
	int         in_affine = 0;
	int         in_convex = 0;
	vec4f_t     v, d, n, a, b;
	float       nn, dd, vv, dn;

	switch (*num_points) {
		case 1:
			cmp = *points[0] == center;
			in_affine = cmp[0] && cmp[1] && cmp[2];
			// the convex hull and affine hull for a single point are the same
			in_convex = in_affine;
			break;
		case 2:
			v = *points[1] - *points[0];
			d = center - (*points[0] + *points[1]) / 2;
			n = crossf (v, d);
			nn = dotf (n, n)[0];
			vv = dotf (v, v)[0];
			in_affine = nn <= 1e-5 * vv * vv;
			break;
		case 3:
			a = *points[1] - *points[0];
			b = *points[2] - *points[0];
			d = center - *points[0];
			n = crossf (a, b);
			dn = dotf (d, n)[0];
			dd = dotf (d, d)[0];
			nn = dotf (n, n)[0];
			in_affine = dn * dn < 1e-5 * dd * nn;
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
		vec4f_t     lambda;
		int         dropped = 0;
		int         count = *num_points;

		lambda = BarycentricCoords_vf (points, count, center);

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

vspheref_t
SmallestEnclosingBall_vf (const vec4f_t *points, int num_points)
{
	set_t       was_support = SET_STATIC_INIT (num_points, alloca);
	vspheref_t  sphere = {};
	vec4f_t     center = {};
	const vec4f_t *best;
	const vec4f_t *support[4];
	int         num_support;
	int         i;
	int         iters = 0;

	if (num_points < 1) {
		return sphere;
	}

	for (i = 0; i < 4; i++) {
		support[i] = 0;
	}
	set_empty (&was_support);

	vec4f_t     dist = {};
	float       best_dist = 0;
	center = points[0];
	best = &points[0];
	for (i = 1; i < num_points; i++) {
		dist = points[i] - center;
		dist = dotf (dist, dist);
		if (dist[0] > best_dist) {
			best_dist = dist[0];
			best = &points[i];
		}
	}
	num_support = 1;
	support[0] = best;
	sphere.radius = best_dist;	// note: radius squared until the end
	set_add (&was_support, best - points);

	while (!test_support_points (support, &num_support, center)) {
		vec4f_t     affine, r, rr;
		vec4f_t     v, p, pv, pp, x;
		vec4f_t     best_x = { };
		int         i;

		//Sys_Printf ("%d: "VEC4F_FMT", %.9g, %d\n", iters, VEC4_EXP (center), sqrt(sphere.radius), num_support);
		if (iters++ > 2 * num_points) {
			//for (i = 0; i < num_points; i++) {
			//	Sys_Printf (VEC4F_FMT",\n", VEC4_EXP (points[i]));
			//}
			Sys_Error ("stuck SEB");
		}

		affine = closest_affine_point (support, num_support, center);
		r = *support[0] - affine; //FIXME better choice
		rr = dotf (r, r);
		v = center - affine;

		best = 0;
		for (i = 0; i < num_points; i++) {
			if (SET_TEST_MEMBER (&was_support, i)) {
				continue;
			}
			p = points[i] - affine;
			pp = dotf (p, p);
			pv = dotf (p, v);
			if (pp[0] <= rr[0] || pv[0] <= 0
				|| (pv[0] * pv[0]) < 1e-6 * rr[0]) {
				continue;
			}
			x = (pp - rr) / (2 * pv);
			if (x[0] > best_x[0]) {
				best = &points[i];
				best_x = x;
			}
		}
		center = affine + best_x * v;
		dist = center - *support[0];
		sphere.radius = dotf (dist, dist)[0];
		if (best) {
			support[num_support++] = best;
			set_add (&was_support, best - points);
		}
	}
	best_dist = 0;
	for (i = 0; i < num_points; i++) {
		dist = center - points[i];
		dist = dotf (dist, dist);
		if (dist[0] > best_dist)
			best_dist = dist[0];
	}
	sphere.center = center;
	sphere.radius = sqrt (best_dist);
	return sphere;
}
