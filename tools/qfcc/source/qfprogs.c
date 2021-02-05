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
#include "QF/mathlib.h"
#include "QF/pr_comp.h"
#include "QF/progs.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/qfprogs.h"
#include "tools/qfcc/include/reloc.h"

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
	"def_field_ofs",
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
	{"types", no_argument, 0, 't'},
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
	"t"		// types
	"v"		// verbose
	;

static edict_t *edicts;
static int      num_edicts;
static int      reserved_edicts = 1;
static progs_t  pr;
static qfo_t   *qfo;

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
"    -t, --types         Dump type encodings.\n"
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
load_file (progs_t *pr, const char *name, off_t *_size)
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

static uintptr_t
func_hash (const void *func, void *unused)
{
	return ((dfunction_t *) func)->first_statement;
}

static int
func_compare (const void *f1, const void *f2, void *unused)
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
	Sys_Init ();

	Cvar_Get ("pr_debug", va ("%d", 1+verbosity), 0, 0, "");
	Cvar_Get ("pr_source_path", source_path, 0, 0, "");
	PR_Init_Cvars ();

	pr.edicts = &edicts;
	pr.num_edicts = &num_edicts;
	pr.reserved_edicts = &reserved_edicts;
	pr.file_error = file_error;
	pr.load_file = load_file;
	pr.allocate_progs_mem = allocate_progs_mem;
	pr.free_progs_mem = free_progs_mem;

	PR_Init (&pr);

	func_tab = Hash_NewTable (1021, 0, 0, 0, 0);
	Hash_SetHashCompare (func_tab, func_hash, func_compare);
}

static int
load_progs (const char *name)
{
	QFile      *file;
	int         size;
	pr_uint_t   i;
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
	qfo = 0;
	if (!strcmp (buff, QFO)) {
		qfo = qfo_read (file);
		Qclose (file);

		if (!qfo)
			return 0;

		return 1;
	} else {
		pr.progs_name = name;
		pr.max_edicts = 1;
		pr.zone_size = 0;
		PR_LoadProgsFile (&pr, file, size);
		Qclose (file);

		if (!pr.progs)
			return 0;

		PR_ResolveGlobals (&pr);
		PR_LoadStrings (&pr);
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
	{dump_strings,		qfo_strings},		// strings
	{dump_fields,		qfo_fields},		// fields
	{dump_functions,	qfo_functions},		// functions
	{dump_lines,		qfo_lines},			// lines
	{dump_modules,		0},					// modules
	{0,					qfo_relocs},		// relocs
	{dump_types,		qfo_types},			// types
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
			case 't':
				func = &operations[8];
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
		else if (!qfo && func->progs)
			func->progs (&pr);
		else
			fprintf (stderr, "can't process %s\n", argv[optind - 1]);
	}
	return 0;
}
