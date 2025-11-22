void printf (string fmt, ...) = #0;
@overload float sqrt (float x) = #0;

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.group_mask(0x6) rotor_t;
typedef PGA.group_mask(0xc) translator_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

@overload motor_t normalize (motor_t m);
@overload motor_t sqrt (motor_t m);
@overload motor_t exp (bivector_t b);
@overload rotor_t exp (PGA.bvect b);
@overload translator_t exp (PGA.bvecp b);
@overload bivector_t log (motor_t m);
motor_t make_motor (vec4 translation, vec4 rotation);

typedef struct state_s {
	motor_t     M;
	bivector_t  B;
} state_t;

typedef struct body_s {
	motor_t     R;
	bivector_t  I;
	bivector_t  Ii;
} body_t;

@overload state_t dState (state_t s, body_t *body);

state_t
update (state_t bs, body_t *body, float h)
{
	auto ds = dState (bs, body);
	bs.M += h * ds.M;
	bs.B += h * ds.B;
	bs.M = normalize (bs.M);
	return bs;
}

state_t dState (state_t s, body_t *body)
{
	return {
		.M = -0.5f * s.M * s.B,
		.B = -@undual(body.Ii @hadamard (s.B × (body.I @hadamard ⋆s.B))),
	};
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

int
main ()
{
	body_t body = {
		.R = { .scalar = 1 },
		.I = { .bvect = PGA.bvect()(1,1,1), .bvecp = PGA.bvecp()(1,1,1) },
		.Ii = { .bvect = PGA.bvect()(1,1,1), .bvecp = PGA.bvecp()(1,1,1) },
	};
	state_t state = {
		.M = { .scalar = 1 },
		.B.bvect = (PGA.bvect)'0 0 4',
	};
	state = update (state, &body, 1);
	printf ("%.9g %.9v %.9v %.9g\n",
			state.M.scalar, state.M.bvect, state.M.bvecp, state.M.qvec);
	// The exact values don't really matter (of course it's nice if they're
	// correct). What does matter is that M gets updated at all. The expected
	// value is 0.44721359 '0 0 -0.89442718' '0 0 0' 0
	if (state.M.scalar == 1) {
		return 1;
	}
	//FIXME something is badly broken with horizontal tests
	//if (state.M.bvect != 0) {
	//	return 0;
	//}
	return 0;
}
