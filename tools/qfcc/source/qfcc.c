/* 
	qfcc.c

	QuakeForge Code Compiler (main program)

	Copyright (C) 1996-1997 id Software, Inc.
	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>
	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_PROCESS_H
# include <process.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include <QF/crc.h>
#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/qendian.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "cmdlib.h"
#include "qfcc.h"
#include "expr.h"
#include "class.h"
#include "type.h"

options_t   options;

char       *sourcedir;
char       *progs_src;
const char *this_program;

static struct option const long_options[] = {
	{"source", required_argument, 0, 's'},
	{"progs-src", required_argument, 0, 'P'},
	{"save-temps", no_argument, 0, 'S'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"code", required_argument, 0, 'C'},
	{"warn", required_argument, 0, 'W'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"files", no_argument, 0, 'F'},
	{"traditional", no_argument, 0, 't'},
	{"strip-path", required_argument, 0, 'p'},
	{"define", required_argument, 0, 'D'},
	{"include", required_argument, 0, 'I'},
	{"undefine", required_argument, 0, 'U'},
	{"cpp", required_argument, 0, 256},
	{NULL, 0, NULL, 0}
};

char        destfile[1024];
char        debugfile[1024];

typedef struct cpp_arg_s {
	struct cpp_arg_s *next;
	const char *arg;
} cpp_arg_t;

cpp_arg_t  *cpp_arg_list;
cpp_arg_t **cpp_arg_tail = &cpp_arg_list;
cpp_arg_t  *cpp_def_list;
cpp_arg_t **cpp_def_tail = &cpp_def_list;
const char **cpp_argv;
char       *cpp_name = CPP_NAME;
static int  cpp_argc = 0;
dstring_t  *tempname;

pr_info_t   pr;

int         pr_source_line;
int         pr_error_count;

float       pr_globals[MAX_REGS];
int         numpr_globals;

char        strings[MAX_STRINGS];
int         strofs;

dstatement_t statements[MAX_STATEMENTS];
int         numstatements;
int         statement_linenums[MAX_STATEMENTS];

function_t *pr_functions;
dfunction_t functions[MAX_FUNCTIONS];
int         numfunctions;

ddef_t      globals[MAX_GLOBALS];
int         numglobaldefs;
int         num_localdefs;
const char *big_function = 0;

ddef_t      fields[MAX_FIELDS];
int         numfielddefs;


/*
	CopyString

	Return an offset from the string heap
*/
static hashtab_t *strings_tab;

static const char *
stings_get_key (void *_str, void *unsued)
{
	return (char *) _str;
}

int
CopyString (const char *str)
{
	int         old;

	if (!strings_tab) {
		strings_tab = Hash_NewTable (16381, stings_get_key, 0, 0);
		Hash_Add (strings_tab, strings);
	}
	old = strofs;
	strcpy (strings + strofs, str);
	strofs += strlen (str) + 1;
	Hash_Add (strings_tab, strings + old);
	return old;
}

int
ReuseString (const char *str)
{
	char       *s;

	if (!strings_tab)
		return CopyString (str);
	s = Hash_Find (strings_tab, str);
	if (s)
		return s - strings;
	return CopyString (str);
}

void
InitData (void)
{
	int         i;

	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;

	def_ret.ofs = OFS_RETURN;
	for (i = 0; i < MAX_PARMS; i++)
		def_parms[i].ofs = OFS_PARM0 + 3 * i;
}


void
WriteData (int crc)
{
	def_t      *def;
	ddef_t     *dd;
	dprograms_t progs;
	pr_debug_header_t debug;
	FILE       *h;
	int         i;

	for (def = pr.def_head.def_next; def; def = def->def_next) {
		if (def->scope)
			continue;
		if (def->type->type == ev_func) {
//          df = &functions[numfunctions];
//          numfunctions++;
		} else if (def->type->type == ev_field) {
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = ReuseString (def->name);
			dd->ofs = G_INT (def->ofs);
		}

		dd = &globals[numglobaldefs];
		numglobaldefs++;
		dd->type = def->type->type;

		if (!def->constant
			&& def->type->type != ev_func
			&& def->type->type != ev_field && def->scope == NULL)
			dd->type |= DEF_SAVEGLOBAL;
		dd->s_name = ReuseString (def->name);
		dd->ofs = def->ofs;
	}

	strofs = (strofs + 3) & ~3;

	if (options.verbosity >= 0) {
		printf ("%6i strofs\n", strofs);
		printf ("%6i statements\n", numstatements);
		printf ("%6i functions\n", numfunctions);
		printf ("%6i globaldefs\n", numglobaldefs);
		printf ("%6i locals size (%s)\n", num_localdefs, big_function);
		printf ("%6i fielddefs\n", numfielddefs);
		printf ("%6i pr_globals\n", numpr_globals);
		printf ("%6i entityfields\n", pr.size_fields);
	}

	h = SafeOpenWrite (destfile);
	SafeWrite (h, &progs, sizeof (progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = strofs;
	SafeWrite (h, strings, strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = numstatements;
	for (i = 0; i < numstatements; i++) {
		statements[i].op = LittleShort (statements[i].op);
		statements[i].a = LittleShort (statements[i].a);
		statements[i].b = LittleShort (statements[i].b);
		statements[i].c = LittleShort (statements[i].c);
	}
	SafeWrite (h, statements, numstatements * sizeof (dstatement_t));

	progs.ofs_functions = ftell (h);
	progs.numfunctions = numfunctions;
	for (i = 0; i < numfunctions; i++) {
		functions[i].first_statement =
			LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong (functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}
	SafeWrite (h, functions, numfunctions * sizeof (dfunction_t));

	progs.ofs_globaldefs = ftell (h);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0; i < numglobaldefs; i++) {
		globals[i].type = LittleShort (globals[i].type);
		globals[i].ofs = LittleShort (globals[i].ofs);
		globals[i].s_name = LittleLong (globals[i].s_name);
	}
	SafeWrite (h, globals, numglobaldefs * sizeof (ddef_t));

	progs.ofs_fielddefs = ftell (h);
	progs.numfielddefs = numfielddefs;
	for (i = 0; i < numfielddefs; i++) {
		fields[i].type = LittleShort (fields[i].type);
		fields[i].ofs = LittleShort (fields[i].ofs);
		fields[i].s_name = LittleLong (fields[i].s_name);
	}
	SafeWrite (h, fields, numfielddefs * sizeof (ddef_t));

	progs.ofs_globals = ftell (h);
	progs.numglobals = numpr_globals;
	for (i = 0; i < numpr_globals; i++)
		((int *) pr_globals)[i] = LittleLong (((int *) pr_globals)[i]);
	SafeWrite (h, pr_globals, numpr_globals * 4);

	if (options.verbosity >= -1)
		printf ("%6i TOTAL SIZE\n", (int) ftell (h));

	progs.entityfields = pr.size_fields;

	progs.version = options.code.progsversion;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof (progs) / 4; i++)
		((int *) &progs)[i] = LittleLong (((int *) &progs)[i]);

	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &progs, sizeof (progs));
	fclose (h);

	if (!options.code.debug) {
		return;
	}

	h = SafeOpenRead (destfile);

	debug.version = LittleLong (PROG_DEBUG_VERSION);
	CRC_Init (&debug.crc);
	while ((i = fgetc (h)) != EOF)
		CRC_ProcessByte (&debug.crc, i);
	fclose (h);
	debug.crc = LittleShort (debug.crc);
	debug.you_tell_me_and_we_will_both_know = 0;

	h = SafeOpenWrite (debugfile);
	SafeWrite (h, &debug, sizeof (debug));

	debug.auxfunctions = LittleLong (ftell (h));
	debug.num_auxfunctions = LittleLong (num_auxfunctions);
	for (i = 0; i < num_auxfunctions; i++) {
		auxfunctions[i].function = LittleLong (auxfunctions[i].function);
		auxfunctions[i].source_line = LittleLong (auxfunctions[i].source_line);
		auxfunctions[i].line_info = LittleLong (auxfunctions[i].line_info);
		auxfunctions[i].local_defs = LittleLong (auxfunctions[i].local_defs);
		auxfunctions[i].num_locals = LittleLong (auxfunctions[i].num_locals);
	}
	SafeWrite (h, auxfunctions, num_auxfunctions * sizeof (auxfunctions[0]));

	debug.linenos = LittleLong (ftell (h));
	debug.num_linenos = LittleLong (num_linenos);
	for (i = 0; i < num_linenos; i++) {
		linenos[i].fa.addr = LittleLong (linenos[i].fa.addr);
		linenos[i].line = LittleLong (linenos[i].line);
	}
	SafeWrite (h, linenos, num_linenos * sizeof (linenos[0]));

	debug.locals = LittleLong (ftell (h));
	debug.num_locals = LittleLong (num_locals);
	for (i = 0; i < num_locals; i++) {
		locals[i].type = LittleShort (locals[i].type);
		locals[i].ofs = LittleShort (locals[i].ofs);
		locals[i].s_name = LittleLong (locals[i].s_name);
	}
	SafeWrite (h, locals, num_locals * sizeof (locals[0]));

	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &debug, sizeof (debug));
	fclose (h);
}


/*
	PR_String

	Returns a string suitable for printing (no newlines, max 60 chars length)
*/
char *
PR_String (char *string)
{
	static char buf[80];
	char       *s;

	s = buf;
	*s++ = '"';
	while (string && *string) {

		if (s == buf + sizeof (buf) - 2)
			break;

		if (*string == '\n') {
			*s++ = '\\';
			*s++ = 'n';
		} else if (*string == '"') {
			*s++ = '\\';
			*s++ = '"';
		} else {
			*s++ = *string;
		}
		string++;

		if (s - buf > 60) {
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;

	return buf;
}


def_t *
PR_DefForFieldOfs (gofs_t ofs)
{
	def_t      *d;

	for (d = pr.def_head.def_next; d; d = d->def_next) {
		if (d->type->type != ev_field)
			continue;
		if (*((int *) &pr_globals[d->ofs]) == ofs)
			return d;
	}

	Error ("PR_DefForFieldOfs: couldn't find %i", ofs);
	return NULL;
}


/*
	PR_BeginCompilation

	called before compiling a batch of files, clears the pr struct
*/
void
PR_BeginCompilation (void)
{
	int         i;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;

	for (i = 0; i < RESERVED_OFS; i++)
		pr_global_defs[i] = &def_void;

	pr_error_count = 0;
}

void
PR_RelocateRefs (def_t *def)
{
	statref_t  *ref;
	int        *d;

	for (ref = def->refs; ref; ref = ref->next) {
		switch (ref->field) {
			case 0:
				ref->statement->a = def->ofs;
				break;
			case 1:
				ref->statement->b = def->ofs;
				break;
			case 2:
				ref->statement->c = def->ofs;
				break;
			case 4:
				d = (int*)ref->statement;
				*d += def->ofs;
				break;
			default:
				abort ();
		}
	}
}

/*
	PR_FinishCompilation

	called after all files are compiled to check for errors.
	Returns false if errors were detected.
*/
qboolean PR_FinishCompilation (void)
{
	def_t      *d;
	qboolean    errors = false;
	function_t *f;
	def_t      *def;
	expr_t      e;

	class_finish_module ();
	// check to make sure all functions prototyped have code
	if (options.warnings.undefined_function)
		for (d = pr.def_head.def_next; d; d = d->def_next) {
			if (d->type->type == ev_func && !d->scope) {	// function args ok
				if (d->used) {
					if (!d->initialized) {
						warning (0, "function %s was called but not defined\n",
								 d->name);
					}
				}
			}
		}

	if (errors)
		return !errors;

	if (options.code.debug) {
		e.type = ex_string;
		e.e.string_val = debugfile;
		PR_ReuseConstant (&e, PR_GetDef (&type_string, ".debug_file", 0,
										 &numpr_globals));
	}

	for (def = pr.def_head.def_next; def; def = def->def_next) {
		if (def->scope || def->absolute)
			continue;
		PR_RelocateRefs (def);
	}

	for (f = pr_functions; f; f = f->next) {
		if (f->builtin)
			continue;
		if (f->def->locals > num_localdefs) {
			num_localdefs = f->def->locals;
			big_function = f->def->name;
		}
		f->dfunc->parm_start = numpr_globals;
		for (def = f->def->scope_next; def; def = def->scope_next) {
			if (def->absolute)
				continue;
			def->ofs += numpr_globals;
			PR_RelocateRefs (def);
		}
	}
	numpr_globals += num_localdefs;

	return !errors;
}

//=============================================================================


void
PR_PrintFunction (def_t *def)
{
	def_t      *d;
	statref_t  *r;

	printf ("%s\n", def->name);
	for (d = def->scope_next; d; d = d->scope_next) {
		printf ("%s: %d %d %d\n",
				d->name ? d->name : "<temp>",
				d->ofs, d->type->type, pr_type_size[d->type->type]);
		for (r = d->refs; r; r = r->next)
			printf (" %ld", (long) (r->statement - statements));
		printf ("\n");
	}
}

static void
usage (int status)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Usage: %s [options]\n", this_program);
	printf ("Options:\n"
			"    -s, --source DIR          Look for progs.src in DIR instead of \".\"\n"
			"    -q, --quiet               Inhibit usual output\n"
			"    -v, --verbose             Display more output than usual\n"
			"    -g,                       Generate debuggin info\n"
			"    -C, --code OPTION,...     Set code generation options\n"
			"    -W, --warn OPTION,...     Set warning options\n"
			"    -h, --help                Display this help and exit\n"
			"    -V, --version             Output version information and exit\n\n"
			"    -S, --save-temps          Do not delete temporary files\n"
			"    -D, --define SYMBOL[=VAL],...  Define symbols for the preprocessor\n"
			"    -I, --include DIR,...          Set directories for the preprocessor \n"
			"                                   to search for #includes\n"
			"    -U, --undefine SYMBOL,...      Undefine preprocessor symbols\n\n"
			"For help on options for --code and --warn, see the qfcc(1) manual page\n");
	exit (status);
}

static void
add_cpp_arg (const char *arg)
{
	cpp_arg_t  *cpp_arg = malloc (sizeof (cpp_arg_t));
	cpp_arg->next = 0;
	cpp_arg->arg = arg;
	*cpp_arg_tail = cpp_arg;
	cpp_arg_tail = &(*cpp_arg_tail)->next;
	cpp_argc++;
}

static void
add_cpp_def (const char *arg)
{
	cpp_arg_t  *cpp_def = malloc (sizeof (cpp_arg_t));
	cpp_def->next = 0;
	cpp_def->arg = arg;
	*cpp_def_tail = cpp_def;
	cpp_def_tail = &(*cpp_def_tail)->next;
	cpp_argc++;
}

static void
parse_cpp_name ()
{
	char       *n;

	n = strdup (cpp_name);
	while (*n) {
		while (*n && *n == ' ')
			n++;
		add_cpp_arg (n);
		while (*n && *n != ' ')
			n++;
		if (*n)
			*n++ = 0;
	}
}

static void
build_cpp_args (const char *in_name, const char *out_name)
{
	cpp_arg_t  *cpp_arg;
	cpp_arg_t  *cpp_def;
	const char **arg;

	if (cpp_argv)
		free (cpp_argv);
	cpp_argv = (const char **)malloc ((cpp_argc + 1) * sizeof (char**));
	for (arg = cpp_argv, cpp_arg = cpp_arg_list;
		 cpp_arg;
		 cpp_arg = cpp_arg->next) {
		if (!strcmp (cpp_arg->arg, "%d")) {
			for (cpp_def = cpp_def_list; cpp_def; cpp_def = cpp_def->next)
				*arg++ = cpp_def->arg;
		} else if (!strcmp (cpp_arg->arg, "%i")) {
			*arg++ = in_name;
		} else if (!strcmp (cpp_arg->arg, "%o")) {
			*arg++ = out_name;
		} else {
			*arg++ = cpp_arg->arg;
		}
	}
	*arg = 0;
	//for (arg = cpp_argv; *arg; arg++)
	//	printf ("%s ", *arg);
	//puts ("");
}

static int
DecodeArgs (int argc, char **argv)
{
	int         c;

	add_cpp_def ("-D__QFCC__=1");
	add_cpp_def ("-D__QUAKEC__=1");
	add_cpp_def ("-D__RUAMOKO__=1");
	add_cpp_def ("-D__RAUMOKO__=1");

	options.code.progsversion = PROG_VERSION;
	options.warnings.uninited_variable = true;

	options.save_temps = false;
	options.verbosity = 0;
	options.strip_path = 0;

	sourcedir = "";
	progs_src = "progs.src";

	while ((c = getopt_long (argc, argv, "s:"	// source dir
										 "P:"	// progs.src name
										 "F"	// generate files.dat
										 "q"	// quiet
										 "v"	// verbose
										 "g"	// debug
										 "C:"	// code options
										 "W:"	// warning options
										 "h"	// help
										 "V"	// version
										 "p:"	// strip path
										 "S"	// save temps
										 "D:"	// define
										 "I:"	// set includes
										 "U:"	// undefine
							 , long_options, (int *) 0)) != EOF) {
		switch (c) {
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE, VERSION);
				exit (0);
				break;
			case 's':					// src dir
				sourcedir = strdup (optarg);
				break;
			case 'P':					// progs-src
				progs_src = strdup (optarg);
				break;
			case 'p':
				options.strip_path = atoi (optarg);
				break;
			case 'F':
				options.files_dat = true;
				break;
			case 'q':					// quiet
				options.verbosity -= 1;
				break;
			case 'v':					// verbose
				options.verbosity += 1;
				break;
			case 'g':					// debug
				options.code.debug = true;
				break;
			case 't':					// traditional
				options.traditional = true;
				options.code.progsversion = PROG_ID_VERSION;
				break;
			case 'C':{					// code options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "cow"))) {
							options.code.cow = true;
						} else if (!(strcasecmp (temp, "no-cow"))) {
							options.code.cow = false;
						} else if (!(strcasecmp (temp, "no-cpp"))) {
							cpp_name = 0;
						} else if (!(strcasecmp (temp, "debug"))) {
							options.code.debug = true;
						} else if (!(strcasecmp (temp, "no-debug"))) {
							options.code.debug = false;
						} else if (!(strcasecmp (temp, "v6only"))) {
							options.code.progsversion = PROG_ID_VERSION;
							add_cpp_def ("-D__VERSION6__=1");
						} else if (!(strcasecmp (temp, "no-v6only"))) {
							options.code.progsversion = PROG_VERSION;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'W':{					// warning options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "all"))) {
							options.warnings.cow = true;
							options.warnings.undefined_function = true;
							options.warnings.uninited_variable = true;
							options.warnings.vararg_integer = true;
							options.warnings.integer_divide = true;
						} else if (!(strcasecmp (temp, "none"))) {
							options.warnings.cow = false;
							options.warnings.undefined_function = false;
							options.warnings.uninited_variable = false;
							options.warnings.vararg_integer = false;
							options.warnings.integer_divide = false;
						} else if (!(strcasecmp (temp, "cow"))) {
							options.warnings.cow = true;
						} else if (!(strcasecmp (temp, "no-cow"))) {
							options.warnings.cow = false;
						} else if (!(strcasecmp (temp, "error"))) {
							options.warnings.promote = true;
						} else if (!(strcasecmp (temp, "no-error"))) {
							options.warnings.promote = false;
						} else if (!(strcasecmp (temp, "undef-function"))) {
							options.warnings.undefined_function = true;
						} else if (!(strcasecmp (temp, "no-undef-function"))) {
							options.warnings.undefined_function = false;
						} else if (!(strcasecmp (temp, "uninited-var"))) {
							options.warnings.uninited_variable = true;
						} else if (!(strcasecmp (temp, "no-uninited-var"))) {
							options.warnings.uninited_variable = false;
						} else if (!(strcasecmp (temp, "vararg-integer"))) {
							options.warnings.vararg_integer = true;
						} else if (!(strcasecmp (temp, "no-vararg-integer"))) {
							options.warnings.vararg_integer = false;
						} else if (!(strcasecmp (temp, "integer-divide"))) {
							options.warnings.integer_divide = true;
						} else if (!(strcasecmp (temp, "no-integer-divide"))) {
							options.warnings.integer_divide = false;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 256:					// --cpp=
				cpp_name = strdup (optarg);
				break;
			case 'S':					// save temps
				options.save_temps = true;
				break;
			case 'D':{					// defines for cpp
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (strdup (va ("%s%s", "-D", temp)));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'I':{					// includes
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (strdup (va ("%s%s", "-I", temp)));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'U':{					// undefines
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (strdup (va ("%s%s", "-U", temp)));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			default:
				usage (1);
		}
	}
	return optind;
}

//============================================================================

FILE *
preprocess_file (const char *filename)
{
#ifndef _WIN32
	pid_t       pid;
	int         tempfd = 0;
#endif
	char       *temp1;
	char       *temp2 = strrchr (this_program, PATH_SEPARATOR);

	if (cpp_name) {
		if (options.save_temps) {
			char	*basename = strdup (filename);
			char	*temp;
		
			temp = strrchr (basename, '.');
			if (temp)
				*temp = '\0';	// ignore the rest of the string

			temp = strrchr (basename, '/');
			if (!temp)
				temp = basename;
			else
				temp++;

			if (*sourcedir) {
				dsprintf (tempname, "%s%c%s", sourcedir,
						  PATH_SEPARATOR, temp);
			} else {
				dsprintf (tempname, "%s.p", temp);
			}
			free (basename);
		} else {
			temp1 = getenv ("TMPDIR");
			if ((!temp1) || (!temp1[0])) {
				temp1 = getenv ("TEMP");
				if ((!temp1) || (!temp1[0])) {
					temp1 = "/tmp";
				}
			}

			dsprintf (tempname, "%s%c%sXXXXXX", temp1,
					  PATH_SEPARATOR, temp2 ? temp2 + 1 : this_program);
		}
		build_cpp_args (filename, tempname->str);

#ifdef _WIN32
		if (!options.save_temps)
			mktemp (tempname->str);

		{
			FILE       *tmp = fopen (tempname->str, "wt");

			fclose (tmp);
		}

		{
			int		status = spawnvp (_P_WAIT, cpp_argv[0], (char **)cpp_argv);

			if (status) {
				fprintf (stderr, "%s: cpp returned error code %d\n",
						filename,
						status);
				exit (1);
			}
		}

		return fopen (tempname->str, "rt");
#else
		if (!options.save_temps)
			tempfd = mkstemp (tempname->str);

		if ((pid = fork ()) == -1) {
			perror ("fork");
			exit (1);
		}
		if (!pid) {
			// we're a child, check for abuse
			//const char **a;
			//for (a = cpp_argv; *a; a++)
			//	printf ("%s ", *a);
			//puts("");
			execvp (cpp_argv[0], (char **)cpp_argv);
			fprintf (stderr, "Child shouldn't reach here\n");
			exit (1);
		} else {
			// give parental guidance (or bury it in the back yard)
			int         status;
			pid_t       rc;

//			printf ("pid = %d\n", pid);
			if ((rc = waitpid (0, &status, 0 | WUNTRACED)) != pid) {
				if (rc == -1) {
					perror ("wait");
					exit (1);
				}
				fprintf (stderr, "%s: The wrong child (%ld) died. Don't ask me, I don't know either.\n",
						this_program,
						(long) rc);
				exit (1);
			}
			if (WIFEXITED (status)) {
				if (WEXITSTATUS (status)) {
					fprintf (stderr, "%s: cpp returned error code %d\n",
							filename,
							WEXITSTATUS (status));
					exit (1);
				}
			} else {
				fprintf (stderr, "%s: cpp returned prematurely.\n", filename);
				exit (1);
			}
		}
		if (options.save_temps)
			return fopen (tempname->str, "rt");
		else
			return fdopen (tempfd, "r+t");
#endif
	}
	return fopen (filename, "rt");
}

const char *
strip_path (const char *filename)
{
	const char *p = filename;
	int         i = options.strip_path;

	while (i-- > 0) {
		while (*p && *p != '/')
			p++;
		if (!*p)
			break;
		filename = ++p;
	}
	return filename;
}

/*
	main

	The nerve center of our little operation
*/
int
main (int argc, char **argv)
{
	char       *src;
	dstring_t  *filename = dstring_newstr ();
	int         crc = 0;
	double      start, stop;

	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);

	tempname = dstring_new ();
	parse_cpp_name ();

	if (options.verbosity >= 1 && strcmp (sourcedir, "")) {
		printf ("Source directory: %s\n", sourcedir);
	}
	if (options.verbosity >= 1 && strcmp (progs_src, "progs.src")) {
		printf ("progs.src: %s\n", progs_src);
	}

	PR_Opcode_Init_Tables ();

	InitData ();
	init_types ();

	if (*sourcedir)
		dsprintf (filename, "%s/%s", sourcedir, progs_src);
	else
		dsprintf (filename, "%s", progs_src);
	LoadFile (filename->str, (void *) &src);

	if (!(src = Parse (src)))
		Error ("No destination filename.  qfcc --help for info.\n");

	strcpy (destfile, qfcc_com_token);
	if (options.verbosity >= 1) {
		printf ("outputfile: %s\n", destfile);
	}
	if (options.code.debug) {
		char       *s;

		strcpy (debugfile, qfcc_com_token);

		s = debugfile + strlen (debugfile);
		while (s-- > debugfile) {
			if (*s == '.')
				break;
			if (*s == '/' || *s == '\\') {
				s = debugfile + strlen (debugfile);
				break;
			}
		}
		strcpy (s, ".sym");
		if (options.verbosity >= 1)
			printf ("debug file: %s\n", debugfile);
	}

	PR_BeginCompilation ();

	// compile all the files
	while ((src = Parse (src))) {
		int         error;

		extern FILE *yyin;
		int         yyparse (void);
		extern void clear_frame_macros (void);

		//extern int yydebug;
		//yydebug = 1;

		if (*sourcedir)
			dsprintf (filename, "%s%c%s", sourcedir, PATH_SEPARATOR,
					  qfcc_com_token);
		else
			dsprintf (filename, "%s", qfcc_com_token);
		if (options.verbosity >= 2)
			printf ("compiling %s\n", filename->str);

		yyin = preprocess_file (filename->str);

		s_file = ReuseString (strip_path (filename->str));
		pr_source_line = 1;
		clear_frame_macros ();
		error = yyparse () || pr_error_count;
		fclose (yyin);
		if (cpp_name && (!options.save_temps)) {
			if (unlink (tempname->str)) {
				perror ("unlink");
				exit (1);
			}
		}
		if (error)
			return 1;
	}

	if (!PR_FinishCompilation ())
		Error ("compilation errors");

	// write progdefs.h
	if (options.code.progsversion == PROG_ID_VERSION)
		crc = WriteProgdefs ("progdefs.h");

	// write data file
	WriteData (crc);

	// write files.dat
	if (options.files_dat)
		WriteFiles (sourcedir);

	stop = Sys_DoubleTime ();
	if (options.verbosity >= 0)
		printf ("Compilation time: %0.3g seconds.\n", (stop - start));
	return 0;
}
