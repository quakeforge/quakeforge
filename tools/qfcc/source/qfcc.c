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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

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
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <QF/cbuf.h>
#include <QF/crc.h>
#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/qendian.h>
#include <QF/script.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "qfcc.h"
#include "class.h"
#include "codespace.h"
#include "cpp.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
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
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"

options_t   options;

const char *sourcedir;
const char *progs_src;

char        debugfile[1024];

pr_info_t   pr;

ddef_t      *globals;
int         numglobaldefs;

int         num_localdefs;
const char *big_function = 0;

ddef_t      *fields;
int         numfielddefs;

#ifdef _WIN32
char *
fix_backslash (char *path)
{
	char       *s;

	for (s = path; *s; s++)
		if (*s == '\\')
			*s = '/';
	return path;
}
#endif

static void
InitData (void)
{
	pr_lineno_t *line;

	if (pr.code) {
		codespace_delete (pr.code);
		strpool_delete (pr.strings);
	}
	memset (&pr, 0, sizeof (pr));
	pr.source_line = 1;
	pr.error_count = 0;
	pr.code = codespace_new ();
	memset (codespace_newstatement (pr.code), 0, sizeof (dstatement_t));
	pr.strings = strpool_new ();
	pr.num_functions = 1;

	pr.num_linenos = 0;
	line = new_lineno ();
	line->fa.func = -1;
	line->line = -1;

	pr.data = defspace_new ();

	pr.far_data = defspace_new ();

	pr.near_data = defspace_new ();
	pr.near_data->data = calloc (65536, sizeof (pr_type_t));
	pr.near_data->max_size = 65536;
	pr.near_data->grow = 0;

	pr.type_data = defspace_new ();
	defspace_alloc_loc (pr.type_data, 4);// reserve space for a null descriptor

	pr.symtab = new_symtab (0, stab_global);
	pr.symtab->space = pr.near_data;

	pr.entity_data = defspace_new ();

	numglobaldefs = 1;
	numfielddefs = 1;
}

static int
WriteProgs (dprograms_t *progs, int size)
{
	//pr_debug_header_t debug;
	QFile      *h;
	unsigned    i;

	dstatement_t *statements;
	dfunction_t *functions;
	ddef_t     *globaldefs;
	ddef_t     *fielddefs;
	pr_type_t  *globals;

#define P(t,o) ((t *)((char *)progs + progs->o))
	statements = P (dstatement_t, ofs_statements);
	functions = P (dfunction_t, ofs_functions);
	globaldefs = P (ddef_t, ofs_globaldefs);
	fielddefs = P (ddef_t, ofs_fielddefs);
	globals = P (pr_type_t, ofs_globals);
#undef P

	for (i = 0; i < progs->numstatements; i++) {
		statements[i].op = LittleShort (statements[i].op);
		statements[i].a = LittleShort (statements[i].a);
		statements[i].b = LittleShort (statements[i].b);
		statements[i].c = LittleShort (statements[i].c);
	}
	for (i = 0; i < (unsigned) progs->numfunctions; i++) {
		dfunction_t *func = functions + i;
		func->first_statement = LittleLong (func->first_statement);
		func->parm_start = LittleLong (func->parm_start);
		func->locals = LittleLong (func->locals);
		func->profile = LittleLong (func->profile);
		func->s_name = LittleLong (func->s_name);
		func->s_file = LittleLong (func->s_file);
		func->numparms = LittleLong (func->numparms);
	}
	for (i = 0; i < progs->numglobaldefs; i++) {
		globaldefs[i].type = LittleShort (globaldefs[i].type);
		globaldefs[i].ofs = LittleShort (globaldefs[i].ofs);
		globaldefs[i].s_name = LittleLong (globaldefs[i].s_name);
	}
	for (i = 0; i < progs->numfielddefs; i++) {
		fielddefs[i].type = LittleShort (fielddefs[i].type);
		fielddefs[i].ofs = LittleShort (fielddefs[i].ofs);
		fielddefs[i].s_name = LittleLong (fielddefs[i].s_name);
	}
	for (i = 0; i < progs->numglobals; i++)
		globals[i].integer_var = LittleLong (globals[i].integer_var);

#if 0 //FIXME
	if (options.verbosity >= 0) {
		if (!big_function)
			big_function = "";
		printf ("%6i strofs\n", pr.strings->size);
		printf ("%6i statements\n", pr.code->size);
		printf ("%6i functions\n", pr.num_functions);
		printf ("%6i global defs\n", numglobaldefs);
		printf ("%6i fielddefs\n", numfielddefs);
		printf ("%6i globals\n", pr.data->size);
		printf ("    %6i near globals\n", pr.near_data->size);
		printf ("        %6i locals size (%s)\n", num_localdefs, big_function);
		printf ("    %6i far globals\n", pr.far_data->size);
		printf ("%6i entity fields\n", pr.entity_data->size);
	}
#endif

	if (!(h = Qopen (options.output_file, "wb")))
		Sys_Error ("%s: %s\n", options.output_file, strerror(errno));
	Qwrite (h, progs, size);

//	if (options.verbosity >= -1)
//		printf ("%6i TOTAL SIZE\n", (int) Qtell (h));

	Qclose (h);

	return 0;
}

static int
WriteSym (pr_debug_header_t *sym, int size)
{
	//pr_debug_header_t debug;
	QFile      *h;
	unsigned    i;

	pr_auxfunction_t *auxfunctions;
	pr_lineno_t *linenos;
	ddef_t     *locals;

#define P(t,o) ((t *)((char *)sym + sym->o))
	auxfunctions = P (pr_auxfunction_t, auxfunctions);
	linenos = P (pr_lineno_t, linenos);
	locals = P (ddef_t, locals);
#undef P

	for (i = 0; i < sym->num_auxfunctions; i++) {
		pr_auxfunction_t *af = auxfunctions++;
		af->function = LittleLong (af->function);
		af->source_line = LittleLong (af->source_line);
		af->line_info = LittleLong (af->line_info);
		af->local_defs = LittleLong (af->local_defs);
		af->num_locals = LittleLong (af->num_locals);
		af->return_type = LittleShort (af->return_type);
	}
	for (i = 0; i < sym->num_linenos; i++) {
		pr_lineno_t *ln = linenos++;
		ln->fa.addr = LittleLong (ln->fa.addr);
		ln->line = LittleLong (ln->line);
	}
	for (i = 0; i < sym->num_locals; i++) {
		locals[i].type = LittleShort (locals[i].type);
		locals[i].ofs = LittleShort (locals[i].ofs);
		locals[i].s_name = LittleLong (locals[i].s_name);
	}

	if (!(h = Qopen (debugfile, "wb")))
		Sys_Error ("%s: %s\n", debugfile, strerror(errno));
	Qwrite (h, sym, size);

	Qclose (h);

	return 0;
}

static void
begin_compilation (void)
{
	pr.func_tail = &pr.func_head;

	pr.error_count = 0;
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

	yyin = preprocess_file (file, 0);
	if (!yyin)
		return !options.preprocess_only;

	InitData ();
	clear_frame_macros ();
	clear_classes ();
	clear_immediates ();
	clear_selectors ();
	chain_initial_types ();
	begin_compilation ();
	pr.source_file = ReuseString (strip_path (file));
	err = yyparse () || pr.error_count;
	fclose (yyin);
	if (cpp_name && !options.save_temps) {
		if (unlink (tempname->str)) {
			perror ("unlink");
			exit (1);
		}
	}
	if (!err) {
		qfo_t      *qfo;

		class_finish_module ();
		qfo = qfo_from_progs (&pr);
		err = qfo_write (qfo, obj);
		qfo_delete (qfo);
	}
	return err;
}

static int
finish_link (void)
{
	qfo_t      *qfo;
	int         flags;

	if (!options.output_file)
		options.output_file = "progs.dat";

	flags = (QFOD_GLOBAL | QFOD_CONSTANT | QFOD_INITIALIZED | QFOD_NOSAVE);
	if (options.code.progsversion != PROG_ID_VERSION) {
		linker_add_def (".param_size", &type_integer, flags,
						type_size (&type_param));
	}

	if (options.code.debug) {
		int         str;

		setup_sym_file (options.output_file);
		str = linker_add_string (debugfile);
		linker_add_def (".debug_file", &type_string, flags, str);
	}

	qfo = linker_finish ();
	if (!qfo)
		return 1;
	if (options.partial_link) {
		qfo_write (qfo, options.output_file);
	} else {
		int         size;
		dprograms_t *progs;

		progs = qfo_to_progs (qfo, &size);
		//finish_compilation ();

		// write progdefs.h
		if (options.progdefs_h)
			progs->crc = WriteProgdefs (progs, "progdefs.h");

		WriteProgs (progs, size);
		if (options.code.debug) {
			pr_debug_header_t *sym;
			int         sym_size = 0;
			sym = qfo_to_sym (qfo, &sym_size);
			sym->crc = CRC_Block ((byte *) progs, size);
			WriteSym (sym, sym_size);
		}
	}
	return 0;
}

static int
separate_compile (void)
{
	const char **file;
	const char **temp_files;
	dstring_t  *output_file = dstring_newstr ();
	dstring_t  *extension = dstring_newstr ();
	char       *f;
	int         err = 0;
	int         i;

	if (options.compile && options.output_file && source_files[1]) {
		fprintf (stderr,
				 "%s: cannot use -c and -o together with multiple files\n",
				 this_program);
		return 1;
	}

	for (file = source_files, i = 0; *file; file++)
		i++;
	temp_files = calloc (i + 1, sizeof (const char*));

	for (file = source_files, i = 0; *file; file++) {
		dstring_clearstr (extension);
		dstring_clearstr (output_file);

		dstring_appendstr (output_file, *file);
		f = output_file->str + strlen (output_file->str);
		while (f >= output_file->str && *f != '.' && *f != '/')
			f--;
		if (f >= output_file->str && *f == '.') {
			output_file->size -= strlen (f);
			dstring_appendstr (extension, f);
			*f = 0;
		}
		if (options.compile && options.output_file) {
			dstring_clearstr (output_file);
			dstring_appendstr (output_file, options.output_file);
		} else {
			dstring_appendstr (output_file, ".qfo");
		}
		if (strncmp (*file, "-l", 2)
			&& (!strcmp (extension->str, ".r")
				|| !strcmp (extension->str, ".qc")
				|| !strcmp (extension->str, ".c"))) {
			if (options.verbosity >= 2)
				printf ("%s %s\n", *file, output_file->str);
			temp_files[i++] = save_string (output_file->str);
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
		chain_initial_types ();
		linker_begin ();
		for (file = source_files; *file; file++) {
			if (strncmp (*file, "-l", 2)) {
				if (strlen (*file) >= 2
					&& strcmp (*file + strlen (*file) - 2, ".a") == 0) {
					err = linker_add_lib (*file);
				} else {
					err = linker_add_object_file (*file);
				}
			} else {
				err = linker_add_lib (*file);
			}
			if (err)
				return err;
		}
		err = finish_link ();
		if (!options.save_temps)
			for (file = temp_files; *file; file++)
				unlink (*file);
	}
	return err;
}

static const char *
load_file (const char *fname)
{
	QFile      *file;
	char       *src;
	FILE       *tmpfile;

	tmpfile = preprocess_file (fname, "i1");
	if (!tmpfile)
		return 0;
	file = Qfopen (tmpfile, "rb");
	if (!file) {
		perror (fname);
		return 0;
	}
	src = malloc (Qfilesize (file) + 1);
	src[Qfilesize (file)] = 0;
	Qread (file, src, Qfilesize (file));
	Qclose (file);
	if (cpp_name && (!options.save_temps)) {
		if (unlink (tempname->str)) {
			perror ("unlink");
			exit (1);
		}
	}
	return src;
}

static void
parse_cpp_line (script_t *script, dstring_t *filename)
{
	if (Script_TokenAvailable (script, 0)) {
		Script_GetToken (script, 0);
		// subtract 1 for \n
		script->line = atoi (script->token->str) - 1;
		if (Script_TokenAvailable (script, 0)) {
			Script_GetToken (script, 0);
			dstring_copystr (filename, script->token->str);
			script->file = filename->str;
		}
	}
	// There shouldn't be anything else, but just to be safe...
	while (Script_TokenAvailable (script, 0))
		Script_GetToken (script, 0);
}

static int
compile_file (const char *filename)
{
	int         err;

	yyin = preprocess_file (filename, 0);
	if (!yyin)
		return !options.preprocess_only;

	pr.source_file = ReuseString (strip_path (filename));
	pr.source_line = 1;
	clear_frame_macros ();
	err = yyparse () || pr.error_count;
	fclose (yyin);
	if (cpp_name && (!options.save_temps)) {
		if (unlink (tempname->str)) {
			perror ("unlink");
			exit (1);
		}
	}
	return err;
}

static int
progs_src_compile (void)
{
	dstring_t  *filename = dstring_newstr ();
	dstring_t  *qc_filename = dstring_newstr ();
	dstring_t  *single_name = dstring_newstr ();
	const char *src;
	//int         crc = 0;
	script_t   *script;
	FILE       *single = 0;
	qfo_t      *qfo;

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

	if (options.single_cpp) {
		intermediate_file (single_name, filename->str, "i2", 1);
		if (!options.save_temps) {
#ifdef _WIN32
			mktemp (single_name->str);
#else
			int tempfd = mkstemp (single_name->str);
			single = fdopen (tempfd, "wb");
#endif
		}
		if (!single)
			single = fopen (single_name->str, "wb");
		if (!single) {
			perror (single_name->str);
			exit (1);
		}
	}

	src = load_file (filename->str);
	if (!src) {
		fprintf (stderr, "couldn't open %s\n", filename->str);
		unlink (single_name->str);
		return 1;
	}
	script = Script_New ();
	Script_Start (script, filename->str, src);

	while (1) {
		if (!Script_GetToken (script, 1)) {
			fprintf (stderr,
					 "No destination filename.  qfcc --help for info.\n");
			return 1;
		}
		if (strcmp (script->token->str, "#") == 0) {
			parse_cpp_line (script, filename);
			continue;
		}
		break;
	}

	if (!options.output_file)
		options.output_file = save_string (script->token->str);
	if (options.verbosity >= 1) {
		printf ("output file: %s\n", options.output_file);
	}
	setup_sym_file (options.output_file);

	InitData ();
	chain_initial_types ();

	begin_compilation ();

	// compile all the files
	while (Script_GetToken (script, 1)) {
		if (strcmp (script->token->str, "#") == 0) {
			parse_cpp_line (script, filename);
			continue;
		}

		while (1) {
			if (*sourcedir)
				dsprintf (qc_filename, "%s%c%s", sourcedir, PATH_SEPARATOR,
						  script->token->str);
			else
				dsprintf (qc_filename, "%s", script->token->str);
			if (options.verbosity >= 2)
				printf ("%s:%d: compiling %s\n", script->file, script->line,
						qc_filename->str);

			if (single) {
				fprintf (single, "$frame_reset\n");
				fprintf (single, "#line %d \"%s\"\n", script->line,
						 script->file);
				fprintf (single, "#include \"%s\"\n", qc_filename->str);
			} else {
				if (compile_file (qc_filename->str))
					return 1;
			}
			if (!Script_TokenAvailable (script, 0))
				break;
			Script_GetToken (script, 0);
		}
	}
	if (single) {
		int         err;
		fclose (single);
		err = compile_file (single_name->str);
		if (!options.save_temps) {
			if (unlink (single_name->str)) {
				perror ("unlink");
				exit (1);
			}
		}
		if (err)
			return 1;
	}

	class_finish_module ();
	qfo = qfo_from_progs (&pr);
	if (options.compile) {
		qfo_write (qfo, options.output_file);
	} else {
		linker_begin ();
		if (linker_add_qfo (qfo) || finish_link ()) {
			fprintf (stderr, "compilation errors\n");
			return 1;
		}

		// write files.dat
		if (options.files_dat)
			if (WriteFiles (sourcedir))
				return 1;
	}
	qfo_delete (qfo);

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

#ifdef _WIN32
	if (putenv ("POSIXLY_INCORRECT_GETOPT=1")) {
		fprintf (stderr, "Warning: putenv failed\n");
	}
#endif
	
	this_program = NORMALIZE (argv[0]);

	DecodeArgs (argc, argv);

	tempname = dstring_new ();
	parse_cpp_name ();

	opcode_init ();
	InitData ();
	init_types ();
	clear_immediates ();

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
