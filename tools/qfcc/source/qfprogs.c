/*
	qfprogs.c

	Progs dumping, main file.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/13

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <string.h>
#include <getopt.h>
#include <sys/types.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/fcntl.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/pr_comp.h"
#include "QF/progs.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "obj_file.h"
#include "obj_type.h"
#include "qfprogs.h"
#include "reloc.h"

const char *reloc_names[] = {
	"none",
	"op_a_def",
	"op_b_def",
	"op_c_def",
	"op_a_op",
	"op_b_op",
	"op_c_op",
	"def_op",
	"def_def",
	"def_func",
	"def_string",
	"def_field",
	"op_a_def_ofs",
	"op_b_def_ofs",
	"op_c_def_ofs",
	"def_def_ofs",
};

int         sorted = 0;
int         verbosity = 0;

static const struct option long_options[] = {
	{"disassemble", no_argument, 0, 'd'},
	{"fields", no_argument, 0, 'f'},
	{"functions", no_argument, 0, 'F'},
	{"globals", no_argument, 0, 'g'},
	{"help", no_argument, 0, 'h'},
	{"lines", no_argument, 0, 'l'},
	{"modules", no_argument, 0, 'M'},
	{"numeric", no_argument, 0, 'n'},
	{"path", required_argument, 0, 'P'},
	{"relocs", no_argument, 0, 'r'},
	{"strings", no_argument, 0, 's'},
	{"verbose", no_argument, 0, 'v'},
	{NULL, 0, NULL, 0},
};

static const char *short_options =
	"d"		// disassemble
	"F"		// functions
	"f"		// fields
	"g"		// globals
	"h"		// help
	"l"		// lines
	"M"		// modules
	"n"		// numeric
	"P:"	// path
	"r"		// relocs
	"s"		// strings
	"v"		// verbose
	;

static edict_t *edicts;
static int      num_edicts;
static int      reserved_edicts = 1;
static progs_t  pr;

static pr_debug_header_t debug;
static qfo_t   *qfo;
static dprograms_t progs;

static const char *source_path = "";

static hashtab_t *func_tab;

static void __attribute__((noreturn))
usage (int status)
{
	printf ("%s - QuakeForge progs utility\n", "qfprogs");
	printf ("Usage: %s [options] [files]\n", "qfprogs");
	printf (
"    -d, --disassemble   Dump code disassembly.\n"
"    -f, --fields        Dump entity fields.\n"
"    -F, --functions     Dump functions.\n"
"    -g, --globals       Dump global variables.\n"
"    -h, --help          Display this help and exit\n"
"    -l, --lines         Dump line number information.\n"
"    -M, --modules       Dump Objective-QuakeC data.\n"
"    -n, --numeric       Sort globals by address.\n"
"    -P, --path DIR      Source path.\n"
"    -r, --relocs        Dump reloc information.\n"
"    -s, --strings       Dump static strings.\n"
"    -v, --verbose       Display more output than usual.\n"
    );
	exit (status);
}

static QFile *
open_file (const char *path, int *len)
{
	QFile      *file = Qopen (path, "rbz");

	if (!file)
		return 0;
	*len = Qfilesize (file);
	return file;
}

static void
file_error (progs_t *pr, const char *name)
{
	perror (name);
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
		if (!file)
			return 0;
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

static uintptr_t
func_hash (void *func, void *unused)
{
	return ((dfunction_t *) func)->first_statement;
}

static int
func_compare (void *f1, void *f2, void *unused)
{
	return ((dfunction_t *) f1)->first_statement
			== ((dfunction_t *) f2)->first_statement;
}

dfunction_t *
func_find (int st_ofs)
{
	dfunction_t f;

	f.first_statement = st_ofs;
	return Hash_FindElement (func_tab, &f);
}

static void
init_qf (void)
{
	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cvar_Init ();
	Sys_Init_Cvars ();
	Cmd_Init ();

	Cvar_Get ("pr_debug", va ("%d", verbosity), 0, 0, "");
	Cvar_Get ("pr_source_path", source_path, 0, 0, "");
	PR_Init_Cvars ();
	PR_Init ();

	pr.edicts = &edicts;
	pr.num_edicts = &num_edicts;
	pr.reserved_edicts = &reserved_edicts;
	pr.file_error = file_error;
	pr.load_file = load_file;
	pr.allocate_progs_mem = allocate_progs_mem;
	pr.free_progs_mem = free_progs_mem;

	func_tab = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (func_tab, func_hash, func_compare);
}

#define P(t,o) ((t *)((char *)pr.progs + pr.progs->o))
static void
convert_qfo (void)
{
	int         size;
	int         i;
	ddef_t     *ld;

	pr.progs = qfo_to_progs (qfo, &size);

	pr.pr_statements = P (dstatement_t, ofs_statements);
	pr.pr_strings = P (char, ofs_strings);
	pr.pr_stringsize = pr.progs->numstrings;
	pr.pr_functions = P (dfunction_t, ofs_functions);
	pr.pr_globaldefs = P (ddef_t, ofs_globaldefs);
	pr.pr_fielddefs = P (ddef_t, ofs_fielddefs);
	pr.pr_globals = P (pr_type_t, ofs_globals);
	pr.globals_size = pr.progs->numglobals;

	pr.auxfunctions = calloc (qfo->num_funcs, sizeof (pr_auxfunction_t));
	pr.auxfunction_map = calloc (progs.numfunctions,
								 sizeof (pr_auxfunction_t *));
	ld = pr.local_defs = calloc (qfo->num_defs, sizeof (ddef_t));
	for (i = 0; i < qfo->num_funcs; i++) {
		qfo_func_t *func = qfo->funcs + i;

		pr.auxfunction_map[i + 1] = pr.auxfunctions + i;
		pr.auxfunctions[i].function = i + 1;
		pr.auxfunctions[i].source_line = func->line;
		pr.auxfunctions[i].line_info = func->line_info;
		pr.auxfunctions[i].local_defs = ld - pr.local_defs;
		pr.auxfunctions[i].num_locals =
			qfo->spaces[func->locals_space].num_defs;
	}
#if 0
	for (i = 0; i < qfo->num_defs; i++) {
		int         j;
		qfo_def_t  *def = defs + i;

		for (j = 0; j < def->num_relocs; j++) {
			qfo_reloc_t *reloc = qfo->relocs + def->relocs + j;
			switch ((reloc_type)reloc->type) {
				case rel_none:
					break;
				case rel_op_a_def:
					pr.pr_statements[reloc->offset].a = def->offset;
					break;
				case rel_op_b_def:
					pr.pr_statements[reloc->offset].b = def->offset;
					break;
				case rel_op_c_def:
					pr.pr_statements[reloc->offset].c = def->offset;
					break;
				case rel_op_a_def_ofs:
					pr.pr_statements[reloc->offset].a += def->offset;
					break;
				case rel_op_b_def_ofs:
					pr.pr_statements[reloc->offset].b += def->offset;
					break;
				case rel_op_c_def_ofs:
					pr.pr_statements[reloc->offset].c += def->offset;
					break;
				case rel_def_def:
					pr.pr_globals[reloc->offset].integer_var = def->offset;
					break;
				case rel_def_def_ofs:
					pr.pr_globals[reloc->offset].integer_var += def->offset;
					break;
				// these are relative and fixed up before the .qfo is written
				case rel_op_a_op:
				case rel_op_b_op:
				case rel_op_c_op:
				// these aren't relevant here
				case rel_def_func:
				case rel_def_op:
				case rel_def_string:
				case rel_def_field:
				case rel_def_field_ofs:
					break;
			}
		}
	}
#endif
	pr.pr_edict_size = progs.entityfields * 4;

	pr.linenos = qfo->lines;
	debug.num_auxfunctions = qfo->num_funcs;
	debug.num_linenos = qfo->num_lines;
	debug.num_locals = ld - pr.local_defs;

	if (verbosity)
		pr.debug = &debug;

}

static int
load_progs (const char *name)
{
	QFile      *file;
	int         i, size;
	char        buff[5];

	Hash_FlushTable (func_tab);

	file = open_file (name, &size);
	if (!file) {
		perror (name);
		return 0;
	}
	Qread (file, buff, 4);
	buff[4] = 0;
	Qseek (file, 0, SEEK_SET);
	if (!strcmp (buff, QFO)) {
		qfo = qfo_read (file);
		Qclose (file);

		if (!qfo)
			return 0;

		convert_qfo ();
	} else {
		pr.progs_name = name;
		PR_LoadProgsFile (&pr, file, size, 1, 0);
		Qclose (file);

		if (!pr.progs)
			return 0;

		PR_LoadStrings (&pr);
		PR_ResolveGlobals (&pr);
		PR_LoadDebug (&pr);
	}
	for (i = 0; i < pr.progs->numfunctions; i++) {
		// don't bother with builtins
		if (pr.pr_functions[i].first_statement > 0)
			Hash_AddElement (func_tab, &pr.pr_functions[i]);
	}
	return 1;
}

typedef struct {
	void      (*progs) (progs_t *pr);
	void      (*qfo) (qfo_t *qfo);
} operation_t;

operation_t operations[] = {
	{disassemble_progs, 0},					// disassemble
	{dump_globals,		qfo_globals},		// globals
	{dump_strings,		0},					// strings
	{dump_fields,		0},					// fields
	{dump_functions,	qfo_functions},		// functions
	{dump_lines,		0},					// lines
	{dump_modules,		0},					// modules
	{0,					qfo_relocs},		// relocs
};

int
main (int argc, char **argv)
{
	int         c;
	operation_t *func = &operations[0];

	while ((c = getopt_long (argc, argv, short_options,
							 long_options, 0)) != EOF) {
		switch (c) {
			case 'd':
				func = &operations[0];
				break;
			case 'F':
				func = &operations[4];
				break;
			case 'f':
				func = &operations[3];
				break;
			case 'g':
				func = &operations[1];
				break;
			case 'h':
				usage (0);
			case 'l':
				func = &operations[5];
				break;
			case 'M':
				func = &operations[6];
				break;
			case 'n':
				sorted = 1;
				break;
			case 'P':
				source_path = strdup (optarg);
				break;
			case 'r':
				func = &operations[7];
				break;
			case 's':
				func = &operations[2];
				break;
			case 'v':
				verbosity++;
				break;
			default:
				usage (1);
		}
	}
	init_qf ();
	while (optind < argc) {
		if (!load_progs (argv[optind++]))
			return 1;
		if (qfo && func->qfo)
			func->qfo (qfo);
		else if (func->progs)
			func->progs (&pr);
		else
			fprintf (stderr, "can't process %s\n", argv[optind - 1]);
	}
	return 0;
}
