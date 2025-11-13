typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
void printf (string str,...)=#0;

typedef PGA.group_mask(0xc) mot_t;

bool fail;

//l:0 '0.707107 -0.707107 0' '1.06066 1.06066 3.53553' 0
//T:1 '0 0 0' '1.43934 1.06066 -0.75' 0
//Tm:-0.707107 '0 0 -0.707107' '1.5 -2.03553 0' 0
//A:0 '0 0 1' '-2.12132 2.87868 0' 0
//set_transform:pc: '0 0 0.923879 0.382683' '-2.87868 -2.12132 1.5 1'
//-0.707106 -0.707107 0 -2.87868
//0.707107 -0.707106 0 -2.12132
//0 0 1 1.5
//0 0 0 1

//var I = 1e0123;
//var eye0 = 1e123;
//var l = 0.707107e23 -0.707107e31 + 1.06066e01 + 1.06066e02 + 3.53553e03;
//var T = 1 + 1.43934e01 + 1.06066e02 -0.75e03;
//var l0 = 1e23;
//var A = 1e12 - 2.12132e01 + 2.87868e02;
//var ll = A l ~A;
//var Tm = ll T l0 ~T;
//var m = 1 + Tm;
//var xx = 1/!((m ~m) I);
//var mag = 0.5411963824870494;
//var s = !(m ^ I);
//var q = (!m ^ I);
//var b = (m - s - q);
//var bt = ~!(b I);
//var bp = b - bt;
//var d = (s q - bt ^ bp) mag mag;
//var dd = m mag;
//var rTm = (m mag + !bt !d - d s);
//var R = rTm T;
//var eye = R eye0 ~R;

motor_t
broken_line (bivec_t l, bivec_t l0, bivec_t A, motor_t T)
{
	//auto Tm = (A * l * ~A) * T * l0 * ~T;
	auto ll = (A * l * ~A);
	//printf ("ll:%g %v %v %g\n", ll.scalar, ll.bvect, ll.bvecp, ll.qvec);
	//printf ("TT:%g %v %v %g\n", TT.scalar, TT.bvect, TT.bvecp, TT.qvec);
	if (fail) return nil;
	//auto TT = T * l0 * ~T;
	//auto Tm = ll * TT;
	auto Tm = ll * (T * l0 * ~T);
	return Tm;
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
		l /= sqrt (l • ~l);
		auto T = sqrt(-eye * eye_0);
		auto A = ((⋆(p0 * e0123) ∧ ⋆(l0 * e0)) • eye) * eye;
		printf ("e:%q\n", eye);
		printf ("t:%q\n", target);
		printf ("l:%g %v %v %g\n", l.scalar, l.bvect, l.bvecp, l.qvec);
		printf ("l0:%g %v %v %g\n", l0.scalar, l0.bvect, l0.bvecp, l0.qvec);
		printf ("T:%g %v %v %g\n", T.scalar, T.bvect, T.bvecp, T.qvec);
		printf ("A:%g %v %v %g\n", A.scalar, A.bvect, A.bvecp, A.qvec);

		auto Tm = broken_line (l, l0, A, T);
		auto R = ~A * sqrt(Tm) * T;
		auto e = R * eye_0 * ~R;
		printf ("Tm:%g %v %v %g\n", Tm.scalar, Tm.bvect, Tm.bvecp, Tm.qvec);
		printf ("R:%g %v %v %g\n", R.scalar, R.bvect, R.bvecp, R.qvec);
		printf ("e:%.9q\n", e);
		auto E = (point_t)'-2.87867975 -2.12132001 1.49999988 1.00000012';
		printf ("E:%.9q\n", E);
		//FIXME conversion of bvec always uses |
		return (e != E) ? 1 : 0;
	}
}
