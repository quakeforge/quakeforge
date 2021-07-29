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

#include <math.h>

#define IMPLEMENT_VEC4F_Funcs
#define IMPLEMENT_VEC4D_Funcs
#define IMPLEMENT_MAT4F_Funcs

#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"
#include "QF/sys.h"

vec4f_t
BarycentricCoords_vf (const vec4f_t **points, int num_points, const vec4f_t p)
{
	vec4f_t zero = { };
	vec4f_t     a, b, c, x, l, ab, bc, ca, d;
	if (num_points > 4)
		Sys_Error ("Don't know how to compute the barycentric coordinates "
				   "for %d points", num_points);
	switch (num_points) {
		case 1:
			l = zero;
			l[0] = 1;
			return l;
		case 2:
			x = p - *points[0];
			a = *points[1] - *points[0];
			d = dotf (x, a) / dotf (a, a);
			l = zero;
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

static vec4f_t
circum_circle (const vec4f_t points[], int num_points)
{
	vec4f_t     a, c, b;
	vec4f_t     bc, ca, ab;
	vec4f_t     aa, bb, cc;
	vec4f_t     div;
	vec4f_t     alpha, beta, gamma;

	switch (num_points) {
		case 1:
			return points[0];
		case 2:
			return (points[0] + points[1]) / 2;
		case 3:
			a = points[0] - points[1];
			b = points[0] - points[2];
			c = points[1] - points[2];
			aa = dotf (a, a);
			bb = dotf (b, b);
			cc = dotf (c, c);
			div = dotf (a, c);
			div = 2 * (aa * cc - div * div);
			alpha = cc * dotf (a, b) / div;
			beta = -bb * dotf (a, c) / div;
			gamma = aa * dotf (b, c) / div;
			return alpha * points[0] + beta * points[1] + gamma * points[2];
		case 4:
			a = points[1] - points[0];
			b = points[2] - points[0];
			c = points[3] - points[0];
			bc = crossf (b, c);
			ca = crossf (c, a);
			ab = crossf (a, b);
			div = 2 * dotf (a, bc);
			aa = dotf (a, a) / div;
			bb = dotf (b, b) / div;
			cc = dotf (c, c) / div;
			return bc * aa + bb * ca + cc * ab + points[0];
	}
	vec4f_t     zero = {};
	return zero;
}

vspheref_t
CircumSphere_vf (const vec4f_t *points, int num_points)
{
	vspheref_t  sphere = {};
	if (num_points > 0 && num_points <= 4) {
		sphere.center = circum_circle (points, num_points);
		vec4f_t     d = sphere.center - points[0];
		sphere.radius = sqrt(dotf (d, d)[0]);
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
			d = center - *points[0];
			n = crossf (v, d);
			nn = dotf (n, n)[0];
			dd = dotf (d, d)[0];
			vv = dotf (v, v)[0];
			in_affine = nn < 1e-5 * vv * dd;
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

	while (!test_support_points (support, &num_support, center)) {
		vec4f_t     affine;
		vec4f_t     center_to_affine, center_to_point;
		float       affine_dist, point_proj, point_dist, bound;
		float       scale = 1;
		int         i;

		if (iters++ > 10)
			Sys_Error ("stuck SEB");

		affine = closest_affine_point (support, num_support, center);
		center_to_affine = affine - center;
		affine_dist = dotf (center_to_affine, center_to_affine)[0];
		if (affine_dist < sphere.radius * 1e-5) {
			// It's possible test_support_points failed due to precision
			// issues
			break;
		}

		best = 0;
		for (i = 0; i < num_points; i++) {
			if (&points[i] == support[0] || &points[i] == support[1]
				|| &points[i] == support[2])
				continue;
			center_to_point = points[i] - center;
			point_proj = dotf (center_to_affine, center_to_point)[0];
			if (affine_dist - point_proj <= 0
				|| ((affine_dist - point_proj) * (affine_dist - point_proj)
					< 1e-6 * sphere.radius * affine_dist))
				continue;
			point_dist = dotf (center_to_point, center_to_point)[0];
			bound = sphere.radius - point_dist;
			bound /= 2 * (affine_dist - point_proj);
			if (bound < scale) {
				best = &points[i];
				scale = bound;
			}
		}
		center = center + scale * center_to_affine;
		dist = center - *support[0];
		sphere.radius = dotf (dist, dist)[0];
		if (best) {
			support[num_support++] = best;
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
