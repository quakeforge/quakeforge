void printf (string fmt, ...) = #0;
void *obj_malloc (int size) = #0;

typedef struct {
	int         val;
	int         cap;
	int         ofs;
	int         res;
} valstruct_t;

int
printval (int val)
{
	printf ("%d\n", val);
	return val;
}

void
test (valstruct_t *v)
{
	if (v.val == v.cap) {
		v.val += v.ofs;
		v.res = printval (v.val * @sizeof (int));
	}
}

int
main ()
{
	valstruct_t *vs;

	vs = obj_malloc (@sizeof (valstruct_t));
	vs.val = 1;
	vs.cap = 1;
	vs.ofs = 2;
	printf ("before: %d\nafter: ", vs.val);
	test (vs);
	printf ("val vs res: %d %d\n", vs.val, vs.res);
	return vs.val != vs.res;
}
