/*
	main.c

	Qwaq

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/01

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
#include <pthread.h>
#include <errno.h>
#include <getopt.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/gib.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/ruamoko.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"
#include "rua_internal.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/debugger/debug.h"

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

qwaq_thread_set_t thread_data;

static QFile *
open_file (const char *path, int *len)
{
	QFile      *file = Qopen (path, "rbz");
	char        errbuff[1024];

	if (!file) {
		strerror_r(errno, errbuff, sizeof (errbuff));
		Sys_Printf ("%s\n", errbuff);
		*len = 0;
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
	Qclose (file);
	*_size = size;
	return sym;
}

static void *
allocate_progs_mem (progs_t *pr, int size)
{
	size = (size + 63) & ~63;
#ifdef _WIN32
	return _aligned_malloc (size, 64);
#else
	return aligned_alloc (64, size);
#endif
}

static void
free_progs_mem (progs_t *pr, void *mem)
{
#ifdef _WIN32
	_aligned_free (mem);
#else
	free (mem);
#endif
}

static void
init_qf (void)
{
	qwaq_cbuf = Cbuf_New (&id_interp);

	Sys_Init ();
	COM_ParseConfig (qwaq_cbuf);

	//Cvar_Set (developer, "1");

	Memory_Init (Sys_Alloc (32 * 1024 * 1024), 32 * 1024 * 1024);
	PR_Init_Cvars ();
}

static void
bi_printf (progs_t *pr, void *_res)
{
	dstring_t  *dstr = dstring_new ();

	RUA_Sprintf (pr, dstr, "printf", 0);
	if (dstr->str) {
		Sys_Printf ("%s", dstr->str);
	}
	dstring_delete (dstr);
}

static void
bi_traceon (progs_t *pr, void *_res)
{
	pr->pr_trace = true;
	pr->pr_trace_depth = pr->pr_depth;
}

static void
bi_traceoff (progs_t *pr, void *_res)
{
	pr->pr_trace = false;
}

#define bi(x,n,np,params...) {#x, bi_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t common_builtins[] = {
	bi(printf,   -1, -2, p(string)),
	bi(traceon,  -1, 0),
	bi(traceoff, -1, 0),
	{},
};

static void
common_builtins_init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, common_builtins, 0);
}

static void
qwaq_thread_clear (progs_t *pr, void *_thread)
{
}

static void
qwaq_thread_destroy (progs_t *pr, void *_res)
{
	// resource block is the thread data: don't own it
}

static progs_t *
create_progs (qwaq_thread_t *thread)
{
	progs_t    *pr = calloc (1, sizeof (*pr));
	progsinit_f *funcs = thread->progsinit;

	pr->load_file = load_file;
	pr->allocate_progs_mem = allocate_progs_mem;
	pr->free_progs_mem = free_progs_mem;
	pr->no_exec_limit = 1;
	pr->hashctx = &thread->hashctx;

	pr_debug = 2;
	pr_boundscheck = 0;
	PR_Init (pr);
	PR_Resources_Register (pr, "qwaq_thread", thread, qwaq_thread_clear,
						   qwaq_thread_destroy);
	RUA_Init (pr, thread->rua_security);
	common_builtins_init (pr);
	while (*funcs) {
		(*funcs++) (pr);
	}

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
	pr->zone_size = 8*1024*1024;
	pr->stack_size = 64*1024;
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
	pr_string_t *pr_argv;
	int         pr_argc = 1, i;
	progs_t    *pr;

	thread->main_func = 0;
	pr = thread->pr = create_progs (thread);
	if (thread->args.size) {
		name = thread->args.a[0];
	}

	if (!load_progs (pr, name)) {
		Sys_Error ("couldn't load %s", name);
	}

	if ((dfunc = PR_FindFunction (pr, ".main"))
		|| (dfunc = PR_FindFunction (pr, "main"))) {
		thread->main_func = dfunc - pr->pr_functions;
	} else {
		PR_Undefined (pr, "function", "main");
	}

	if (thread->pr->debug_handler) {
		thread->pr->debug_handler (prd_begin, &thread->main_func,
								   thread->pr->debug_data);
	}

	if (!PR_RunPostLoadFuncs (pr)) {
		PR_Error (pr, "unable to load %s", pr->progs_name);
	}

	PR_PushFrame (pr);
	if (thread->args.size) {
		pr_argc = thread->args.size;
	}
	pr_argv = PR_Zone_Malloc (pr, (pr_argc + 1) * 4);
	pr_argv[0] = PR_SetTempString (pr, name);
	for (i = 1; i < pr_argc; i++) {
		pr_argv[i] = PR_SetTempString (pr, thread->args.a[i]);
	}
	pr_argv[i] = 0;

	PR_SetupParams (pr, 2, 1);
	P_INT (pr, 0) = pr_argc;
	P_POINTER (pr, 1) = PR_SetPointer (pr, pr_argv);
}

static void *
run_progs (void *data)
{
	__auto_type thread = (qwaq_thread_t *) data;

	spawn_progs (thread);
	//Sys_Printf ("starting thread for %s\n", thread->args.a[0]);

	PR_ExecuteProgram (thread->pr, thread->main_func);
	PR_PopFrame (thread->pr);
	thread->return_code = R_INT (thread->pr);
	if (thread->pr->debug_handler) {
		thread->pr->debug_handler (prd_terminate, &thread->return_code,
								   thread->pr->debug_data);
	}
	PR_Shutdown (thread->pr);
	free (thread->pr);
	thread->pr = 0;
	Hash_DelContext (thread->hashctx);
	thread->hashctx = 0;
	Sys_Shutdown ();
	return thread;
}

static void
start_progs_thread (qwaq_thread_t *thread)
{
	pthread_create (&thread->thread_id, 0, run_progs, thread);
}

qwaq_thread_t *
create_thread (void *(*thread_func) (qwaq_thread_t *), void *data)
{
	qwaq_thread_t *thread = calloc (1, sizeof (*thread));

	thread->data = data;
	DARRAY_APPEND (&thread_data, thread);
	pthread_create (&thread->thread_id, 0,
					(void*(*)(void*))thread_func, thread);
	return thread;
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
		const char *args[] = { this_program, 0 };
		COM_InitArgv (1, args);
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

	main_ind = qwaq_init_threads (&thread_data);
	if (main_ind >= 0) {
		// threads might start new threads before the end is reached
		size_t      count = thread_data.size;
		for (size_t i = 0; i < count; i++) {
			if (thread_data.a[i]->progsinit) {
				start_progs_thread (thread_data.a[i]);
			}
		}
		pthread_join (thread_data.a[main_ind]->thread_id, 0);
		ret = thread_data.a[main_ind]->return_code;
	}

	for (size_t i = 0; i < thread_data.size; i++) {
		qwaq_thread_t *thread = thread_data.a[i];
		DARRAY_CLEAR (&thread->args);
		free (thread_data.a[i]);
	}
	DARRAY_CLEAR (&thread_data);
	Cbuf_DeleteStackReverse (qwaq_cbuf);
	Sys_Shutdown ();
	return ret;
}
