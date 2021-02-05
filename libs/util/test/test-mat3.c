#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/mathlib.h"

//PITCH YAW ROLL
static vec3_t test_angles[] = {
	{ 0,  0,  0},
	{45,  0,  0},
	{ 0, 45,  0},
	{ 0,  0, 45},
	{45, 45,  0},
	{ 0, 45, 45},
	{45,  0, 45},
	{45, 45, 45},
	{0, 180, 180},
	{180, 0, 180},
	{180, 180, 0},
};
#define num_angle_tests \
	(sizeof (test_angles) / sizeof (test_angles[0]))

static vec3_t test_scales[] = {
	{ 1, 1, 1},
	{ 2, 1, 1},
	{ 1, 2, 1},
	{ 1, 1, 2},
	{ 2, 2, 1},
	{ 1, 2, 2},
	{ 2, 1, 2},
	{ 2, 2, 2},
	{ 1, 2, 3},
	{ 1, 3, 2},
	{ 2, 1, 3},
	{ 2, 3, 1},
	{ 3, 1, 2},
	{ 3, 2, 1},
};
#define num_scale_tests \
	(sizeof (test_scales) / sizeof (test_scales[0]))

// return true if a and b are close enough (yay, floats)
static int
compare (vec_t a, vec_t b)
{
	vec_t       diff = a - b;
	return diff * diff < 0.001;
}

static int
test_angle (const vec3_t angles)
{
	int         i;
	quat_t      rotation, r;
	vec3_t      scale, shear;
	mat3_t      mat;

	AngleQuat (angles, rotation);
	QuatToMatrix (rotation, mat, 0, 1);
	Mat3Decompose (mat, r, shear, scale);
	for (i = 0; i < 4; i++)
		if (!compare (rotation[i], r[i]))
			goto negate;
	return 1;
negate:
	// Mat3Decompose always sets the rotation quaternion's scalar to +ve
	// but AngleQuat might produce a -ve scalar.
	QuatNegate (r, r);
	for (i = 0; i < 4; i++)
		if (!compare (rotation[i], r[i]))
			goto fail;

	return 1;
fail:
	printf ("\ntest_angle\n");
	printf ("(%g %g %g)\n", VectorExpand (angles));
	printf ("  [%g %g %g %g]\n", QuatExpand (rotation));
	printf ("  [%g %g %g %g] [%g %g %g] [%g %g %g]\n",
			QuatExpand (r), VectorExpand (scale), VectorExpand (shear));
	return 0;
}

static int
test_transform (const vec3_t angles, const vec3_t scale)
{
	int         i;
	const vec3_t v = {4,5,6};
	vec3_t      x, y;
	quat_t      rotation;
	mat3_t      mat;

	VectorCopy (v, x);
	AngleQuat (angles, rotation);
	VectorCompMult (scale, x, x);
	QuatMultVec (rotation, x, x);

	Mat3Init (rotation, scale, mat);
	Mat3MultVec (mat, v, y);

	for (i = 0; i < 3; i++)
		if (!compare (x[i], y[i]))
			goto fail;

	return 1;
fail:
	printf ("\ntest_transform\n");
	printf ("(%g %g %g) (%g %g %g)\n", VectorExpand (angles),
			VectorExpand (scale));
	printf ("  (%g %g %g)\n", VectorExpand (x));
	printf ("  (%g %g %g)\n", VectorExpand (y));
	return 0;
}

static int
test_transform2 (const vec3_t angles, const vec3_t scale)
{
	int         i;
	const vec3_t v = {4,5,6};
	vec3_t      x, y;
	quat_t      rotation;
	mat3_t      mat;
	vec3_t      rot, sc, sh;

	VectorCopy (v, x);
	AngleQuat (angles, rotation);
	VectorCompMult (scale, x, x);
	QuatMultVec (rotation, x, x);

	Mat3Init (rotation, scale, mat);
	Mat3Decompose (mat, rot, sh, sc);

	VectorCopy (v, y);
	QuatMultVec (rot, y, y);
	VectorShear (sh, y, y);
	VectorCompMult (sc, y, y);//scale

	for (i = 0; i < 3; i++)
		if (!compare (x[i], y[i]))
			goto fail;

	return 1;
fail:
	printf ("\ntest_transform2\n");
	printf ("(%g %g %g) (%g %g %g) (%g %g %g)\n",
			VectorExpand (angles), VectorExpand (scale), VectorExpand (v));
	printf ("  (%g %g %g)\n", VectorExpand (x));
	printf ("  (%g %g %g)\n", VectorExpand (y));
	return 0;
}

static int
test_inverse (const vec3_t angles, const vec3_t scale)
{
	int         i;
	quat_t      rotation;
	mat3_t      mat, inv, I, res;

	AngleQuat (angles, rotation);
	Mat3Init (rotation, scale, mat);

	Mat3Identity (I);
	Mat3Inverse (mat, inv);
	Mat3Mult (mat, inv, res);

	for (i = 0; i < 3 * 3; i++)
		if (!compare (I[i], res[i]))
			goto fail;

	return 1;
fail:
	printf ("\ntest_inverse\n");
	printf ("(%g %g %g) (%g %g %g)\n",
			VectorExpand (angles), VectorExpand (scale));
	printf ("  [%g %g %g]\n  [%g %g %g]\n  [%g %g %g]\n\n", Mat3Expand (mat));
	printf ("  [%g %g %g]\n  [%g %g %g]\n  [%g %g %g]\n\n", Mat3Expand (inv));
	printf ("  [%g %g %g]\n  [%g %g %g]\n  [%g %g %g]\n\n", Mat3Expand (res));
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i, j;

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_angle (test_angles[i]))
			res = 1;
	}
	for (i = 0; i < num_angle_tests; i ++) {
		for (j = 0; j < num_scale_tests; j ++) {
			if (!test_transform (test_angles[i], test_scales[j]))
				res = 1;
		}
	}
	for (i = 0; i < num_angle_tests; i ++) {
		for (j = 0; j < num_scale_tests; j ++) {
			if (!test_transform2 (test_angles[i], test_scales[j]))
				res = 1;
		}
	}
	for (i = 0; i < num_angle_tests; i ++) {
		for (j = 0; j < num_scale_tests; j ++) {
			if (!test_inverse (test_angles[i], test_scales[j]))
				res = 1;
		}
	}
	return res;
}
