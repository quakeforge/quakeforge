#include <math.h>

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

typedef struct state_s {
	motor_t     M;
	bivector_t  B;
} state_t;

@overload motor_t normalize (motor_t m);
@overload state_t dState (state_t s);

void
camera_first_person (state_t *state)
{
	vector dpos = {};
	dpos.x -= 20;
	dpos.y -= 20;
	dpos.z -= 20;

	vector drot = {};
	drot.x -= 10;
	drot.y -= 10;
	drot.z -= 10;

	dpos *= 0.01;
	drot *= ((float)M_PI / 360);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
//		printf ("%g %v %v %g\n", state.M.scalar,
//				state.M.bvect, state.M.bvecp,
//				state.M.qvec);
//		printf ("%v %v\n", state.B.bvect, state.B.bvecp);
	state.M = normalize (state.M);
}

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

state_t
dState (state_t s)
{
	return {
		.M = -0.5f * s.M * s.B,
		.B = -@undual(s.B × ⋆s.B),
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

void printf (string fmt, ...) = #0;
float (float x) sqrt = #0;

int main ()
{
	state_t state = {
		.M = make_motor ({ -4, 0, 3, 0, }, { 0, 0, 0, 1 }),
	};

	camera_first_person (&state);
	printf ("e:0.999998868 '0.000872663571 0.000872663571 0.000872663571' '2.00068855 0.00505431555 -1.4997437' 0.000441567507\n");
	printf ("e:'-0.0872664601 -0.0872664601 -0.0872664601' '-0.199999988 -0.199999988 -0.199999988'\n");
	printf ("g:%.9g %.9v %.9v %.9g\n", state.M.scalar,
	        state.M.bvect, state.M.bvecp,
	        state.M.qvec);
	printf ("g:%.9v %.9v\n", state.B.bvect, state.B.bvecp);
	bool ok = true;
	if (state.M.scalar != 0.999998868
		|| (vec3)state.M.bvect != '0.000872663571 0.000872663571 0.000872663571'
		|| (vec3)state.M.bvecp != '2.00068855 0.00505431555 -1.4997437'
		|| (float)state.M.qvec != 0.000441567507
		|| (vec3)state.B.bvect != '-0.0872664601 -0.0872664601 -0.0872664601'
		|| (vec3)state.B.bvecp != '-0.199999988 -0.199999988 -0.199999988') {
		ok = false;
	}
	return ok ? 0 : 1;
}
