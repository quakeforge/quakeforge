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
draw_collider (collider_t col, transform_t xform)
{
	mat4 mat = Transform_GetWorldMatrix(xform);
	switch (col.type) {
	case col_plane:
	{
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
	}
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

float
clamp (float x, float a, float b)
{
	return max (a, min(x, b));
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

get_contact_t get_contact[3][3] = {
	//col_plane
	{
		nil,	// two infinite planes almost always collide, so ignore
		get_contact_plane_ball,
		get_contact_plane_capsule,
	},
	{
		get_contact_ball_plane,
		get_contact_ball_ball,
		get_contact_ball_capsule,
	},
	{
		get_contact_capsule_plane,
		get_contact_capsule_ball,
		get_contact_capsule_capsule,
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

	if (has_component (ent, qent_grav)) {
		state = update_grav_state (state, body, xform);
	} else {
		state = update_block_state (state, body, xform);
	}
	set_component (ent, qent_state, &state);

	auto M = state.M * body.R;
	set_transform (M, xform);
	draw_axes (xform);

	if (has_component (ent, qent_collider)) {
		//FIXME O(N^2)
		collider_t col;
		get_component (ent, qent_collider, &col);
		draw_collider (col, xform);
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

vec3 abs(vec3 x)
{
	uvec3 m = x < '0 0 0';
	return (vec3) ((uvec3) x & ~m) - (vec3) ((uvec3) x & m);
}

@overload
vec3 max(vec3 a, vec3 b)
{
	uvec3 m = a < b;
	return (vec3) ((uvec3) a & ~m) + (vec3) ((uvec3) b & m);
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
	vec3 mdir = max (adir.yxz, adir.zxy);
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

	printf ("%v %v\n", body.I.bvect, body.I.bvecp);
	printf ("%v %v\n", body.Ii.bvect, body.Ii.bvecp);
	printf ("%g %v %v %g\n", body.R.scalar, body.R.bvect, body.R.bvecp, body.R.qvec);

	return body;
}
