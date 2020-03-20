/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/gib.h"
#include "QF/idparse.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/ruamoko.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "qwaq.h"

#define MAX_EDICTS 1024

cbuf_t *qwaq_cbuf;

const char *this_program;

enum {
	start_opts = 255,
	OPT_QARGS,
};

static struct option const long_options[] = {
	{"qargs", no_argument, 0, OPT_QARGS},
	{NULL, 0, NULL, 0},
};

static const char *short_options =
	"-"		// magic option parsing mode doohicky (must come first)
	;

typedef struct qwaq_thread_s {
	struct DARRAY_TYPE (const char *) args;
	sys_printf_t sys_printf;
	progs_t    *pr;
	func_t      main_func;
} qwaq_thread_t;

struct DARRAY_TYPE(qwaq_thread_t *) thread_data;

static QFile *
open_file (const char *path, int *len)
{
	QFile      *file = Qopen (path, "rbz");

	if (!file) {
		Sys_Printf ("%s\n", sys_errlist[errno]);
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
		file = open_file (va ("%s.gz", name), &size);
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

static void *
allocate_progs_mem (progs_t *pr, int size)
{
	return malloc (size);
}

static void
free_progs_mem (progs_t *pr, void *mem)
{
	free (mem);
}

static void
init_qf (void)
{
	qwaq_cbuf = Cbuf_New (&id_interp);

	Sys_Init ();
	COM_ParseConfig ();

	//Cvar_Set (developer, "1");

	Memory_Init (malloc (8 * 1024 * 1024), 8 * 1024 * 1024);

	Cvar_Get ("pr_debug", "2", 0, 0, 0);
	Cvar_Get ("pr_boundscheck", "0", 0, 0, 0);

}

static progs_t *
create_progs (void)
{
	progs_t    *pr = calloc (1, sizeof (*pr));

	pr->load_file = load_file;
	pr->allocate_progs_mem = allocate_progs_mem;
	pr->free_progs_mem = free_progs_mem;
	pr->no_exec_limit = 1;

	PR_Init_Cvars ();
	PR_Init (pr);
	RUA_Init (pr, 0);
	PR_Cmds_Init (pr);
	BI_Init (pr);

	return pr;
}

static int
load_progs (progs_t *pr, const char *name)
{
	QFile      *file;
	int         size;

	file = open_file (name, &size);
	if (!file) {
		return 0;
	}
	pr->progs_name = name;
	pr->max_edicts = 1;
	pr->zone_size = 1024*1024;
	PR_LoadProgsFile (pr, file, size);
	Qclose (file);
	if (!PR_RunLoadFuncs (pr))
		PR_Error (pr, "unable to load %s", pr->progs_name);
	return 1;
}

static void
spawn_progs (qwaq_thread_t *thread)
{
	dfunction_t *dfunc;
	const char *name = 0;
	string_t   *pr_argv;
	int         pr_argc = 1, i;
	progs_t    *pr;

	thread->main_func = 0;
	pr = thread->pr = create_progs ();
	if (thread->args.size) {
		name = thread->args.a[0];
	}

	if (!load_progs (pr, name)) {
		Sys_Error ("couldn't load %s", name);
	}

	//pr->pr_trace = 1;
	//pr->pr_trace_depth = -1;

	PR_PushFrame (pr);
	if (thread->args.size > 2) {
		pr_argc = thread->args.size - 1;
	}
	pr_argv = PR_Zone_Malloc (pr, (pr_argc + 1) * 4);
	pr_argv[0] = PR_SetTempString (pr, name);
	for (i = 1; i < pr_argc; i++)
		pr_argv[i] = PR_SetTempString (pr, thread->args.a[1 + i]);
	pr_argv[i] = 0;

	if ((dfunc = PR_FindFunction (pr, ".main"))
		|| (dfunc = PR_FindFunction (pr, "main"))) {
		thread->main_func = dfunc - pr->pr_functions;
	} else {
		PR_Undefined (pr, "function", "main");
	}
	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = pr_argc;
	P_POINTER (pr, 1) = PR_SetPointer (pr, pr_argv);
	pr->pr_argc = 2;
}

static int
run_progs (qwaq_thread_t *thread)
{
	PR_ExecuteProgram (thread->pr, thread->main_func);
	PR_PopFrame (thread->pr);
	return R_INT (thread->pr);
}

static void
usage (int status)
{
	printf ("%s - QuakeForge runtime\n", this_program);
	printf ("sorry, no help yet\n");
	exit (status);
}

static int
parse_argset (int argc, char **argv)
{
	qwaq_thread_t *thread = calloc (1, sizeof (*thread));
	DARRAY_INIT (&thread->args, 8);

	DARRAY_APPEND (&thread->args, 0);
	while (optind < argc && strcmp (argv[optind], "--")) {
		DARRAY_APPEND (&thread->args, argv[optind++]);
	}
	if (optind < argc) {
		optind++;
	}
	DARRAY_APPEND (&thread_data, thread);
	return thread_data.size - 1;
}

static int
parse_args (int argc, char **argv)
{
	int         c;
	qwaq_thread_t *main_thread = calloc (1, sizeof (*main_thread));
	int         qargs_ind = -1;

	DARRAY_INIT (&main_thread->args, 8);

	while ((c = getopt_long (argc, argv,
							 short_options, long_options, 0)) != -1) {
		switch (c) {
			case 1:
				DARRAY_APPEND (&main_thread->args, argv[optind - 1]);
				break;
			case OPT_QARGS:
				if (qargs_ind < 0) {
					qargs_ind = parse_argset (argc, argv);
					thread_data.a[qargs_ind]->args.a[0] = "--qargs";
					goto done;
				} else {
					printf ("more than one set of qargs given");
					exit (1);
				}
				break;
			default:
				usage (1);
		}
	}
done:

	free (thread_data.a[0]->args.a);
	free (thread_data.a[0]);
	thread_data.a[0] = main_thread;

	while (optind < argc) {
		parse_argset (argc, argv);
	}
	return qargs_ind;
}

int
main (int argc, char **argv)
{
	int         qargs_ind = -1;
	int         main_ind = -1;
	int         ret = 0;
	size_t      num_sys = 1;

	this_program = argv[0];

	DARRAY_INIT (&thread_data, 4);
	for (optind = 1; optind < argc; ) {
		parse_argset (argc, argv);
	}
	if (thread_data.size) {
		qwaq_thread_t *thread = thread_data.a[0];
		// the first arg is initialized to null, but this is for getopt, so
		// set to main program name
		thread->args.a[0] = this_program;
		optind = 0;
		qargs_ind = parse_args (thread->args.size, (char **) thread->args.a);
	} else {
		// create a blank main thread set
		qwaq_thread_t *thread = calloc (1, sizeof (*thread));
		DARRAY_INIT (&thread->args, 4);
		DARRAY_APPEND (&thread_data, thread);
	}

	if (qargs_ind >= 0) {
		qwaq_thread_t *qargs = thread_data.a[qargs_ind];
		// the first arg is initialized to --qargs, so
		// set to main program name for now
		qargs->args.a[0] = this_program;
		COM_InitArgv (qargs->args.size, qargs->args.a);
		num_sys++;
	} else {
		qwaq_thread_t qargs = {};
		DARRAY_INIT (&qargs.args, 2);
		DARRAY_APPEND (&qargs.args, this_program);
		COM_InitArgv (qargs.args.size, qargs.args.a);
	}

	init_qf ();

	if (thread_data.a[0]->args.size < 1) {
		DARRAY_APPEND (&thread_data.a[0]->args, "qwaq-app.dat");
	}

	while (thread_data.size < thread_data.a[0]->args.size + num_sys) {
		qwaq_thread_t *thread = calloc (1, sizeof (*thread));
		DARRAY_INIT (&thread->args, 4);
		DARRAY_APPEND (&thread->args, 0);
		DARRAY_APPEND (&thread_data, thread);
	}

	for (size_t i = 1, thread_ind = 0; i < thread_data.size; i++) {
		qwaq_thread_t *thread = thread_data.a[i];
		if (thread->args.size && thread->args.a[0]
			&& strcmp (thread->args.a[0], "--qargs")) {
			// skip the args set that's passed to qargs
			continue;
		}
		if (thread_ind < thread_data.a[0]->args.size) {
			thread->args.a[0] = thread_data.a[0]->args.a[thread_ind++];
		} else {
			printf ("ignoring extra arg sets\n");
			break;
		}
		if (main_ind < 0) {
			main_ind = i;
		}
		spawn_progs (thread);
	}
	if (main_ind >= 0) {
		ret = run_progs (thread_data.a[main_ind]);
	}
	Sys_Shutdown ();
	return ret;
}
