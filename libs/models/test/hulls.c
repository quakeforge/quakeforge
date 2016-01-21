#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "hulls.h"

//  0,0
//   |\   .
//   |s\  .
//   |ss\ .
//   0   1

static mclipnode_t clipnodes_simple_wedge[] = {
	{  0, {             1, CONTENTS_EMPTY}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_simple_wedge[] = {
	{{1, 0, 0}, 0, 0, 0},		//  0
	{{0.8, 0, 0.6}, 0, 4, 0},	//  1
};

hull_t hull_simple_wedge = {
	clipnodes_simple_wedge,
	planes_simple_wedge,
	0,
	1,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     0   1   2

static mclipnode_t clipnodes_tpp1[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             2, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

static plane_t planes_tpp1[] = {
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp1 = {
	clipnodes_tpp1,
	planes_tpp1,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     1   0   2

static mclipnode_t clipnodes_tpp2[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_SOLID, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

static plane_t planes_tpp2[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp2 = {
	clipnodes_tpp2,
	planes_tpp2,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|   |www|sss
//  sss|   |www|sss
//     1   0   2

static mclipnode_t clipnodes_tppw[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_WATER}},
};

static plane_t planes_tppw[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tppw = {
	clipnodes_tppw,
	planes_tppw,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//       2
// 32 ---+--- 1
//       |sss
//    ---+--- 0
//    ss0,0ss
static mclipnode_t clipnodes_step1[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {CONTENTS_EMPTY,              2}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

static plane_t planes_step1[] = {
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
	{{1, 0, 0},   0, 0, 0},
};

hull_t hull_step1 = {
	clipnodes_step1,
	planes_step1,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//      0
//      |
// 32   +--- 1
//      |sss
//   ---+sss 2
//   ss0,0ss
static mclipnode_t clipnodes_step2[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_step2[] = {
	{{1, 0, 0},   0, 0, 0},
	{{0, 0, 1},  32, 2, 0},
	{{0, 0, 1},   0, 2, 0},
};

hull_t hull_step2 = {
	clipnodes_step2,
	planes_step2,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//     0
//     |
// 2---+    32
//  sss|
//  sss+--- 1
//  ss0,0ss
static mclipnode_t clipnodes_step3[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_step3[] = {
	{{1, 0, 0},   0, 0, 0},
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
};

hull_t hull_step3 = {
	clipnodes_step3,
	planes_step3,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//      3 2
//    ss| |
//  4 --+ |   -20,40
//      | |
// 32 --+-+--- 1
//        |sss
//     ---+--- 0
//     ss0,0ss
static mclipnode_t clipnodes_covered_step[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             3,              2}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
	{  3, {CONTENTS_EMPTY,              4}},
	{  4, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

static plane_t planes_covered_step[] = {
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
	{{1, 0, 0},   0, 0, 0},
	{{1, 0, 0}, -20, 0, 0},
	{{0, 0, 1},  40, 2, 0},
};

hull_t hull_covered_step = {
	clipnodes_covered_step,
	planes_covered_step,
	0,
	4,
	{0, 0, 0},
	{0, 0, 0},
};

//     0
//     |
//  0,0+--- 1
//    /ssss
//   2 ssss
static mclipnode_t clipnodes_ramp[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_ramp[] = {
	{{   1, 0,   0},   0, 0, 0},
	{{   0, 0,   1},   0, 2, 0},
	{{-0.6, 0, 0.8},   0, 4, 0},
};

hull_t hull_ramp = {
	clipnodes_ramp,
	planes_ramp,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//   2   1
// ss|sss|ss
// ss+-3-+ss 8
// ss|   |ss
// ss+-4-+ss -8
// ss|sss|ss
//  -8   8
//  looking at plane 0: back of 0 is empty, front of 0 has above hole
static mclipnode_t clipnodes_hole[] = {
	{  0, {             1, CONTENTS_EMPTY}},
	{  1, {CONTENTS_SOLID,              2}},
	{  2, {             3, CONTENTS_SOLID}},
	{  3, {CONTENTS_SOLID,              4}},
	{  4, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_hole[] = {
	{{ 0, 1, 0},   0, 1, 0},
	{{ 1, 0, 0},   8, 0, 0},
	{{ 1, 0, 0},  -8, 0, 0},
	{{ 0, 0, 1},   8, 2, 0},
	{{ 0, 0, 1},  -8, 2, 0},
};

hull_t hull_hole = {
	clipnodes_hole,
	planes_hole,
	0,
	4,
	{0, 0, 0},
	{0, 0, 0},
};

//       2   3
// 32 ---+---+--- 1
//       |sss|
//    ---+---+--- 0
//    ss0,0s8,0ss
static mclipnode_t clipnodes_ridge[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {CONTENTS_EMPTY,              2}},
	{  2, {             3, CONTENTS_EMPTY}},
	{  3, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

static plane_t planes_ridge[] = {
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
	{{1, 0, 0},   0, 0, 0},
	{{1, 0, 0},   8, 0, 0},
};

hull_t hull_ridge = {
	clipnodes_ridge,
	planes_ridge,
	0,
	3,
	{0, 0, 0},
	{0, 0, 0},
};

//               5
//   ssssssssssss|6
//  1 sssssss 2 s.
//   \sssssss/ss/|
//   s\sssss/ss/ |
//   ss\s 4.--.--.-- 20
//   sss\s/        .
// 3 ----. 0,0     .
//   wwwww\        .
//   wwwwww\       .
// 0 -------.------- -20
//   sssssssssssssss
//   sssssssssssssss
static mclipnode_t clipnodes_cave[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             2,              3}},
	{  2, {CONTENTS_SOLID,              4}},
	{  3, {CONTENTS_SOLID, CONTENTS_WATER}},
	{  4, {             5, CONTENTS_EMPTY}},
	{  5, {CONTENTS_EMPTY,              6}},
	{  6, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

static plane_t planes_cave[] = {
	{{   0, 0,   1}, -20, 2, 0},
	{{ 0.6, 0, 0.8},   0, 3, 0},
	{{-0.8, 0, 0.6},   0, 3, 0},
	{{   0, 0,   1},   0, 2, 0},
	{{   0, 0,   1},  20, 2, 0},
	{{   1, 0,   0},  50, 0, 0},
	{{-0.8, 0, 0.6}, -20, 3, 0},
};

hull_t hull_cave = {
	clipnodes_cave,
	planes_cave,
	0,
	6,
	{0, 0, 0},
	{0, 0, 0},
};
