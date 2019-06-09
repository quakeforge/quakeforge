void printf (string fmt, ...) = #0;
void *obj_malloc (int size) = #0;

typedef struct {
	int         val;
	int         cap;
	int         ofs[];
} valstruct_t;

valstruct_t *vs;
int dst;

int
main ()
{
	for (int i = 0; i < 2; i++) {
		dst = vs.ofs[i];
	}
	return 1;
}
