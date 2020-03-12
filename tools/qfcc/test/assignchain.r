#include "test-harness.h"

typedef struct foo {
	int x;
	float y;
} foo;

int x, y;
int z = 42;
foo bar, baz;
foo foo_init = { 5, 6.25 };

int test_simple_global (void)
{
	int         ret = 0;
	x = y = z;
	if (x != z || y != z) {
		printf ("test_simple_global: x=%d y=%d z=%d\n", x, y, z);
		ret |= 1;
	}
	return ret;
}

int test_struct_global (void)
{
	int         ret = 0;
	bar = baz = foo_init;
	if (bar.x != foo_init.x || bar.y != foo_init.y) {
		printf ("test_struct: bar={%d %g} foo_init={%d %g}\n",
				bar.x, bar.y, foo_init.x, foo_init.y);
		ret |= 1;
	}
	if (baz.x != foo_init.x || baz.y != foo_init.y) {
		printf ("test_struct: baz={%d %g} foo_init={%d %g}\n",
				baz.x, baz.y, foo_init.x, foo_init.y);
		ret |= 1;
	}
	bar = baz = nil;
	if (bar.x || baz.x || bar.y || baz.y) {
		printf ("test_struct: bar={%d %g} baz={%d %g}\n",
				bar.x, bar.y, baz.x, baz.y);
		ret |= 1;
	}
	return ret;
}

int test_simple_pointer (int *x, int *y)
{
	int         ret = 0;
	*x = *y = z;
	if (*x != z || *y != z) {
		printf ("test_simple_global: *x=%d *y=%d z=%d\n", *x, *y, z);
		ret |= 1;
	}
	return ret;
}

int test_struct_pointer (foo *bar, foo *baz)
{
	int         ret = 0;
	*bar = *baz = foo_init;
	if (bar.x != foo_init.x || bar.y != foo_init.y) {
		printf ("test_struct: bar={%d %g} foo_init={%d %g}\n",
				bar.x, bar.y, foo_init.x, foo_init.y);
		ret |= 1;
	}
	if (baz.x != foo_init.x || baz.y != foo_init.y) {
		printf ("test_struct: baz={%d %g} foo_init={%d %g}\n",
				baz.x, baz.y, foo_init.x, foo_init.y);
		ret |= 1;
	}
	*bar = foo_init;
	*baz = foo_init;
	*bar = *baz = nil;
	if (bar.x || baz.x || bar.y || baz.y) {
		printf ("test_struct: bar={%d %g} baz={%d %g}\n",
				bar.x, bar.y, baz.x, baz.y);
		ret |= 1;
	}
	return ret;
}

int main ()
{
	int ret = 0;
	ret |= test_simple_global ();
	ret |= test_struct_global ();
	ret |= test_simple_pointer (&x, &y);
	ret |= test_struct_pointer (&bar, &baz);
	return ret;
}
