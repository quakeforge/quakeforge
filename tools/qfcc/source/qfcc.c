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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
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

#include <stdio.h>

#include <getopt.h>

#include <QF/crc.h>
#include <QF/hash.h>
#include <QF/qendian.h>
#include <QF/sys.h>

#include "qfcc.h"

options_t	options;

char		*sourcedir;
const char	*this_program;

static struct option const long_options[] = {
	{"source",	required_argument,	0, 's'},
	{"quiet",	no_argument,		0, 'q'},
	{"verbose",	no_argument,		0, 'v'},
	{"code",	required_argument,	0, 'C'},
	{"warn",	required_argument,	0, 'W'},
	{"help",	no_argument,		0, 'h'},
	{"version", no_argument,		0, 'V'},
#ifdef USE_CPP
	{"define",	required_argument,	0, 'D'},
	{"include",	required_argument,	0, 'I'},
	{"undefine",required_argument,	0, 'U'},
#endif
	{NULL, 0, NULL, 0}
};

char        destfile[1024];
char        debugfile[1024];

#ifdef USE_CPP
# define CPP_MAX_USER_ARGS	250
# define CPP_MAX_ARGS		256
/*
	We reserve 6 args at the end.
*/
char		*cpp_argv[CPP_MAX_ARGS];
static int	cpp_argc = 0;
#endif

pr_info_t	pr;

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
int			num_localdefs;
const char *big_function = 0;

ddef_t      fields[MAX_FIELDS];
int         numfielddefs;

char        precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
int         precache_sounds_block[MAX_SOUNDS];
int         numsounds;

char        precache_models[MAX_MODELS][MAX_DATA_PATH];
int         precache_models_block[MAX_SOUNDS];
int         nummodels;

char        precache_files[MAX_FILES][MAX_DATA_PATH];
int         precache_files_block[MAX_SOUNDS];
int         numfiles;

/*
	WriteFiles

	Generates files.dat, which contains all of the data files actually used by
	the game, to be processed by qfiles
*/
void
WriteFiles (void)
{
	FILE	*f;
	int 	i;
	char	filename[1024];

	snprintf (filename, sizeof (filename), "%s%cfiles.dat", sourcedir, PATH_SEPARATOR);
	f = fopen (filename, "w");
	if (!f)
		Error ("Couldn't open %s", filename);

	fprintf (f, "%i\n", numsounds);
	for (i = 0; i < numsounds; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i], precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i = 0; i < nummodels; i++)
		fprintf (f, "%i %s\n", precache_models_block[i], precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i = 0; i < numfiles; i++)
		fprintf (f, "%i %s\n", precache_files_block[i], precache_files[i]);

	fclose (f);
}

/*
	CopyString

	Return an offset from the string heap
*/
static hashtab_t *strings_tab;

static const char *
stings_get_key (void *_str, void *unsued)
{
	return (char*)_str;
}

int
CopyString (const char *str)
{
	int 	old;

	if (!strings_tab) {
		strings_tab = Hash_NewTable (16381, stings_get_key, 0, 0);
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
	char *s;

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
	int 	i;

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
	def_t		*def;
	ddef_t		*dd;
	dprograms_t progs;
	pr_debug_header_t debug;
	FILE		*h;
	int 		i;

	for (def = pr.def_head.def_next; def; def = def->def_next) {
		if (def->scope)
			continue;
		if (def->type->type == ev_func) {
//			df = &functions[numfunctions];
//			numfunctions++;
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

//	PrintStrings ();
//	PrintFunctions ();
//	PrintFields ();
//	PrintGlobals ();
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
		functions[i].first_statement = LittleLong (functions[i].first_statement);
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
	char		*s;

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
	def_t	*d;

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
	int 	i;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;

	for (i = 0; i < RESERVED_OFS; i++)
		pr_global_defs[i] = &def_void;

	// link the function type in so state forward declarations match proper type
	pr.types = &type_function;
	type_function.next = NULL;
	pr_error_count = 0;
}

void
PR_RelocateRefs (def_t *def)
{
	statref_t	*ref;
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
			default:
				abort();
		}
	}
}

/*
	PR_FinishCompilation

	called after all files are compiled to check for errors.
	Returns false if errors were detected.
*/
qboolean
PR_FinishCompilation (void)
{
	def_t		*d;
	qboolean	errors = false;
	function_t	*f;
	def_t		*def;
	expr_t       e;

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
		if (def->scope)
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
			def->ofs += numpr_globals;
			PR_RelocateRefs (def);
		}
	}
	numpr_globals += num_localdefs;

	return !errors;
}

//=============================================================================

/*
	PR_WriteProgdefs

	Writes the global and entity structures out.
	Returns a crc of the header, to be stored in the progs file for comparison
	at load time.
*/
int
PR_WriteProgdefs (char *filename)
{
	def_t			*d;
	FILE			*f;
	unsigned short	crc;
	int 			c;

	if (options.verbosity >= 1)
		printf ("writing %s\n", filename);
	f = fopen (filename, "w");

	// print global vars until the first field is defined
	fprintf (f,	"\n/* file generated by qcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n", RESERVED_OFS);

	for (d = pr.def_head.def_next; d; d = d->def_next) {
		if (!strcmp (d->name, "end_sys_globals"))
			break;

		switch (d->type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->def_next->def_next->def_next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} globalvars_t;\n\n");

	// print all fields
	fprintf (f, "typedef struct\n{\n");
	for (d = pr.def_head.def_next; d; d = d->def_next) {
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;

		switch (d->type->aux_type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->def_next->def_next->def_next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} entvars_t;\n\n");

	fclose (f);

	// do a crc of the file
	CRC_Init (&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc (f)) != EOF)
		CRC_ProcessByte (&crc, (byte) c);

	fprintf (f, "#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}


void
PR_PrintFunction (def_t *def)
{
	def_t *d;
	statref_t *r;

	printf ("%s\n", def->name);
	for (d = def->scope_next; d; d = d->scope_next) {
		printf ("%s: %d %d %d\n",
				d->name ? d->name : "<temp>",
				d->ofs, d->type->type, pr_type_size[d->type->type]);
		for (r = d->refs; r; r = r->next)
			printf (" %d", r->statement - statements);
		printf ("\n");
	}
}

static void
usage (int status)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Usage: %s [options]\n", this_program);
	printf (
"Options:\n"
"    -s, --source DIR          Look for progs.src in DIR instead of \".\"\n"
"    -q, --quiet               Inhibit usual output\n"
"    -v, --verbose             Display more output than usual\n"
"    -g,                       Generate debuggin info\n"
"    -C, --code OPTION,...     Set code generation options\n"
"    -W, --warn OPTION,...     Set warning options\n"
"    -h, --help                Display this help and exit\n"
"    -V, --version             Output version information and exit\n\n"
#ifdef USE_CPP
"    -D, --define SYMBOL[=VAL],...  Define symbols for the preprocessor\n"
"    -I, --include DIR,...          Set directories for the preprocessor \n"
"                                   to search for #includes\n"
"    -U, --undefine SYMBOL,...      Undefine preprocessor symbols\n\n"
#endif
"For help on options for --code and --warn, see the qfcc(1) manual page\n"
	);
	exit (status);
}

static int
DecodeArgs (int argc, char **argv)
{
	int		c;

#ifdef USE_CPP
	for (c = 0; c < CPP_MAX_ARGS; cpp_argv[c++] = NULL);	// clear the args
	cpp_argv[cpp_argc++] = "cpp";
	cpp_argv[cpp_argc++] = "-D__QUAKEC__=1";
	cpp_argv[cpp_argc++] = "-D__QFCC__=1";
#endif

	options.code.cpp = true;
	options.code.progsversion = PROG_VERSION;
	options.warnings.uninited_variable = true;
	options.verbosity = 0;

	sourcedir = ".";

	while ((c = getopt_long (argc, argv,
			"s:"	// source dir
			"q"		// quiet
			"v"		// verbose
			"g"		// debug
			"C:"	// code options
			"W:"	// warning options
			"h"		// help
			"V"		// version
#ifdef USE_CPP
			"D:"	// define
			"I:"	// set includes
			"U:"	// undefine
#endif
			, long_options, (int *) 0)) != EOF) {
		switch (c) {
			case 'h':	// help
				usage (0);
				break;
			case 'V':	// version
				printf ("%s version %s\n", PACKAGE, VERSION);
				exit (0);
				break;
			case 's':	// src dir
				sourcedir = strdup (optarg);
				break;
			case 'q':	// quiet
				options.verbosity -= 1;
				break;
			case 'v':	// verbose
				options.verbosity += 1;
				break;
			case 'g':	// debug
				options.code.debug = 1;
				break;
			case 'C': {	// code options
					char	*opts = strdup (optarg);
					char	*temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "cow"))) {
							options.code.cow = true;
						} else if (!(strcasecmp (temp, "no-cow"))) {
							options.code.cow = false;
						} else if (!(strcasecmp (temp, "cpp"))) {
							options.code.cpp = true;
						} else if (!(strcasecmp (temp, "no-cpp"))) {
							options.code.cpp = false;
						} else if (!(strcasecmp (temp, "debug"))) {
							options.code.debug = true;
						} else if (!(strcasecmp (temp, "no-debug"))) {
							options.code.debug = false;
						} else if (!(strcasecmp (temp, "v6only"))) {
							options.code.progsversion = PROG_ID_VERSION;
#ifdef USE_CPP
							cpp_argv[cpp_argc++] = "-D__VERSION6__=1";
#endif
						} else if (!(strcasecmp (temp, "no-v6only"))) {
							options.code.progsversion = PROG_VERSION;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'W': {	// warning options
					char	*opts = strdup (optarg);
					char	*temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "all"))) {
							options.warnings.cow = true;
							options.warnings.undefined_function = true;
							options.warnings.uninited_variable = true;
							options.warnings.vararg_integer = true;
						} else if (!(strcasecmp (temp, "none"))) {
							options.warnings.cow = false;
							options.warnings.undefined_function = false;
							options.warnings.uninited_variable = false;
							options.warnings.vararg_integer = false;
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
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
#ifdef USE_CPP
			case 'D': {	// defines for cpp
					char	*opts = strdup (optarg);
					char	*temp = strtok (opts, ",");

					while (temp && (cpp_argc < CPP_MAX_USER_ARGS)) {
						char	temp2[1024];

						snprintf (temp2, sizeof (temp2), "%s%s", "-D", temp);
						cpp_argv[cpp_argc++] = strdup (temp2);
						temp = strtok (NULL, ",");
 					}
					free (opts);
				}
				break;
			case 'I': {	// includes
					char	*opts = strdup (optarg);
					char	*temp = strtok (opts, ",");

					while (temp && (cpp_argc < CPP_MAX_USER_ARGS)) {
						char	temp2[1024];

						snprintf (temp2, sizeof (temp2), "%s%s", "-I", temp);
						cpp_argv[cpp_argc++] = strdup (temp2);
						temp = strtok (NULL, ",");
 					}
					free (opts);
				}
				break;
			case 'U': {	// undefines
					char	*opts = strdup (optarg);
					char	*temp = strtok (opts, ",");

					while (temp && (cpp_argc < CPP_MAX_USER_ARGS)) {
						char	temp2[1024];

						snprintf (temp2, sizeof (temp2), "%s%s", "-U", temp);
						cpp_argv[cpp_argc++] = strdup (temp2);
						temp = strtok (NULL, ",");
 					}
					free (opts);
				}
				break;
#endif
			default:
				usage (1);
		}
	}
	return optind;
}
	
//============================================================================

/*
	main

	The nerve center of our little operation
*/
int
main (int argc, char **argv)
{
	char		*src;
	char		filename[1024];
	long int 	crc;
	double		start, stop;

	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);

	if (strcmp (sourcedir, ".")) {
		printf ("Source directory: %s\n", sourcedir);
	}

	PR_Opcode_Init_Tables ();

	InitData ();

	snprintf (filename, sizeof (filename), "%s/progs.src", sourcedir);
	LoadFile (filename, (void *) &src);

	if (!(src = Parse (src)))
		Error ("No destination filename.  qfcc --help for info.\n");

	strcpy (destfile, com_token);
	if (options.verbosity >= 1) {
		printf ("outputfile: %s\n", destfile);
	}
	if (options.code.debug) {
		char *s;
		strcpy (debugfile, com_token);

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
#ifdef USE_CPP
# ifndef _WIN32
		pid_t	pid;
		int 	tempfd;
# endif
		char	*temp1;
		char	*temp2 = strrchr (argv[0], PATH_SEPARATOR);
		char	tempname[1024];
#endif
		int		error;

		extern FILE *yyin;
		int yyparse (void);
		extern void clear_frame_macros (void);

		//extern int yydebug;
		//yydebug = 1;

		snprintf (filename, sizeof (filename), "%s%c%s", sourcedir, PATH_SEPARATOR, com_token);
		if (options.verbosity >= 2)
			printf ("compiling %s\n", filename);

#ifdef USE_CPP
		if (options.code.cpp) {
			temp1 = getenv ("TMPDIR");
			if ((!temp1) || (!temp1[0])) {
				temp1 = getenv ("TEMP");
				if ((!temp1) || (!temp1[0])) {
					temp1 = "/tmp";
				}
			}

			snprintf (tempname, sizeof (tempname), "%s%c%sXXXXXX", temp1,
					  PATH_SEPARATOR, temp2 ? temp2 + 1 : argv[0]);

# ifdef _WIN32

			mktemp (tempname);
			yyin = fopen (tempname, "wt");
			fclose (yyin);

			{
				int status = spawnvp (_P_WAIT, "cpp", cpp_argv);

				if (status) {
					fprintf (stderr, "cpp returned error code %d", status);
					exit (1);
				}
			}

			yyin = fopen (tempname, "rt");
# else
			tempfd = mkstemp (tempname);
			if ((pid = fork ()) == -1) {
				perror ("fork");
				return 1;
			}
			if (!pid) { // we're a child, check for abuse
				cpp_argv[cpp_argc++] = "-o";
				cpp_argv[cpp_argc++] = tempname;
				cpp_argv[cpp_argc++] = filename;
				
				execvp ("cpp", cpp_argv);
				printf ("Child shouldn't reach here\n");
				exit (1);
			} else { 	// give parental guidance (or bury it in the back yard)
				int 	status;
				pid_t	rc;
					
//				printf ("pid = %d\n", pid);
				if ((rc = waitpid (0, &status, 0 | WUNTRACED)) != pid) {
					if (rc == -1) {
						perror ("wait");
						exit (1);
					}
					printf ("*** Uhh, dude, the wrong child (%d) just died.\n"
							"*** Don't ask me, I can't figure it out either.\n",
							rc);
					exit (1);
				}
				if (WIFEXITED (status)) {
					if (WEXITSTATUS (status)) {
						printf ("cpp returned error code %d",
								WEXITSTATUS (status));
						exit (1);
					}
				} else {
					printf ("cpp returned prematurely.");
					exit (1);
				}
			}
			yyin = fdopen (tempfd, "r+t");
# endif
		} else {
			yyin = fopen (filename, "rt");
		}
#else
		yyin = fopen (filename, "rt");
#endif
		s_file = ReuseString (filename);
		pr_source_line = 1;
		clear_frame_macros ();
		error = yyparse () || pr_error_count;
		fclose (yyin);
#ifdef USE_CPP
		if (options.code.cpp) {
			if (unlink (tempname)) {
				perror ("unlink");
				exit (1);
			}
		}
#endif
		if (error)
			return 1;
	}

	if (!PR_FinishCompilation ())
		Error ("compilation errors");

	// write progdefs.h
	crc = PR_WriteProgdefs ("progdefs.h");

	// write data file
	WriteData (crc);

	// write files.dat
	WriteFiles ();

	stop = Sys_DoubleTime ();
	if (options.verbosity >= 0)
		printf ("Compilation time: %0.3g seconds.\n", (stop - start));
	return 0;
}
