#include <string.h>
#include <math.h>
#include <imui.h>
#include <input.h>
#include <plist.h>
#include <cvar.h>
#include <draw.h>
#include <scene.h>
#include <qfs.h>

#include "pga3d.h"

void printf (string fmt, ...);

motor_t
normalize (motor_t m)
{
	auto mag2 = 1 / (m * ~m).scalar;
	auto mag = sqrt(mag2);
	auto d = (m.scalar * m.qvec - m.bvect ∧ m.bvecp) * mag2;
	m *= mag;
	m.bvecp += ⋆m.bvect * ⋆d;
	m.qvec -= d * m.scalar;
	return m;
}

motor_t
sqrt (motor_t m)
{
	return normalize (1 + m);
}

motor_t
exp (bivector_t b)
{
	auto l = (b * ~b).scalar;
	if (l == 0) {
		return { .scalar = 1, .bvecp = b.bvecp };
	}
	auto m = b.bvect ∧ b.bvecp;
	auto a = sqrt (l);
	auto sc = sincos (a);
	sc[0] /= a;
	auto t = (sc[1] - sc[0]) * (⋆m / l);
	return {
		.scalar = sc[1],
		.bvect = sc[0] * b.bvect,
		.bvecp = sc[0] * b.bvecp + t * ⋆b.bvect,
		.qvec = m * sc[0],
	};
}

rotor_t
exp (PGA.bvect b)
{
	auto l = b * ~b;
	if (l == 0) {
		return { .scalar = 1 };
	}
	auto a = sqrt (l);
	auto sc = sincos (a);
	sc[0] /= a;
	return {
		.scalar = sc[1],
		.bvect = sc[0] * b.bvect,
	};
}

translator_t
exp (PGA.bvecp b)
{
	return { .scalar = 1, .bvecp = b.bvecp };
}

bivector_t
log (motor_t m)
{
	float a = (m * ~m).scalar;
	if (!a) {
		return { .bvecp = m.bvecp };
	}
	auto s = sqrt (-m.bvect • m.bvect);
	auto b = s ? atan2 (s, m.scalar) / s : 1;
	auto c = ⋆m.qvec * (1 - m.scalar * b) / a;
	return {
		.bvect = b * m.bvect,
		.bvecp = b * m.bvecp + c * ⋆m.bvect,
	};
}

motor_t
make_motor (vec4 translation, vec4 rotation)
{
	@algebra(PGA) {
		auto dt = (PGA.group_mask (0x18)) translation;
		auto q = (PGA.group_mask(0x06)) rotation;
		motor_t t = { .scalar = 1, .bvecp = -dt.bvecp / 2 };
		motor_t r = { .scalar = q.scalar, .bvect = -q.bvect };
		motor_t m = t * r;
		return m;
	}
}

void
set_transform (motor_t m, transform_t transform)
{
	@algebra (PGA) {
		vec4 scale = Transform_GetLocalScale (transform);
		// Extract the rotation quaternion. Removes the non-Euclidean
		// components by zeroing them, and reverses the Euclidean parts.
		auto r = ⋆(m * e0123);
		// Extract the translation. Right multiplication by the quaterion
		// is the inverse of left multiplication.
		auto t = m * ⋆(m * e0123);
		vec4 rot = (vec4) r;
		// Motor translations have double-cover too.
		vec4 trans = [(vec3)t.bvecp * -2, 1];
		Transform_SetLocalTransform (transform, scale, rot, trans);
	}
}

void
printvec (string name, vec4 v)
{
	//printf ("%s: [%g %g %g %g]\n", name, v[0], v[1], v[2], v[3]);
	printf ("%s: %q\n", name, v);
}

void
printmat (string name, mat4x4 m)
{
	int len = strlen (name) + 2;
	printf ("%s: [%g %g %g %g]\n", name,   m[0][0], m[1][0], m[2][0], m[3][0]);
	printf ("%*s[%g %g %g %g]\n", len, "", m[0][1], m[1][1], m[2][1], m[3][1]);
	printf ("%*s[%g %g %g %g]\n", len, "", m[0][2], m[1][2], m[2][2], m[3][2]);
	printf ("%*s[%g %g %g %g]\n", len, "", m[0][3], m[1][3], m[2][3], m[3][3]);
}

void
dump_transform (transform_t transform)
{
	printf ("child count: %d\n", Transform_ChildCount (transform));
	printf ("parent: %ld\n", Transform_GetParent (transform));
	printf ("tag: %d\n", Transform_GetTag (transform));
	printmat ("local", Transform_GetLocalMatrix (transform));
	printmat ("world", Transform_GetWorldMatrix (transform));
	printmat ("l inverse", Transform_GetLocalInverse (transform));
	printmat ("w inverse", Transform_GetWorldInverse (transform));
	printvec ("l position", Transform_GetLocalPosition (transform));
	printvec ("w position", Transform_GetWorldPosition (transform));
	printvec ("l rotation", Transform_GetLocalRotation (transform));
	printvec ("w rotation", Transform_GetWorldRotation (transform));
	printvec ("l scale", Transform_GetLocalScale (transform));
	printvec ("w scale", Transform_GetWorldScale (transform));
	printvec ("forward", Transform_Forward (transform));
	printvec ("right", Transform_Right (transform));
	printvec ("up", Transform_Up (transform));
}

state_t
dState (state_t s)
{
	return {
		.M = -0.5f * s.M * s.B,
		.B = -@undual(s.B × ⋆s.B),
	};
}

state_t
dState (state_t s, bivector_t f)
{
	return {
		.M = -0.5f * s.M * s.B,
		.B = @undual(f - s.B × ⋆s.B),
	};
}

state_t
dState (state_t s, body_t *body)
{
	return {
		.M = -0.5f * s.M * s.B,
		.B = -@undual(body.Ii @hadamard (s.B × (body.I @hadamard ⋆s.B))),
	};
}

void
draw_3dline (transform_t camera, vec4 p1, vec4 p2, int color)
{
	auto camp = Transform_GetWorldPosition (camera);
	auto camf = Transform_Forward (camera);
	auto camu = Transform_Up (camera);
	auto camr = Transform_Right (camera);

	p1 -= camp;
	p2 -= camp;
	float near = 0.05;
	vec4 cp1 = { p1 • camr, p1 • camu, near, p1 • camf };
	vec4 cp2 = { p2 • camr, p2 • camu, near, p2 • camf };
	if (cp1[3] < near && cp2[3] < near) {
		return;
	}
	if (cp1[3] < near) {
		cp1 += (cp2 - cp1) * (near - cp1[3]) / (cp2[3] - cp1[3]);
	} else if (cp2[3] < near) {
		cp2 += (cp1 - cp2) * (near - cp2[3]) / (cp1[3] - cp2[3]);
	}
	cp1 /= cp1[3];
	cp2 /= cp2[3];

	int         width = Draw_Width ();
	int         height = Draw_Height ();
	vec2        center = { width / 2.0f, height / 2.0f };
	float       size = center[0];

	vec2 sp1 = { cp1[0], -cp1[1] };
	vec2 sp2 = { cp2[0], -cp2[1] };
	sp1 = sp1 * size + center;
	sp2 = sp2 * size + center;
	Draw_Line (sp1[0], sp1[1], sp2[0], sp2[1], color);
}
#if 1
typedef struct edge_s {
	int a;
	int b;
} edge_t;
point_t points[10];
point_t cur_points[10];
edge_t edges[14];
state_t cube_state;

void
create_cube ()
{
	cube_state.M = make_motor ({ 0, 0, 0, 0, }, { 0, 0, 0, 1 });
	cube_state.B = {};

	@algebra(PGA) {
		for (int i = 0; i < 8; i++) {
			points[i] = e123 + (2 * ((i >> 0) & 1) - 1) * e032
							 + (2 * ((i >> 1) & 1) - 1) * e013
							 + (2 * ((i >> 2) & 1) - 1) * e021;
		}
		// anchor
		points[8] = e123 + 0*e013;
	}
	points[9] = points[0];
	int c = 0;
	for (int j = 0; j < 8; j++) {
		for (int i = j + 1; i < 8; i++) {
			if (!((i ^ j) & ((i ^ j) - 1))) {
				edges[c++] = {i, j};
			}
		}
	}
	edges[c++] = {0, 8};
	edges[c++] = {1, 9};
	for (int i = 0; i < 8; i++) {
		printf ("%d %q\n", i, points[i]);
	}
}

void
draw_cube ()
{
	@algebra(PGA) {
		static plane_t s = e1 + 5*e0;
		//NOTE: moving the camera point has the opposite effect to moving
		//the camera transform: moving the point up drags the scene up
		//as well. This is because the intersected points remain relative
		//to the plane's "origin" (ie, projection of the origin on the plane)
		//instead of the camera (projection of camera point on the plane)
		static point_t c = e123 - 6*e032 + 0*e021 + 0*e013;

		point_t a[10];
		for (int i = 0; i < 10; i++) {
			auto p = cur_points[i];
			a[i] = (p ∨ c) ∧ s;
		}

		//printf ("p:%q a:%q p:%q a:%q c:%q s:%q\n",
		//		cur_points[0], cur_points[8], a[0], a[8], c, s);
		int         width = Draw_Width ();
		int         height = Draw_Height ();
		vec2        center = { width / 2.0f, height / 2.0f };
		float       size = center[0] < center[1] ? center[0] : center[1];
		for (int i = 0; i < 14; i++) {
			auto P1 = a[edges[i].a] * size / a[edges[i].a][3];
			auto P2 = a[edges[i].b] * size / a[edges[i].b][3];
			vec2 p1 = {-P1[1], -P1[2]};
			vec2 p2 = {-P2[1], -P2[2]};
			p1 += center;
			p2 += center;
			Draw_Line (p1[0], p1[1], p2[0], p2[1], 15);
		}
	}
}

bivector_t
force (state_t s, point_t p0, point_t p1, point_t anchor, point_t anchor2)
{
	@algebra(PGA) {
		auto grav = ⋆((~s.M * (-9.81f * e03) * s.M));
		//auto hook = 12f * ((~s.M * anchor * s.M) ∨ p0);
		// P ∨ Q gives the line from P to Q (so Q - P),
		// and the anchor pulls p0 towards the anchor thus
		// want the line from p0 to anchor to be positive force
		auto hook = 12f * (p0 ∨ (~s.M * anchor * s.M));
		hook += 6f * (p1 ∨ (~s.M * anchor2 * s.M));
		auto damp = ⋆(-.25f * s.B);
		auto f = grav + 0.5f*hook + damp;
		return f.bvect + f.bvecp;
	}
}

void
update_cube (float dt)
{
	auto p0 = points[0];
	auto p1 = points[1];
	auto anchor = points[8];
	auto anchor2 = points[9];
	float h = dt / 100;
	for (int i = 0; i < 100; i++) {
		auto ds = dState (cube_state, force (cube_state, p0, p1, anchor, anchor2));
		cube_state.M += h * ds.M;
		cube_state.B += h * ds.B;
		cube_state.M = normalize (cube_state.M);
	}
	for (int i = 0; i < 8; i++) {
		auto p = points[i];
		p = cube_state.M * p * ~cube_state.M;
		cur_points[i] = p;
	}
	cur_points[8] = anchor;
	cur_points[9] = anchor2;
}
#endif
