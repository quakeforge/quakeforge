#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "QF/va.h"

#define num_tests (sizeof (tests) / sizeof (tests[0]))
static int test_enabled[num_tests] = { 0 };

#include "getopt.h"

#include "QF/cmd.h"
#include "QF/cvar.h"

static bfunction_t test_functions[] = {
	{},	// null function
	{ .first_statement = 0 }
};
static dprograms_t test_progs = {
	.version = PROG_VERSION,
};
static progs_t test_pr;

static jmp_buf jump_buffer;

static void
test_debug_handler (prdebug_t event, void *param, void *data)
{
	progs_t    *pr = data;

	switch (event) {
		case prd_breakpoint:
			if (verbose > 0) {
				printf ("debug: %s\n", prdebug_names[event]);
			}
			longjmp (jump_buffer, 1);
		case prd_subenter:
			if (verbose > 0) {
				printf ("debug: subenter %d\n", *(func_t *) param);
			}
		case prd_subexit:
			break;
		case prd_trace:
			dstatement_t *st = test_pr.pr_statements + test_pr.pr_xstatement;
			if (verbose > 1) {
				printf ("---\n");
				printf ("debug: trace %05x %04x %04x %04x %04x%s\n",
						test_pr.pr_xstatement, st->op, st->a, st->b, st->c,
						pr->globals.stack ? va (0, " %05x", *pr->globals.stack)
										  : "");
				printf ("                        %04x %04x %04x\n",
						st->a + PR_BASE (pr, st, A),
						st->b + PR_BASE (pr, st, B),
						st->c + PR_BASE (pr, st, C));
			}
			if (verbose > 0) {
				PR_PrintStatement (&test_pr, st, 0);
			}
			if (pr->globals.stack) {
				if (*pr->globals.stack & 3) {
					printf ("stack not aligned: %d\n", *pr->globals.stack);
					longjmp (jump_buffer, 3);
				}
			}
			break;
		case prd_runerror:
			printf ("debug: %s: %s\n", prdebug_names[event], (char *)param);
			longjmp (jump_buffer, 3);
		case prd_watchpoint:
		case prd_begin:
		case prd_terminate:
		case prd_error:
		case prd_none:
			printf ("debug: unexpected:%s %p\n", prdebug_names[event], param);
			longjmp (jump_buffer, 2);
	}
}

static void
setup_test (test_t *test)
{
	memset (&test_pr, 0, sizeof (test_pr));
	PR_Init (&test_pr);
	PR_Debug_Init (&test_pr);
	test_pr.progs = &test_progs;
	test_pr.debug_handler = test_debug_handler;
	test_pr.debug_data = &test_pr;
	test_pr.pr_trace = 1;
	test_pr.pr_trace_depth = -1;
	if (test->num_functions && test->functions) {
		test_pr.function_table = calloc ((test->num_functions + 2),
										 sizeof (bfunction_t));
		memcpy (test_pr.function_table, test_functions,
				2 * sizeof (bfunction_t));
		memcpy (test_pr.function_table + 2, test->functions,
				test->num_functions * sizeof (bfunction_t));
	} else {
		test_pr.function_table = test_functions;
	}

	pr_uint_t   num_globals = test->num_globals;
	num_globals += test->extra_globals + test->stack_size;

	test_pr.globals_size = num_globals;
	test_pr.pr_globals = Sys_Alloc (num_globals * sizeof (pr_type_t));
	memcpy (test_pr.pr_globals, test->init_globals,
			test->num_globals * sizeof (pr_type_t));
	memset (test_pr.pr_globals + test->num_globals, 0,
			test->extra_globals * sizeof (pr_type_t));
	if (test->stack_size) {
		pr_ptr_t    stack = num_globals - test->stack_size;
		test_pr.stack_bottom = stack + 4;
		test_pr.globals.stack = (pr_ptr_t *) (test_pr.pr_globals + stack);
		*test_pr.globals.stack = num_globals;
	}
	if (test->edict_area) {
		test_pr.pr_edict_area = test_pr.pr_globals + test->edict_area;
	}
	if (test->double_time || test->float_time) {
		test_pr.fields.nextthink = test->nextthink;
		test_pr.fields.frame = test->frame;
		test_pr.fields.think = test->think;
		test_pr.globals.self = (pr_uint_t *) &test_pr.pr_globals[test->self];
		if (test->double_time) {
			test_pr.globals.dtime = (double *)&test_pr.pr_globals[test->dtime];
			*test_pr.globals.dtime = *test->double_time;
		}
		if (test->float_time) {
			test_pr.globals.ftime = (float *) &test_pr.pr_globals[test->ftime];
			*test_pr.globals.ftime = *test->float_time;
		}
	}

	test_progs.numstatements = test->num_statements + 1;
	test_pr.pr_statements
		= malloc ((test->num_statements + 1) * sizeof (dstatement_t));
	memcpy (test_pr.pr_statements, test->statements,
			(test->num_statements + 1) * sizeof (dstatement_t));
	test_pr.pr_statements[test->num_statements] =
		(dstatement_t) { OP_BREAK, 0, 0, 0 };

	test_pr.pr_strings = (char *) test->strings;
	test_pr.pr_stringsize = test->string_size;
}

static int
check_result (test_t *test)
{
	int         ret = 0;

	if (memcmp (test_pr.pr_globals, test->expect_globals,
				test->num_globals * sizeof (pr_int_t)) == 0) {
		ret = 1;
		printf ("test #%zd: %s: OK\n", test - tests, test->desc);
	} else {
		printf ("test #%zd: %s: words differ\n", test - tests, test->desc);
	}
	return ret;
}

static int
run_test (test_t *test)
{
	int         jump_ret;
	int         ret = 0;

	setup_test (test);

	if (!(jump_ret = setjmp (jump_buffer))) {
		PR_ExecuteProgram (&test_pr, 1);
		printf ("returned from progs\n");
	}
	if (jump_ret == 1) {
		ret = check_result (test);
	} else {
		printf ("test #%zd: %s: critical failure\n", test - tests, test->desc);
	}

	pr_uint_t   num_globals = test->num_globals;
	num_globals += test->extra_globals + test->stack_size;
	if (test->num_functions && test->functions) {
		free (test_pr.function_table);
	}
	Sys_Free (test_pr.pr_globals, num_globals * sizeof (pr_type_t));

	free (test_pr.pr_statements);
	return ret;
}

int
main (int argc, char **argv)
{
	int         c;
	size_t      i, test;
	int         pass = 1;

	Cmd_Init_Hash ();
	Cvar_Init_Hash ();
	Cmd_Init ();
	Cvar_Init ();
	PR_Init_Cvars ();
	pr_boundscheck->int_val = 1;

	while ((c = getopt (argc, argv, "qvt:")) != EOF) {
		switch (c) {
			case 'q':
				verbose--;
				break;
			case 'v':
				verbose++;
				break;
			case 't':
				test = atoi (optarg);
				if (test < num_tests) {
					test_enabled[test] = 1;
				} else {
					fprintf (stderr, "Bad test number (0 - %zd)\n", num_tests);
					return 1;
				}
				break;
			default:
				fprintf (stderr, "-q (quiet) -v (verbose) and/or -t TEST "
								 "(test number)\n");
				return 1;
		}
	}

	for (i = 0; i < num_tests; i++)
		if (test_enabled[i])
			break;
	if (i == num_tests) {
		for (i = 0; i < num_tests; i++)
			test_enabled[i] = 1;
	}

	for (i = 0; i < num_tests; i++) {
		if (!test_enabled[i])
			continue;
		pass &= run_test (&tests[i]);
	}

	return !pass;
}
