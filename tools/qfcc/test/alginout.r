typedef @algebra(float(3,0,1)) PGA;
typedef PGA.tvec point_t;

typedef struct foo_s {
	point_t a, b;
} foo_t;

void swap (@inout point_t a, @inout point_t b)
{
	point_t t = a;
	a = b;
	b = t;
}

void check (foo_t *b)
{
	swap (b.a, b.b);
}

int
main ()
{
	foo_t f = {
		.a = '1 0 0 1',
		.b = '0 1 1 0',
	};
	check (&f);
	if ((vec4) f.a != '0 1 1 0' || (vec4) f.b != '1 0 0 1') {
		return 1;
	}
	return 0;
}
