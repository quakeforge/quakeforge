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
#include "linker.h"
#include "method.h"
#include "obj_file.h"
#include "opcodes.h"
#include "options.h"
#include "reloc.h"
#include "struct.h"
#include "type.h"

options_t   options;

const char *sourcedir;
const char *progs_src;

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

hashtab_t  *saved_strings;

static const char *
ss_get_key (void *s, void *unused)
{
	return (const char *)s;
}

const char *
save_string (const char *str)
{
	char       *s;
	if (!saved_strings)
		saved_strings = Hash_NewTable (16381, ss_get_key, 0, 0);
	s = Hash_Find (saved_strings, str);
	if (s)
		return s;
	s = strdup (str);
	Hash_Add (saved_strings, s);
	return s;
}

void
InitData (void)
{
	int         i;

	if (pr.statements) {
		free (pr.statements);
		memset (&pr, 0, sizeof (pr));
	}
	chain_initial_types ();
	pr_source_line = 1;
	pr_error_count = 0;
	pr.num_statements = 1;
	pr.statements_size = 16384;
	pr.statements = calloc (pr.statements_size, sizeof (dstatement_t));
	pr.statement_linenums = calloc (pr.statements_size, sizeof (int));
	pr.strofs = 0;
	CopyString ("");
	pr.num_functions = 1;

	pr.near_data = new_defspace ();
	pr.near_data->data = calloc (65536, sizeof (pr_type_t));
	pr.scope = new_scope (sc_global, pr.near_data, 0);
	current_scope = pr.scope;

	pr.entity_data = new_defspace ();

	numglobaldefs = 1;
	numfielddefs = 1;

	def_ret.ofs = OFS_RETURN;
	for (i = 0; i < MAX_PARMS; i++)
		def_parms[i].ofs = OFS_PARM0 + 3 * i;
}


int
WriteData (int crc)
{
	def_t      *def;
	ddef_t     *dd;
	dprograms_t progs;
	pr_debug_header_t debug;
	FILE       *h;
	int         i;

	globals = calloc (pr.scope->num_defs + 1, sizeof (ddef_t));
	fields = calloc (pr.scope->num_defs + 1, sizeof (ddef_t));

	for (def = pr.scope->head; def; def = def->def_next) {
		if (!def->global || !def->name)
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
			&& def->type->type != ev_field && def->global)
			dd->type |= DEF_SAVEGLOBAL;
		dd->s_name = ReuseString (def->name);
		dd->ofs = def->ofs;
	}

	pr.strofs = (pr.strofs + 3) & ~3;

	if (options.verbosity >= 0) {
		printf ("%6i strofs\n", pr.strofs);
		printf ("%6i statements\n", pr.num_statements);
		printf ("%6i functions\n", pr.num_functions);
		printf ("%6i global defs\n", numglobaldefs);
		printf ("%6i locals size (%s)\n", num_localdefs, big_function);
		printf ("%6i fielddefs\n", numfielddefs);
		printf ("%6i globals\n", pr.near_data->size);
		printf ("%6i entity fields\n", pr.entity_data->size);
	}

	h = SafeOpenWrite (options.output_file);
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
		dfunction_t *df;

		progs.ofs_functions = ftell (h);
		progs.numfunctions = pr.num_functions;
		for (i = 0, df = pr.functions + 1; i < pr.num_functions; i++, df++) {
			df->first_statement = LittleLong (df->first_statement);
			df->parm_start      = LittleLong (df->parm_start);
			df->s_name          = LittleLong (df->s_name);
			df->s_file          = LittleLong (df->s_file);
			df->numparms        = LittleLong (df->numparms);
			df->locals          = LittleLong (df->locals);
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
	progs.numglobals = pr.near_data->size;
	for (i = 0; i < pr.near_data->size; i++)
		G_INT (i) = LittleLong (G_INT (i));
	SafeWrite (h, pr.near_data->data, pr.near_data->size * 4);

	if (options.verbosity >= -1)
		printf ("%6i TOTAL SIZE\n", (int) ftell (h));

	progs.entityfields = pr.entity_data->size;

	progs.version = options.code.progsversion;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof (progs) / 4; i++)
		((int *) &progs)[i] = LittleLong (((int *) &progs)[i]);

	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &progs, sizeof (progs));
	fclose (h);

	if (!options.code.debug) {
		return 0;
	}

	h = SafeOpenRead (options.output_file);

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
	return 0;
}


void
begin_compilation (void)
{
	pr.near_data->size = RESERVED_OFS;
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
	dfunction_t *df;

	class_finish_module ();
	// check to make sure all functions prototyped have code
	if (options.warnings.undefined_function)
		for (d = pr.scope->head; d; d = d->def_next) {
			if (d->type->type == ev_func && d->global) {
				// function args ok
				if (d->used) {
					if (!d->initialized) {
						warning (0, "function %s was called but not defined\n",
								 d->name);
					}
				}
			}
		}

	for (d = pr.scope->head; d; d = d->def_next) {
		if (d->external) {
			errors = true;
			error (0, "undefined global %s\n", d->name);
		}
	}
	if (errors)
		return !errors;

	if (options.code.debug) {
		e.type = ex_string;
		e.e.string_val = debugfile;
		ReuseConstant (&e, get_def (&type_string, ".debug_file", pr.scope,
					   st_global));
	}

	for (def = pr.scope->head; def; def = def->def_next) {
		if (def->global || def->absolute)
			continue;
		relocate_refs (def->refs, def->ofs);
	}

	pr.functions = calloc (pr.num_functions + 1, sizeof (dfunction_t));
	for (df = pr.functions + 1, f = pr.func_head; f; df++, f = f->next) {
		df->s_name = f->s_name;
		df->s_file = f->s_file;
		df->numparms = function_parms (f, df->parm_size);
		if (f->scope)
			df->locals = f->scope->space->size;
		if (f->builtin) {
			df->first_statement = -f->builtin;
			continue;
		}
		if (!f->code)
			continue;
		df->first_statement = f->code;
		if (f->scope->space->size > num_localdefs) {
			num_localdefs = f->scope->space->size;
			big_function = f->def->name;
		}
		df->parm_start = pr.near_data->size;
		for (def = f->scope->head; def; def = def->def_next) {
			if (def->absolute)
				continue;
			def->ofs += pr.near_data->size;
			relocate_refs (def->refs, def->ofs);
		}
	}
	pr.near_data->size += num_localdefs;

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

static void
setup_sym_file (const char *output_file)
{
	if (options.code.debug) {
		char       *s;

		strcpy (debugfile, output_file);

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
}

static int
compile_to_obj (const char *file, const char *obj)
{
	int         err;

	yyin = preprocess_file (file);

	InitData ();
	begin_compilation ();
	clear_frame_macros ();
	clear_classes ();
	clear_defs ();
	clear_immediates ();
	clear_selectors ();
	clear_structs ();
	clear_enums ();
	clear_typedefs ();
	s_file = ReuseString (strip_path (file));
	err = yyparse () || pr_error_count;
	fclose (yyin);
	if (cpp_name && (!options.save_temps)) {
		if (unlink (tempname->str)) {
			perror ("unlink");
			exit (1);
		}
	}
	if (!err)
		write_obj_file (obj);
	return err;
}

static int
separate_compile (void)
{
	const char **file;
	dstring_t  *output_file = dstring_newstr ();
	dstring_t  *extension = dstring_newstr ();
	char       *f;
	int         err = 0;

	if (options.compile && options.output_file && source_files[1]) {
		fprintf (stderr, "%s: cannot use -c and -o together with multiple "
				 "files\n", this_program);
		return 1;
	}

	for (file = source_files; *file; file++) {
		dstring_clearstr (extension);
		dstring_clearstr (output_file);
		dstring_appendstr (output_file, *file);

		f = output_file->str + strlen (output_file->str);
		while (f >= output_file->str && *f != '.' && *f != '/')
			f--;
		if (*f == '.') {
			output_file->size -= strlen (f);
			dstring_appendstr (extension, f);
			*f = 0;
		}
		dstring_appendstr (output_file, ".qfo");
		if (strncmp (*file, "-l", 2)
			&& (!strcmp (extension->str, ".r")
				|| !strcmp (extension->str, ".qc"))) {
			printf ("%s %s\n", *file, output_file->str);
			err = compile_to_obj (*file, output_file->str) || err;

			free ((char *)*file);
			*file = strdup (output_file->str);
		} else {
			if (options.compile)
				fprintf (stderr, "%s: %s: ignoring object file since linking "
						 "not done\n", this_program, *file);
		}
	}
	if (!err && !options.compile) {
		linker_begin ();
		for (file = source_files; *file; file++) {
			linker_add_object_file (*file);
		}
		linker_finish ();
	}
	return err;
}

static int
progs_src_compile (void)
{
	dstring_t  *filename = dstring_newstr ();
	char       *src;
	int         crc = 0;

	if (options.verbosity >= 1 && strcmp (sourcedir, "")) {
		printf ("Source directory: %s\n", sourcedir);
	}
	if (options.verbosity >= 1 && strcmp (progs_src, "progs.src")) {
		printf ("progs.src: %s\n", progs_src);
	}

	if (*sourcedir)
		dsprintf (filename, "%s/%s", sourcedir, progs_src);
	else
		dsprintf (filename, "%s", progs_src);
	LoadFile (filename->str, (void *) &src);

	if (!(src = Parse (src))) {
		fprintf (stderr, "No destination filename.  qfcc --help for info.\n");
		return 1;
	}

	if (!options.output_file)
		options.output_file = strdup (qfcc_com_token);
	if (options.verbosity >= 1) {
		printf ("output file: %s\n", options.output_file);
	}
	setup_sym_file (options.output_file);

	InitData ();

	begin_compilation ();

	// compile all the files
	while ((src = Parse (src))) {
		int         err;

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
		err = yyparse () || pr_error_count;
		fclose (yyin);
		if (cpp_name && (!options.save_temps)) {
			if (unlink (tempname->str)) {
				perror ("unlink");
				exit (1);
			}
		}
		if (err)
			return 1;
	}

	if (options.compile) {
		write_obj_file (options.output_file);
	} else {
		if (!finish_compilation ()) {
			fprintf (stderr, "compilation errors\n");
			return 1;
		}

		// write progdefs.h
		if (options.code.progsversion == PROG_ID_VERSION)
			crc = WriteProgdefs ("progdefs.h");

		// write data file
		if (WriteData (crc))
			return 1;

		// write files.dat
		if (options.files_dat)
			if (WriteFiles (sourcedir))
				return 1;
	}

	return 0;
}

/*
	main

	The nerve center of our little operation
*/
int
main (int argc, char **argv)
{
	double      start, stop;
	int         res;

	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);

	tempname = dstring_new ();
	parse_cpp_name ();

	opcode_init ();
	init_types ();

	if (source_files) {
		res = separate_compile ();
	} else {
		res = progs_src_compile ();
	}
	if (res)
		return res;
	stop = Sys_DoubleTime ();
	if (options.verbosity >= 0)
		printf ("Compilation time: %0.3g seconds.\n", (stop - start));
	return 0;
}
