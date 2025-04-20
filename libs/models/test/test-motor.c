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
	if (fabs (a - b) < 1e-6) {
		return true;
	}
	typedef union {
		float   f;
		uint32_t i;
	} fi_t;
	fi_t ia = { .f = a };
	fi_t ib = { .f = b };
	uint32_t mask = ~((1 << bits) - 1);
	uint32_t diff;
	if (ia.i > ib.i) {
		diff = ia.i - ib.i;
	} else {
		diff = ib.i - ia.i;
	}
	printf ("%08x %08x %08x %08x\n", mask, diff, ia.i, ib.i);
	return ((diff) & mask) == 0;
}

static bool
check_mats (mat4f_t jmat, mat4f_t mmat, int bits)
{
	printf ("jmat%*smmat\n", 38, "");
	auto jstr = dstring_new ();
	auto mstr = dstring_new ();
	bool pass = true;
	for (int i = 0; i < 4; i++) {
		dsprintf (jstr, "[");
		dsprintf (mstr, "[");
		for (int j = 0; j < 4; j++) {
			const char *sep = j ? "," : "";
			bool ok = compare (jmat[j][i], mmat[j][i], bits);
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

	return check_mats (jmat, mmat, 2);
}

static bool
test_chain (const qfm_joint_t *chain, int chain_len)
{
	qfm_joint_t joints[chain_len];
	for (int i = 0; i < chain_len; i++) {
		// due to how the data was obtained, the incoming chain is reversed
		// (root is last)
		joints[i] = chain[chain_len - i - 1];
	}
	qfm_motor_t motors[chain_len];
	mat4f_t motor_mats[chain_len];
	mat4f_t joint_mats[chain_len];
	for (int i = 0; i < chain_len; i++) {
		joints[i].rotate = normalf (joints[i].rotate);
		motors[i] = qfm_make_motor (joints[i]);
		qfm_motor_to_mat (motor_mats[i], motors[i]);
		set_matrix (joint_mats[i],
					joints[i].translate, joints[i].rotate, joints[i].scale);
		if (i > 0) {
			motors[i] = qfm_motor_mul (motors[i - 1], motors[i]);
			qfm_motor_to_mat (motor_mats[i], motors[i]);
			mmulf (joint_mats[i], joint_mats[i - 1], joint_mats[i]);
		}
	}

	bool pass = true;
	for (int i = 0; i < chain_len; i++) {
		pass &= check_mats (joint_mats[i], motor_mats[i], 12);
	}
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

	{ .translate = {0, 0,-1},
	  .rotate = {-0.707106829, -0, -0, -0.707106829},
	  .scale = {1, 1, 1}
	},
	{ .translate = {0, 2, 0},
	  .rotate = {-0.707106888, -0.243210375, -0.707106829, -1},
	  .scale = {1, 1, 1},
	},
	{ .translate = {-1.78813934e-07, 1.99999964, -3.35276127e-08},
	  .rotate = {-0.13106361,-1.09332357e-08,-0.399508953,-1},
	  .scale = {1, 1, 1},
	},
};

static qfm_joint_t chain_joints[] = {
	{ .translate = {-7.04858394e-12, 0.129213497, -1.48989443e-08},
	  .rotate = {-0.436430037, 0.175517023, -0.0877642334, -0.878077865},
	  //.scale = {0.999649048, 0.971740901, 1.03048599}
	  .scale = {1, 1, 1}
	},
	{ .translate = {2.23605525e-08, 0.129213542, -4.470985e-08},
	  .rotate = {-7.60717285e-08, 0.173551738, -9.35314119e-08, -0.984822154},
	  .scale = {1, 1, 1}
	},
	{ .translate = {0.00694327988, 0.172899857, -0.0175056811},
	  .rotate = {0.0997654796, -0.709096134, 0.4472965, -0.535878003},
	  //.scale = {1.01577735, 0.969177067, 1.01577735}
	  .scale = {1, 1, 1}
	},
	{ .translate = {0.0158015266, 0.118177742, 0.0674064159},
	  .rotate = {0.551790953, 0.413088262, 0.45563665, -0.563272595},
	  //.scale = {1.00009155, 1.00009155, 0.999816895}
	  .scale = {1, 1, 1}
	},
	{ .translate = {-0.0412524939, -0.105564594, 0.852893293},
	  .rotate = {-0.877423167, 0.216562629, 0.181933701, -0.387447715},
	  //.scale = {0.999908447, 1.00018311, 0.999908447}
	  .scale = {1, 1, 1}
	},
};

int
main ()
{
	bool pass = true;
	for (size_t i = 0; i < countof (joints); i++) {
		pass &= test_motor (joints[i]);
	}
	printf ("\n\n");
	pass &= test_chain (chain_joints, countof (chain_joints));
	return !pass;
}
