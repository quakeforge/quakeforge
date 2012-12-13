struct foobar_s {
	float a;
	struct foobar_s *b[2];
	float c;
};
typedef struct foobar_s foobar_t;
typedef foobar_t *foobar_p;

void (foobar_p foo, vector x, integer y, foobar_t z) bar =
{
	foo.a = x.z;
	foo.b[1] = (foobar_p)(integer)x.y;
	z.a = x_z;
	z.c = x.x;
	x.y = z.c;
	x_x = 0;
	x_y = 0;
	x.z = 0;
};
