#pragma bug die
#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec  plane_t;

typedef struct {
	vec4        trans;
	vec4        rot;
} xform_t;

motor_t
make_motor (vec4 translation, vec4 rotation)
{
	@algebra(PGA) {
		auto dt = (PGA.group_mask (0x18)) translation;
		auto q = (PGA.group_mask(0x06)) rotation;
		motor_t t = { .scalar = 1, .bvecp = -dt.bvecp / 2 };
		motor_t r = { .scalar = q.scalar, .bvect = q.bvect };
		motor_t m = t * r;
		return m;
	}
}

xform_t
set_transform (motor_t m, int p)
{
	@algebra (PGA) {
		auto r = ⋆(m * e0123);
		auto t = (m * ⋆(m * e0123)).bvecp;
		vec4 rot = (vec4) r;
		vec4 trans = [(vec3)t * -2, 1];
		if (p) printf ("set_transform: %q %q\n", rot, trans);
		return {trans, rot};
	}
}

PGA.scalar rh = 0.7071067811865476;

int
main()
{
	motor_t start = make_motor ({ 10, 4, -1.5, 0 }, { 0, 0, 0, 1 });
	vec3 bvt = { 0, 0, rh };
	vec3 bvp = { 0, -6*rh, 0 };
	motor_t delta = {
		.scalar = rh,
		.bvect = (PGA.bvect) bvt,
		.bvecp = (PGA.bvecp) bvp,
	};
	motor_t testm = delta * start;
	printf ("start: %g %v %v %g\n",
			start.scalar, start.bvect, start.bvecp, start.qvec);
	printf ("delta: %g %v %v %g\n",
			delta.scalar, delta.bvect, delta.bvecp, delta.qvec);
	printf ("testm: %g %v %v %g\n",
			testm.scalar, testm.bvect, testm.bvecp, testm.qvec);
	auto xform = set_transform (testm, 1);
	printf ("xform: %.9q %.9q\n", xform.trans, xform.rot);
	if (start.scalar != 1 || (vec3) start.bvect != '0 0 0'
		|| (vec3) start.bvecp != '-5 -2 0.75'
		|| (PGA.scalar) start.qvec != 0) {
		printf ("start incorrect\n");
		return 1;
	}
	if (delta.scalar != rh || (vec3) delta.bvect != '0 0 0.7071067811865476'
		|| (vec3) delta.bvecp != '0 -4.242640687119285 0'
		|| (PGA.scalar) delta.qvec != 0) {
		printf ("delta incorrect\n");
		return 1;
	}
	if (testm.scalar != rh || (vec3) testm.bvect != '0 0 0.7071067811865476'
		|| (vec3) testm.bvecp != '-4.949747468305833 -2.1213203435596424 0.5303300858899106'
		|| (PGA.scalar) testm.qvec != 0.5303300858899106) {
		printf ("testm incorrect\n");
		return 1;
	}
	if (xform.trans != '10 -4 -1.49999988 1'
		|| xform.rot != '0 0 -0.7071067811865476 0.7071067811865476') {
		printf ("xform incorrect\n");
		return 1;
	}
	return 0;
}
