/*
	gl_sky_clip.c

	sky polygons

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

#define NH_DEFINE
#include "namehack.h"

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_sky.h"
#include "QF/GL/qf_vid.h"

#include "r_internal.h"

#include "compat.h"

#define BOX_WIDTH 2056

/* cube face to sky texture offset conversion */
// see gl_sky.c for naming:       rt bk up lf ft dn
static const int skytex_offs[] = { 0, 1, 4, 2, 3, 5 };

/* convert axis and face distance into face */
static const int faces_table[3][6] = {
	{-1, 0, 0, -1, 3, 3},
	{-1, 4, 4, -1, 1, 1},
	{-1, 2, 2, -1, 5, 5},
};

/* convert face magic bit mask to index into visit array */
static const int faces_bit_magic[] = { 2, 1, -1, 0, 3, -1, 4, -1 };

/* axis the cube face cuts (also index into vec3_t and n % 3 for 0 <= n < 6) */
static const int face_axis[] = { 0, 1, 2, 0, 1, 2 };

/* offset on the axis the cube face cuts */
static const vec_t face_offset[] = { 1024, 1024, 1024, -1024, -1024, -1024 };

/* cube face */
struct face_def {
	int         tex;					// texture to bind to
	glpoly_t    poly;					// describe the polygon of this face
	float       verts[32][VERTEXSIZE];
};

struct visit_def {
	int         face;					// face being visited
	int         leave;					// vertex departed through
};

/* our cube */
struct box_def {
	/* keep track of which cube faces we visit and in what order */
	struct visit_def visited_faces[9];
	int         face_visits[6];
	int         face_count;
	/* the cube faces */
	struct face_def face[6];
};



/*
	determine_face

	return the face of the cube which v hits first
	0	+x
	1	+y
	2	+z
	3	-x
	4	-y
	5	-z
	Also scales v so it touches that face.
*/
static int
determine_face (vec3_t v)
{
	float       m;
	float       a[3];
	int         i = 0;

	m = a[0] = fabs (v[0]);
	a[1] = fabs (v[1]);
	a[2] = fabs (v[2]);
	if (a[1] > m) {
		m = a[1];
		i = 1;
	}
	if (a[2] > m) {
		m = a[2];
		i = 2;
	}
	if (!m) {
		Sys_Error ("You are speared by a sky poly edge");
	}
	if (v[i] < 0)
		i += 3;
	VectorScale (v, 1024 / m, v);
	return i;
}

/*
	find_intersect (for want of a better name)

	finds the point of intersection of the plane formed by the eye and the two
	points on the cube and the edge of the cube defined by the two faces.
	Currently, this will break if the two points are not on adjoining cube
	faces (ie either on opposing faces or the same face).

	The equation for the point of intersection of a line and a plane is:

		        (x - p).n
		y = x - _________ v
		           v.n

	where n is the normal to the plane, p is a point on the plane, x is a
	point on the line, and v is the direction vector of the line. n is found
	by (x1 - e) cross (x2 - e) and p is taken to be e (e = eye coords) for
	simplicity. However, because e is at 0,0,0, this simplifies to n = x1
	cross x2 and p = 0,0,0, so the equation above simplifies to:

		        x.n
		y = x - ___ v
		        v.n
*/
static int
find_intersect (int face1, const vec3_t x1, int face2, const vec3_t x2,
				vec3_t y)
{
	int         axis;
	vec3_t      n;						// normal to the plane formed by the
										// eye and the two points on the cube.
	vec3_t      x = { 0, 0, 0 };		// point on cube edge of adjoining
										// faces. always on an axis plane.
	vec3_t      v = { 0, 0, 0 };		// direction vector of cube edge.
	// always +ve
	vec_t       x_n, v_n;				// x.n and v.n
	vec3_t      t;

	x[face_axis[face1]] = face_offset[face1];
	x[face_axis[face2]] = face_offset[face2];

	axis = 3 - ((face_axis[face1]) + (face_axis[face2]));
	v[axis] = 1;

	CrossProduct (x1, x2, n);

	x_n = DotProduct (x, n);
	v_n = DotProduct (v, n);
	VectorScale (v, x_n / v_n, t);
	VectorSubtract (x, t, y);

	return axis;
}

/*
	find_cube_vertex

	get the coords of the vertex common to the three specified faces of the
	cube. NOTE: this WILL break if the three faces do not share a common
	vertex. ie works = ((face1 % 3 != face2 % 3)
	                    && (face2 % 3 != face3 % 3)
						&& (face1 % 3 != face3 % 3))
*/
static void
find_cube_vertex (int face1, int face2, int face3, vec3_t v)
{
	v[face_axis[face1]] = face_offset[face1];
	v[face_axis[face2]] = face_offset[face2];
	v[face_axis[face3]] = face_offset[face3];
}

/*
	set_vertex

	add the vertex to the polygon describing the face of the cube. Offsets
	the vertex relative to r_refdef.viewposition so the cube is always centered
	on the player and also calculates the texture coordinates of the vertex
	(wish I could find a cleaner way of calculating s and t).
*/
static void
set_vertex (struct box_def *box, int face, int ind, const vec3_t v)
{
	VectorAdd (v, r_refdef.viewposition, box->face[face].poly.verts[ind]);
	switch (face) {
		case 0:
			box->face[face].poly.verts[ind][3] = (1024 - v[1] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 - v[2] + 4) / BOX_WIDTH;
			break;
		case 1:
			box->face[face].poly.verts[ind][3] = (1024 + v[0] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 - v[2] + 4) / BOX_WIDTH;
			break;
		case 2:
			box->face[face].poly.verts[ind][3] = (1024 - v[1] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 + v[0] + 4) / BOX_WIDTH;
			break;
		case 3:
			box->face[face].poly.verts[ind][3] = (1024 + v[1] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 - v[2] + 4) / BOX_WIDTH;
			break;
		case 4:
			box->face[face].poly.verts[ind][3] = (1024 - v[0] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 - v[2] + 4) / BOX_WIDTH;
			break;
		case 5:
			box->face[face].poly.verts[ind][3] = (1024 - v[1] + 4) / BOX_WIDTH;
			box->face[face].poly.verts[ind][4] = (1024 - v[0] + 4) / BOX_WIDTH;
			break;
	}
}

/*
 	add_vertex

	append a vertex to the poly vertex list.
*/
static inline void
add_vertex (struct box_def *box, int face, const vec3_t v)
{
	set_vertex (box, face, box->face[face].poly.numverts++, v);
}

/*
	insert_cube_vertices

	insert the given cube vertices into the vertex list of the poly in the
	correct location.
*/
static void
insert_cube_vertices (struct box_def *box, struct visit_def visit, int count,
					  ...)
{
	int         i;
	int         face = visit.face;
	int         ind = visit.leave + 1;
	va_list     args;
	vec3_t    **v;

	va_start (args, count);
	v = (vec3_t **) alloca (count * sizeof (vec3_t *));

	for (i = 0; i < count; i++) {
		v[i] = va_arg (args, vec3_t *);
	}
	va_end (args);

	if (ind == box->face[face].poly.numverts) {
		// the vertex the sky poly left this cube face through is very
		// conveniently the last vertex of the face poly. this means we can
		// just append the vertices
		for (i = 0; i < count; i++)
			add_vertex (box, face, *v[i]);
	} else {
		// we have to insert the cube vertices into the face poly vertex list
		glpoly_t   *p = &box->face[face].poly;
		int         c = p->numverts - ind;
		const int   vert_size = sizeof (p->verts[0]);

		memmove (p->verts[ind + count], p->verts[ind], c * vert_size);
		p->numverts += count;
		for (i = 0; i < count; i++)
			set_vertex (box, face, ind + i, *v[i]);
	}
}

/*
	cross_cube_edge

	add the vertex formed by the poly edge crossing the cube edge to the
	polygon for the two faces on that edge. Actually, the two faces define
	the edge :). The poly edge is going from face 1 to face 2 (for
	enter/leave purposes).
*/
static void
cross_cube_edge (struct box_def *box, int face1, const vec3_t v1, int face2,
				 const vec3_t v2)
{
	int         axis;
	int         face = -1;
	vec3_t      l;

	axis = find_intersect (face1, v1, face2, v2, l);
	if (l[axis] > 1024)
		face = axis;
	else if (l[axis] < -1024)
		face = axis + 3;
	if (face >= 0) {
		vec3_t      x;

		VectorAdd (v1, v2, x);
		VectorScale (x, 0.5, x);
		cross_cube_edge (box, face1, v1, face, x);
		cross_cube_edge (box, face, x, face2, v2);
	} else {
		struct visit_def *visit = box->visited_faces;

		visit[box->face_count - 1].leave = box->face[face1].poly.numverts;
		visit[box->face_count].face = face2;
		box->face_count++;
		box->face_visits[face2]++;

		add_vertex (box, face1, l);
		add_vertex (box, face2, l);
	}
}

/*
	process_corners

	egad, veddy complicated :)
*/
static void
process_corners (struct box_def *box)
{
	int         i;
	int         center = -1, max_visit = 0;
	struct visit_def *visit = box->visited_faces;

	if (visit[box->face_count - 1].face == visit[0].face) {
		box->face_count--;
	}

	for (i = 0; i < 6; i++) {
		if (max_visit < box->face_visits[i]) {
			max_visit = box->face_visits[i];
		}
	}

	switch (box->face_count) {
		case 1: // a
		case 2:	// b
		case 8:	// f
			// no corners
			return;
		case 3: // g
			// one corner, no edges
			{
				vec3_t      v;

				find_cube_vertex (visit[0].face, visit[1].face, visit[2].face,
								  v);
				insert_cube_vertices (box, visit[0], 1, v);
				insert_cube_vertices (box, visit[1], 1, v);
				insert_cube_vertices (box, visit[2], 1, v);
			}
			break;
		case 4:	// c d j n
			if (max_visit > 1) // c d
				return;
			if (abs (visit[2].face - visit[0].face) == 3
				&& abs (visit[3].face - visit[1].face) == 3) {
				// 4 vertices, n
				int         sum, diff;
				vec3_t      v[4];

				sum = visit[0].face + visit[1].face + visit[2].face +
					visit[3].face;
				diff = visit[1].face - visit[0].face;
				sum %= 3;
				diff = (diff + 6) % 6;

				center = faces_table[sum][diff];
				for (i = 0; i < 4; i++) {
					find_cube_vertex (visit[i].face, visit[(i + 1) & 3].face,
									  center, v[i]);
					add_vertex (box, center, v[i]);
				}
				for (i = 0; i < 4; i++)
					insert_cube_vertices (box, visit[i], 2, v[i],
										  v[(i - 1) & 3]);
			} else {
				// 2 vertices, j
				int         l_f, t_f, r_f, b_f;
				vec3_t      v_l, v_r;

				if (abs (visit[2].face - visit[0].face) == 3) {
					l_f = 0;
					t_f = 1;
					r_f = 2;
					b_f = 3;
				} else if (abs (visit[3].face - visit[1].face) == 3) {
					l_f = 1;
					t_f = 2;
					r_f = 3;
					b_f = 0;
				} else {
					return;
				}
				find_cube_vertex (visit[l_f].face, visit[t_f].face,
								  visit[b_f].face, v_l);
				find_cube_vertex (visit[r_f].face, visit[t_f].face,
								  visit[b_f].face, v_r);

				insert_cube_vertices (box, visit[t_f], 2, v_r, v_l);
				insert_cube_vertices (box, visit[b_f], 2, v_l, v_r);

				insert_cube_vertices (box, visit[l_f], 1, v_l);
				insert_cube_vertices (box, visit[r_f], 1, v_r);
			}
			break;
		case 5:	// h m
			if (max_visit > 1) {
				// one vertex, h
				vec3_t      v;

				for (i = 0; i < 4; i++) {
					// don't need to check the 5th visit
					if (visit[(i + 2) % 5].face == visit[(i + 4) % 5].face)
						break;
				}
				find_cube_vertex (visit[i].face, visit[(i + 1) % 5].face,
								  visit[(i + 2) % 5].face, v);
				insert_cube_vertices (box, visit[i], 1, v);
				insert_cube_vertices (box, visit[(i + 1) % 5], 1, v);
				insert_cube_vertices (box, visit[(i + 4) % 5], 1, v);
			} else {
				// 3 vertices, m
				unsigned int sel =
					(((abs (visit[2].face - visit[0].face) == 3) << 2) |
					 ((abs (visit[3].face - visit[1].face) == 3) << 1) |
					 ((abs (visit[4].face - visit[2].face) == 3) << 0));
				vec3_t      v[3];

				center = faces_bit_magic[sel];
				// Sys_Printf ("%02o %d  %d %d %d %d %d\n", sel, center,
				// visit[0].face,
				// visit[1].face, visit[2].face, visit[3].face,
				// visit[4].face);
				for (i = 0; i < 3; i++)
					find_cube_vertex (visit[center].face,
									  visit[(center + 1 + i) % 5].face,
									  visit[(center + 2 + i) % 5].face, v[i]);
				insert_cube_vertices (box, visit[center], 3, v[0], v[1], v[2]);
				insert_cube_vertices (box, visit[(center + 1) % 5], 1, v[0]);
				insert_cube_vertices (box, visit[(center + 2) % 5], 2, v[1],
									  v[0]);
				insert_cube_vertices (box, visit[(center + 3) % 5], 2, v[2],
									  v[1]);
				insert_cube_vertices (box, visit[(center + 4) % 5], 1, v[2]);
			}
			break;
		case 6:	// e k l o
			if (max_visit > 2) // e
				return;
			for (i = 0; i < 5; i++) {
				// don't need to check the last point
				if (visit[(i + 3) % 6].face == visit[(i + 5) % 6].face
					|| visit[(i + 2) % 6].face == visit[(i + 5) % 6].face)
					break;
			}
			if (visit[(i + 3) % 6].face == visit[(i + 5) % 6].face) {
				// adjacant vertices, l o
				vec3_t      v[2];

				if (visit[(i + 0) % 6].face == visit[(i + 2) % 6].face) // o
					return;
				// l
				find_cube_vertex (visit[i].face,
								  visit[(i + 1) % 6].face,
								  visit[(i + 5) % 6].face, v[0]);
				find_cube_vertex (visit[(i + 1) % 6].face,
								  visit[(i + 2) % 6].face,
								  visit[(i + 3) % 6].face, v[1]);

				insert_cube_vertices (box, visit[(i + 5) % 6], 2, v[0], v[1]);

				insert_cube_vertices (box, visit[i], 1, v[0]);
				insert_cube_vertices (box, visit[(i + 1) % 6], 2, v[1], v[0]);
				insert_cube_vertices (box, visit[(i + 2) % 6], 1, v[1]);
			} else {
				// opposing vertices, k
				vec3_t      v[2];

				find_cube_vertex (visit[i].face, visit[(i + 1) % 6].face,
								  visit[(i + 2) % 6].face, v[0]);
				find_cube_vertex (visit[(i + 3) % 6].face,
								  visit[(i + 4) % 6].face,
								  visit[(i + 5) % 6].face, v[1]);

				insert_cube_vertices (box, visit[i], 1, v[0]);
				insert_cube_vertices (box, visit[(i + 1) % 6], 1, v[0]);

				insert_cube_vertices (box, visit[(i + 3) % 6], 1, v[1]);
				insert_cube_vertices (box, visit[(i + 4) % 6], 1, v[1]);

				insert_cube_vertices (box, visit[(i + 2) % 6], 1, v[1]);
				insert_cube_vertices (box, visit[(i + 5) % 6], 1, v[0]);
			}
			break;
		case 7:	// i
			for (i = 0; i < 6; i++) {
				// don't need to check the last point
				if (visit[(i + 2) % 7].face == visit[(i + 4) % 7].face
					&& visit[(i + 4) % 7].face == visit[(i + 6) % 7].face)
					break;
			}
			{
				vec3_t      v;

				find_cube_vertex (visit[i].face, visit[(i + 1) % 7].face,
								  visit[(i + 2) % 7].face, v);

				insert_cube_vertices (box, visit[i], 1, v);
				insert_cube_vertices (box, visit[(i + 1) % 7], 1, v);
				insert_cube_vertices (box, visit[(i + 6) % 7], 1, v);
			}
			break;
	}
}

/*
	render_box

	draws all faces of the cube with 3 or more vertices.
*/
static void
render_box (const struct box_def *box)
{
	int         i, j;

	for (i = 0; i < 6; i++) {
		if (box->face[i].poly.numverts <= 2)
			continue;
		qfglBindTexture (GL_TEXTURE_2D, box->face[i].tex);
		qfglBegin (GL_POLYGON);
		for (j = 0; j < box->face[i].poly.numverts; j++) {
			qfglTexCoord2fv (box->face[i].poly.verts[j] + 3);
			qfglVertex3fv (box->face[i].poly.verts[j]);
		}
		qfglEnd ();
	}
}

static void
R_DrawSkyBoxPoly (const glpoly_t *poly)
{
	int         i;
	struct box_def box;

	/* projected vertex and face of the previous sky poly vertex */
	vec3_t      last_v;
	int         prev_face;

	/* projected vertex and face of the current sky poly vertex */
	vec3_t      v;
	int         face;

	memset (&box, 0, sizeof (box));
	for (i = 0; i < 6; i++) {
		box.face[i].tex = SKY_TEX + skytex_offs[i];
	}

	if (poly->numverts >= 32) {
		Sys_Error ("too many verts!");
	}

	VectorSubtract (poly->verts[poly->numverts - 1], r_refdef.viewposition, last_v);
	prev_face = determine_face (last_v);

	box.visited_faces[0].face = prev_face;
	box.face_count = 1;

	for (i = 0; i < poly->numverts; i++) {
		VectorSubtract (poly->verts[i], r_refdef.viewposition, v);
		face = determine_face (v);
		if (face != prev_face) {
			if ((face_axis[face]) == (face_axis[prev_face])) {
				int         x_face;
				vec3_t      x;

				VectorAdd (v, last_v, x);
				VectorScale (x, 0.5, x);
				x_face = determine_face (x);

				cross_cube_edge (&box, prev_face, last_v, x_face, x);
				cross_cube_edge (&box, x_face, x, face, v);
			} else {
				cross_cube_edge (&box, prev_face, last_v, face, v);
			}
		}
		add_vertex (&box, face, v);

		VectorCopy (v, last_v);
		prev_face = face;
	}

	process_corners (&box);

	render_box (&box);
}

static void
EmitSkyPolys (float speedscale, const instsurf_t *sc)
{
	float       length, s, t;
	float      *v;
	int         i;
	glpoly_t   *p;
	vec3_t      dir;
	msurface_t *surf = sc->surface;

	//FIXME transform/color
	for (p = surf->polys; p; p = p->next) {
		qfglBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3.0;	// flatten the sphere

			length = DotProduct (dir, dir);
			length = 2.953125 / sqrt (length);

			dir[0] *= length;
			dir[1] *= length;

			s = speedscale + dir[0];
			t = speedscale + dir[1];

			qfglTexCoord2f (s, t);
			qfglVertex3fv (v);
		}
		qfglEnd ();
	}
}

static inline void
draw_poly (const glpoly_t *poly)
{
	int         i;

	qfglBegin (GL_POLYGON);
	for (i = 0; i < poly->numverts; i++) {
		qfglVertex3fv (poly->verts[i]);
	}
	qfglEnd ();
}

static void
draw_black_sky_polys (const instsurf_t *sky_chain)
{
	const instsurf_t *sc = sky_chain;

	qfglDisable (GL_BLEND);
	qfglDisable (GL_TEXTURE_2D);
	qfglColor3ubv (color_black);
	while (sc) {
		glpoly_t   *p = sc->surface->polys;

		if (sc->transform) {
			qfglPushMatrix ();
			qfglLoadMatrixf (sc->transform);
		}
		while (p) {
			draw_poly (p);
			p = p->next;
		}
		if (sc->transform)
			qfglPopMatrix ();
		sc = sc->tex_chain;
	}
	qfglEnable (GL_TEXTURE_2D);
	qfglEnable (GL_BLEND);
	qfglColor3ubv (color_white);
}

static void
draw_skybox_sky_polys (const instsurf_t *sky_chain)
{
	const instsurf_t *sc = sky_chain;

	qfglDepthMask (GL_FALSE);
	qfglDisable (GL_DEPTH_TEST);
	while (sc) {
		glpoly_t   *p = sc->surface->polys;

		//FIXME transform/color
		while (p) {
			R_DrawSkyBoxPoly (p);
			p = p->next;
		}
		sc = sc->tex_chain;
	}
	qfglEnable (GL_DEPTH_TEST);
	qfglDepthMask (GL_TRUE);
}

static void
draw_skydome_sky_polys (const instsurf_t *sky_chain)
{
	// this function is not yet implemented so just draw black
	draw_black_sky_polys (sky_chain);
}

static void
draw_id_sky_polys (const instsurf_t *sky_chain)
{
	const instsurf_t *sc = sky_chain;
	float       speedscale;

	speedscale = vr_data.realtime / 16;
	speedscale -= floor (speedscale);

	qfglBindTexture (GL_TEXTURE_2D, gl_solidskytexture);
	while (sc) {
		EmitSkyPolys (speedscale, sc);
		sc = sc->tex_chain;
	}

	if (gl_sky_multipass->int_val) {
		sc = sky_chain;

		speedscale = vr_data.realtime / 8;
		speedscale -= floor (speedscale);

		qfglBindTexture (GL_TEXTURE_2D, gl_alphaskytexture);
		while (sc) {
			EmitSkyPolys (speedscale, sc);
			sc = sc->tex_chain;
		}
	}
}

static void
draw_z_sky_polys (const instsurf_t *sky_chain)
{
	const instsurf_t *sc = sky_chain;

	qfglColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	qfglDisable (GL_BLEND);
	qfglDisable (GL_TEXTURE_2D);
	qfglColor3ubv (color_black);
	while (sc) {
		glpoly_t   *p = sc->surface->polys;

		if (sc->transform) {
			qfglPushMatrix ();
			qfglLoadMatrixf (sc->transform);
		}
		while (p) {
			draw_poly (p);
			p = p->next;
		}
		if (sc->transform)
			qfglPopMatrix ();
		sc = sc->tex_chain;
	}
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
	qfglEnable (GL_BLEND);
	qfglColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void
gl_R_DrawSkyChain (const instsurf_t *sky_chain)
{
	if (gl_sky_clip->int_val > 2) {
		draw_black_sky_polys (sky_chain);
		return;
	}

	if (gl_skyloaded) {
		if (gl_sky_clip->int_val) {
			draw_skybox_sky_polys (sky_chain);
		}
		draw_z_sky_polys (sky_chain);
	} else if (gl_sky_clip->int_val == 2) {
		draw_id_sky_polys (sky_chain);
	} else if (gl_sky_clip->int_val) {
		// XXX not properly implemented
		draw_skydome_sky_polys (sky_chain);
		//draw_z_sky_polys (sky_chain);
	} else {
		draw_z_sky_polys (sky_chain);
	}

	if (gl_sky_debug->int_val) {
		const instsurf_t *sc;

		qfglDisable (GL_TEXTURE_2D);
		if (gl_sky_debug->int_val & 1) {
			sc = sky_chain;
			qfglColor3ub (255, 255, 255);
			while (sc) {
				glpoly_t   *p = sc->surface->polys;

				if (sc->transform) {
					qfglPushMatrix ();
					qfglLoadMatrixf (sc->transform);
				}
				while (p) {
					int         i;

					qfglBegin (GL_LINE_LOOP);
					for (i = 0; i < p->numverts; i++) {
						qfglVertex3fv (p->verts[i]);
					}
					qfglEnd ();
					p = p->next;
				}
				if (sc->transform)
					qfglPopMatrix ();
				sc = sc->tex_chain;
			}
		}
		if (gl_sky_debug->int_val & 2) {
			sc = sky_chain;
			qfglColor3ub (0, 255, 0);
			qfglBegin (GL_POINTS);
			while (sc) {
				glpoly_t   *p = sc->surface->polys;

				if (sc->transform) {
					qfglPushMatrix ();
					qfglLoadMatrixf (sc->transform);
				}
				while (p) {
					int         i;
					vec3_t      x, c = { 0, 0, 0 };

					for (i = 0; i < p->numverts; i++) {
						VectorSubtract (p->verts[i], r_refdef.viewposition, x);
						VectorAdd (x, c, c);
					}
					VectorScale (c, 1.0 / p->numverts, c);
					VectorAdd (c, r_refdef.viewposition, c);
					qfglVertex3fv (c);
					p = p->next;
				}
				if (sc->transform)
					qfglPopMatrix ();
				sc = sc->tex_chain;
			}
			qfglEnd ();
		}
		if (gl_sky_debug->int_val & 4) {
			if (gl_skyloaded) {
				int         i, j;

				qfglColor3ub (255, 0, 0);
				for (i = 0; i < 6; i++) {
					vec3_t      v;

					qfglBegin (GL_LINE_LOOP);
					for (j = 0; j < 4; j++) {
						VectorScale (&gl_skyvec[i][j][2], 1.0 / 128.0, v);
						VectorAdd (v, r_refdef.viewposition, v);
						qfglVertex3fv (v);
					}
					qfglEnd ();
				}
			}
		}
		qfglColor3ubv (color_white);
		qfglEnable (GL_TEXTURE_2D);
	}
}
