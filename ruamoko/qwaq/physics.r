#include <math.h>
#include "gizmo.h"
#include "pga3d.h"
#include "physics.h"
#include "qwaq-ed.h"

extern float frametime;

void set_update (uint ent, void (*update) (uint ent)) = #0;
bool has_component (uint ent, uint comp) = #0;
void get_component (uint ent, uint comp, void *data) = #0;
void set_component (uint ent, uint comp, void *data) = #0;
uint new_entity () = #0;
void del_entity (uint ent) = #0;

float
clamp (float x, float a, float b)
{
	return max (a, min(x, b));
}

@generic (v3 = [PGA.bvect, vec3], v4 = [PGA.vec, PGA.tvec, vec4]) {

v3 abs(v3 x)
{
	uvec3 m = (vec3) x < '0 0 0';
	return (v3) ((uvec3) x & ~m) - (v3) ((uvec3) x & m);
}

v4 abs(v4 x)
{
	uvec4 m = (vec4) x < '0 0 0 0';
	return (v4) ((uvec4) x & ~m) - (v4) ((uvec4) x & m);
}

v3 max(v3 a, v3 b)
{
	uvec3 m = a < b;
	return (v3) ((uvec3) a & ~m) + (v3) ((uvec3) b & m);
}

v4 max(v4 a, v4 b)
{
	uvec4 m = a < b;
	return (v4) ((uvec4) a & ~m) + (v4) ((uvec4) b & m);
}

};

//FIXME having PGA.group_mask(0xc) here and then providing a defintion causes
//a segfault in qfcc
@generic (genObj = [PGA.group_mask(0xe)]) {

genObj
sqrt (genObj x)
{
	auto a = x + 1;
	return a / sqrt (a • ~a);
}

};

//@overload
//PGA.group_mask(0xc)
//sqrt (PGA.group_mask(0xc) x)
//{
//	return (x + x.scalar) / 2;
//}

static void
draw_principle_axes (motor_t M, bivector_t I)
{
	@algebra(PGA) {
		auto o = M * e123 * ~M;
		auto p1 = M * (e123 + e032 * I.bvecp[0]) * ~M;
		auto p2 = M * (e123 + e013 * I.bvecp[1]) * ~M;
		auto p3 = M * (e123 + e021 * I.bvecp[2]) * ~M;
		Gizmo_AddCapsule ((vec4) o, (vec4) p1, 0.25, {1, 0, 0, 0.5});
		Gizmo_AddCapsule ((vec4) o, (vec4) p2, 0.25, {0, 1, 0, 0.5});
		Gizmo_AddCapsule ((vec4) o, (vec4) p3, 0.25, {0, 0, 1, 0.5});
	}
}

state_t
update_block_state(state_t state, body_t body, transform_t xform)
{
	float h = frametime / 100;
	for (int i = 0; i < 100; i++) {
		auto ds = dState (state, &body);
		state.M += h * ds.M;
		state.B += h * ds.B;
		state.M = normalize (state.M);
	}
	return state;
}

state_t
update_grav_state(state_t state, body_t body, transform_t xform)
{
	@algebra(PGA) {
		float h = frametime / 100;
		for (int i = 0; i < 100; i++) {
			bivector_t grav = ⋆((~state.M * (-0.0981f * e03) * state.M));
			grav = grav @hadamard body.I;

			auto ds = dState (state, grav, &body);
			state.M += h * ds.M;
			state.B += h * ds.B;
			state.M = normalize (state.M);
		}
	}
	return state;
}

void
draw_axes (transform_t xform)
{
	//draw_principle_axes (state.M, body.I);
	auto mat = Transform_GetWorldMatrix (xform);
	vec4 x = mat[0];
	vec4 y = mat[1];
	vec4 z = mat[2];
	vec4 p = mat[3];
	Gizmo_AddCapsule (p, p + x, 0.025, vec4(1, 0, 0, 0.2));
	Gizmo_AddCapsule (p, p + y, 0.025, vec4(0, 1, 0, 0.2));
	Gizmo_AddCapsule (p, p + z, 0.025, vec4(0, 0, 1, 0.2));
}

void
draw_collider (collider_t col, transform_t xform, motor_t M)
{
	mat4 mat = Transform_GetWorldMatrix(xform);
	switch (col.type) {
	case col_plane:
		break;
		@algebra (PGA) {
			auto plane = col.plane;
			// Gizmo_AddPlane expects a point and two spanning vectors
			// so it knows where the plane's origin is (for the grid)
			auto P = (plane • e123) * plane;
			auto p = mat * (vec4)(P / ⋆(e0 * P));
			quaternion q;
			if (plane[3] < 0) {
				auto r = sqrt (plane * (plane_t)'0 0 -1 0');
				q = [r.scalar, r.bvect];
			} else {
				auto r = sqrt (plane * (plane_t)'0 0 1 0');
				q = [r.scalar, r.bvect];
			}
			auto s = vec4(q * mat[0].xyz, 0);
			auto t = vec4(q * mat[1].xyz, 0);
			auto c1= vec4(0.8, 1, 0.8, 0.5);
			auto c2= vec4(0.8, 0, 0, 0.5);
			auto c3= vec4(0, 0.8, 0, 0.5);
			Gizmo_AddPlane (p, s, t, c1, c2, c3);
		}
		break;
	case col_ball:
		Gizmo_AddSphere (mat * vec4(col.ball.offset, 1), col.ball.radius,
						 vec4(0.8, 0.4, 0.2, 0.9));
		break;
	case col_capsule:
	{
		vec4 p1 = vec4 (col.capsule.offset + col.capsule.axis, 1);
		vec4 p2 = vec4 (col.capsule.offset - col.capsule.axis, 1);
		Gizmo_AddCapsule (mat * p1, mat * p2, col.capsule.radius,
						 vec4(0.2, 0.8, 0.9, 0.9));
		break;
	}
	case col_box:
	{
		break;
		vec3 e = col.box.extent;
		vec3 o = col.box.offset;
		//auto q = M;
		auto q = M.scalar + M.bvect;
		static gizmo_node_t box_brush[] = {
			{ .plane = {1, 0, 0, 1 }, .children = { 1, -1} },
			{ .plane = {1, 0, 0,-1 }, .children = {-1,  2} },
			{ .plane = {0, 1, 0, 1 }, .children = { 3, -1} },
			{ .plane = {0, 1, 0,-1 }, .children = {-1,  4} },
			{ .plane = {0, 0, 1, 1 }, .children = { 5, -1} },
			{ .plane = {0, 0, 1,-1 }, .children = {-1, -2} },
		};
		gizmo_node_t rotated_box[countof(box_brush)];
		for (uint i = 0; i < countof(box_brush); i++) {
			auto p = box_brush[i].plane;
			p.w = p.w * e[i >> 1] + o[i >> 1];
			rotated_box[i] = {
				.plane = (vec4) (q * (plane_t) p * ~q),
				//.plane = (vec4) (M * (plane_t) p * ~M),
				.children = box_brush[i].children,
			};
		}
		vec4 box_mins = '-20 -20 -20 0';
		vec4 box_maxs = '20 20 20 0';
		auto p = vec4 (col.box.offset, 1);
		//Gizmo_AddBrush ('0 0 0 1', box_mins, box_maxs,
		Gizmo_AddBrush (mat * p, box_mins, box_maxs,
						countof (rotated_box), rotated_box,
						vec4(0x55, 0xda, 0xba, 255)/255);
		break;
	}
	}
}

uint max_collider_ents = 0;
uint num_collider_ents = 0;
uint *collider_ents;

typedef struct contact_s {
	point_t world_a, world_b;
	point_t local_a, local_b;
	plane_t normal;
	float separation;
	float time;
	uint a, b;
} contact_t;

typedef bool (*get_contact_t) (uint a, collider_t acol,
							   uint b, collider_t bcol,
							   contact_t *contact);

bool get_contact_plane_ball (uint a, collider_t acol,
							 uint b, collider_t bcol,
							 contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (a, qent_state, &aS);
	get_component (b, qent_state, &bS);
	get_component (a, qent_body, &aB);
	get_component (b, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	auto p = aM * acol.plane * ~aM;
	//FIXME bug in qfcc
	//auto P = bM * (point_t) vec4(bcol.ball.offset, 1) * ~bM;
	auto P = (point_t) vec4(bcol.ball.offset, 1);
	P = bM * P * ~bM;
	float r = bcol.ball.radius;
	float d = p ∨ P;
	if (d <= r) {
		@algebra (PGA) {
			auto n = p * e0123;
			auto world_a = (P • p) * p;
			auto world_b = P - r * n;
			*contact = {
				.world_a = world_a,
				.world_b = world_b,
				.local_a = ~aM * world_a * aM,
				.local_b = ~bM * world_b * bM,
				.normal = @undual (n),
				.separation = d - r,
				.a = a,
				.b = b,
			};
			return true;
		}
	}
	return false;
}

bool get_contact_plane_capsule (uint a, collider_t acol,
								uint b, collider_t bcol,
								contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (a, qent_state, &aS);
	get_component (b, qent_state, &bS);
	get_component (a, qent_body, &aB);
	get_component (b, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	auto p = aM * acol.plane * ~aM;
	auto P = bM * (point_t) [bcol.capsule.offset, 1] * ~bM;
	auto A = bM * (point_t) [bcol.capsule.axis, 0] * ~bM;
	float r = bcol.capsule.radius;
	float end = A ∨ p;
	if (end < 0) {
		P = P - A;
	} else if (end > 0) {
		P = P + A;
	}
	// If end == 0 (unlikely, but...) then P is the center
	float d = p ∨ P;
	if (d <= r) {
		@algebra (PGA) {
			auto n = p * e0123;
			auto world_a = (P • p) * p;
			auto world_b = P - r * n;
			*contact = {
				.world_a = world_a,
				.world_b = world_b,
				.local_a = ~aM * world_a * aM,
				.local_b = ~bM * world_b * bM,
				.normal = @undual (n),
				.separation = d - r,
				.a = a,
				.b = b,
			};
			return true;
		}
	}
	return false;
}

bool get_contact_ball_ball (uint a, collider_t acol,
							uint b, collider_t bcol,
							contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (a, qent_state, &aS);
	get_component (b, qent_state, &bS);
	get_component (a, qent_body, &aB);
	get_component (b, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	auto p = aM * acol.plane * ~aM;
	//FIXME bug in qfcc
	//auto P = bM * (point_t) vec4(acol.ball.offset, 1) * ~bM;
	auto P = (point_t) vec4(acol.ball.offset, 1);
	P = aM * P * ~aM;
	auto Q = (point_t) vec4(bcol.ball.offset, 1);
	Q = bM * Q * ~bM;
	float r = acol.ball.radius + bcol.ball.radius;
	auto d = P ∨ Q;
	if (d • ~d <= r * r) {
		@algebra (PGA) {
			auto n = -(e0 * d) / sqrt (d • ~d);
			auto world_a = P + n * acol.ball.radius;
			auto world_b = Q - n * bcol.ball.radius;
			*contact = {
				.world_a = world_a,
				.world_b = world_b,
				.local_a = ~aM * world_a * aM,
				.local_b = ~bM * world_b * bM,
				.normal = @undual (n),
				.separation = sqrt (d • ~d) - r,
				.a = a,
				.b = b,
			};
			return true;
		}
	}
	return false;
}

bool get_contact_ball_capsule (uint aent, collider_t acol,
							   uint bent, collider_t bcol,
							   contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	auto p = aM * (point_t) [acol.ball.offset,    1] * ~aM;
	auto X = bM * (point_t) [bcol.capsule.offset, 1] * ~bM;
	auto A = bM * (point_t) [bcol.capsule.axis,   0] * ~bM;

	auto a = X - A;
	auto b = X + A;

	auto ap = (a∨p);
	auto ab = (a∨b);
	float h = (ap•ab)/(ab•ab);
	h = h < 0 ? 0 : h > 1 ? 1 : h;
	auto d = ap - h * ab;
	float r = acol.ball.radius + bcol.capsule.radius;

	if (d • ~d < r * r) {
		@algebra (PGA) {
			auto n = (e0 * d) / sqrt (d • ~d);
			auto x = a + h * (b - a);
			auto world_a = p + n * acol.ball.radius;
			auto world_b = x - n * bcol.capsule.radius;
			*contact = {
				.world_a = world_a,
				.world_b = world_b,
				.local_a = ~aM * world_a * aM,
				.local_b = ~bM * world_b * bM,
				.normal = @undual (n),
				.separation = sqrt (d • ~d) - r,
				.a = aent,
				.b = bent,
			};
			return true;
		}
	}

	return false;
}

static point_t
box_plane_point (plane_t p, point_t c, point_t e)
{
	@algebra (PGA) {
		// a plane is just a sphere with infinite radius and its center at
		// infinity, however, the center is behind the plane rather than in
		// front of it
		auto q = (c ∨ (-p * e0123)).bvect;
		auto mins = -(e123 ∨ e).bvect;
		auto maxs =  (e123 ∨ e).bvect;
		auto tmin = (uvec3) ((vec3) q < '0 0 0');
		auto tmax = (uvec3) ('0 0 0' < (vec3) q);
		auto tcen = ~tmin & ~tmax;
		auto x = (PGA.bvect) ( (tmin & (uvec3) mins)
							 + (tcen & (uvec3) q)
							 + (tmax & (uvec3) maxs));
		return c - e0 * x;	// e0 * x gives the negative point
	}
}

bool get_contact_plane_box (uint aent, collider_t acol,
							 uint bent, collider_t bcol,
							 contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	// transform the plane into the box's space
	auto M = ~bM * aM;
	auto p = M * acol.plane * ~M;

	auto c = (point_t) [bcol.box.offset, 1];
	auto e = (point_t) [bcol.box.extent, 0];

	@algebra (PGA) {
		auto P = box_plane_point (p, c, e);
		auto d = p ∨ P;
		if (d < 0) {
			auto n = p * e0123;
			auto box_a = (P • p) * p;
			auto box_b = P;
			*contact = {
				.world_a = bM * box_a * ~bM,
				.world_b = bM * box_b * ~bM,
				.local_a = ~M * box_a * M,
				.local_b = box_b,
				.normal = bM * @undual (n) * ~bM,
				.separation = d,
				.a = aent,
				.b = bent,
			};
			return true;
		}
	}
	return false;

}

bool get_contact_ball_box (uint aent, collider_t acol,
						   uint bent, collider_t bcol,
						   contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	// transform the ball's center point into the box's space
	auto M = ~bM * aM;
	auto p = M * (point_t) [acol.ball.offset, 1] * ~M;

	auto c = (point_t) [bcol.box.offset, 1];
	auto e = (point_t) [bcol.box.extent, 0];
	float r = acol.ball.radius;

	@algebra (PGA) {
		auto q = (c∨p).bvect;
		auto mins = -(e123 ∨ e).bvect;
		auto maxs =  (e123 ∨ e).bvect;
		auto tmin = (uvec3) (q < mins);
		auto tmax = (uvec3) (maxs < q);
		auto tcen = ~tmin & ~tmax;
		auto x = (PGA.bvect) ( (tmin & (uvec3) mins)
							 + (tcen & (uvec3) q)
							 + (tmax & (uvec3) maxs));
		auto d = q - x;
		if (d • ~d < r * r) {
			auto n = (e0 * d) / sqrt (d • ~d);
			auto box_a = p + n * acol.ball.radius;
			auto box_b = c - e0 * x;	// e0 * x gives the negative point
			*contact = {
				.world_a = bM * box_a * ~bM,
				.world_b = bM * box_b * ~bM,
				.local_a = ~M * box_a * M,
				.local_b = box_b,
				.normal = bM * @undual (n) * ~bM,
				.separation = sqrt (d • ~d) - r,
				.a = aent,
				.b = bent,
			};
			return true;
		}
	}
	return false;
}

bool get_contact_capsule_capsule (uint aent, collider_t acol,
								  uint bent, collider_t bcol,
								  contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	auto C1 = aM * (point_t) [acol.capsule.offset, 1] * ~aM;
	auto A1 = aM * (point_t) [acol.capsule.axis,   0] * ~aM;
	auto C2 = bM * (point_t) [bcol.capsule.offset, 1] * ~bM;
	auto A2 = bM * (point_t) [bcol.capsule.axis,   0] * ~bM;

	auto p1 = C1 - A1;
	auto q1 = C1 + A1;
	auto p2 = C2 - A2;
	auto q2 = C2 + A2;

	auto d1 = p1 ∨ q1;
	auto d2 = p2 ∨ q2;
	auto r  = p2 ∨ p1;
	float a = d1 • ~d1;
	float e = d2 • ~d2;
	float f = d2 • ~r;

	float s, t;

	if (a <= 1e-6 && e <= 1e-6) {
		// both segments are degenerate
		s = t = 0;
		printf ("both degenerate: %g %g %g %g\n", a, e, s, t);
	} else if (a <= 1e-6) {
		// first segment is degenerate
		s = 0;
		t = clamp (f / e, 0, 1);
		printf ("first degenerate: %g %g %g\n", a, s, t);
	} else {
		float c = d1 • ~r;
		if (e < 1e-3) {
			// second segment is degenerate
			t = 0;
			s = clamp (-c / a, 0, 1);
			printf ("second degenerate: %g %g %g\n", e, s, t);
		} else {
			float b = d1 • ~d2;
			float den = a * e - b * b;
			if (den) {
				s = clamp ((b * f - c * e) / den, 0, 1);
			} else {
				s = 0;
			}
			t = (b * s + f) / e;
			if (t < 0) {
				t = 0;
				s = clamp (-c / a, 0, 1);
			} else {
				t = 1;
				s = clamp ((b - c) / a, 0, 1);
			}
		}
	}

	@algebra (PGA) {
		auto c1 = p1 - e0 * d1 * s;
		auto c2 = p2 - e0 * d2 * t;
		//printf ("p1:%q q1:%q\n", p1, q1);
		//printf ("p2:%q q2:%q\n", p2, q2);
		//printf ("c1:%q c2:%q\n", c1, c2);
		//printf ("[%v %v] [%v %v] %g %g\n",
		//		d1.bvect, d1.bvecp, d2.bvect, d2.bvecp, s, t);

		auto d = c1 ∨ c2;
		float R = acol.capsule.radius + bcol.capsule.radius;

		if (d • ~d < R * R) {
			auto n = -(e0 * d) / sqrt (d • ~d);
			auto world_a = c1 + n * acol.capsule.radius;
			auto world_b = c2 - n * bcol.capsule.radius;
			*contact = {
				.world_a = world_a,
				.world_b = world_b,
				.local_a = ~aM * world_a * aM,
				.local_b = ~bM * world_b * bM,
				.normal = @undual (n),
				.separation = sqrt (d • ~d) - R,
				.a = aent,
				.b = bent,
			};
			return true;
		}
	}

	return false;
}

static point_t
box_closest_point (point_t p, point_t c, point_t e)
{
	@algebra (PGA) {
		auto q = (c∨p).bvect;
		auto mins = -(e123 ∨ e).bvect;
		auto maxs =  (e123 ∨ e).bvect;
		auto tmin = (uvec3) (q < mins);
		auto tmax = (uvec3) (maxs < q);
		auto tcen = ~tmin & ~tmax;
		auto x = (PGA.bvect) ( (tmin & (uvec3) mins)
							 + (tcen & (uvec3) q)
							 + (tmax & (uvec3) maxs));
		return c - e0 * x;
	}
}

bool get_contact_capsule_box (uint aent, collider_t acol,
							  uint bent, collider_t bcol,
							  contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	// transform the capsule into the box's space
	auto M = ~bM * aM;

	auto C = M * (point_t) [acol.capsule.offset, 1] * ~M;
	auto A = M * (point_t) [acol.capsule.axis,   0] * ~M;
	auto c = (point_t) [bcol.box.offset, 1];
	auto e = (point_t) [bcol.box.extent, 0];

	auto P = C - A;
	auto Q = C + A;

	bivector_t L = P ∨ Q;

	@algebra (PGA) {
		bivector_t line;
		point_t    box_point;
		point_t    cap_point;

		point_t Px = box_closest_point (P, c, e);
		bivector_t LPx = P ∨ Px;
		if (LPx • ~L < 0) {
			box_point = Px;
			cap_point = P;
			line = LPx;

			auto wP = (vec4) (bM * P * ~bM);
			auto wPx = (vec4) (bM * Px * ~bM);
			Gizmo_AddSphere (wP, 0.1, {1, 1, 1, 0.5});
			Gizmo_AddSphere (wPx, 0.1, {1, 1, 1, 0.5});
			Gizmo_AddCapsule (wP, wPx, 0.03, {0.5, 0, 0.5, 0.5});
			goto check_distance;
		}

		point_t Qx = box_closest_point (Q, c, e);
		bivector_t LQx = Q ∨ Qx;
		if (LQx • ~L > 0) {
			box_point = Qx;
			cap_point = Q;
			line = LQx;

			auto wQ = (vec4) (bM * Q * ~bM);
			auto wQx = (vec4) (bM * Qx * ~bM);
			Gizmo_AddSphere (wQ, 0.1, {1, 1, 1, 0.5});
			Gizmo_AddSphere (wQx, 0.1, {1, 1, 1, 0.5});
			Gizmo_AddCapsule (wQ, wQx, 0.03, {0.5, 0.5, 0, 0.5});
			goto check_distance;
		}
		auto wP = (vec4) (bM * P * ~bM);
		auto wQ = (vec4) (bM * Q * ~bM);
		Gizmo_AddSphere (wP, 0.03, {1, 1, 1, 0.5});
		Gizmo_AddSphere (wQ, 0.03, {1, 1, 1, 0.5});

		// Get the direction from the box center to the capsule's axis
		// multiplying by e0 gives negates the result, so do axis to center
		// FIXME .tvec required because the info required for cancelling the
		// bivector terms is lost. This may be fixable with better high-level
		// dags
		auto dir = e0 * (((c • L) * ~L).tvec ∨ c);

		// get the corner of the box most in the direction of the capsule's
		// axis
		auto tmin = (uvec4) ((vec4)dir <= vec4(0));
		auto tmax = ~tmin;
		auto p = (point_t) ( (tmin & (uvec4) -e)
						   + (tmax & (uvec4)  e)) + c;
		// find the other ends of the 3 edges coming off p and create lines
		// for those three edges by first finding the corner furthest from
		// the capsule's axis
		// the actual length doesn't matter, but using 2*e helps when
		// visualizing the edges
		auto v = (point_t) ( (tmin & (uvec4) ( 2*e))
						   + (tmax & (uvec4) (-2*e)));
		// FIXME make it easier to get at components of GA objects
		auto Lx = p ∨ (point_t) ((uvec4) v & '-1 0 0 0'u);
		auto Ly = p ∨ (point_t) ((uvec4) v & '0 -1 0 0'u);
		auto Lz = p ∨ (point_t) ((uvec4) v & '0 0 -1 0'u);
		// Find the lines of shortest distance between the edges and the
		// capsule's axis
		auto LLx = L × Lx;
		auto LLy = L × Ly;
		auto LLz = L × Lz;

		// Find the lines of projection of the corner of the box closest to
		// the axis of the capsule on the capsule's axis
		auto dx = (((p • LLx) * ~LLx).tvec ∨ p) / (LLx • ~LLx);
		auto dy = (((p • LLy) * ~LLy).tvec ∨ p) / (LLy • ~LLy);
		auto dz = (((p • LLz) * ~LLz).tvec ∨ p) / (LLz • ~LLz);

		// Find box side end points of the lines of shortest distance between
		// the capsule's axis and the box edges, but clamped to be on the box
		// edge.
		auto bx = p - e0 * (min (max ((dx • Lx) / (Lx • ~Lx), 0), 1) * Lx);
		auto by = p - e0 * (min (max ((dy • Ly) / (Ly • ~Ly), 0), 1) * Ly);
		auto bz = p - e0 * (min (max ((dz • Lz) / (Lz • ~Lz), 0), 1) * Lz);

		// Find capsule side end points of the lines of shortest distance
		// between the capsule's axis and the box edges.
		point_t cx = P - e0 * (((P ∨ bx) • ~L) / (L • ~L) * L);
		point_t cy = P - e0 * (((P ∨ by) • ~L) / (L • ~L) * L);
		point_t cz = P - e0 * (((P ∨ bz) • ~L) / (L • ~L) * L);

		auto wbx = (vec4) (bM * bx * ~bM);
		auto wby = (vec4) (bM * by * ~bM);
		auto wbz = (vec4) (bM * bz * ~bM);
		auto wcx = (vec4) (bM * cx * ~bM);
		auto wcy = (vec4) (bM * cy * ~bM);
		auto wcz = (vec4) (bM * cz * ~bM);
		Gizmo_AddSphere (wbx, 0.1, {1, 0, 0, 0.5});
		Gizmo_AddSphere (wby, 0.1, {0, 1, 0, 0.5});
		Gizmo_AddSphere (wbz, 0.1, {0, 0, 1, 0.5});
		Gizmo_AddCapsule (wbx, wcx, 0.03, {0, 1, 1, 0.5});
		Gizmo_AddCapsule (wby, wcy, 0.03, {1, 0, 1, 0.5});
		Gizmo_AddCapsule (wbz, wcz, 0.03, {1, 1, 0, 0.5});
		Gizmo_AddSphere (wcx, 0.1, {0, 1, 1, 0.5});
		Gizmo_AddSphere (wcy, 0.1, {1, 0, 1, 0.5});
		Gizmo_AddSphere (wcz, 0.1, {1, 1, 0, 0.5});

		float distx = (cx ∨ bx) • (bx ∨ cx);
		float disty = (cy ∨ by) • (by ∨ cy);
		float distz = (cz ∨ bz) • (bz ∨ cz);
		float dist = min (distx, min (disty, distz));
		if (dist == distx) {
			box_point = bx;
			cap_point = cx;
			line = cx ∨ bx;
			goto check_distance;
		} else if (dist == disty) {
			box_point = by;
			cap_point = cy;
			line = cy ∨ by;
			goto check_distance;
		} else {
			box_point = bz;
			cap_point = cz;
			line = cz ∨ bz;
			goto check_distance;
		}
		// this shouldn't happen as there is always a line of closest approach,
		return false;
check_distance:
		auto wbox = (vec4) (bM * box_point * ~bM);
		auto wcap = (vec4) (bM * cap_point * ~bM);
		Gizmo_AddCapsule (wbox, wcap, 0.1, {0, 1, 0, 0.5});

		float r = acol.capsule.radius;
		if (line • ~line < r * r) {
			auto n = -(e0 * line) / sqrt (line • ~line);
			*contact = {
				.world_a = bM * cap_point * ~bM,
				.world_b = bM * box_point * ~bM,
				.local_a = ~M * cap_point * M,
				.local_b = box_point,
				.normal = bM * @undual (n) * ~bM,
				.separation = sqrt (line • ~line) - r,
				.a = aent,
				.b = bent,
			};
			return true;
		}
		return false;
	}

}

static point_t
plane_closest_point (plane_t p, point_t c, point_t e)
{
	@algebra (PGA) {
		auto q = (c ∨ (-p * e0123)).bvect;
		auto mins = -(e123 ∨ e).bvect;
		auto maxs =  (e123 ∨ e).bvect;
		auto tmin = (uvec3) ((vec3) q < '0 0 0');
		auto tmax = (uvec3) ('0 0 0' < (vec3) q);
		auto tcen = ~tmin & ~tmax;
		auto x = (PGA.bvect) ( (tmin & (uvec3) mins)
							 + (tcen & (uvec3) q)
							 + (tmax & (uvec3) maxs));
		return c - e0 * x;
	}
}

typedef struct boxvoronoi_s {
	uvec4 min;
	uvec4 cen;
	uvec4 max;
	point_t x;
	vec3 sign;
} boxvoronoi_t;

typedef struct boxstate_s {
	point_t c[2];
	point_t e[2];
	motor_t M[2];
	motor_t abM[2];

	plane_t px[2];
	plane_t py[2];
	plane_t pz[2];
	boxvoronoi_t v[2];
	int planes[2];
} boxstate_t;

@generic (bivec = [bivector_t]) {
void Gizmo_AddLine (bivec bv, float radius, vec4 color)
{
	Gizmo_AddLine ((vec3) bv.bvect, (vec3) bv.bvecp, radius, color);
}
};

static boxvoronoi_t
box_voronoi_classification (point_t p, point_t c, point_t e)
{
	const vec3 pone = ' 1.0  1.0  1.0'f;
	const vec3 mone = '-1.0 -1.0 -1.0'f;

	@algebra (PGA) {
		auto tmin = (uvec4) ((vec4) p <= vec4(0));
		auto tmax = (uvec4) ((vec4) p >  vec4(0));
		auto tcen = ~tmin & ~tmax;
		boxvoronoi_t v = {
			.min = tmin,
			.cen = tcen,
			.max = tmax,
			.x = (point_t) ( (tmin & (uvec4) -e)
						   + (tcen & (uvec4)  p)
						   + (tmax & (uvec4)  e)) + c,
			.sign = (vec3) ( (tmin.xyz & @bitcast (uvec3, mone))
						   + (tmax.xyz & @bitcast (uvec3, pone))),
		};
		return v;
	}
}

static void
box_test_space (boxstate_t *s, bool swapped)
{
	int a = 1+!swapped;
	int b = 1+swapped;
	@algebra (PGA) {
		auto v = box_voronoi_classification (s.abM[b] * s.c[b] * ~s.abM[b],
											 s.c[a], s.e[a]);
		s.v[a] = v;
		s.px[a] = v.x • (e32 * v.sign.x);
		s.py[a] = v.x • (e13 * v.sign.y);
		s.pz[a] = v.x • (e21 * v.sign.z);
		s.planes[a] = -@horiz(+ v.min.xyz | v.max.xyz);

		auto p = s.M[a] * v.x * ~s.M[a];

		Gizmo_AddSphere ((vec4) p, 0.05, {1, 1+swapped, 1, 0.5});
	}
}

void
draw_edges (boxstate_t *s, bool swapped)
{
	int a = 1+!swapped;
	int b = 1+swapped;

	auto px = s.px[a];
	auto py = s.py[a];
	auto pz = s.pz[a];

	auto lyz = s.M[a] * (py ∧ pz) * ~s.M[a];
	auto lzx = s.M[a] * (pz ∧ px) * ~s.M[a];
	auto lxy = s.M[a] * (px ∧ py) * ~s.M[a];

	//FIXME need the cast otherwise qfcc segfaults when looking for the
	//generic function
	Gizmo_AddLine (lyz, 0.01, (vec4) {1, 0, 0, 0.5});
	Gizmo_AddLine (lzx, 0.01, (vec4) {0, 1, 0, 0.5});
	Gizmo_AddLine (lxy, 0.01, (vec4) {0, 0, 1, 0.5});
if (!swapped) return;
	{
		// FIXME qfcc fails to optimize  M * ⋆(a ∧ b) * ~M correctly, missing
		// cancelations in the trivector due to insufficnet distirbution (the
		// cancelations do not depend on any hidden knowlege)
		//auto dyz = ⋆(py ∧ pz);
		//auto dzx = ⋆(pz ∧ px);
		//auto dxy = ⋆(px ∧ py);
		//dyz = s.M[a] * dyz * ~s.M[a];
		//dzx = s.M[a] * dzx * ~s.M[a];
		//dxy = s.M[a] * dxy * ~s.M[a];

		auto dyz = ⋆(s.abM[a] * (py ∧ pz) * ~s.abM[a]);
		auto dzx = ⋆(s.abM[a] * (pz ∧ px) * ~s.abM[a]);
		auto dxy = ⋆(s.abM[a] * (px ∧ py) * ~s.abM[a]);
		dyz = s.M[b] * dyz * ~s.M[b];
		dzx = s.M[b] * dzx * ~s.M[b];
		dxy = s.M[b] * dxy * ~s.M[b];
		Gizmo_AddLine (dyz, 0.01, (vec4) {0, 1, 1, 0.5});
		Gizmo_AddLine (dzx, 0.01, (vec4) {1, 0, 1, 0.5});
		Gizmo_AddLine (dxy, 0.01, (vec4) {1, 1, 0, 0.5});
	}
	{
		@algebra (PGA) {
			auto dx = ⋆(s.abM[a] * s.v[a].x * ~s.abM[a]);
			auto px = (e123 • dx) * dx;
			Gizmo_AddSphere ((vec4) (s.M[b]*px*~s.M[b]), 0.015, {0, 1, 0, 0.5});
		}
	}
	for (int i = 0; i < 21; i++) {
		@algebra (PGA) {
		for (int j = 0; j < 21; j++) {
			auto px = point_t(-1, (i-10)/10f, (j-10)/10f, 0) @hadamard s.e[b]
					+ s.c[b];
			auto py = point_t((i-10)/10f, -1, (j-10)/10f, 0) @hadamard s.e[b]
					+ s.c[b];
			auto pz = point_t((i-10)/10f, (j-10)/10f, -1, 0) @hadamard s.e[b]
					+ s.c[b];
			auto dpx = ⋆px;
			auto dpy = ⋆py;
			auto dpz = ⋆pz;
			auto pdx = (e123 • dpx) * dpx;
			auto pdy = (e123 • dpy) * dpy;
			auto pdz = (e123 • dpz) * dpz;
			Gizmo_AddSphere ((vec4) (s.M[b]*pdx*~s.M[b]), 0.005, {0, 1, 1, 0.5});
			Gizmo_AddSphere ((vec4) (s.M[b]*pdy*~s.M[b]), 0.005, {1, 0, 1, 0.5});
			Gizmo_AddSphere ((vec4) (s.M[b]*pdz*~s.M[b]), 0.005, {1, 1, 0, 0.5});
		}
		}
	}

	for (int i = 0; i < 12; i++) {
		//FIXME non-static initializes incorrectly
		static vec3 n[12][2] = {
			{' 1  0 0', '0  1  0'},
			{' 1  0 0', '0  0  1'},
			{' 1  0 0', '0 -1  0'},
			{' 1  0 0', '0  0 -1'},
			{'-1  0 0', '0  1  0'},
			{'-1  0 0', '0  0  1'},
			{'-1  0 0', '0 -1  0'},
			{'-1  0 0', '0  0 -1'},
			{' 0  1 0', '0  0  1'},
			{' 0  1 0', '0  0 -1'},
			{' 0 -1 0', '0  0  1'},
			{' 0 -1 0', '0  0 -1'},
		};
		@algebra (PGA) {
			auto x = s.c[a] + (point_t) vec4(n[i][0], 0) @hadamard s.e[a];
			auto y = s.c[a] + (point_t) vec4(n[i][1], 0) @hadamard s.e[a];
			auto p1 = x • ((e32 + e13 + e21) @hadamard (PGA.bvect) n[i][0]);
			auto p2 = y • ((e32 + e13 + e21) @hadamard (PGA.bvect) n[i][1]);

			auto dl = ⋆(s.abM[a] * (p1 ∧ p2) * ~s.abM[a]);
			dl = s.M[b] * dl * ~s.M[b];
			//Gizmo_AddLine (dl, 0.005, (vec4) {1, 1, 1, 0.5});
			auto z = s.M[b] * s.c[b] * ~s.M[b];
			z = ((z • dl) * ~dl).tvec;
			Gizmo_AddSphere ((vec4) z, 0.015, {1, 0, 1, 0.5});
			auto P1 = ⋆(s.abM[a] * p1 * ~s.abM[a]);
			auto P2 = ⋆(s.abM[a] * p2 * ~s.abM[a]);
			P1 /= ⋆(e0∧P1);
			P2 /= ⋆(e0∧P2);
			Gizmo_AddSphere ((vec4) (s.M[b]*P1*~s.M[b]), 0.015, {1, 0, 0, 0.5});
			Gizmo_AddSphere ((vec4) (s.M[b]*P2*~s.M[b]), 0.015, {1, 0, 0, 0.5});
		}
	}
	for (int i = 0; i < 12; i++) {
		//FIXME non-static initializes incorrectly
		static vec3 n[12][2] = {
			{' 1  0 0', '0  1  0'},
			{' 1  0 0', '0  0  1'},
			{' 1  0 0', '0 -1  0'},
			{' 1  0 0', '0  0 -1'},
			{'-1  0 0', '0  1  0'},
			{'-1  0 0', '0  0  1'},
			{'-1  0 0', '0 -1  0'},
			{'-1  0 0', '0  0 -1'},
			{' 0  1 0', '0  0  1'},
			{' 0  1 0', '0  0 -1'},
			{' 0 -1 0', '0  0  1'},
			{' 0 -1 0', '0  0 -1'},
		};
		@algebra (PGA) {
			auto x = s.c[b] + (point_t) vec4(n[i][0], 0) @hadamard s.e[b];
			auto y = s.c[b] + (point_t) vec4(n[i][1], 0) @hadamard s.e[b];
			auto p1 = x • ((e32 + e13 + e21) @hadamard (PGA.bvect) n[i][0]);
			auto p2 = y • ((e32 + e13 + e21) @hadamard (PGA.bvect) n[i][1]);

			auto P1 = s.M[b]*⋆p1*~s.M[b];
			auto P2 = s.M[b]*⋆p2*~s.M[b];
			P1 /= ⋆(e0∧P1);
			P2 /= ⋆(e0∧P2);
			Gizmo_AddCapsule ((vec4) P1, (vec4) P2, 0.005, {0, 1, 0, 0.5});
		}
	}
}

bool get_contact_box_box (uint aent, collider_t acol,
						  uint bent, collider_t bcol,
						  contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (aent, qent_state, &aS);
	get_component (bent, qent_state, &bS);
	get_component (aent, qent_body, &aB);
	get_component (bent, qent_body, &bB);
	auto aM = aS.M * aB.R;
	auto bM = bS.M * bB.R;

	// transform the capsule into the box's space
	auto abM = ~bM * aM;
	auto baM = ~aM * bM;

	auto ac = (point_t) [acol.box.offset, 1];
	auto ae = (point_t) [acol.box.extent, 0];
	auto bc = (point_t) [bcol.box.offset, 1];
	auto be = (point_t) [bcol.box.extent, 0];

	Gizmo_AddSphere ((vec4) (bM*bc*~bM), 0.15, {1, 0, 1, 0.5});

	boxstate_t state = {
		.c = {ac, bc},
		.e = {ae, be},
		.M = {aM, bM},
		.abM = {abM, baM},
	};
	box_test_space (&state, false);
	box_test_space (&state, true);
	draw_edges (&state, false);
	draw_edges (&state, true);
	if (state.planes[1] == 1) {
		// find closest point on a to b's plane
		@algebra (PGA) {
			auto v = state.v[1];
			auto p = ((e32 + e13 + e21) @hadamard (PGA.bvect) v.sign) • v.x;
			auto P = box_plane_point (baM * p * ~baM, ac, ae);
			//printf ("%q %q\n", p, P);
			Gizmo_AddSphere ((vec4) (aM*P*~aM), 0.15,
							 {0.729, 0.855, 0.333, 0.5});
		}
	}
	auto x = box_closest_point (abM * state.v[0].x * ~abM, bc, be);
	auto y = box_voronoi_classification (baM * x * ~baM, ac, ae);
	Gizmo_AddSphere ((vec4) (bM*x*~bM), 0.05, {0, 1, 0, 0.5});
	int planes = -@horiz(+ y.min.xyz | y.max.xyz);
	Gizmo_AddSphere ((vec4) (aM*y.x*~aM), 0.05,
					 {planes == 1, planes == 2, planes == 3, 0.5});
	Gizmo_AddSphere ((vec4) (aM*y.x*~aM), 0.002, {1, 1, 1, 0.5});

	//printf ("%d %d\n", state.planes[0], state.planes[1]);
	return false;
}

bool get_contact_ball_plane (uint a, collider_t acol,
							 uint b, collider_t bcol,
							 contact_t *contact)
{
	return get_contact_plane_ball (b, bcol, a, acol, contact);
}

bool get_contact_capsule_plane (uint a, collider_t acol,
								uint b, collider_t bcol,
								contact_t *contact)
{
	return get_contact_plane_capsule (b, bcol, a, acol, contact);
}

bool get_contact_capsule_ball (uint a, collider_t acol,
							   uint b, collider_t bcol,
							   contact_t *contact)
{
	return get_contact_ball_capsule (b, bcol, a, acol, contact);
}

bool get_contact_box_plane (uint a, collider_t acol,
						   uint b, collider_t bcol,
						   contact_t *contact)
{
	return get_contact_plane_box (b, bcol, a, acol, contact);
}

bool get_contact_box_ball (uint a, collider_t acol,
						   uint b, collider_t bcol,
						   contact_t *contact)
{
	return get_contact_ball_box (b, bcol, a, acol, contact);
}

bool get_contact_box_capsule (uint a, collider_t acol,
						   uint b, collider_t bcol,
						   contact_t *contact)
{
	return get_contact_capsule_box (b, bcol, a, acol, contact);
}

get_contact_t get_contact[4][4] = {
	[col_plane] = {
		[col_plane]   = nil,	// two infinite planes almost always collide
		[col_ball]    = get_contact_plane_ball,
		[col_capsule] = get_contact_plane_capsule,
		[col_box]     = get_contact_plane_box,
	},
	[col_ball] = {
		[col_plane]   = get_contact_ball_plane,
		[col_ball]    = get_contact_ball_ball,
		[col_capsule] = get_contact_ball_capsule,
		[col_box]     = get_contact_ball_box,
	},
	[col_capsule] = {
		[col_plane]   = get_contact_capsule_plane,
		[col_ball]    = get_contact_capsule_ball,
		[col_capsule] = get_contact_capsule_capsule,
		[col_box]     = get_contact_capsule_box,
	},
	[col_box] = {
		[col_plane]   = get_contact_box_plane,
		[col_ball]    = get_contact_box_ball,
		[col_capsule] = get_contact_box_capsule,
		[col_box]     = get_contact_box_box,
	},
};

void
resolve_contact (contact_t *contact)
{
	state_t aS, bS;
	body_t aB, bB;
	get_component (contact.a, qent_state, &aS);
	get_component (contact.a, qent_body, &aB);
	get_component (contact.b, qent_state, &bS);
	get_component (contact.b, qent_body, &bB);
	float aM = aB.Ii.bvect[0];
	float bM = bB.Ii.bvect[0];
	float ad = contact.separation * aM / (aM + bM);
	float bd = contact.separation * bM / (aM + bM);
	@algebra (PGA) {
		// separation is negative
		auto aD = 1 - e0 * ad * contact.normal * 0.5;
		auto bD = 1 + e0 * bd * contact.normal * 0.5;
		aS.M = aD * aS.M;
		bS.M = bD * bS.M;

		auto Q = aD * contact.world_a * ~aD;
		impact2(&aS, &bS, &aB, &bB, Q, contact.normal, 0.5, 0.8);
	};
	set_component (contact.a, qent_state, &aS);
	set_component (contact.b, qent_state, &bS);
}

void
update_physics (uint ent)
{
	state_t state;
	body_t  body;
	transform_t xform;

	get_component (ent, qent_state, &state);
	get_component (ent, qent_body, &body);
	get_component (ent, qent_transform, &xform);

	if (!physics_paused) {
		if (has_component (ent, qent_grav)) {
			state = update_grav_state (state, body, xform);
		} else {
			state = update_block_state (state, body, xform);
		}
		set_component (ent, qent_state, &state);
	}

	auto M = state.M * body.R;
	set_transform (M, xform);
	draw_axes (xform);

	if (has_component (ent, qent_collider)) {
		//FIXME O(N^2)
		collider_t col;
		get_component (ent, qent_collider, &col);
		draw_collider (col, xform, M);
		for (uint i = 0; i < num_collider_ents; i++) {
			uint        oent = collider_ents[i];
			collider_t  ocol;
			get_component (oent, qent_collider, &ocol);

			auto gc = get_contact[col.type][ocol.type];
			if (!gc) {
				continue;
			}
			contact_t contact;
			if (!gc (ent, col, oent, ocol, &contact)) {
				continue;
			}
			state_t ostate;
			body_t obody;
			//printf ("%x %x %q %q %q %q %q %f\n", contact.a, contact.b,
			//		contact.world_a, contact.world_b,
			//		contact.local_a, contact.local_b,
			//		contact.normal, contact.separation);
			resolve_contact (&contact);
		}
		collider_ents[num_collider_ents++] = ent;
	}
}

body_t
calc_inertia_plane (collider_t collider, float invDensity)
{
	body_t body = {};//infinite mass
	body.R = 1;
	return body;
}

body_t
calc_inertia_ball (collider_t collider, float invDensity)
{
	body_t body = {};//infinite mass
	body.R = 1;
	float r = collider.ball.radius;
	if (!invDensity || !r) {
		return body;
	}
	float vol = (4 * (float)M_PI * r * r * r / 3);
	float I = 2 * r * r / 5;
	invDensity /= vol;
	body.Ii.bvect = '1 1 1';
	body.Ii.bvecp = '1 1 1';
	body.I.bvect = body.Ii.bvect / invDensity;
	body.I.bvecp = body.Ii.bvecp * I / invDensity;
	body.Ii.bvect *= invDensity;
	body.Ii.bvecp *= invDensity / I;
	return body;
}

vec3 best_axis(vec3 dir, @out int ind)
{
	static const int indices[] = { 0, 0, 1, 0, 2, 2, 2, 2 };
	static const vec3 axes[] = {
		'1 0 0',
		'0 1 0',
		'0 0 1',
	};
	vec3 adir = abs (dir);
	vec3 mdir = max (adir.yzx, adir.zxy);
	uvec3 m = adir >= mdir;
	ind = indices[@horiz(| '1 2 4' & m)];
	vec3 a = axes[ind];
	return a • dir < 0 ? -a : a;
}

body_t
calc_inertia_capsule (collider_t collider, float invDensity)
{
	body_t body = {};//infinite mass
	body.R = 1;
	vec3 offset = collider.capsule.offset;
	float r = collider.capsule.radius;
	vec3 a = collider.capsule.axis;
	if (!a) {
		return calc_inertia_ball (collider, invDensity);
	}
	if (!invDensity || !r) {
		return body;
	}
	// the centers of bases of the two end caps are at +/- a
	float l = sqrt (a • a);
	// hemisphere volume
	float h_vol = (2 * (float)M_PI * r * r * r / 3);
	// half cylinder volume
	float c_vol = ((float)M_PI * r * r * l);
	// half total volume
	float vol = h_vol + c_vol;

	// The moment of inertia of a solid hemisphere is the same about either
	// the axis of symmetry or an axis through the diameter of its base (and
	// also the same as that of a solid sphere of half the density).
	float Iah = r * r * h_vol * 2 / 5;
	// The moment of inertia of the cylinder about its axis of rotational
	// symmetry.
	float Iac = r * r * c_vol;
	// The moment of inertia of the cylinder about its axis cross the diameter
	// of its base (same as for one through the center but half the density).
	float Itc = (r * r * 3 + l * l * 2) * c_vol / 6;
	// The center of mass of the hemisphere is at 3/8*r, but applying
	// the parallel axis theorem cancels things out.
	float Ith = Iah + l * (l + 3 * r / 4) * h_vol;
	float Ia = 2 * (Iah + Iac);
	float It = 2 * (Ith + Itc);
	int ind;
	vec3 axis = best_axis (a, ind);
	vec3 Ivec;
	Ivec[ind] = Ia / invDensity;
	Ivec[(ind + 1) % 3] = It / invDensity;
	Ivec[(ind + 2) % 3] = It / invDensity;
	body.I.bvect = (PGA.bvect)'2 2 2' * vol / invDensity;
	body.Ii.bvect = (PGA.bvect)'0.5 0.5 0.5' * invDensity / vol;
	body.I.bvecp = (PGA.bvecp)Ivec;
	body.Ii.bvecp = (PGA.bvecp)(1/Ivec);

	auto R = sqrt ((plane_t)[a, 0] * (plane_t)[axis, 0]);
	auto T = 1 + (PGA.bvecp)offset / 2;

	body.R = R * T;


	return body;
}

body_t
calc_inertia_box (collider_t collider, float invDensity)
{
	body_t body = {};//infinite mass
	body.R = 1;
	vec3 e = collider.box.extent;
	if (!invDensity || @horiz(| e == '0 0 0')) {
		return body;
	}
	float vol = 8 * e.x * e.y * e.z;
	e = e * e;
	float I = 1.0f / 3;
	invDensity /= vol;
	body.Ii.bvect = '1 1 1';
	body.Ii.bvecp = (PGA.bvecp) [e.y + e.z, e.z + e.x, e.x + e.y];
	//body.Ii.bvecp = (PGA.bvecp) (e.yzx + e.zxy);
	body.I.bvect = body.Ii.bvect / invDensity;
	body.I.bvecp = body.Ii.bvecp * I / invDensity;
	body.Ii.bvect *= invDensity;
	body.Ii.bvecp *= invDensity / I;
	return body;
}
