int foo[128];
@algebra(float) pgaf1;
//@algebra(double) pgad1;
//@algebra(float(3)) vgaf;
//@algebra(double(3)) vgad;
//@algebra(float(3,0,1)) pgaf2;
//@algebra(double(3,0,1)) pgad2;
//@algebra(float(0,1)) complexf;
//@algebra(double(0,1)) complexd;
typedef @algebra(float(3,0,1)) PGA;
typedef @algebra(float(4,1)) CGA;

typedef @algebra(double(2,0,1)) PGA2;
PGA2 pga2;

float sin(float x) = #0;

int
main (void)
{
	@algebra (PGA) {
		auto p1 = 3*e1 + e2 - e3 + e0;
		auto p2 = e1 + 3*e2 + e3 - e0;
		auto v = 4*(e1 + e032 + e123);
		pgaf1 = p1 + v * p2;
//		pgaf1 = (p1 + v)∧p2;
//		pgaf1 = v • p2;
#if 0
		auto rx = e23;
		auto ry = e31;
		auto rz = e12;
		auto tx = e01;
		auto ty = e02;
		auto tz = e03;
		auto x = e032;
		auto y = e013;
		auto z = e021;
		auto w = e123;
		auto mvec = rx + ry;
#endif
	}
	@algebra (PGA2) {
		auto l1 = e1 + 2 * e2 + 5 * e0;
		auto l2 = 3 * e1 - e2 + 10 * e0;
		auto p = l1∧l2;
//		pga2 = p + (1 + p)∧l1;
		pga2 = (l1 • p)*l1;
	}
	return 0;		// to survive and prevail :)
}
