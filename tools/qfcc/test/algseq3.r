typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.vec plane_t;

motor_t normalize (motor_t m) { return m; }
void keep_live (void *x) {}

motor_t foo (bivec_t l, motor_t R, motor_t Rm)
{
	auto L = Rm * R;
	L = ~l * L;
	L = normalize(L);
	return L;
}

void garbage ()
{
	int x[128];
	for (int i = 0; i < countof (x); i++) {
		x[i] = 0x7fffffff;	// a nan
	}
	keep_live (x);
}

int main()
{
	bivec_t l = nil;
	bivec_t R = nil;
	bivec_t Rm = nil;
	garbage ();
	auto M = foo (l, R, Rm);
	if (M.scalar != M.scalar) {
		return 1;
	}
	return 0;
}
