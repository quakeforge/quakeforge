typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;

typedef struct {
	motor_t     M;
	bivector_t  B;
} state_t;

void do_something (state_t *s, motor_t m)
{
	s.M += m;
}

int
main (void)
{
	int ret = 1;

	return ret;
}
