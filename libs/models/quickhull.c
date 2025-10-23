/*
	quickhull.c

	quickhull implementation

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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
// Models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/quakeio.h"

#include "quickhull.h"

size_t
quickhull_context_size (int num_verts)
{
	int max_faces = 2 * num_verts - 4;
	size_t size = sizeof (qhullctx_t)
				+ sizeof (vec4f_t[num_verts])
				+ sizeof (vec4f_t[max_faces])
				+ sizeof (set_t[max_faces])
				+ sizeof (int[max_faces])
				+ sizeof (float[max_faces])
				+ sizeof (quarteredge_t[max_faces * 3])
				+ sizeof (uint32_t[max_faces]);
	if (SET_LARGE_SET (num_verts)) {
		size += max_faces * SET_SIZE (num_verts) / 8;
	}
	if (SET_LARGE_SET (max_faces)) {
		size += SET_SIZE (max_faces) / 8;
	}
	return (size + 15) & ~UINT64_C(15);
}

void
quickhull_init_context (int num_verts, void *memory)
{
	int max_faces = 2 * num_verts - 4;

	qhullctx_t *qhull = memory;
	auto verts = (vec4f_t *) &qhull[1];
	auto planes = (vec4f_t *) &verts[num_verts];
	auto outside_sets = (set_t *) &planes[max_faces];
	auto highest_points = (int *) &outside_sets[max_faces];
	auto heights = (float *) &highest_points[max_faces];
	auto halfedges = (quarteredge_t *) &heights[max_faces];
	auto ids = (uint32_t *) &halfedges[max_faces * 3];
	auto bits = (byte *) &ids[max_faces];
	*qhull = (qhullctx_t) {
		.verts = verts,
		.faces = {
			.planes = planes,
			.outside_sets = outside_sets,
			.highest_points = highest_points,
			.heights = heights,
		},
		.halfedges = halfedges,
		.face_ids = {
			.ids = ids,
			.next = Ent_Index (nullent),
			.max_ids = max_faces,
		},
		.num_verts = num_verts,
	};
	if (SET_LARGE_SET (num_verts)) {
		#define outside_alloc(x) (set_bits_t *)(bits + i*SET_SIZE(num_verts)/8)
		for (int i = 0; i < max_faces; i++) {
			outside_sets[i] = SET_STATIC_INIT (num_verts, outside_alloc);
		}
		#undef outside_alloc
		bits += max_faces * SET_SIZE(num_verts) / 8;
	} else {
		#define outside_alloc(x) outside_sets[i].defmap
		for (int i = 0; i < max_faces; i++) {
			outside_sets[i] = SET_STATIC_INIT (num_verts, outside_alloc);
		}
		#undef outside_alloc
	}
	set_pool_init (&qhull->set_pool);
}

qhullctx_t *
quickhull_new_context (int num_verts)
{
	size_t size = quickhull_context_size (num_verts);
	qhullctx_t *qhull = malloc (size);
	quickhull_init_context (num_verts, qhull);
	return qhull;
}

void
quickhull_delete_context (qhullctx_t *qhull)
{
	free (qhull);
}

static qh_edge_t
qhull_edge (int a, int b, qhullctx_t *qhull, bool normalize)
{
	vec4f_t va = qhull->verts[a];
	vec4f_t vb = qhull->verts[b];
	vec4f_t m = crossf (va, vb);
	vec4f_t v = va[3] * vb - vb[3] * va;
	if (normalize) {
		v /= sqrtf (dotf (v,v)[0]);
		m /= sqrtf (dotf (v,v)[0]);
	}
	return (qh_edge_t) {
		.v = { VectorExpand (v) },
		.m = { VectorExpand (m) },
		.a = a,
		.b = b,
	};
}

//NOTE length squared
static float
qhull_edge_length (const qh_edge_t *edge)
{
	vec4f_t     v = loadvec3f (edge->v);
	return dotf (v, v)[0];
}

//NOTE dist squared
static float
qhull_edge_dist (const qh_edge_t *edge, int point, qhullctx_t *qhull)
{
	vec4f_t p = qhull->verts[point];
	vec4f_t v = loadvec3f (edge->v);
	vec4f_t m = loadvec3f (edge->m);
	vec4f_t d = crossf (v, p) + p[3] * m;
	return dotf (d, d)[0];
}

static vec4f_t
qhull_plane (int a, int b, int c, qhullctx_t *qhull)
{
	vec4f_t va = qhull->verts[a];
	vec4f_t vb = qhull->verts[b];
	vec4f_t vc = qhull->verts[c];
	vec4f_t n = crossf (vb - va, vc - va);
	n /= sqrtf (dotf (n, n)[0]);
	n[3] = -dotf(va, n)[0];
	return n;
}

static float
qhull_plane_dist (int face, int point, qhullctx_t *qhull)
{
	vec4f_t P = qhull->verts[point];
	vec4f_t p = qhull->faces.planes[face];
	return dotf (p, P)[0];
}

static bool
qhull_can_see (int face, int point, qhullctx_t *qhull)
{
	auto edges = &qhull->halfedges[face * 3];
	if (point == edges[0].vert
		|| point == edges[1].vert
		|| point == edges[2].vert) {
		return true;
	}
	return qhull_plane_dist (face, point, qhull) >= 0;
}

static void
qhull_make_simplex (qhullctx_t *qhull)
{
	int points[6] = {};
	vec4f_t *verts = qhull->verts;

	for (int i = 0; i < qhull->num_verts; i++) {
		vec4f_t v = verts[i];
		for (int j = 0; j < 3; j++) {
			if (v[j] < verts[points[0 + j]][j]) { points[0 + j] = i; }
			if (v[j] > verts[points[3 + j]][j]) { points[3 + j] = i; }
		}
	}

	// find the two points with the greatest separation
	int a = 0;
	int b = 1;
	qh_edge_t tEdge = qhull_edge (a, b, qhull, false);
	float bestd = qhull_edge_length (&tEdge);
	for (int i = 0; i < 6; i++) {
		int p = points[i];
		if (a == p || b == p) {
			continue;
		}
		for (int j = 0; j < 6; j++) {
			int q = points[j];
			if (q == p || a == q || b == q) {
				continue;
			}
			tEdge = qhull_edge (p, q, qhull, false);
			float r = qhull_edge_length (&tEdge);
			if (r > bestd) {
				a = tEdge.a;
				b = tEdge.b;
				bestd = r;
			}
		}
	}
	tEdge = qhull_edge (a, b, qhull, false);

	// Find a third point that maximizes the area of the triangle
	bestd = 0;
	int c = 0;
	for (int i = 0; i < 6; i++) {
		int p = points[i];
		if (a == p || b == p) {
			continue;
		}
		float r = qhull_edge_dist (&tEdge, p, qhull);
		if (r > bestd) {
			c = p;
			bestd = r;
		}
	}

	// Find a fourth point that maximizes the volume of the tetrahedron
	vec4f_t plane = qhull_plane (a, b, c, qhull);
	bestd = 0;
	int d = 0;
	for (int p = 0; p < qhull->num_verts; p++) {
		if (a == p || b == p || c == p) {
			continue;
		}
		float r = dotf (plane, qhull->verts[p])[0];
		if (r * r > bestd) {
			d = p;
			bestd = r * r;
		}
	}
	// If the point is above the plane, swap two points so it will be below
	// the plane when creating the tetrahedron
	if (dotf (plane, qhull->verts[d])[0] > 0) {
		int t = b;
		b = c;
		c = t;
	}
	int faces[4] = {
		ECS_NewId (&qhull->face_ids),
		ECS_NewId (&qhull->face_ids),
		ECS_NewId (&qhull->face_ids),
		ECS_NewId (&qhull->face_ids),
	};
	int simp_points[4][3] = {
		{c, a, b},
		{b, a, d},
		{d, a, c},
		{c, b, d},
	};
	for (int i = 0; i < 4; i++) {
		int f = faces[i];
		auto pts = simp_points[i];
		qhull->faces.planes[f] = qhull_plane (pts[0], pts[1], pts[2], qhull);
		set_empty (&qhull->faces.outside_sets[f]);
		qhull->faces.highest_points[f] = -1;
		qhull->faces.heights[f] = 0;
		for (int j = 0; j < 3; j++) {
			int h = f * 3 + j;
			qhull->halfedges[h] = (quarteredge_t) {
				.vert = pts[j],
				.edge = -1,
			};
		}
		if (f < 3) {
			qhull->halfedges[f * 3 + 0].twin = (f * 3 + 7) % 9;
			qhull->halfedges[f * 3 + 1].twin = (f * 3 + 3) % 9;
			qhull->halfedges[f * 3 + 2].twin = 9 + f;
		} else {
			qhull->halfedges[f * 3 + 0].twin = 2;
			qhull->halfedges[f * 3 + 1].twin = 5;
			qhull->halfedges[f * 3 + 2].twin = 8;
		}
	}
}

static bool
qhull_add_point (int face, int p, qhullctx_t *qhull)
{
	auto edges = qhull->halfedges + face * 3;

	if (p == edges[0].vert
		|| p == edges[1].vert
		|| p == edges[2].vert) {
		// p is one of the initial support points, "discard" it
		return true;
	};
	vec4f_t v = qhull->verts[p];

	float d = dotf (v, qhull->faces.planes[face])[0];
	if (d <= 0) {
		// p is below the face, so not in its outside set
		return false;
	}
	for (int j = 0; j < 3; j++) {
		vec4f_t fv = qhull->verts[edges[j].vert];
		if (dotf (v - fv, v - fv)[0] < 1e-6) {
			// p is a dup of (ie too close to) a face point
			return true;
		}
	}
	if (d > qhull->faces.heights[face]) {
		qhull->faces.heights[face] = d;
		qhull->faces.highest_points[face] = p;
	}
	set_add (&qhull->faces.outside_sets[face], p);
	return true;
}

static int
qhull_set_pop (set_t *set, qhullctx_t *qhull)
{
	auto iter = set_first_r (&qhull->set_pool, set);
	if (iter) {
		int val = iter->element;
		set_del_iter_r (&qhull->set_pool, iter);
		set_remove (set, iter->element);
		return val;
	}
	return -1;
}

static void
qh_light_faces (set_t *visited, set_t *litFaces, int face, int point,
				qhullctx_t *qhull)
{
	if (set_is_member (visited, face)) {
		return;
	}
	set_add (visited, face);
	if (qhull_can_see (face, point, qhull)) {
		set_add (litFaces, face);
		auto edges = qhull->halfedges + face * 3;
		for (int i = 0; i < 3; i++) {
			if (edges[i].twin < 0) {
				continue;
			}
			qh_light_faces (visited, litFaces, edges[i].twin / 3, point, qhull);
		}
	}
}

static void
qhull_light_faces (set_t *litFaces, int face, int point, qhullctx_t *qhull)
{
	set_t *visited = set_new_r (&qhull->set_pool);
	set_empty (litFaces);
	qh_light_faces (visited, litFaces, face, point, qhull);
}

static void
qhull_horizon_edges (set_t *horizon, set_t *faces, qhullctx_t *qhull)
{
	set_empty (horizon);
	for (auto fi = set_first_r (&qhull->set_pool, faces); fi;
		 fi = set_next_r (&qhull->set_pool, fi)) {
		auto edges = qhull->halfedges + fi->element * 3;
		for (int i = 0; i < 3; i++) {
			if (edges[i].twin >= 0
				&& !set_is_member (faces, edges[i].twin / 3)) {
				set_add (horizon, edges[i].twin);
			}
		}
	}
}

static int
qh_next (int e)
{
	return e < 0 ? e : e - (e % 3) + ((e + 1) % 3);
}

static void
qhull_make_face (int e, int f, int point, int prev_face, qhullctx_t *qhull)
{
	int n = qh_next (e);
	int a = qhull->halfedges[e].vert;
	int b = qhull->halfedges[n].vert;
	qhull->faces.planes[f] = qhull_plane (b, a, point, qhull);
	set_empty (&qhull->faces.outside_sets[f]);
	qhull->faces.highest_points[f] = -1;
	qhull->faces.heights[f] = 0;
	auto edges = &qhull->halfedges[f * 3];
	qhull->halfedges[e].twin = f * 3;
	edges[0] = (quarteredge_t) {
		.twin = e,
		.vert = b,
		.edge = -1,
	};
	edges[1] = (quarteredge_t) {
		.twin = prev_face * 3 + 2,
		.vert = a,
		.edge = -1,
	};
	edges[2] = (quarteredge_t) {
		.twin = -1,
		.vert = point,
		.edge = -1,
	};
}

bool
quickhull_make_hull (qhullctx_t *qhull)
{
	qhull_make_simplex (qhull);
	for (int p = 0; p < qhull->num_verts; p++) {
		for (int i = 0; i < 4; i++) {
			if (qhull_add_point (i, p, qhull)) {
				// the point was added to (or consumed by) the face
				break;
			}
		}
	}

	set_t *faces = set_new_r (&qhull->set_pool);
	set_t *finalFaces = set_new_r (&qhull->set_pool);
	set_t *litFaces = set_new_r (&qhull->set_pool);
	set_t *horizonEdges = set_new_r (&qhull->set_pool);
	set_t *vispoints = set_new_r (&qhull->set_pool);
	set_t *newFaces = set_new_r (&qhull->set_pool);

	set_add_range (faces, 0, 4);
	while (!set_is_empty (faces)) {
		int f = qhull_set_pop (faces, qhull);
		if (set_is_empty (&qhull->faces.outside_sets[f])) {
			set_add (finalFaces, f);
			continue;
		}

		int point = qhull->faces.highest_points[f];
		if (qhull_plane_dist (f, point, qhull) < 1e-4) {
			// the point furthest out from the face is too close to worry
			// about, so remove the face from all the points "above" the face
			// and make it an exterior face: it's done.
			set_empty (&qhull->faces.outside_sets[f]);
			set_add (finalFaces, f);
			continue;
		}

		qhull_light_faces (litFaces, f, point, qhull);
		qhull_horizon_edges (horizonEdges, litFaces, qhull);
		set_empty (vispoints);
		for (auto fi = set_first_r (&qhull->set_pool, litFaces); fi;
			 fi = set_next_r (&qhull->set_pool, fi)) {
			set_union (vispoints, &qhull->faces.outside_sets[fi->element]);
			uint32_t f = qhull->face_ids.ids[fi->element];
			ECS_DelId (&qhull->face_ids, f);
		}
		set_empty (newFaces);
		{
			int first_face = -1;
			int prev_face = -1;
			int e = qhull_set_pop (horizonEdges, qhull);
			set_add (horizonEdges, e);
			int first_edge = e;
			do {
				int f = Ent_Index (ECS_NewId (&qhull->face_ids));
				qhull_make_face (e, f, point, prev_face, qhull);
				if (prev_face >= 0) {
					qhull->halfedges[prev_face * 3 + 2].twin = f * 3 + 1;
				}
				set_add (newFaces, f);
				prev_face = f;
				if (first_face == -1) {
					first_face = f;
				}
				int s = e;
				e = qh_next (e);
				while (e != s && e >= 0 && !set_is_member (horizonEdges, e)) {
					e = qh_next (qhull->halfedges[e].twin);
				}
				if (e == s) {
					e = -1;
				}
			} while (e >= 0 && e != first_edge);
			int le = prev_face * 3 + 2;
			int fe = first_face * 3 + 1;
			qhull->halfedges[le].twin = fe;
			qhull->halfedges[fe].twin = le;
		}
		for (auto pi = set_first_r (&qhull->set_pool, vispoints); pi;
			 pi = set_next_r (&qhull->set_pool, pi)) {
			for (auto fi = set_first_r (&qhull->set_pool, newFaces); fi;
				 fi = set_next_r (&qhull->set_pool, fi)) {
				if (qhull_add_point (fi->element, pi->element, qhull)) {
					// the point was added to (or consumed by) the face
					break;
				}
			}
		}
		set_union (faces, newFaces);
	}
	printf ("ff:%s\n", set_as_string (finalFaces));
	return false;
}
