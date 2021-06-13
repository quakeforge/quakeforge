#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/ui/vrect.h"

#define VR(x,y,w,h) {x,y,w,h,0}
#define VRn(x,y,w,h,n) {x,y,w,h,n}

static vrect_t de_0_1 = VR (0,0,2,1);
static vrect_t de_0_0 = VRn(0,1,1,1,&de_0_1);
static vrect_t de_1_2 = VR (0,0,5,1);
static vrect_t de_1_1 = VRn(0,1,1,1,&de_1_2);
static vrect_t de_1_0 = VRn(4,1,1,1,&de_1_1);
static vrect_t de_2_0 = VR (2,0,2,1);
static vrect_t de_3_1 = VR (3,0,2,1);
static vrect_t de_3_0 = VRn(4,1,1,1,&de_3_1);

static vrect_t de_4_2 = VR (0,0,2,1);
static vrect_t de_4_1 = VRn(0,4,2,1,&de_4_2);
static vrect_t de_4_0 = VRn(0,1,1,3,&de_4_1);
static vrect_t de_5_3 = VR (0,0,5,1);
static vrect_t de_5_2 = VRn(0,4,5,1,&de_5_3);
static vrect_t de_5_1 = VRn(0,1,1,3,&de_5_2);
static vrect_t de_5_0 = VRn(4,1,1,3,&de_5_1);
static vrect_t de_6_1 = VR (2,0,2,1);
static vrect_t de_6_0 = VRn(2,4,2,1,&de_6_1);
static vrect_t de_7_2 = VR (3,0,2,1);
static vrect_t de_7_1 = VRn(3,4,2,1,&de_7_2);
static vrect_t de_7_0 = VRn(4,1,1,3,&de_7_1);

static vrect_t de_8_0 = VR (0,2,1,2);
static vrect_t de_9_1 = VR (0,2,1,2);
static vrect_t de_9_0 = VRn(4,2,1,2,&de_9_1);
static vrect_t de_11_0 = VR (4,2,1,2);

static vrect_t de_12_1 = VR (0,4,2,1);
static vrect_t de_12_0 = VRn(0,3,1,1,&de_12_1);
static vrect_t de_13_2 = VR (0,4,5,1);
static vrect_t de_13_1 = VRn(0,3,1,1,&de_13_2);
static vrect_t de_13_0 = VRn(4,3,1,1,&de_13_1);
static vrect_t de_14_0 = VR (2,4,2,1);
static vrect_t de_15_1 = VR (3,4,2,1);
static vrect_t de_15_0 = VRn(4,3,1,1,&de_15_1);

static vrect_t de_16_0 = VR (4,4,2,2);

struct {
	vrect_t    *(*func)(const vrect_t *r1, const vrect_t *r2);
	vrect_t     r1;
	vrect_t     r2;
	vrect_t    *expect;
	vrect_t     e;
	int         use_e;
} tests [] = {
	{VRect_Intersect, VR (0,0,2,2), VR (1,1,3,3), 0, VR (1,1,1,1), 1},
	{VRect_Intersect, VR (0,0,5,2), VR (1,1,3,3), 0, VR (1,1,3,1), 1},
	{VRect_Intersect, VR (2,0,2,2), VR (1,1,3,3), 0, VR (2,1,2,1), 1},
	{VRect_Intersect, VR (3,0,2,2), VR (1,1,3,3), 0, VR (3,1,1,1), 1},

	{VRect_Intersect, VR (0,0,2,5), VR (1,1,3,3), 0, VR (1,1,1,3), 1},
	{VRect_Intersect, VR (0,0,5,5), VR (1,1,3,3), 0, VR (1,1,3,3), 1},
	{VRect_Intersect, VR (2,0,2,5), VR (1,1,3,3), 0, VR (2,1,2,3), 1},
	{VRect_Intersect, VR (3,0,2,5), VR (1,1,3,3), 0, VR (3,1,1,3), 1},

	{VRect_Intersect, VR (0,2,2,2), VR (1,1,3,3), 0, VR (1,2,1,2), 1},
	{VRect_Intersect, VR (0,2,5,2), VR (1,1,3,3), 0, VR (1,2,3,2), 1},
	{VRect_Intersect, VR (2,2,2,2), VR (1,1,3,3), 0, VR (2,2,2,2), 1},
	{VRect_Intersect, VR (3,2,2,2), VR (1,1,3,3), 0, VR (3,2,1,2), 1},

	{VRect_Intersect, VR (0,3,2,2), VR (1,1,3,3), 0, VR (1,3,1,1), 1},
	{VRect_Intersect, VR (0,3,5,2), VR (1,1,3,3), 0, VR (1,3,3,1), 1},
	{VRect_Intersect, VR (2,3,2,2), VR (1,1,3,3), 0, VR (2,3,2,1), 1},
	{VRect_Intersect, VR (3,3,2,2), VR (1,1,3,3), 0, VR (3,3,1,1), 1},

	{VRect_Difference, VR (0,0,2,2), VR (1,1,3,3), &de_0_0},
	{VRect_Difference, VR (0,0,5,2), VR (1,1,3,3), &de_1_0},
	{VRect_Difference, VR (2,0,2,2), VR (1,1,3,3), &de_2_0},
	{VRect_Difference, VR (3,0,2,2), VR (1,1,3,3), &de_3_0},

	{VRect_Difference, VR (0,0,2,5), VR (1,1,3,3), &de_4_0},
	{VRect_Difference, VR (0,0,5,5), VR (1,1,3,3), &de_5_0},
	{VRect_Difference, VR (2,0,2,5), VR (1,1,3,3), &de_6_0},
	{VRect_Difference, VR (3,0,2,5), VR (1,1,3,3), &de_7_0},

	{VRect_Difference, VR (0,2,2,2), VR (1,1,3,3), &de_8_0},
	{VRect_Difference, VR (0,2,5,2), VR (1,1,3,3), &de_9_0},
	{VRect_Difference, VR (2,2,2,2), VR (1,1,3,3), 0},
	{VRect_Difference, VR (3,2,2,2), VR (1,1,3,3), &de_11_0},

	{VRect_Difference, VR (0,3,2,2), VR (1,1,3,3), &de_12_0},
	{VRect_Difference, VR (0,3,5,2), VR (1,1,3,3), &de_13_0},
	{VRect_Difference, VR (2,3,2,2), VR (1,1,3,3), &de_14_0},
	{VRect_Difference, VR (3,3,2,2), VR (1,1,3,3), &de_15_0},

	{VRect_Difference, VR (4,4,2,2), VR (1,1,3,3), &de_16_0},

	{VRect_Union, VR (0, 0, 0, 0), VR (1, 1, -1, 1), 0, VR (1, 1, 1, -1), 1},
	{VRect_Union, VR (0, 0, 0, 0), VR (1, 1, 3, 3), 0, VR (1, 1, 3, 3), 1},
	{VRect_Union, VR (0, 0, 2, 5), VR (1, 1, -1, 1), 0, VR (0, 0, 2, 5), 1},
	{VRect_Union, VR (0, 0, 2, 5), VR (1, 1, 3, 3), 0, VR (0, 0, 4, 5), 1},

	{VRect_Merge, VR (0, 0, 0, 0), VR (1, 1, -1, 1), 0},
	{VRect_Merge, VR (0, 0, 0, 0), VR (1, 1, 3, 3), 0, VR (1, 1, 3, 3), 1},
	{VRect_Merge, VR (0, 0, 2, 5), VR (1, 1, -1, 1), 0, VR (0, 0, 2, 5), 1},
	{VRect_Merge, VR (0, 0, 2, 5), VR (1, 1, 3, 3), 0},

	{VRect_Merge, VR (0,0,2,2), VR (1,1,3,3), 0},
	{VRect_Merge, VR (0,0,5,2), VR (1,1,3,3), 0},
	{VRect_Merge, VR (2,0,2,2), VR (1,1,3,3), 0},
	{VRect_Merge, VR (3,0,2,2), VR (1,1,3,3), 0},

	{VRect_Merge, VR (0,0,4,1), VR (1,1,3,3), 0},
	{VRect_Merge, VR (1,-1,3,1), VR (1,1,3,3), 0},
	{VRect_Merge, VR (0,4,4,1), VR (1,1,3,3), 0},
	{VRect_Merge, VR (1,5,3,1), VR (1,1,3,3), 0},

	{VRect_Merge, VR (1,0,3,1), VR (1,1,3,3), 0, VR (1,0,3,4), 1},
	{VRect_Merge, VR (1,4,3,1), VR (1,1,3,3), 0, VR (1,1,3,4), 1},
	{VRect_Merge, VR (0,1,1,3), VR (1,1,3,3), 0, VR (0,1,4,3), 1},
	{VRect_Merge, VR (4,1,1,3), VR (1,1,3,3), 0, VR (1,1,4,3), 1},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

static void
print_rects (vrect_t *rect)
{
	while (rect) {
		printf ("[%d, %d, %d, %d]",
				rect->x, rect->y, rect->width, rect->height);
		rect = rect->next;
	}
	printf ("\n");
}

static __attribute__((pure)) int
compare_rects (vrect_t *r1, vrect_t *r2)
{
	if (!r1 && !r2)
		return 1;
	if (!r1 || !r2)
		return 0;
	// when both rects are empty, their exact values don't matter.
	if (VRect_IsEmpty (r1) && VRect_IsEmpty (r2))
		return 1;
	if (r1->x != r2->x || r1->y != r2->y
		|| r1->width != r2->width || r1->height != r2->height)
		return 0;
	return compare_rects (r1->next, r2->next);
}

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_tests; i++) {
		vrect_t    *r, *e;

		e = tests[i].use_e ? &tests[i].e : tests[i].expect;
		r = tests[i].func(&tests[i].r1, &tests[i].r2);
		if (!compare_rects (r, e)) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: ");
			print_rects (e);
			printf ("got   : ");
			print_rects (r);
		}
		while (r) {
			vrect_t    *t = r->next;
			VRect_Delete (r);
			r = t;
		}
	}
	return res;
}
