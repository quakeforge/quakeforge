struct foobar_s = {
	integer a;
	(struct foobar_s [])[4]b;
};
typedef struct foobar_s foobar_t;
typedef foobar_t [] foobar_p;

void (foobar_p foo, vector x, integer y) bar =
{
	foo.a = x.z;
	foo.b[1] = (foobar_p)(integer)x.y;
};
