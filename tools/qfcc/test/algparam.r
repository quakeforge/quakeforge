typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;

void printf (string str,...)=#0;

bool broken_param (motor_t T, float s, vec3 bt, vec3 bp, float q);


int main ()
{
	float x = 0.707106781;
	@algebra (PGA) {
		point_t eye = (-5 + 3*x) * e032 + (-3*x) * e013 + 1.5 * e021 + e123;
		point_t eye_0 = e123;
		auto U = -eye * eye_0;
		auto T = (U + U.scalar) / 2;

		return !broken_param(T, U.scalar, (vec3)U.bvect/2,
							 (vec3)U.bvecp/2, (float)U.qvec/2);
	}
	return 1;
}

bool broken_param (motor_t T, float s, vec3 bt, vec3 bp, float q)
{
	printf ("T:%g %v %v %g\n", T.scalar, T.bvect, T.bvecp, T.qvec);
	printf ("e:%g %v %v %g\n", s, bt, bp, q);
	return T.scalar == s && (vec3)T.bvect == bt && (vec3)T.bvecp == bp && (float)T.qvec == q;
}
