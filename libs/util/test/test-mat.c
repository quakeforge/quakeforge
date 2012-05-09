#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"

//PITCH YAW ROLL
static vec3_t test_angles[] = {
	{ 0,  0,  0},
	{ 0,  0,  0},
	{45,  0,  0},
	{45,  0,  0},
	{ 0, 45,  0},
	{ 0, 45,  0},
	{ 0,  0, 45},
	{ 0,  0, 45},
	{45, 45,  0},
	{45, 45,  0},
	{ 0, 45, 45},
	{ 0, 45, 45},
	{45,  0, 45},
	{45,  0, 45},
	{45, 45, 45},
	{45, 45, 45},
	{0, 180, 180},
	{180, 0, 180},
	{180, 180, 0},
};
#define num_angle_tests \
	(sizeof (test_angles) / sizeof (test_angles[0]))

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

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;

	for (i = 0; i < num_angle_tests; i ++) {
		if (!test_angle (test_angles[i]))
			res = 1;
	}
	return res;
}
