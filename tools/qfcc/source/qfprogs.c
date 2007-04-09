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
	{"globals", no_argument, 0, 'g'},
	{"strings", no_argument, 0, 's'},
	{"fields", no_argument, 0, 'f'},
	{"functions", no_argument, 0, 'F'},
	{"lines", no_argument, 0, 'l'},
	{"modules", no_argument, 0, 'M'},
	{"path", required_argument, 0, 'P'},
	{"verbose", no_argument, 0, 'v'},
	{"numeric", no_argument, 0, 'n'},
	{NULL, 0, NULL, 0},
};

static edict_t *edicts;
static int      num_edicts;
static int      reserved_edicts = 1;
static progs_t  pr;

static pr_debug_header_t debug;
static qfo_t   *qfo;
static dprograms_t progs;

static const char *source_path = "";

static hashtab_t *func_tab;

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

static etype_t
get_auxtype (const char *type)
{
	if (type[0] != 'F') {
		return ev_void;
	} else {
		switch (type[1]) {
			default:
			case 'v':
				return ev_void;
			case '*':
				return ev_string;
			case 'f':
				return ev_float;
			case 'V':
				return ev_vector;
			case 'E':
				return ev_entity;
			case 'F':
				return ev_field;
			case '(':
				return ev_func;
			case ':':
				return ev_sel;
			case '@':	// id
			case '#':	// class
			case '^':
				return ev_pointer;
			case 'Q':
				return ev_quat;
			case 'i':
				return ev_integer;
			case 'I':
				return ev_uinteger;
			case 's':
				return ev_short;
			case '{':
				return ev_struct;
			case '[':
				return ev_array;
		}
	}
}

static void
convert_def (const qfo_def_t *def, ddef_t *ddef)
{
	ddef->type = def->basic_type;
	ddef->ofs = def->ofs;
	ddef->s_name = def->name;
	if (!(def->flags & QFOD_NOSAVE)
		&& !(def->flags & QFOD_CONSTANT)
		&& (def->flags & QFOD_GLOBAL)
		&& def->basic_type != ev_func
		&& def->basic_type != ev_field)
		ddef->type |= DEF_SAVEGLOBAL;
}

static void
convert_qfo (void)
{
	int         i, j, num_locals = 0, num_externs = 0;
	qfo_def_t  *defs;
	ddef_t     *ld;

	defs = malloc (qfo->num_defs * sizeof (qfo_def_t));
	memcpy (defs, qfo->defs, qfo->num_defs * sizeof (qfo_def_t));

	pr.progs = &progs;
	progs.version = PROG_VERSION;

	pr.pr_statements = malloc (qfo->code_size * sizeof (dstatement_t));
	memcpy (pr.pr_statements, qfo->code,
			qfo->code_size * sizeof (dstatement_t));
	progs.numstatements = qfo->code_size;

	pr.pr_strings = qfo->strings;
	progs.numstrings = qfo->strings_size;
	pr.pr_stringsize = qfo->strings_size;

	progs.numfunctions = qfo->num_funcs + 1;
	pr.pr_functions = calloc (progs.numfunctions, sizeof (dfunction_t));
	pr.auxfunctions = calloc (qfo->num_funcs, sizeof (pr_auxfunction_t));
	pr.auxfunction_map = calloc (progs.numfunctions,
								 sizeof (pr_auxfunction_t *));
	ld = pr.local_defs = calloc (qfo->num_defs, sizeof (ddef_t));
	for (i = 0; i < qfo->num_funcs; i++) {
		qfo_func_t *func = qfo->funcs + i;
		dfunction_t df;

		df.first_statement = func->builtin ? -func->builtin : func->code;
		df.parm_start = qfo->data_size;
		df.locals = func->locals_size;
		df.profile = 0;
		df.s_name = func->name;
		df.s_file = func->file;
		df.numparms = func->num_parms;
		memcpy (df.parm_size, func->parm_size, sizeof (df.parm_size));

		if (df.locals > num_locals)
			num_locals = df.locals;

		pr.pr_functions[i + 1] = df;

		pr.auxfunction_map[i + 1] = pr.auxfunctions + i;
		pr.auxfunctions[i].function = i + 1;
		pr.auxfunctions[i].source_line = func->line;
		pr.auxfunctions[i].line_info = func->line_info;
		pr.auxfunctions[i].local_defs = ld - pr.local_defs;
		pr.auxfunctions[i].num_locals = func->num_local_defs;

		for (j = 0; j < func->num_local_defs; j++) {
			qfo_def_t  *d = defs + func->local_defs + j;
			convert_def (d, ld++);
			d->ofs += qfo->data_size;
		}
	}

	progs.numglobaldefs = 0;
	progs.numfielddefs = 0;
	progs.entityfields = 0;
	pr.pr_globaldefs = calloc (qfo->num_defs, sizeof (ddef_t));
	pr.pr_fielddefs = calloc (qfo->num_defs, sizeof (ddef_t));
	for (i = 0; i < qfo->num_defs; i++) {
		qfo_def_t  *def = defs + i;
		ddef_t      ddef;

		if (!(def->flags & QFOD_LOCAL) && def->name) {
			if (def->flags & QFOD_EXTERNAL) {
				int         size = pr_type_size[def->basic_type]; //FIXME struct etc
				if (!size)
					size = 1;
				def->ofs += qfo->data_size + num_locals + num_externs;
				num_externs += size;
			}

			convert_def (def, &ddef);
			pr.pr_globaldefs[progs.numglobaldefs++] = ddef;
			if (ddef.type == ev_field) {
				ddef.type = get_auxtype (qfo->types + def->full_type);
				progs.entityfields += pr_type_size[ddef.type];
				ddef.ofs = qfo->data[ddef.ofs].integer_var;
				pr.pr_fielddefs[progs.numfielddefs++] = ddef;
			}
		}
	}

	progs.numglobals = qfo->data_size;
	pr.globals_size = progs.numglobals + num_locals + num_externs;
	pr.pr_globals = calloc (pr.globals_size, sizeof (pr_type_t));
	memcpy (pr.pr_globals, qfo->data, qfo->data_size * sizeof (pr_type_t));

	for (i = 0; i < qfo->num_defs; i++) {
		qfo_def_t  *def = defs + i;

		for (j = 0; j < def->num_relocs; j++) {
			qfo_reloc_t *reloc = qfo->relocs + def->relocs + j;
			switch ((reloc_type)reloc->type) {
				case rel_none:
					break;
				case rel_op_a_def:
					pr.pr_statements[reloc->ofs].a = def->ofs;
					break;
				case rel_op_b_def:
					pr.pr_statements[reloc->ofs].b = def->ofs;
					break;
				case rel_op_c_def:
					pr.pr_statements[reloc->ofs].c = def->ofs;
					break;
				case rel_op_a_def_ofs:
					pr.pr_statements[reloc->ofs].a += def->ofs;
					break;
				case rel_op_b_def_ofs:
					pr.pr_statements[reloc->ofs].b += def->ofs;
					break;
				case rel_op_c_def_ofs:
					pr.pr_statements[reloc->ofs].c += def->ofs;
					break;
				case rel_def_def:
					pr.pr_globals[reloc->ofs].integer_var = def->ofs;
					break;
				case rel_def_def_ofs:
					pr.pr_globals[reloc->ofs].integer_var += def->ofs;
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
	{disassemble_progs, 0},		// disassemble
	{dump_globals,		0},		// globals
	{dump_strings,		0},		// strings
	{dump_fields,		0},		// fields
	{dump_functions,	0},		// functions
	{dump_lines,		0},		// lines
	{dump_modules,		0},		// modules
};

int
main (int argc, char **argv)
{
	int         c;
	operation_t *func = &operations[0];

	while ((c = getopt_long (argc, argv,
							 "dgsfFlMP:vn", long_options, 0)) != EOF) {
		switch (c) {
			case 'd':
				func = &operations[0];
				break;
			case 'g':
				func = &operations[1];
				break;
			case 's':
				func = &operations[2];
				break;
			case 'f':
				func = &operations[3];
				break;
			case 'F':
				func = &operations[4];
				break;
			case 'l':
				func = &operations[5];
				break;
			case 'M':
				func = &operations[6];
				break;
			case 'P':
				source_path = strdup (optarg);
				break;
			case 'v':
				verbosity++;
				break;
			case 'n':
				sorted = 1;
				break;
			default:
				return 1;
		}
	}
	init_qf ();
	while (optind < argc) {
		if (!load_progs (argv[optind++]))
			return 1;
		if (qfo && func->qfo)
			func->qfo (qfo);
		else
			func->progs (&pr);
	}
	return 0;
}
