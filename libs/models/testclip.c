#include "world.h"

dclipnode_t clipnodes1[] = {
	{  0, { 1, -2}},
	{  1, {-2,  2}},
	{  2, {-2,  3}},
	{  3, { 4, -2}},
	{  4, { 5,  6}},
	{  5, {-2, -1}},
	{  6, { 7,  8}},
	{  7, {-1, -2}},
	{  8, { 9, 13}},
	{  9, {10, -1}},
	{ 10, {-1, 11}},
	{ 11, {12, -2}},
	{ 12, {-2, -1}},
	{ 13, {-1, -2}},
};

mplane_t planes1[] = {
	{{0, 1, 0}, -96, 1, 0},	//  0	wall at right
	{{0, 1, 0},  96, 1, 0},	//  1   wall at left (with ditch)
	{{1, 0, 0}, 128, 0, 0},	//  2   wall at front (with shelves)
	{{1, 0, 0}, -32, 0, 0},	//  3   wall at back
	{{0, 0, 1},  32, 2, 0},	//  4   top of shelves
	{{0, 0, 1}, 104, 2, 0},	//  5   ceiling
	{{0, 1, 0},  32, 1, 0},	//  6   edge of ditch
	{{0, 0, 1}, -24, 2, 0},	//  7   floor of ditch
	{{0, 0, 1},  24, 2, 0},	//  8   bottom of selves
	{{1, 0, 0},  64, 0, 0},	//  9   end of shelves
	{{0, 1, 0}, -16, 1, 0},	// 10   ditch side of 2nd shelf
	{{0, 1, 0}, -64, 1, 0},	// 11   ditch side of 1st shelf
	{{0, 1, 0}, -48, 1, 0},	// 12   1st shelf side of 2nd shelf
	{{0, 0, 1},   8, 2, 0},	// 13   main floor
};

hull_t hull1 = {
	clipnodes1,
	planes1,
	0,
	13,
	{0, 0, 0},
	{0, 0, 0},
};

dclipnode_t clipnodes2[] = {
	{0, {1, 12}},	// 0
	{1, {-1, 2}},	// 1
	{2, {3, 6}},	// 2
	{3, {-1, 4}},	// 3
	{4, {5, -1}},	// 4
	{5, {-1, -2}},	// 5
	{6, {7, 9}},	// 6
	{7, {-1, 8}},	// 7
	{4, {-2, -1}},	// 8
	{8, {10, -1}},	// 9
	{5, {-1, 11}},	// 10
	{4, {-2, -1}},	// 11
	{9, {13, 25}},	// 12
	{2, {-1, 14}},	// 13
	{6, {15, 19}},	// 14
	{10, {-1, 16}},	// 15
	{11, {17, 18}},	// 16
	{7, {-1, -2}},	// 17
	{4, {-2, -1}},	// 18
	{11, {20, 23}},	// 19
	{12, {-1, 21}},	// 20
	{8, {22, -1}},	// 21
	{5, {-1, -2}},	// 22
	{8, {24, -1}},	// 23
	{4, {-2, -1}},	// 24
	{3, {-1, 26}},	// 25
	{8, {27, -1}},	// 26
	{11, {28, -1}},	// 27
	{12, {-1, 29}},	// 28
	{13, {-2, -1}},	// 29
};

mplane_t planes2[] = {
	{{1, 0, 0}, 912, 0, 0, {0, 0}},						// 0	0
	{{1, 0, 0}, 1168, 0, 0, {0, 0}},					// 1	1
	{{0, 1, 0}, 2112, 1, 0, {0, 0}},					// 2	2 13
	{{0, 1, 0}, 2128, 1, 0, {0, 0}},					// 3	3 25
	{{0.229038, 0, 0.973417}, 18.3218, 5, 0, {0, 0}},	// 4	4 8 11 18 24
	{{0.229038, -0, 0.973417}, 57.2585, 5, 0, {0, 0}},	// 5	5 10 22
	{{0, 1, 0}, 1984, 1, 0, {0, 0}},					// 6	6 14
	{{0.229039, -0, 0.973417}, 33.8978, 5, 0, {0, 0}},	// 7	7 17
	{{0, 1, 0}, 1968, 1, 0, {0, 0}},					// 8	9 21 23 26
	{{1, 0, 0}, 736, 0, 0, {0, 0}},						// 9	12
	{{0, 0, 1}, -176, 2, 0, {0, 0}},					// 10	15
	{{0, 0, 1}, -192, 2, 0, {0, 0}},					// 11	16 19 27
	{{0, 0, 1}, -152, 2, 0, {0, 0}},					// 12	20 28
	{{1, 0, 0}, 720, 0, 0, {0, 0}},						// 13	29
};

hull_t hull2 = {
	clipnodes2,
	planes2,
	0,
	6,
	{0, 0, 0},
	{0, 0, 0},
};

static void
do_trace (hull_t *hull, vec3_t start, vec3_t end)
{
	trace_t trace;

	trace.allsolid = true;
	trace.startsolid = false;
	trace.inopen = false;
	trace.inwater = false;
	trace.fraction = 1;
	trace.extents[0] = 16;
	trace.extents[1] = 16;
	trace.extents[2] = 28;
	trace.isbox = true;
	VectorCopy (end, trace.endpos);
	MOD_TraceLine (hull, 0, start, end, &trace);
	printf ("(%g %g %g) (%g %g %g) -> %3g    %d %d %d %d\n",
			start[0], start[1], start[2],
			trace.endpos[0], trace.endpos[1], trace.endpos[2], trace.fraction,
			trace.allsolid, trace.startsolid, trace.inopen, trace.inwater);
}

int
main (int argc, char **argv)
{
	vec3_t start, end;
	int i;
#if 1
	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 0;
		start[1] = 47 + i;
		start[2] = 40;
		VectorCopy (start, end);
		end[2] -= 8;
		do_trace (&hull1, start, end);
	}

	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 0;
		start[1] = 52;
		start[2] = 35 + i;
		VectorCopy (start, end);
		end[1] -= 8;
		do_trace (&hull1, start, end);
	}

	puts ("\nexpect 0.484375, 1  (0 0 1 0), (1 1 0 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 47 + i;
		start[1] = -32;
		start[2] = 36;
		VectorCopy (start, end);
		end[0] += 2;
		do_trace (&hull1, start, end);
	}

	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 44;
		start[1] = -32;
		start[2] = 59 + i;
		VectorCopy (start, end);
		end[0] += 8;
		do_trace (&hull1, start, end);
	}

	puts ("\nexpect 1, 0.499023  (0 1 1 0), (0 1 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 47 + i;
		start[1] = -32;
		start[2] = 76;
		VectorCopy (start, end);
		end[2] -= 32;
		do_trace (&hull1, start, end);
	}
#endif
	puts ("\nexpect 1, 0.499023  (0 1 1 0), (0 1 1 0)");
//	for (i = 0; i < 30; i++) {
i = 5;
		start[0] = 891 + i;
		start[1] = 2048;
		start[2] = -144;
		VectorCopy (start, end);
		end[2] -= 8;
		do_trace (&hull2, start, end);
//	}
#if 1
	puts ("\nexpect 0  (0 0 1 0)");
	start[0] = 96;
	start[1] = -47.9612;
	start[2] = 56.0312 + 4;
	end[0] = 96;
	end[1] = -47.1025;
	end[2] = 55.8737 + 4;
	do_trace (&hull1, start, end);

	puts ("\nexpect 0  (0 0 1 0)");
	start[0] = 94.8916;
	start[1] = -34.8506;
	start[2] = 56.0312 + 4;
	end[0] = 94.8146;
	end[1] = -28.5696;
	end[2] = 55.5683 + 4;
	do_trace (&hull1, start, end);
#endif
	return 0;
}
