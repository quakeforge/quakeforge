typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;

@overload float sqrt(float x) = #0;
void printf (string fmt, ...) = #0;

plane_t broken_plane (point_t eye, point_t target, bivector_t l)
{
	//printf ("e:%q\n", eye);
	//printf ("t:%q\n", target);
	@algebra (PGA) {
		auto p = (eye ∨ target ∨ e021);
		l /= sqrt (l • ~l);
		p /= sqrt (p • p);
		//printf ("l:%v %v\n", l.bvect, l.bvecp);
		return p;
	}
}

int
main ()
{
	float x = sqrt(0.5f);
	@algebra (PGA) {
		point_t eye = -5 * e032 + (-3*x) * e013 + (1.5 + 3*x) * e021 + e123;
		point_t target = -5 * e032 + 1.5 * e021 + e123;
		auto l = -(eye ∨ target);
		auto p = broken_plane (eye, target, l);
		printf ("p:%q\n", p);
		//FIXME conversion of bvec always uses |
		return ((vec4)p != '1 0 0 5') ? 1 : 0;
	}
}
