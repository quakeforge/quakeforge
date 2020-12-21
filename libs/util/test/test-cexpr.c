/*
	test-cexpr.c

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/hash.h"

int a = 5;
int b = 6;
int c;

exprsym_t symbols[] = {
	{ "a", &cexpr_int, &a },
	{ "b", &cexpr_int, &b },
	{}
};
exprval_t result = { &cexpr_int, &c };

exprtab_t symtab = {
	symbols,
	0
};

exprctx_t context = { &result, &symtab };

#define TEST_BINOP(op)													\
	do {																\
		c = -4096;														\
		cexpr_eval_string ("a " #op " b", &context);					\
		printf ("c = a %s b -> %d = %d %s %d\n", #op, c, a, #op, b);	\
		if (c != (a op b)) {											\
			ret |= 1;													\
		}																\
	} while (0)

int
main(int argc, const char **argv)
{
	int         ret = 0;

	cexpr_init_symtab (&symtab, &context);
	context.memsuper = new_memsuper();

	TEST_BINOP (<<);
	TEST_BINOP (>>);
	TEST_BINOP (+);
	TEST_BINOP (-);
	TEST_BINOP (*);
	TEST_BINOP (/);
	TEST_BINOP (&);
	TEST_BINOP (|);
	TEST_BINOP (^);
	TEST_BINOP (%);

	Hash_DelTable (symtab.tab);
	delete_memsuper (context.memsuper);
	return ret;
}
