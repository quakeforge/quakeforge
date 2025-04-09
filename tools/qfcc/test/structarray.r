#pragma bug die
void printf (string fmt, ...) = #0;
void *obj_malloc (int size) = #0;

typedef struct {
	int         val;
	int         cap;
	int         ofs[];
} valstruct_t;

int foo[] = {1, 2, 3, 4, 5};
valstruct_t *vs;
int dst;

int
main ()
{
	vs = (valstruct_t *)foo;
	for (int i = 0; i < 2; i++) {
		dst = vs.ofs[i];
	}
	printf("dst = %d\n", dst);
	return sizeof(foo) != 5 || dst != 4;
}
