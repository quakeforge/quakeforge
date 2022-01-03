#include "head.c"

static pr_int_t test_globals_init1[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// source data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// destination data
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
};

static pr_int_t test_globals_expect1[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// source data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// destination data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

static pr_int_t test_globals_init2[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// destination data
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	// source data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

static pr_int_t test_globals_expect2[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// destination data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// source data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

static dstatement_t stack_AA_statements[] = {
	{OP(0, 0, 0, OP_PUSH_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 22, 0, 0},

	{OP(0, 0, 0, OP_POP_A_2), 24, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 26, 0, 0},
	{OP(0, 0, 0, OP_POP_A_1), 28, 0, 0},
	{OP(0, 0, 0, OP_POP_A_3), 29, 0, 0},
	{OP(0, 0, 0, OP_POP_A_4), 32, 0, 0},
};

static dstatement_t stack_AB_statements[] = {
	{OP(0, 0, 0, OP_PUSH_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 22, 0, 0},

	{OP(0, 0, 0, OP_POP_B_2), 0, 0, 0},
	{OP(0, 0, 0, OP_POP_B_2), 1, 0, 0},
	{OP(0, 0, 0, OP_POP_B_1), 2, 0, 0},
	{OP(0, 0, 0, OP_POP_B_3), 3, 0, 0},
	{OP(0, 0, 0, OP_POP_B_4), 4, 0, 0},
};

static dstatement_t stack_AC_statements[] = {
	{OP(0, 0, 0, OP_PUSH_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 22, 0, 0},

	{OP(0, 0, 0, OP_POP_C_2), 2, -4, 0},
	{OP(0, 0, 0, OP_POP_C_2), 2, -2, 0},
	{OP(0, 0, 0, OP_POP_C_1), 2, 0, 0},
	{OP(0, 0, 0, OP_POP_C_3), 2, 1, 0},
	{OP(0, 0, 0, OP_POP_C_4), 2, 4, 0},
};

static dstatement_t stack_AD_statements[] = {
	{OP(0, 0, 0, OP_PUSH_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_PUSH_A_2), 22, 0, 0},

	{OP(0, 0, 0, OP_POP_D_2), 2, 5, 0},
	{OP(0, 0, 0, OP_POP_D_2), 2, 6, 0},
	{OP(0, 0, 0, OP_POP_D_1), 2, 7, 0},
	{OP(0, 0, 0, OP_POP_D_3), 2, 8, 0},
	{OP(0, 0, 0, OP_POP_D_4), 2, 9, 0},
};

static dstatement_t stack_BA_statements[] = {
	{OP(0, 0, 0, OP_PUSH_B_2), 0, 0, 0},
	{OP(0, 0, 0, OP_PUSH_B_2), 1, 0, 0},
	{OP(0, 0, 0, OP_PUSH_B_1), 2, 0, 0},
	{OP(0, 0, 0, OP_PUSH_B_3), 3, 0, 0},
	{OP(0, 0, 0, OP_PUSH_B_4), 4, 0, 0},

	{OP(0, 0, 0, OP_POP_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_POP_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_POP_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 22, 0, 0},
};

static dstatement_t stack_CA_statements[] = {
	{OP(0, 0, 0, OP_PUSH_C_2), 2, -4, 0},
	{OP(0, 0, 0, OP_PUSH_C_2), 2, -2, 0},
	{OP(0, 0, 0, OP_PUSH_C_1), 2, 0, 0},
	{OP(0, 0, 0, OP_PUSH_C_3), 2, 1, 0},
	{OP(0, 0, 0, OP_PUSH_C_4), 2, 4, 0},

	{OP(0, 0, 0, OP_POP_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_POP_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_POP_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 22, 0, 0},
};

static dstatement_t stack_DA_statements[] = {
	{OP(0, 0, 0, OP_PUSH_D_2), 2, 5, 0},
	{OP(0, 0, 0, OP_PUSH_D_2), 2, 6, 0},
	{OP(0, 0, 0, OP_PUSH_D_1), 2, 7, 0},
	{OP(0, 0, 0, OP_PUSH_D_3), 2, 8, 0},
	{OP(0, 0, 0, OP_PUSH_D_4), 2, 9, 0},

	{OP(0, 0, 0, OP_POP_A_4), 12, 0, 0},
	{OP(0, 0, 0, OP_POP_A_3), 16, 0, 0},
	{OP(0, 0, 0, OP_POP_A_1), 19, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 20, 0, 0},
	{OP(0, 0, 0, OP_POP_A_2), 22, 0, 0},
};

test_t tests[] = {
	{
		.desc = "stack push A pop A",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init1, test_globals_expect1),
		.num_statements = num_statements (stack_AA_statements),
		.statements = stack_AA_statements,
		.init_globals = test_globals_init1,
		.expect_globals = test_globals_expect1,
	},
	{
		.desc = "stack push A pop B",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init1, test_globals_expect1),
		.num_statements = num_statements (stack_AB_statements),
		.statements = stack_AB_statements,
		.init_globals = test_globals_init1,
		.expect_globals = test_globals_expect1,
	},
	{
		.desc = "stack push A pop C",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init1, test_globals_expect1),
		.num_statements = num_statements (stack_AC_statements),
		.statements = stack_AC_statements,
		.init_globals = test_globals_init1,
		.expect_globals = test_globals_expect1,
	},
	{
		.desc = "stack push A pop D",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init1, test_globals_expect1),
		.num_statements = num_statements (stack_AD_statements),
		.statements = stack_AD_statements,
		.init_globals = test_globals_init1,
		.expect_globals = test_globals_expect1,
	},
	{
		.desc = "stack push B pop A",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init2, test_globals_expect2),
		.num_statements = num_statements (stack_BA_statements),
		.statements = stack_BA_statements,
		.init_globals = test_globals_init2,
		.expect_globals = test_globals_expect2,
	},
	{
		.desc = "stack push C pop A",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init2, test_globals_expect2),
		.num_statements = num_statements (stack_CA_statements),
		.statements = stack_CA_statements,
		.init_globals = test_globals_init2,
		.expect_globals = test_globals_expect2,
	},
	{
		.desc = "stack push D pop A",
		.stack_size = 32,
		.num_globals = num_globals (test_globals_init2, test_globals_expect2),
		.num_statements = num_statements (stack_DA_statements),
		.statements = stack_DA_statements,
		.init_globals = test_globals_init2,
		.expect_globals = test_globals_expect2,
	},
};

#include "main.c"
