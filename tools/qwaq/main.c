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

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/progs.h>
#include <QF/quakefs.h>
#include "QF/ruamoko.h"
#include <QF/sys.h>
#include "QF/va.h"
#include <QF/zone.h>

#include "qwaq.h"

#define MAX_EDICTS 1024

static edict_t *edicts;
static int num_edicts;
static int reserved_edicts;
static progs_t pr;

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
load_file (progs_t *pr, const char *name)
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
	Sys_Init ();
	//Cvar_Get ("developer", "128", 0, 0, 0);

	Memory_Init (malloc (1024 * 1024), 1024 * 1024);

	Cvar_Get ("pr_debug", "2", 0, 0, 0);
	Cvar_Get ("pr_boundscheck", "0", 0, 0, 0);

	pr.pr_edicts = &edicts;
	pr.num_edicts = &num_edicts;
	pr.reserved_edicts = &reserved_edicts;
	pr.load_file = load_file;
	pr.allocate_progs_mem = allocate_progs_mem;
	pr.free_progs_mem = free_progs_mem;
	pr.no_exec_limit = 1;

	PR_Init_Cvars ();
	PR_Init ();
	RUA_Init (&pr, 0);
	PR_Cmds_Init(&pr);
	BI_Init (&pr);
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
	pr.progs_name = name;
	PR_LoadProgsFile (&pr, file, size, 1, 1024 * 1024);
	Qclose (file);
	if (!PR_RunLoadFuncs (&pr))
		PR_Error (&pr, "unable to load %s", pr.progs_name);
	return 1;
}

int
main (int argc, char **argv)
{
	dfunction_t *dfunc;
	func_t      main_func = 0;
	const char *name = "qwaq.dat";
	string_t   *pr_argv;
	int         pr_argc = 1, i;

	init_qf ();

	if (argc > 1)
		name = argv[1];

	if (!load_progs (name))
		Sys_Error ("couldn't load %s", "qwaq.dat");

	PR_PushFrame (&pr);
	if (argc > 2)
		pr_argc = argc - 1;
	pr_argv = PR_Zone_Malloc (&pr, (pr_argc + 1) * 4);
	pr_argv[0] = PR_SetTempString (&pr, name);
	for (i = 1; i < pr_argc; i++)
		pr_argv[i] = PR_SetTempString (&pr, argv[1 + i]);
	pr_argv[i] = 0;

	if ((dfunc = PR_FindFunction (&pr, ".main"))
		|| (dfunc = PR_FindFunction (&pr, "main")))
		main_func = dfunc - pr.pr_functions;
	else
		PR_Undefined (&pr, "function", "main");
	PR_RESET_PARAMS (&pr);
	P_INT (&pr, 0) = pr_argc;
	P_POINTER (&pr, 1) = PR_SetPointer (&pr, pr_argv);
	PR_ExecuteProgram (&pr, main_func);
	PR_PopFrame (&pr);
	return R_INT (&pr);
}
