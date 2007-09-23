#include "world.h"

dclipnode_t clipnodes[] = {
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

mplane_t planes[] = {
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

hull_t hull = {
	clipnodes,
	planes,
	0,
	13,
	{0, 0, 0},
	{0, 0, 0},
};

static void do_trace (vec3_t start, vec3_t end)
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
	MOD_TraceLine (&hull, 0, start, end, &trace);
	printf ("(%g %g %g) (%g %g %g) -> %3g    %d %d %d %d\n",
			start[0], start[1], start[2],
			end[0], end[1], end[2], trace.fraction,
			trace.allsolid, trace.startsolid, trace.inopen, trace.inwater);
}

int main (int argc, char **argv)
{
	int i;
	vec3_t start, end;

	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 0;
		start[1] = 47 + i;
		start[2] = 40;
		VectorCopy (start, end);
		end[2] -= 8;
		do_trace (start, end);
	}

	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 0;
		start[1] = 52;
		start[2] = 35 + i;
		VectorCopy (start, end);
		end[1] -= 8;
		do_trace (start, end);
	}

	puts ("\nexpect 0.484375, 1  (0 0 1 0), (1 1 0 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 47 + i;
		start[1] = -80;
		start[2] = 36;
		VectorCopy (start, end);
		end[0] += 2;
		do_trace (start, end);
	}

	puts ("\nexpect 0.496094, 1  (0 0 1 0), (0 0 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 44;
		start[1] = -80;
		start[2] = 59 + i;
		VectorCopy (start, end);
		end[0] += 8;
		do_trace (start, end);
	}

	puts ("\nexpect 1, 0.499023  (0 1 1 0), (0 1 1 0)");
	for (i = 0; i < 2; i++) {
		start[0] = 47 + i;
		start[1] = -80;
		start[2] = 76;
		VectorCopy (start, end);
		end[2] -= 32;
		do_trace (start, end);
	}
	return 0;
}
