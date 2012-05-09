#ifdef HAVE_CONFIG_H
# include "config.h"
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

static vec3_t test_translations[] = {
	{ 0, 0, 0},
	{ 1, 2, 3},
	{-1, 2, 3},
	{ 1,-2, 3},
	{ 1, 2,-3},
	{-1,-2, 3},
	{-1, 2,-3},
	{ 1,-2,-3},
	{-1,-2,-3},
};
#define num_translation_tests \
	(sizeof (test_translations) / sizeof (test_translations[0]))

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
#define num_translation_tests \
	(sizeof (test_translations) / sizeof (test_translations[0]))

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
	vec3_t      scale, shear, trans;
	mat4_t      mat;

	AngleQuat (angles, rotation);
	QuatToMatrix (rotation, mat, 1, 1);
	Mat4Decompose (mat, r, scale, shear, trans);
	for (i = 0; i < 4; i++)
		if (!compare (rotation[i], r[i]))
			goto negate;
	return 1;
negate:
	// Mat4Decompose always sets the rotation quaternion's scalar to +ve
	// but AngleQuat might produce a -ve scalar.
	QuatNegate (r, r);
	for (i = 0; i < 4; i++)
		if (!compare (rotation[i], r[i]))
			goto fail;

	return 1;
fail:
	printf ("\n\n(%g %g %g)\n", VectorExpand (angles));
	printf ("  [%g %g %g %g]\n", QuatExpand (rotation));
	printf ("  [%g %g %g %g] [%g %g %g] [%g %g %g] [%g %g %g]\n",
			QuatExpand (r), VectorExpand (scale), VectorExpand (shear),
			VectorExpand (trans));
	return 0;
}

static int
test_transform (const vec3_t angles, const vec3_t scale,
				const vec3_t translation)
{
	int         i;
	const vec3_t v = {4,5,6};
	vec3_t      x, y;
	quat_t      rotation;
	mat4_t      mat;

	VectorCopy (v, x);
	AngleQuat (angles, rotation);
	VectorCompMult (scale, x, x);
	QuatMultVec (rotation, x, x);
	VectorAdd (x, translation, x);

	Mat4Init (rotation, scale, translation, mat);
	Mat4MultVec (mat, v, y);

	for (i = 0; i < 3; i++)
		if (!compare (x[i], y[i]))
			goto fail;

	return 1;
fail:
	printf ("\n\n(%g %g %g) (%g %g %g) (%g %g %g)\n", VectorExpand (angles),
			VectorExpand (scale), VectorExpand (translation));
	printf ("  (%g %g %g)\n", VectorExpand (x));
	printf ("  (%g %g %g)\n", VectorExpand (y));
	return 0;
}

static int
test_transform2 (const vec3_t angles, const vec3_t scale,
				 const vec3_t translation)
{
	int         i;
	const vec3_t v = {4,5,6};
	vec3_t      x, y;
	quat_t      rotation;
	mat4_t      mat;
	vec3_t      rot, sc, sh, tr;

	VectorCopy (v, x);
	AngleQuat (angles, rotation);
	VectorCompMult (scale, x, x);
	QuatMultVec (rotation, x, x);
	VectorAdd (translation, x, x);

	Mat4Init (rotation, scale, translation, mat);
	Mat4Decompose (mat, rot, sc, sh,  tr);

	VectorCopy (v, y);
	QuatMultVec (rot, y, y);
	VectorShear (sh, y, y);
	VectorCompMult (sc, y, y);//scale
	VectorAdd (tr, y, y);

	for (i = 0; i < 3; i++)
		if (!compare (x[i], y[i]))
			goto fail;

	return 1;
fail:
	printf ("\n\n(%g %g %g) (%g %g %g) (%g %g %g) (%g %g %g)\n",
			VectorExpand (angles), VectorExpand (scale),
			VectorExpand (translation), VectorExpand (v));
	printf ("  (%g %g %g)\n", VectorExpand (x));
	printf ("  (%g %g %g)\n", VectorExpand (y));
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i, j, k;

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_angle (test_angles[i]))
			res = 1;
	}
	for (i = 0; i < num_angle_tests; i ++) {
		for (j = 0; j < num_translation_tests; j ++) {
			for (k = 0; k < num_translation_tests; k ++) {
				if (!test_transform (test_angles[i], test_scales[j],
									 test_translations[k]))
					res = 1;
			}
		}
	}
	for (i = 0; i < num_angle_tests; i ++) {
		for (j = 0; j < num_translation_tests; j ++) {
			for (k = 0; k < num_translation_tests; k ++) {
				if (!test_transform2 (test_angles[i], test_scales[j],
									  test_translations[k]))
					res = 1;
			}
		}
	}
	return res;
}
