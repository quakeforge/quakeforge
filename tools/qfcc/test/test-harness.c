/*
	test-harness.c

	Program for testing qfcc generated code.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/22

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <getopt.h>
#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/progs.h>
#include <QF/quakefs.h>
#include "QF/ruamoko.h"
#include <QF/sys.h>
#include "QF/va.h"
#include <QF/zone.h>

#include "test-bi.h"

#define MAX_EDICTS 64		// 64 edicts should be enough for testing
#define MAX_HEAP 1024*1024	// 1MB should be enough for testing
#define MAX_STACK 64*1024	// 64kB should be enough for testing

// keep me sane when adding long options :P
enum {
	start_opts = 255,   // not used, starts the enum.
	OPT_DEVELOPER,
	OPT_FLOAT,
};

static struct option const long_options[] = {
	{"developer",	required_argument,	0, OPT_DEVELOPER},
	{"float",		no_argument,		0, OPT_FLOAT},
	{"trace",		no_argument,		0, 't'},
	{"help",		no_argument,		0, 'h'},
	{"version",		no_argument,		0, 'V'},
};

static const char *short_options =
	"+"		// magic option parsing mode doohicky (must come first)
	"h"		// help
	"t"		// tracing
	"V"		// version
	;

static edict_t test_edicts[MAX_EDICTS];

static edict_t *edicts;
static pr_uint_t num_edicts;
static pr_uint_t reserved_edicts;
static progs_t test_pr;
static const char *this_program;

static struct {
	int         developer;
	int         trace;
	int         flote;
} options;

static QFile *
open_file (const char *path, int *len)
{
	QFile      *file = Qopen (path, "rbz");

	if (!file) {
		perror (path);
		return 0;
	}
	*len = Qfilesize (file);
	return file;
}

static void *
load_file (progs_t *pr, const char *name, off_t *_size)
{
	QFile      *file;
	int         size;
	char       *sym;

	file = open_file (name, &size);
	if (!file) {
		file = open_file (va (0, "%s.gz", name), &size);
		if (!file) {
			return 0;
		}
	}
	sym = malloc (size + 1);
	sym[size] = 0;
	Qread (file, sym, size);
	*_size = size;
	return sym;
}

#define ALIGN 32

static void *
allocate_progs_mem (progs_t *pr, int size)
{
	intptr_t    mem = (intptr_t) malloc (size + ALIGN);
	mem = (mem + ALIGN - 1) & ~(ALIGN - 1);
	return (void *) mem;
}

static void
free_progs_mem (progs_t *pr, void *mem)
{
	free (mem);
}

static int
init_edicts (progs_t *pr)
{
	memset (test_edicts, 0, sizeof (test_edicts));

	// init the data field of the edicts
	for (int i = 0; i < MAX_EDICTS; i++) {
		edict_t    *ent = EDICT_NUM (&test_pr, i);
		ent->pr = &test_pr;
		ent->entnum = i;
		ent->edict = EDICT_TO_PROG (&test_pr, ent);
	}
	return 1;
}

static void
init_qf (void)
{
	Sys_Init ();
	developer = options.developer;

	Memory_Init (Sys_Alloc (1024 * 1024), 1024 * 1024);

	test_pr.pr_edicts = &edicts;
	test_pr.num_edicts = &num_edicts;
	test_pr.reserved_edicts = &reserved_edicts;
	test_pr.load_file = load_file;
	test_pr.allocate_progs_mem = allocate_progs_mem;
	test_pr.free_progs_mem = free_progs_mem;
	test_pr.no_exec_limit = 0;	// absolutely want a limit!

	PR_Init_Cvars ();

	pr_debug = options.trace > 1 ? 4 : 2;
	pr_boundscheck = 2;
	pr_deadbeef_locals = 1;

	PR_AddLoadFunc (&test_pr, init_edicts);
	PR_Init (&test_pr);

	RUA_Init (&test_pr, 0);
	PR_Cmds_Init(&test_pr);
	BI_Init (&test_pr);
}

static int
load_progs (const char *name)
{
	QFile      *file;
	int         size;

	file = open_file (name, &size);
	if (!file) {
		return 0;
	}
	test_pr.progs_name = name;
	test_pr.max_edicts = MAX_EDICTS;
	test_pr.zone_size = MAX_HEAP;
	test_pr.stack_size = MAX_STACK;
	edicts = test_edicts;

	PR_LoadProgsFile (&test_pr, file, size);
	Qclose (file);
	if (!PR_RunLoadFuncs (&test_pr))
		PR_Error (&test_pr, "unable to load %s", test_pr.progs_name);
	if (!PR_RunPostLoadFuncs (&test_pr))
		PR_Error (&test_pr, "unable to load %s", test_pr.progs_name);
	test_pr.pr_trace_depth = -1;
	test_pr.pr_trace = options.trace;
	return 1;
}

static void
usage (int exitval)
{
	printf ("%s - QuakeForge VM test harness\n", this_program);
	printf ("Usage: %s [options] [progs.dat [progs options]]", this_program);
	printf (
"Options:\n"
"        --developer FLAGS     Set the developer cvar to FLAGS.\n"
"        --float               main () returns float instead of int.\n"
"    -h, --help                Display this help and exit\n"
"    -t, --trace               Set the trace flag in the VM.\n"
"    -V, --version             Output version information and exit\n"
	);
	exit (exitval);
}

static int
parse_options (int argc, char **argv)
{
	int         c;

	this_program = argv[0];
	while ((c = getopt_long (argc, argv, short_options, long_options, 0))
		   != -1) {
		switch (c) {
			case OPT_DEVELOPER:
				options.developer = atoi (optarg);
				break;
			case OPT_FLOAT:
				options.flote = 1;
				break;
			case 't':
				options.trace++;
				break;
			case 'h':
				usage (0);
				break;
			case 'V':
				printf ("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
				exit (0);
			default:
				usage (1);
		}
	}
	return optind;
}

int
main (int argc, char **argv)
{
	dfunction_t *dfunc;
	pr_func_t   main_func = 0;
	const char *name = "progs.dat";
	pr_string_t *pr_argv;
	int         pr_argc = 1, i;

	i = parse_options (argc, argv);
	argc -= i;
	argv += i;

	init_qf ();

	if (argc > 0)
		name = argv[0];

	if (!load_progs (name))
		Sys_Error ("couldn't load %s", name);

	if ((dfunc = PR_FindFunction (&test_pr, ".main"))
		|| (dfunc = PR_FindFunction (&test_pr, "main"))) {
		main_func = dfunc - test_pr.pr_functions;
	} else {
		PR_Undefined (&test_pr, "function", "main");
	}

	PR_PushFrame (&test_pr);
	if (argc) {
		pr_argc = argc;
	}
	pr_argv = PR_Zone_Malloc (&test_pr, (pr_argc + 1) * 4);
	pr_argv[0] = PR_SetTempString (&test_pr, name);
	for (i = 1; i < pr_argc; i++) {
		pr_argv[i] = PR_SetTempString (&test_pr, argv[i]);
	}
	pr_argv[i] = 0;

	PR_RESET_PARAMS (&test_pr);
	P_INT (&test_pr, 0) = pr_argc;
	P_POINTER (&test_pr, 1) = PR_SetPointer (&test_pr, pr_argv);
	test_pr.pr_argc = 2;

	PR_ExecuteProgram (&test_pr, main_func);
	PR_PopFrame (&test_pr);
	if (options.flote)
		return R_FLOAT (&test_pr);
	return R_INT (&test_pr);
}
