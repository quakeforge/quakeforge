#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"

//PITCH YAW ROLL
static vec3_t test_angles[] = {
	{0, 0, 0},
	{45, 0, 0},
	{0, 45, 0},
	{0, 0, 45},
	{45, 45, 0},
	{0, 45, 45},
	{45, 0, 45},
	{45, 45, 45},
	{0, 180, 180},
	{180, 0, 180},
	{180, 180, 0},
};
#define num_angle_tests (sizeof (test_angles) / sizeof (test_angles[0]))

// return true if a and b are close enough (yay, floats)
static int
compare (vec_t a, vec_t b)
{
	vec_t       diff = a - b;
	return diff * diff < 0.001;
}

static int
test_rotation (const vec3_t angles)
{
	int         i;
	vec3_t      forward, right, up;

	quat_t      quat, f, r, u, t;
	quat_t      qf = {0, 1,  0, 0};
	quat_t      qr = {0, 0, -1, 0};
	quat_t      qu = {0, 0,  0, 1};

	AngleVectors (angles, forward, right, up);

	AngleQuat (angles, quat);
	// rotate forward vector
	QuatConj (quat, t);
	QuatMult (qf, t, t);
	QuatMult (quat, t, f);
	// rotate right vector
	QuatConj (quat, t);
	QuatMult (qr, t, t);
	QuatMult (quat, t, r);
	// rotate up vector
	QuatConj (quat, t);
	QuatMult (qu, t, t);
	QuatMult (quat, t, u);

	if (!compare (f[0], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (forward[i], f[i + 1]))
			goto fail;

	if (!compare (r[0], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (right[i], r[i + 1]))
			goto fail;

	if (!compare (u[0], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (up[i], u[i + 1]))
			goto fail;
	return 1;
fail:
	printf ("\n\n%g %g %g\n\n", angles[0], angles[1], angles[2]);
	printf ("%g %g %g\n", forward[0], forward[1], forward[2]);
	printf ("%g %g %g\n", right[0], right[1], right[2]);
	printf ("%g %g %g\n\n", up[0], up[1], up[2]);

	printf ("%g %g %g %g\n", f[0], f[1], f[2], f[3]);
	printf ("%g %g %g %g\n", r[0], r[1], r[2], r[3]);
	printf ("%g %g %g %g\n", u[0], u[1], u[2], u[3]);
	return 0;
}

static int
test_rotation2 (const vec3_t angles)
{
	int         i;
	vec3_t      forward, right, up;

	quat_t      quat;
	vec3_t      f, r, u;
	vec3_t      vf = {1,  0, 0};
	vec3_t      vr = {0, -1, 0};
	vec3_t      vu = {0,  0, 1};

	AngleVectors (angles, forward, right, up);

	AngleQuat (angles, quat);
	// rotate forward vector
	QuatMultVec (quat, vf, f);
	// rotate right vector
	QuatMultVec (quat, vr, r);
	// rotate up vector
	QuatMultVec (quat, vu, u);

	for (i = 0; i < 3; i++)
		if (!compare (forward[i], f[i]))
			goto fail;

	for (i = 0; i < 3; i++)
		if (!compare (right[i], r[i]))
			goto fail;

	for (i = 0; i < 3; i++)
		if (!compare (up[i], u[i]))
			goto fail;
	return 1;
fail:
	printf ("\n\n%g %g %g\n\n", angles[0], angles[1], angles[2]);
	printf ("%g %g %g\n", forward[0], forward[1], forward[2]);
	printf ("%g %g %g\n", right[0], right[1], right[2]);
	printf ("%g %g %g\n\n", up[0], up[1], up[2]);

	printf ("%g %g %g\n", f[0], f[1], f[2]);
	printf ("%g %g %g\n", r[0], r[1], r[2]);
	printf ("%g %g %g\n", u[0], u[1], u[2]);
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_rotation (test_angles[i]))
			res = 1;
	}

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_rotation2 (test_angles[i]))
			res = 1;
	}
	return res;
}
