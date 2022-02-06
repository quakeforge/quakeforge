#include "head.c"

#include "QF/mathlib.h"

static pr_uivec4_t uint_hop_init[7] = {
	{ 0x0000ff00, 0x0000f0f0, 0x0000cccc, 0x0000aaaa },
};

static pr_uivec4_t uint_hop_expect[] = {
	{ 0x0000ff00, 0x0000f0f0, 0x0000cccc, 0x0000aaaa },
	{ 0x00008888, 0x0000eeee, 0x00006666, 0x00017776 },
	{ 0xffff7777, 0xffff1111, 0xffff9999,          0 },
	{ 0x00008080, 0x0000fefe, 0x00009696, 0x00026866 },
	{ 0xffff7f7f, 0xffff0101, 0xffff6969,          0 },
	{ 0x00008000, 0x0000fffe, 0x00006996, 0x00036766 },
	{ 0xffff7fff, 0xffff0001, 0xffff9669,          0 },
};

static dstatement_t uint_hop_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 0,  4, 1 },
	{ OP(0, 0, 0, OP_WITH), 0, 12, 2 },
	{ OP(0, 0, 0, OP_WITH), 0, 20, 3 },

	{ OP(0, 0, 1, OP_HOPS), 2, 010, 0 },
	{ OP(0, 0, 1, OP_HOPS), 2, 011, 1 },
	{ OP(0, 0, 1, OP_HOPS), 2, 012, 2 },
	{ OP(0, 0, 1, OP_HOPS), 2, 013, 3 },
	{ OP(0, 0, 1, OP_HOPS), 2, 014, 4 },
	{ OP(0, 0, 1, OP_HOPS), 2, 015, 5 },
	{ OP(0, 0, 1, OP_HOPS), 2, 016, 6 },

	{ OP(0, 0, 2, OP_HOPS), 1, 020, 0 },
	{ OP(0, 0, 2, OP_HOPS), 1, 021, 1 },
	{ OP(0, 0, 2, OP_HOPS), 1, 022, 2 },
	{ OP(0, 0, 2, OP_HOPS), 1, 023, 3 },
	{ OP(0, 0, 2, OP_HOPS), 1, 024, 4 },
	{ OP(0, 0, 2, OP_HOPS), 1, 025, 5 },
	{ OP(0, 0, 2, OP_HOPS), 1, 026, 6 },

	{ OP(0, 0, 3, OP_HOPS), 0, 030, 0 },
	{ OP(0, 0, 3, OP_HOPS), 0, 031, 1 },
	{ OP(0, 0, 3, OP_HOPS), 0, 032, 2 },
	{ OP(0, 0, 3, OP_HOPS), 0, 033, 3 },
	{ OP(0, 0, 3, OP_HOPS), 0, 034, 4 },
	{ OP(0, 0, 3, OP_HOPS), 0, 035, 5 },
	{ OP(0, 0, 3, OP_HOPS), 0, 036, 6 },
};

static pr_ulvec4_t ulong_hop_init[7] = {
	{ UINT64_C(0x00ff00000000), UINT64_C(0x0f0f00000000),
		UINT64_C(0x333300000000), UINT64_C(0x555500000000) },
};

static pr_ulvec4_t ulong_hop_expect[] = {
	{ UINT64_C(0x000000ff00000000), UINT64_C(0x00000f0f00000000),
		UINT64_C(0x0000333300000000), UINT64_C(0x0000555500000000) },
	{ UINT64_C(0x0000111100000000), UINT64_C(0x0000777700000000),
		UINT64_C(0x0000666600000000), UINT64_C(0x0000888800000000) },
	{ UINT64_C(0xffffeeeeffffffff), UINT64_C(0xffff8888ffffffff),
		UINT64_C(0xffff9999ffffffff),                            0 },
	{ UINT64_C(0x0000010100000000), UINT64_C(0x00007f7f00000000),
		UINT64_C(0x0000696900000000), UINT64_C(0x0000979700000000) },
	{ UINT64_C(0xfffffefeffffffff), UINT64_C(0xffff8080ffffffff),
		UINT64_C(0xffff9696ffffffff),                            0 },
	{ UINT64_C(0x0000000100000000), UINT64_C(0x00007fff00000000),
		UINT64_C(0x0000699600000000), UINT64_C(0x0000989600000000) },
	{ UINT64_C(0xfffffffeffffffff), UINT64_C(0xffff8000ffffffff),
		UINT64_C(0xffff9669ffffffff),                            0 },
};

static dstatement_t ulong_hop_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 0,  8, 1 },
	{ OP(0, 0, 0, OP_WITH), 0, 24, 2 },
	{ OP(0, 0, 0, OP_WITH), 0, 40, 3 },

	{ OP(0, 0, 1, OP_HOPS), 4, 050,  0 },
	{ OP(0, 0, 1, OP_HOPS), 4, 051,  2 },
	{ OP(0, 0, 1, OP_HOPS), 4, 052,  4 },
	{ OP(0, 0, 1, OP_HOPS), 4, 053,  6 },
	{ OP(0, 0, 1, OP_HOPS), 4, 054,  8 },
	{ OP(0, 0, 1, OP_HOPS), 4, 055, 10 },
	{ OP(0, 0, 1, OP_HOPS), 4, 056, 12 },

	{ OP(0, 0, 2, OP_HOPS), 2, 060,  0 },
	{ OP(0, 0, 2, OP_HOPS), 2, 061,  2 },
	{ OP(0, 0, 2, OP_HOPS), 2, 062,  4 },
	{ OP(0, 0, 2, OP_HOPS), 2, 063,  6 },
	{ OP(0, 0, 2, OP_HOPS), 2, 064,  8 },
	{ OP(0, 0, 2, OP_HOPS), 2, 065, 10 },
	{ OP(0, 0, 2, OP_HOPS), 2, 066, 12 },

	{ OP(0, 0, 3, OP_HOPS), 0, 070,  0 },
	{ OP(0, 0, 3, OP_HOPS), 0, 071,  2 },
	{ OP(0, 0, 3, OP_HOPS), 0, 072,  4 },
	{ OP(0, 0, 3, OP_HOPS), 0, 073,  6 },
	{ OP(0, 0, 3, OP_HOPS), 0, 074,  8 },
	{ OP(0, 0, 3, OP_HOPS), 0, 075, 10 },
	{ OP(0, 0, 3, OP_HOPS), 0, 076, 12 },
};

static pr_vec4_t float_hop_init[2] = {
	{ 1, 2, 3, 4 },
};

static pr_vec4_t float_hop_expect[] = {
	{ 1, 2,  3, 4 },
	{ 3, 6, 10, 0 },
};

static dstatement_t float_hop_statements[] = {
	{ OP(0, 0, 0, OP_HOPS), 0, 017, 4 },
	{ OP(0, 0, 0, OP_HOPS), 0, 027, 5 },
	{ OP(0, 0, 0, OP_HOPS), 0, 037, 6 },
};

static pr_dvec4_t double_hop_init[2] = {
	{ 1, 2, 3, 4 },
};

static pr_dvec4_t double_hop_expect[] = {
	{ 1, 2,  3, 4 },
	{ 3, 6, 10, 0 },
};

static dstatement_t double_hop_statements[] = {
	{ OP(0, 0, 0, OP_HOPS), 0, 057,  8 },
	{ OP(0, 0, 0, OP_HOPS), 0, 067, 10 },
	{ OP(0, 0, 0, OP_HOPS), 0, 077, 12 },
};

test_t tests[] = {
	{
		.desc = "uint hops",
		.num_globals = num_globals(uint_hop_init,uint_hop_expect),
		.num_statements = num_statements (uint_hop_statements),
		.statements = uint_hop_statements,
		.init_globals = (pr_int_t *) uint_hop_init,
		.expect_globals = (pr_int_t *) uint_hop_expect,
	},
	{
		.desc = "ulong hops",
		.num_globals = num_globals(ulong_hop_init,ulong_hop_expect),
		.num_statements = num_statements (ulong_hop_statements),
		.statements = ulong_hop_statements,
		.init_globals = (pr_int_t *) ulong_hop_init,
		.expect_globals = (pr_int_t *) ulong_hop_expect,
	},
	{
		.desc = "float hops",
		.num_globals = num_globals(float_hop_init,float_hop_expect),
		.num_statements = num_statements (float_hop_statements),
		.statements = float_hop_statements,
		.init_globals = (pr_int_t *) float_hop_init,
		.expect_globals = (pr_int_t *) float_hop_expect,
	},
	{
		.desc = "double hops",
		.num_globals = num_globals(double_hop_init,double_hop_expect),
		.num_statements = num_statements (double_hop_statements),
		.statements = double_hop_statements,
		.init_globals = (pr_int_t *) double_hop_init,
		.expect_globals = (pr_int_t *) double_hop_expect,
	},
};

#include "main.c"
