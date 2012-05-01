#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"

//PITCH YAW ROLL
static vec3_t test_transforms[][2] = {
	{{ 0,  0,  0}, { 0, 0, 0}},
	{{ 0,  0,  0}, { 1, 2, 3}},
	{{45,  0,  0}, { 0, 0, 0}},
	{{45,  0,  0}, {-1, 2, 3}},
	{{ 0, 45,  0}, { 0, 0, 0}},
	{{ 0, 45,  0}, { 1,-2, 3}},
	{{ 0,  0, 45}, { 0, 0, 0}},
	{{ 0,  0, 45}, { 1, 2,-3}},
	{{45, 45,  0}, { 0, 0, 0}},
	{{45, 45,  0}, {-1,-2, 3}},
	{{ 0, 45, 45}, { 0, 0, 0}},
	{{ 0, 45, 45}, {-1, 2,-3}},
	{{45,  0, 45}, { 0, 0, 0}},
	{{45,  0, 45}, { 1,-2,-3}},
	{{45, 45, 45}, { 0, 0, 0}},
	{{45, 45, 45}, {-1,-2,-3}},
};
#define num_transform_tests \
	(sizeof (test_transforms) / sizeof (test_transforms[0]))

// return true if a and b are close enough (yay, floats)
static int
compare (vec_t a, vec_t b)
{
	vec_t       diff = a - b;
	return diff * diff < 0.001;
}

static int
test_transform (const vec3_t angles, const vec3_t translation)
{
	int         i;
	const vec3_t v = {4,5,6};
	vec3_t      x;
	quat_t      rotation;
	DualQuat_t  transform, conj;
	DualQuat_t  inverse, iconj;
	DualQuat_t  vd, xd, ix;
	Dual_t      dual;

	VectorZero (x);
	DualQuatZero (xd);
	DualQuatZero (ix);

	AngleQuat (angles, rotation);
	DualQuatSetVect (v, vd);
	DualQuatRotTrans (rotation, translation, transform);
	DualQuatConjQE (transform, conj);

	DualQuatConjQ (transform, inverse);
	DualQuatConjQE (inverse, iconj);

	DualQuatNorm (vd, dual);
	if (!DualIsUnit (dual)) {
		printf ("dual vector not unit: "
				"[(%g %g %g %g) (%g %g %g %g)] -> [%g %g]\n",
				DualQuatExpand (vd), DualExpand (dual));
		goto fail;
	}

	DualQuatNorm (transform, dual);
	if (!DualIsUnit (dual)) {
		printf ("dual quat not unit: "
				"[(%g %g %g %g) (%g %g %g %g)] -> [%g %g]\n",
				DualQuatExpand (transform), DualExpand (dual));
		goto fail;
	}

	QuatMultVec (rotation, v, x);
	VectorAdd (x, translation, x);

	DualQuatMult (transform, vd, xd);
	DualQuatMult (xd, conj, xd);

	DualQuatMult (inverse, xd, ix);
	DualQuatMult (ix, iconj, ix);

	DualQuatNorm (xd, dual);
	if (!DualIsUnit (dual)) {
		printf ("dual result not unit: "
				"[(%g %g %g %g) (%g %g %g %g)] -> [%g %g]\n",
				DualQuatExpand (xd), DualExpand (dual));
		goto fail;
	}

	DualQuatNorm (ix, dual);
	if (!DualIsUnit (dual)) {
		printf ("dual inverse not unit: "
				"[(%g %g %g %g) (%g %g %g %g)] -> [%g %g]\n",
				DualQuatExpand (ix), DualExpand (dual));
		goto fail;
	}

	for (i = 0; i < 3; i++)
		if (!compare (xd.qe.sv.v[i], x[i]))
			goto fail;
	for (i = 0; i < 3; i++)
		if (!compare (ix.qe.sv.v[i], v[i]))
			goto fail;
	return 1;
fail:
	printf ("\n\n(%g %g %g) (%g %g %g)\n",
			VectorExpand (angles), VectorExpand (translation));
	printf ("  [(%g %g %g %g) (%g %g %g %g)]\n", DualQuatExpand (transform));
	printf ("  [(%g %g %g %g) (%g %g %g %g)]\n", DualQuatExpand (vd));
	printf ("  [(%g %g %g %g) (%g %g %g %g)]\n", DualQuatExpand (conj));
	printf ("  (%g %g %g)\n", VectorExpand (x));
	printf ("  (%g %g %g)\n", VectorExpand (xd.qe.sv.v));
	printf ("  (%g %g %g)\n", VectorExpand (ix.qe.sv.v));
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;

	for (i = 0; i < num_transform_tests; i ++) {
		if (!test_transform (test_transforms[i][0], test_transforms[i][1]))
			res = 1;
	}
	return res;
}
