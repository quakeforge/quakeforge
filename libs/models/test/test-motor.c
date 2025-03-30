#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/qfmodel.h"
#include "QF/sys.h"

static void
set_matrix (mat4f_t mat, const vec3_t trans, vec4f_t rot, const vec3_t scale)
{
	mat4fquat (mat, rot);
	VectorCompMult (mat, mat, scale);
	mat[3] = loadvec3f (trans) + (vec4f_t) { 0, 0, 0, 1 };
}

static bool
compare (float a, float b, int bits)
{
	int32_t mask = ~((1 << bits) - 1);
	int32_t ia = *(int32_t *) &a;
	int32_t ib = *(int32_t *) &b;
	return ((ia - ib) & mask) == 0;
}

static bool
test_motor (qfm_joint_t joint)
{
	joint.rotate = normalf (joint.rotate);

	mat4f_t jmat;
	set_matrix (jmat, joint.translate, joint.rotate, joint.scale);

	auto motor = qfm_make_motor (joint);
	mat4f_t mmat;
	qfm_motor_to_mat (mmat, motor);

	printf ("j: " VectorFMT " " VEC4F_FMT " " VectorFMT "\n",
			VectorExpand (joint.translate), VEC4_EXP (joint.rotate),
			VectorExpand (joint.scale));
	printf ("m: " VEC4F_FMT " " VEC4F_FMT " " VectorFMT "\n",
			VEC4_EXP (motor.q), VEC4_EXP (motor.t), VectorExpand (motor.s));

	printf ("jmat%*smmat\n", 38, "");
	auto jstr = dstring_new ();
	auto mstr = dstring_new ();
	bool pass = true;
	for (int i = 0; i < 4; i++) {
		dsprintf (jstr, "[");
		dsprintf (mstr, "[");
		for (int j = 0; j < 4; j++) {
			const char *sep = j ? "," : "";
			bool ok = compare (jmat[j][i], mmat[j][i], 2);
			const char *col = ok ? GRN : RED;
			pass &= ok;
			dasprintf (jstr, "%s%s%9.6f%s", sep, col, jmat[j][i], DFL);
			dasprintf (mstr, "%s%s%9.6f%s", sep, col, mmat[j][i], DFL);
		}
		dasprintf (jstr, "]");
		dasprintf (mstr, "]");
		printf ("%-40s %s\n", jstr->str, mstr->str);
	}
	dstring_delete (jstr);
	dstring_delete (mstr);
	return pass;
}

static qfm_joint_t joints[] = {
	{ .translate = {1, 2, 3}, .rotate = {0, 0, 0, 1}, .scale = {1, 1, 1} },

	{ .translate = {1, 2, 3}, .rotate = {0, 0, 1, 0}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {0, 1, 0, 0}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {1, 0, 0, 0}, .scale = {1, 1, 1} },

	{ .translate = {1, 2, 3}, .rotate = {0, 0, 1, 1}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {0, 1, 0, 1}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {1, 0, 0, 1}, .scale = {1, 1, 1} },

	{ .translate = {1, 2, 3}, .rotate = {0, 1, 1, 1}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {1, 1, 0, 1}, .scale = {1, 1, 1} },
	{ .translate = {1, 2, 3}, .rotate = {1, 0, 1, 1}, .scale = {1, 1, 1} },

	{ .translate = {1, 2, 3}, .rotate = {1, 1, 1, 1}, .scale = {1, 1, 1} },
};

int
main ()
{
	bool pass = true;
	for (size_t i = 0; i < countof (joints); i++) {
		pass &= test_motor (joints[i]);
	}
	return !pass;
}
