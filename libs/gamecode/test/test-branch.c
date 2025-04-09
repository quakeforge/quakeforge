#include "head.c"

#define DB 0xdeadbeef

static pr_int_t test_globals_init[] = {
	-1, 1, 0, DB, DB,
};

static pr_int_t test_globals_expect[] = {
	-1, 1, 0, DB, 1,
};

static dstatement_t ifz_taken_statements[] = {
	{ OP_IFZ, 4, 0, 2 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFZ, -2, 0, 2 },
};

static dstatement_t ifb_taken_statements[] = {
	{ OP_IFB, 4, 0, 0 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFB, -2, 0, 0 },
};

static dstatement_t ifa_taken_statements[] = {
	{ OP_IFA, 4, 0, 1 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFA, -2, 0, 1 },
};

static dstatement_t ifnz_taken_statements[] = {
	{ OP_IFNZ, 4, 0, 0 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFNZ, -2, 0, 1 },
};

static dstatement_t ifae_taken_statements[] = {
	{ OP_IFAE, 4, 0, 1 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFAE, -2, 0, 2 },
};

static dstatement_t ifbe_taken_statements[] = {
	{ OP_IFBE, 4, 0, 0 },
	{ OP_LEA_A, 1, 0, 3 },
	{ OP_LEA_A, 1, 0, 4 },
	{ OP_BREAK },
	{ OP_IFBE, -2, 0, 2 },
};

static dstatement_t ifz_not_taken_statements[] = {
	{ OP_IFZ, 3, 0, 0 },
	{ OP_IFZ, 2, 0, 1 },
	{ OP_LEA_A, 1, 0, 4 },
};

static dstatement_t ifb_not_taken_statements[] = {
	{ OP_IFB, 3, 0, 2 },
	{ OP_IFB, 2, 0, 1 },
	{ OP_LEA_A, 1, 0, 4 },
};

static dstatement_t ifa_not_taken_statements[] = {
	{ OP_IFA, 3, 0, 2 },
	{ OP_IFA, 2, 0, 0 },
	{ OP_LEA_A, 1, 0, 4 },
};

static dstatement_t ifnz_not_taken_statements[] = {
	{ OP_IFNZ, 3, 0, 2 },
	{ OP_LEA_A, 1, 0, 4 },
};

static dstatement_t ifae_not_taken_statements[] = {
	{ OP_IFAE, 2, 0, 0 },
	{ OP_LEA_A, 1, 0, 4 },
};

static dstatement_t ifbe_not_taken_statements[] = {
	{ OP_IFBE, 2, 0, 1 },
	{ OP_LEA_A, 1, 0, 4 },
};

test_t tests[] = {
	{
		.desc = "ifz taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifz_taken_statements),
		.statements = ifz_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifb taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifb_taken_statements),
		.statements = ifb_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifa taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifa_taken_statements),
		.statements = ifa_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifnz taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifnz_taken_statements),
		.statements = ifnz_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifae taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifae_taken_statements),
		.statements = ifae_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifbe taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifbe_taken_statements),
		.statements = ifbe_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifz not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifz_not_taken_statements),
		.statements = ifz_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifb not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifb_not_taken_statements),
		.statements = ifb_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifa not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifa_not_taken_statements),
		.statements = ifa_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifnz not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifnz_not_taken_statements),
		.statements = ifnz_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifae not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifae_not_taken_statements),
		.statements = ifae_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "ifbe not taken",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (ifbe_not_taken_statements),
		.statements = ifbe_not_taken_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
};

#include "main.c"
