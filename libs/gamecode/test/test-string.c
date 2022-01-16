#include "head.c"

const char test_strings[] =
	"\0"
	"abc\0"
	"def\0"
	"abc\0";

static pr_int_t string_globals_init[] = {
	0, 1, 5, 9,		// string pointers
	0, 0, 0, 0,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

static pr_int_t string_globals_expect[] = {
	0, 1, 5, 9,		// string pointers
	0, 0, 8, 0,

//   "\0"             "abc"            "def"            "abc"
	-1,  0,  0,  0,   0, -1,  0, -1,   0,  0, -1,  0,   0, -1,  0, -1,	// eq
	 0, -1, -1, -1,   0,  0, -1,  0,   0,  0,  0,  0,   0,  0, -1,  0,	// lt
	 0,  0,  0,  0,  -1,  0,  0,  0,  -1, -1,  0, -1,  -1,  0,  0,  0,	// gt
	 0,-97,-100,-97, 97,  0, -3,  0, 100,  3,  0,  3,  97,  0, -3,  0,	// cmp
	-1,  0,  0,  0,  -1, -1,  0, -1,  -1, -1, -1, -1,  -1, -1,  0, -1,	// ge
	-1, -1, -1, -1,   0, -1, -1, -1,   0,  0, -1,  0,   0, -1, -1, -1,	// le
	-1, -1, -1, -1,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,	// not
};

static dstatement_t string_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 24, 0, 6 },	// init k
// for (i = 4; i-- > 0; ) {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 4 },
	{ OP(0, 0, 0, OP_IFA), 2, 0, 4 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_C), 4, -1, 4 },	// dec i

	{ OP(0, 0, 0, OP_WITH), 4, 4, 1 },		// load i

//     for (j = 4; j-- > 0; ) {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 5 },	// init j

	{ OP(0, 0, 0, OP_IFA), 2, 0, 5 },
	{ OP(0, 0, 0, OP_JUMP_A), -6, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_C), 5, -1, 5 },	// dec j

	{ OP(0, 0, 0, OP_WITH), 4, 5, 2 },		// load j

	{ OP(0, 0, 0, OP_LEA_C), 6, -1, 6 },	// dec k
	{ OP(0, 0, 0, OP_WITH), 4, 6, 3 },		// load k

	//   i  j  k
	{ OP(1, 2, 3, OP_EQ_S),  0, 0,  0 },
	{ OP(1, 2, 3, OP_LT_S),  0, 0, 16 },
	{ OP(1, 2, 3, OP_GT_S),  0, 0, 32 },
	{ OP(1, 2, 3, OP_CMP_S), 0, 0, 48 },
	{ OP(1, 2, 3, OP_GE_S),  0, 0, 64 },
	{ OP(1, 2, 3, OP_LE_S),  0, 0, 80 },
	{ OP(1, 2, 3, OP_NOT_S), 0, 0, 96 },

//      }
	{ OP(0, 0, 0, OP_JUMP_A), -13, 0, 0 },
//  }
};

test_t tests[] = {
	{
		.desc = "string",
		.num_globals = num_globals (string_globals_init, string_globals_expect),
		.num_statements = num_statements (string_statements),
		.statements = string_statements,
		.init_globals = string_globals_init,
		.expect_globals = string_globals_expect,
		.strings = test_strings,
		.string_size = sizeof (test_strings),
	},
};

#include "main.c"
