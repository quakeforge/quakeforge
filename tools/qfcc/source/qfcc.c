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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <stdio.h>

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

const char *sourcedir;
const char *progs_src;

char        destfile[1024];
char        debugfile[1024];

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
