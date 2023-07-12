#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"

static struct {
	quat_t      q1;
	quat_t      q2;
	quat_t      expect;
} quat_mult_tests[] = {
	{{1, 2, 3, 4}, {5, 6, 7, 8}, {24, 48, 48, -6}},
};
#define num_quat_mult_tests (sizeof (quat_mult_tests) / sizeof (quat_mult_tests[0]))

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

static struct {
	vec3_t      a;
	vec3_t      b;
	quat_t      expect;
} quat_vector_rotation_tests[] = {
	{{1, 0, 0}, {1, 0, 0}, {0, 0, 0, 1}},
	{{0, 1, 0}, {0, 1, 0}, {0, 0, 0, 1}},
	{{0, 0, 1}, {0, 0, 1}, {0, 0, 0, 1}},
	{{1, 0, 0}, {8, 0, 0}, {0, 0, 0, 1}},
	{{0, 8, 0}, {0, 1, 0}, {0, 0, 0, 1}},
	{{0, 0, 8}, {0, 0, 8}, {0, 0, 0, 1}},
	{{1, 0, 0}, {-1, 0, 0}, {0, 0, 0, 0}}, // x, y, z = NaN
	{{0, -1, 0}, {0, 1, 0}, {0, 0, 0, 0}}, // x, y, z = NaN
	{{0, 0, 1}, {0, 0, -1}, {0, 0, 0, 0}}, // x, y, z = NaN
	{{-1, 0, 0}, {8, 0, 0}, {0, 0, 0, 0}}, // x, y, z = NaN
	{{0, 8, 0}, {0, -1, 0}, {0, 0, 0, 0}}, // x, y, z = NaN
	{{0, 0, -8}, {0, 0, 8}, {0, 0, 0, 0}}, // x, y, z = NaN
	// excessive for float, but if vec_t becomes double...
	// 1/50 second orbiting JNSQ Kerbin at 120km altitude. While this has
	// nothing to do with quakeforge (yet?), it came up when testing camera
	// rotation in KSP and Unity failed miserably. I don't remember the exact
	// details, but I could see significant snapping in the rotation (I thought
	// it was a few times per second, but these numbers indicate it must have
	// been every few seconds).
	// The quaternion is unit to 9 sigfigs (a little larger)
	{{1720000, 0, 0}, {1719999.9983028059, 76.409082595828366, 0}, {0, 0, 2.22119434e-5, 1}},
	// 1/20 second, same situation
	{{1720000, 0, 0}, {1719999.9893925365, 191.02270615971355, 0}, {0, 0, 5.55298575e-5, 1}},
	// 1/5 second, same situation
	{{1720000, 0, 0}, {1719999.8302805868, 764.09080107761736, 0}, {0, 0, 2.2211943e-4, 1}},
	// 1/4 second, same situation
	{{1720000, 0, 0}, {1719999.7348134194, 955.11348367609469, 0}, {0, 0, 2.77649262e-4, 1}},
	// 1/3 second, same situation. This is (float) 1 ulp in w: about 0.0424
	// degrees.
	// The quaternion is unit to 9 sigfigs (a little larger)
	{{1720000, 0, 0}, {1719999.5285571995, 1273.4845939975546, 0}, {0, 0, 3.70199094e-4, 0.99999994}},
};
#define num_quat_vector_rotation_tests (sizeof (quat_vector_rotation_tests) / sizeof (quat_vector_rotation_tests[0]))

// return true if a and b are close enough (yay, floats)
static int
compare (vec_t a, vec_t b)
{
	vec_t       diff = a - b;
	return diff * diff < 0.001;
}

static int
test_quat_mult(const quat_t q1, const quat_t q2, const quat_t expect)
{
	int         i;
	quat_t      r;

	QuatMult (q1, q2, r);

	for (i = 0; i < 4; i++)
		if (!compare (r[i], expect[i]))
			goto fail;
	return 1;
fail:
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (q1));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (q2));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (r));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (expect));
	return 0;
}


static void
rotate_vec (const quat_t r, const vec3_t v, vec3_t out)
{
	quat_t      qv = {0, 0, 0, 0};
	quat_t      t;

	VectorCopy (v, qv);

	QuatConj (r, t);
	QuatMult (qv, t, t);
	QuatMult (r, t, t);
	VectorCopy (t, out);
}

static int
test_rotation (const vec3_t angles)
{
	int         i;
	vec3_t      forward, right, up;

	quat_t      quat, conj, f, r, u, t;
	quat_t      qf = {1,  0, 0, 0};
	quat_t      qr = {0, -1, 0, 0};
	quat_t      qu = {0,  0, 1, 0};

	AngleVectors (angles, forward, right, up);

	AngleQuat (angles, quat);
	QuatConj (quat, conj);
	// rotate forward vector
	QuatMult (qf, conj, t);
	QuatMult (quat, t, f);
	// rotate right vector
	QuatMult (qr, conj, t);
	QuatMult (quat, t, r);
	// rotate up vector
	QuatMult (qu, conj, t);
	QuatMult (quat, t, u);

	if (!compare (f[3], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (forward[i], f[i]))
			goto fail;

	if (!compare (r[3], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (right[i], r[i]))
			goto fail;

	if (!compare (u[3], 0))
		goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (up[i], u[i]))
			goto fail;
	return 1;
fail:
	printf ("\ntest_rotation\n");
	printf ("%11.9g %11.9g %11.9g\n", VectorExpand (angles));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (quat));
	printf ("%11.9g %11.9g %11.9g %11.9g\n\n", QuatExpand (conj));
	printf ("%11.9g %11.9g %11.9g\n", VectorExpand (forward));
	printf ("%11.9g %11.9g %11.9g\n", VectorExpand (right));
	printf ("%11.9g %11.9g %11.9g\n\n", VectorExpand (up));

	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (f));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (r));
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (u));
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
	printf ("\ntest_rotation2\n");
	printf ("\n\n%11.9g %11.9g %11.9g\n\n", angles[0], angles[1], angles[2]);
	printf ("%11.9g %11.9g %11.9g\n", forward[0], forward[1], forward[2]);
	printf ("%11.9g %11.9g %11.9g\n", right[0], right[1], right[2]);
	printf ("%11.9g %11.9g %11.9g\n\n", up[0], up[1], up[2]);

	printf ("%11.9g %11.9g %11.9g\n", f[0], f[1], f[2]);
	printf ("%11.9g %11.9g %11.9g\n", r[0], r[1], r[2]);
	printf ("%11.9g %11.9g %11.9g\n", u[0], u[1], u[2]);
	return 0;
}

static int
test_rotation3 (const vec3_t angles)
{
	int         i;
	quat_t      quat;
	vec3_t      v = {1, 1, 1};
	vec3_t      a, b;

	AngleQuat (angles, quat);
	QuatMultVec (quat, v, a);
	rotate_vec (quat, v, b);

	for (i = 0; i < 3; i++)
		if (!compare (a[i], b[i]))
			goto fail;
	return 1;
fail:
	printf ("\ntest_rotation3\n");
	printf ("%11.9g %11.9g %11.9g\n", VectorExpand(a));
	printf ("%11.9g %11.9g %11.9g\n", VectorExpand(b));
	return 0;
}

// XXX FIXME see usage in test_rotation4. need to investigate whether
// -ffast-math is any real benefit
#define ISNAN(x) (((x) & 0x7f800000) == 0x7f800000 && ((x) & 0x7fffff))

// FIXME differences in precision between archs
static int
cmp (float a, float b)
{
	typedef union {
		float       f;
		int         i;
	} fi;
	fi          ax;
	fi          bx;
	int         x;

	ax.f = a;
	bx.f = b;
	x = ax.i - bx.i;
	if (x < 0) {
		x = -x;
	}
	return (x & 0x7ffffffc) == 0;
}

static int
test_rotation4 (const vec3_t a, const vec3_t b, const quat_t expect)
{
	int         i;
	union { int x[4]; vec_t q[4]; } q;
	vec_t      *quat = q.q;
	vec3_t      t;
	vec_t       d = 0;

	VectorZero (t);

	// find the rotation vector between a and b
	QuatRotation (a, b, quat);

	if (quat[3] == 0) {
		if (expect[3] != 0) {
			goto fail;
		}
		// expect NaN for the vector components because the vectors are
		// anti-parallel and thus the rotation axis is undefined
		//XXX FIXME(?) still using -ffast-math which implies
		// -ffinite-math-only which in turn disables nan/inf checks, so have
		// to do it by hand
		// if (!(isnan(quat[0]) && isnan(quat[1]) && isnan(quat[2]))) {
		if (!(ISNAN(q.x[0]) && ISNAN(q.x[1]) && ISNAN(q.x[2]))) {
			goto fail;
		}
	} else {
		// the vectors are not anti-parallel and thus the rotation axis is
		// defined, so NaN is invalid
		// XXX FIXME see above
		//if (isnan(quat[0]) || isnan(quat[1]) || isnan(quat[2])) {
		if (ISNAN(q.x[0]) || ISNAN(q.x[1]) || ISNAN(q.x[2])) {
			goto fail;
		}
		for (i = 0; i < 4; i++) {
			// yes, float precision will make it difficult to set up expect
			// but it is at least consistent (ie, the "errors" are not at all
			// random and thus will be the same from run to run)
			if (!cmp (quat[i], expect[i])) {
				goto fail;
			}
		}
		QuatMultVec(quat, a, t);

		d = DotProduct (t, b) / (VectorLength (t) * VectorLength (b)) - 1;
		if (d * d > 1e-8) {
			goto fail;
		}
	}
	return 1;
fail:
	printf ("\ntest_rotation4\n");
	printf ("a: %11.9g %11.9g %11.9g\n", VectorExpand(a));
	printf ("b: %11.9g %11.9g %11.9g\n", VectorExpand(b));
	printf ("q: %11.9g %11.9g %11.9g %11.9g\n", QuatExpand(quat));
	printf ("e: %11.9g %11.9g %11.9g %11.9g\n", QuatExpand(expect));
	printf ("t: %11.9g %11.9g %11.9g\n", VectorExpand(t));
	printf ("d: %11.9g\n", d);
	return 0;
}

#define s05 0.70710678118654757

static struct {
	quat_t      q;
	vec_t       expect[9];
} quat_mat_tests[] = {
	{{0, 0, 0, 1},
		{1, 0, 0,
		 0, 1, 0,
		 0, 0, 1}},
	{{1, 0, 0, 0},
		{1,  0,  0,
		 0, -1,  0,
		 0,  0, -1}},
	{{0, 1, 0, 0},
		{-1, 0,  0,
		  0, 1,  0,
		  0, 0, -1}},
	{{0, 0, 1, 0},
		{-1,  0, 0,
		  0, -1, 0,
		  0,  0, 1}},
	{{0.5, 0.5, 0.5, 0.5},
		{0, 0, 1,
		 1, 0, 0,
		 0, 1, 0}},
#if (defined(__i686__) || defined(__aarch64__)) && defined(__OPTIMIZE__)
	// the fp unit carries more precision than a 32-bit float, so
	// the close-to-zero errors are different
	{{s05, 0.0, 0.0, s05},
		{1,             0,             0,
		 0, 3.42285418e-08,   -0.99999994,
		 0,    0.99999994, 3.42285418e-08}},
#else
	{{s05, 0.0, 0.0, s05},
		{1,             0,             0,
		 0, 5.96046448e-8,   -0.99999994,
		 0,    0.99999994, 5.96046448e-8}},
#endif
};
#define num_quat_mat_tests (sizeof (quat_mat_tests) / sizeof (quat_mat_tests[0]))

static int
test_quat_mat(const quat_t q, const quat_t expect)
{
	int         i;
	vec_t       m[9];

	QuatToMatrix (q, m, 0, 0);

	for (i = 0; i < 9; i++)
		if (m[i] != expect[i])	// exact tests here
			goto fail;
	return 1;
fail:
	printf ("\ntest_quat_mat\n");
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (q));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 0), VectorExpand (expect + 0));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 3), VectorExpand (expect + 3));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 6), VectorExpand (expect + 6));
	return 0;
}

static int
test_quat_mat2(const quat_t q, const quat_t expect)
{
	int         i;
	vec_t       m[9];

	QuatToMatrix (q, m, 0, 1);
	Mat3Transpose (m, m);

	for (i = 0; i < 9; i++)
		if (m[i] != expect[i])	// exact tests here
			goto fail;
	return 1;
fail:
	printf ("\ntest_quat_mat\n");
	printf ("%11.9g %11.9g %11.9g %11.9g\n", QuatExpand (q));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 0), VectorExpand (expect + 0));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 3), VectorExpand (expect + 3));
	printf ("%11.9g %11.9g %11.9g   %11.9g %11.9g %11.9g\n",
			VectorExpand (m + 6), VectorExpand (expect + 6));
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;

	for (i = 0; i < num_quat_mult_tests; i ++) {
		vec_t      *q1 = quat_mult_tests[i].q1;
		vec_t      *q2 = quat_mult_tests[i].q2;
		vec_t      *expect = quat_mult_tests[i].expect;
		if (!test_quat_mult (q1, q2, expect))
			res = 1;
	}

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_rotation (test_angles[i]))
			res = 1;
	}

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_rotation2 (test_angles[i]))
			res = 1;
	}

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_rotation3 (test_angles[i]))
			res = 1;
	}

	for (i = 0; i < num_quat_vector_rotation_tests; i++) {
		vec_t      *a = quat_vector_rotation_tests[i].a;
		vec_t      *b = quat_vector_rotation_tests[i].b;
		vec_t      *expect = quat_vector_rotation_tests[i].expect;
		if (!test_rotation4 (a, b, expect)) {
			res = 1;
		}
	}

	for (i = 0; i < num_quat_mat_tests; i ++) {
		vec_t      *q = quat_mat_tests[i].q;
		vec_t      *expect = quat_mat_tests[i].expect;
		if (!test_quat_mat (q, expect))
			res = 1;
	}

	for (i = 0; i < num_quat_mat_tests; i ++) {
		vec_t      *q = quat_mat_tests[i].q;
		vec_t      *expect = quat_mat_tests[i].expect;
		if (!test_quat_mat2 (q, expect))
			res = 1;
	}

	return res;
}
