#include "head.c"

#define DB 0xdeadbeef

static pr_int_t mem_globals_init[] = {
	0, 8, 68, 9, 80, 112, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8,
	0, 0, 68, 6, 70,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 16

	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,
};

static pr_int_t mem_globals_expect[] = {
	0, 8, 68, 9, 80, 112, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8,	// 0
	0, 0, 68, 6, 70,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 16

	DB,  0,  0,  0,  0,  0, DB, DB,  9,  9, DB, DB, DB, DB, DB, DB,	// 32
	 1,  2,  3,  4,  5,  6,  7,  8, DB, DB, DB, DB,  5,  6,  7,  8,	// 48
	DB, DB, DB, DB,  1,  2,  1,  2,  3,  4,  5,  6, DB, DB, DB, DB,	// 64
	68, 68, 68, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,	// 80
	DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB, DB,	// 96
	 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, DB,	// 112
};

static dstatement_t mem_statements[] = {
	{OP(0, 0, 0, OP_MEMSET_I), 0, 5, 33},
	{OP(0, 0, 0, OP_MEMSET_I), 3, 2, 40},
	{OP(0, 0, 0, OP_MOVE_I), 8, 8, 48},
	{OP(0, 0, 0, OP_MOVE_I), 12, 4, 60},
	{OP(0, 0, 0, OP_MOVE_PI), 1, 8, 2},
	{OP(0, 0, 0, OP_MEMSET_P), 2, 10, 4},
	{OP(0, 0, 0, OP_MEMSET_PI), 1, 15, 5},
	{OP(0, 0, 0, OP_MOVE_P), 18, 19, 20},
};

test_t tests[] = {
	{
		.desc = "mem",
		.num_globals = num_globals (mem_globals_init, mem_globals_expect),
		.num_statements = num_statements (mem_statements),
		.statements = mem_statements,
		.init_globals = mem_globals_init,
		.expect_globals = mem_globals_expect,
	},
};

#include "main.c"
