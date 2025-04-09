#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;

typedef struct {
	motor_t     M;
	bivector_t  B;
} state_t;

void do_something (state_t *s, motor_t m)
{
	s.M += m;
}

int
main (void)
{
	int ret = 0;
	state_t state = {
		.M = {
			.bvect = (PGA.bvect)'0 0 1'f,
			.scalar = 1,
			.bvecp = (PGA.bvecp)'0 1 0'f,
			.qvec = (PGA.qvec)1f,
		},
		.B = {
			.bvect = (PGA.bvect)'1 0 0'f,
			.bvecp = (PGA.bvecp)'0 0 1'f,
		},
	};
	motor_t m = {
		.bvect = (PGA.bvect)'1 0 1'f,
		.scalar = 2,
		.bvecp = (PGA.bvecp)'-1 1 2'f,
		.qvec = (PGA.qvec)3f,
	};
	do_something (&state, m);
	printf ("%v %g %v %g %v %v\n",
			state.M.bvect, state.M.scalar,
			state.M.bvecp, state.M.qvec,
			state.B.bvect, state.B.bvecp);
	if ((vec3)state.M.bvect != '1 0 2'
		|| state.M.scalar != 3
		|| (vec3)state.M.bvecp != '-1 2 2'
		|| (float)state.M.qvec != 4) {
		ret = 1;
	}

	return ret;
}
