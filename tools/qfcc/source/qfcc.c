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

#include "qfcc.h"
#include "class.h"
#include "cmdlib.h"
#include "cpp.h"
#include "debug.h"
#include "def.h"
#include "expr.h"
#include "function.h"
#include "idstuff.h"
#include "immediate.h"
#include "opcodes.h"
#include "options.h"
#include "reloc.h"
#include "type.h"

options_t   options;

const char *sourcedir;
const char *progs_src;

char        destfile[1024];
char        debugfile[1024];

pr_info_t   pr;

int         pr_source_line;
int         pr_error_count;

ddef_t      *globals;
int         numglobaldefs;

int         num_localdefs;
const char *big_function = 0;

ddef_t      *fields;
int         numfielddefs;


void
InitData (void)
{
	int         i;

	pr.num_statements = 1;
	pr.strofs = 1;
	pr.num_functions = 1;

	pr.globals = new_defspace ();
	pr.globals->data = calloc (65536, sizeof (pr_type_t));
	pr.scope = new_scope (pr.globals, 0);
	current_scope = pr.scope;

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

	globals = calloc (pr.scope->num_defs, sizeof (ddef_t));
	fields = calloc (pr.scope->num_defs, sizeof (ddef_t));

	for (def = pr.scope->head; def; def = def->def_next) {
		if (def->scope->parent)
			continue;
		if (def->type->type == ev_func) {
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

	pr.strofs = (pr.strofs + 3) & ~3;

	if (options.verbosity >= 0) {
		printf ("%6i strofs\n", pr.strofs);
		printf ("%6i statements\n", pr.num_statements);
		printf ("%6i functions\n", pr.num_functions);
		printf ("%6i global defs\n", pr.scope->num_defs);
		printf ("%6i locals size (%s)\n", num_localdefs, big_function);
		printf ("%6i fielddefs\n", numfielddefs);
		printf ("%6i globals\n", pr.globals->size);
		printf ("%6i entity fields\n", pr.size_fields);
	}

	h = SafeOpenWrite (destfile);
	SafeWrite (h, &progs, sizeof (progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = pr.strofs;
	SafeWrite (h, pr.strings, pr.strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = pr.num_statements;
	for (i = 0; i < pr.num_statements; i++) {
		pr.statements[i].op = LittleShort (pr.statements[i].op);
		pr.statements[i].a = LittleShort (pr.statements[i].a);
		pr.statements[i].b = LittleShort (pr.statements[i].b);
		pr.statements[i].c = LittleShort (pr.statements[i].c);
	}
	SafeWrite (h, pr.statements, pr.num_statements * sizeof (dstatement_t));

	{
		function_t *f;

		progs.ofs_functions = ftell (h);
		progs.numfunctions = pr.num_functions;
		pr.functions = malloc (pr.num_functions * sizeof (dfunction_t));
		for (i = 1, f = pr.func_head; f; i++, f = f->next) {
			pr.functions[i].first_statement =
				LittleLong (f->dfunc->first_statement);
			pr.functions[i].parm_start = LittleLong (f->dfunc->parm_start);
			pr.functions[i].s_name = LittleLong (f->dfunc->s_name);
			pr.functions[i].s_file = LittleLong (f->dfunc->s_file);
			pr.functions[i].numparms = LittleLong (f->dfunc->numparms);
			pr.functions[i].locals = LittleLong (f->dfunc->locals);
			memcpy (pr.functions[i].parm_size, f->dfunc->parm_size, MAX_PARMS);
		}
		SafeWrite (h, pr.functions, pr.num_functions * sizeof (dfunction_t));
	}

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
	progs.numglobals = pr.globals->size;
	for (i = 0; i < pr.globals->size; i++)
		G_INT (i) = LittleLong (G_INT (i));
	SafeWrite (h, pr.globals->data, pr.globals->size * 4);

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


void
begin_compilation (void)
{
	pr.globals->size = RESERVED_OFS;
	pr.func_tail = &pr.func_head;

	pr_error_count = 0;
}

qboolean
finish_compilation (void)
{
	def_t      *d;
	qboolean    errors = false;
	function_t *f;
	def_t      *def;
	expr_t      e;
	ex_label_t *l;

	class_finish_module ();
	// check to make sure all functions prototyped have code
	if (options.warnings.undefined_function)
		for (d = pr.scope->head; d; d = d->def_next) {
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
		ReuseConstant (&e, get_def (&type_string, ".debug_file", pr.scope, 1));
	}

	for (def = pr.scope->head; def; def = def->def_next) {
		if (def->scope || def->absolute)
			continue;
		relocate_refs (def->refs, def->ofs);
	}

	for (f = pr.func_head; f; f = f->next) {
		if (f->builtin || !f->code)
			continue;
		if (f->scope->space->size > num_localdefs) {
			num_localdefs = f->scope->space->size;
			big_function = f->def->name;
		}
		f->dfunc->parm_start = pr.globals->size;
		for (def = f->scope->head; def; def = def->def_next) {
			if (def->absolute)
				continue;
			def->ofs += pr.globals->size;
			relocate_refs (def->refs, def->ofs);
		}
	}
	pr.globals->size += num_localdefs;

	for (l = pr.labels; l; l = l->next)
		relocate_refs (l->refs, l->ofs);

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

	opcode_init ();

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

	begin_compilation ();

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

	if (!finish_compilation ())
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
