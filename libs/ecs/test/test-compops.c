#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/ecs.h"

enum {
	test_comp,
};

static int counter = 0;
static void
create_test_comp (void *comp)
{
	*(int *) comp = counter++;
}

static const component_t test_component[] = {
	[test_comp] = {
		.size = sizeof (int),
		.create = create_test_comp,
		.name = "position",
	},
};

static int
test_rotation (const component_t *comp, int *array, uint32_t array_count,
			   uint32_t dstIndex, uint32_t srcIndex, uint32_t count,
			   int *expect)
{
	counter = 0;
	Component_CreateElements (comp, array, 0, array_count);
	for (uint32_t i = 0; i < array_count; i++) {
		if (array[i] != (int) i) {
			printf ("array initialized incorrectly");
			return 1;
		}
	}
	Component_RotateElements (comp, array, dstIndex, srcIndex, count);
	for (uint32_t i = 0; i < array_count; i++) {
		printf ("%d ", expect[i]);
	}
	puts ("");
	for (uint32_t i = 0; i < array_count; i++) {
		printf ("%d ", array[i]);
	}
	puts ("");
	for (uint32_t i = 0; i < array_count; i++) {
		if (array[i] != expect[i]) {
			printf ("array rotated incorrectly");
			return 1;
		}
	}
	return 0;
}

static int expect1[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static int expect2[] = { 0, 1, 6, 7, 8, 2, 3, 4, 5, 9 };
static int expect3[] = { 0, 1, 4, 5, 6, 7, 8, 2, 3, 9 };
static int expect4[] = { 0, 6, 7, 8, 1, 2, 3, 4, 5, 9 };
static int expect5[] = { 0, 4, 5, 6, 7, 8, 1, 2, 3, 9 };

static int *test_array = 0;

int
main (void)
{
	auto comp = &test_component[test_comp];

	Component_ResizeArray (comp, (void **) &test_array, 10);
	if (test_rotation (comp, test_array, 10, 2, 2, 3, expect1))
		return 1;
	if (test_rotation (comp, test_array, 10, 2, 6, 3, expect2))
		return 1;
	if (test_rotation (comp, test_array, 10, 2, 4, 5, expect3))
		return 1;
	if (test_rotation (comp, test_array, 10, 4, 1, 5, expect4))
		return 1;
	if (test_rotation (comp, test_array, 10, 6, 1, 3, expect5))
		return 1;
	return 0;
}
