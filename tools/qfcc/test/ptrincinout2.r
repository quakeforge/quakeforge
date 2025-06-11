#include "test-harness.h"

static int
func (@inout void *ptr)
{
	auto p = (int *)ptr;
	int v = *p++;
	ptr = p;
	return v;
}

int data[] = {1, 2, 3};
int dest[countof(data)];

void
set_dest (int i, int v)
{
	dest[i] = v + 1;
}

int
main ()
{
	int *ptr = data;
	for (int i = 0; i < countof (data); i++) {
		set_dest(i, func(ptr));
	}
	printf ("ptr - data: %d\n", ptr - data);
	bool fail = false;
	if (ptr - data != countof (data)) {
		fail = true;
	}
	for (int i = 0; i < countof (data); i++) {
		printf ("%d %d %d\n", i, data[i], dest[i]);
		if (dest[i] != data[i] + 1) {
			fail = true;
		}
	}
	return fail & 1;
}
