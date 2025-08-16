typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;
void printf (string str,...)=#0;

motor_t
broken_plane (plane_t p, plane_t p0, motor_t R)
{
	//auto pp = (R * p0 * ~R).vec;
	//auto Rm = p * pp;
	auto Rm = p * (R * p0 * ~R);
	return Rm;
}

@overload float (float x) sqrt = #0;

motor_t
normalize (motor_t m)
{
	printf ("m: %g %v %v %g\n", m.scalar, m.bvect, m.bvecp, m.qvec);
	auto mag2 = 1 / (m * ~m).scalar;
	auto mag = sqrt(mag2);
	auto d = (m.scalar * m.qvec - m.bvect ∧ m.bvecp) * mag2;
	printf ("d: %g\n", d);
	m *= mag;
	m.bvecp += ⋆m.bvect * ⋆d;
	m.qvec -= d * m.scalar;
	printf ("m: %g %v %v %g\n", m.scalar, m.bvect, m.bvecp, m.qvec);
	return m;
}

motor_t
sqrt (motor_t m)
{
	return normalize (1 + m);
}

PGA.group_mask(0xc)
sqrt (PGA.group_mask(0xc) x)
{
	return (x + x.scalar) / 2;
}

int
main ()
{
	float x = sqrt(0.5f);
	@algebra (PGA) {
		point_t eye = (-5 + 3*x) * e032 + (-3*x) * e013 + 1.5 * e021 + e123;
		point_t target = -5 * e032 + 1.5 * e021 + e123;
		point_t eye_0 = e123;
		point_t eye_fwd = e032;
		point_t eye_up = e021;
		auto p0 = (eye_0 ∨ eye_fwd ∨ eye_up);
		auto l0 = (eye_0 ∨ eye_fwd);
		auto l = -(eye ∨ target);
		auto p = (eye ∨ target ∨ e021);
		l /= sqrt (l • ~l);
		p /= sqrt (p • p);
		auto T = sqrt(-eye * eye_0);
		auto A = ((⋆(p0 * e0123) ∧ ⋆(l0 * e0)) • eye) * eye;
		printf ("e:%q\n", eye);
		printf ("t:%q\n", target);
		printf ("l:%g %v %v %g\n", l.scalar, l.bvect, l.bvecp, l.qvec);
		printf ("l0:%g %v %v %g\n", l0.scalar, l0.bvect, l0.bvecp, l0.qvec);
		printf ("T:%g %v %v %g\n", T.scalar, T.bvect, T.bvecp, T.qvec);
		printf ("A:%g %v %v %g\n", A.scalar, A.bvect, A.bvecp, A.qvec);

		auto Tm = (A * l * ~A) * T * l0 * ~T;
		auto R = ~A * sqrt(Tm) * T;
		auto Rm = broken_plane (p, p0, R);
		auto L = sqrt(Rm) * R;
		auto e = L * eye_0 * ~L;
		printf ("Rm:%g %v %v %g\n", Rm.scalar, Rm.bvect, Rm.bvecp, Rm.qvec);
		printf ("L:%g %v %v %g\n", L.scalar, L.bvect, L.bvecp, L.qvec);
		printf ("e:%.9q\n", e);
		return e != (point_t)'-2.87867999 -2.12132025 1.5 1.00000012';
	}
}
