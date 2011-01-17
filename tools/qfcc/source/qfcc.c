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
#include <ctype.h>
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

const char *
make_string (char *token, char **end)
{
	char        s[2];
	int         c;
	int         i;
	int         mask;
	int         boldnext;
	int         quote;
	static dstring_t *str;

	if (!str)
		str = dstring_newstr ();
	dstring_clearstr (str);

	s[1] = 0;

	mask = 0x00;
	boldnext = 0;

	quote = *token++;
	do {
		c = *token++;
		if (!c)
			error (0, "EOF inside quote");
		if (c == '\n')
			error (0, "newline inside quote");
		if (c == '\\') {				// escape char
			c = *token++;
			if (!c)
				error (0, "EOF inside quote");
			switch (c) {
				case '\\':
					c = '\\';
					break;
				case 'n':
					c = '\n';
					break;
				case '"':
					c = '\"';
					break;
				case '\'':
					c = '\'';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					if (!options.qccx_escapes) {
						for (i = c = 0; i < 3
							 && *token >= '0'
							 && *token <= '7'; i++, token++) {
							c *= 8;
							c += *token - '0';
						}
						if (!*token)
							error (0, "EOF inside quote");
						break;
					}
				case '8':
				case '9':
					c = 18 + c - '0';
					break;
				case 'x':
					c = 0;
					while (*token && isxdigit ((unsigned char)*token)) {
						c *= 16;
						if (*token <= '9')
							c += *token - '0';
						else if (*token <= 'F')
							c += *token - 'A' + 10;
						else
							c += *token - 'a' + 10;
						token++;
					}
					if (!*token)
						error (0, "EOF inside quote");
					break;
				case 'a':
					c = '\a';
					break;
				case 'b':
					if (options.qccx_escapes)
						mask ^= 0x80;
					else
						c = '\b';
					break;
				case 'e':
					c = '\033';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'v':
					c = '\v';
					break;
				case '^':
					if (*token == '\"')
						error (0, "Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '[':
					c = 0x90;		// gold [
					break;
				case ']':
					c = 0x91;		// gold ]
					break;
				case '.':
					c = 28;			// center dot
					break;
				case '<':
					if (options.qccx_escapes)
						c = 29;			// brown left end
					else
						mask = 0x80;
					continue;
				case '-':
					c = 30;			// brown center bit
					break;
				case '>':
					if (options.qccx_escapes)
						c = 31;			// broun right end
					else
						mask = 0x00;
					continue;
				case '(':
					c = 128;		// left slider end
					break;
				case '=':
					c = 129;		// slider center
					break;
				case ')':
					c = 130;		// right slider end
					break;
				case '{':
					c = 0;
					while (*token && *token != '}'
						   && isdigit ((unsigned char)*token)) {
						c *= 10;
						c += *token++ - '0';
					}
					if (!*token)
						error (0, "EOF inside quote");
					if (*token != '}')
						error (0, "non-digit inside \\{}");
					else
						token++;
					if (c > 255)
						warning (0, "\\{%d} > 255", c);
					break;
				default:
					error (0, "Unknown escape char");
					break;
			}
		} else if (c == quote) {
			break;
		}
		if (boldnext)
			c = c ^ 0x80;
		boldnext = 0;
		c = c ^ mask;
		s[0] = c;
		dstring_appendstr (str, s);
	} while (1);

	if (end)
		*end = token;

	return save_string (str->str);
}

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

	pr.near_data = new_defspace ();
	pr.near_data->data = calloc (65536, sizeof (pr_type_t));
	pr.near_data->max_size = 65536;
	pr.near_data->grow = 0;
	pr.scope = new_scope (sc_global, pr.near_data, 0);

	pr.entity_data = new_defspace ();

	numglobaldefs = 1;
	numfielddefs = 1;
}


static int
WriteData (int crc)
{
	def_t      *def;
	ddef_t     *dd;
	dprograms_t progs;
	pr_debug_header_t debug;
	QFile      *h;
	int         i;

	globals = calloc (pr.scope->num_defs + 1, sizeof (ddef_t));
	fields = calloc (pr.scope->num_defs + 1, sizeof (ddef_t));

	for (def = pr.scope->head; def; def = def->def_next) {
		if (def->local || !def->name)
			continue;
		if (options.code.progsversion == PROG_ID_VERSION && *def->name == '.'
			&& strcmp (def->name, ".imm") != 0
			&& strcmp (def->name, ".debug_file") != 0)
			continue;
		if (def->type->type == ev_func) {
		} else if (def->type->type == ev_field
				   && strcmp (def->name, ".imm") != 0) {
			dd = &fields[numfielddefs++];
			def_to_ddef (def, dd, 1);
			dd->ofs = G_INT (def->ofs);
		}

		dd = &globals[numglobaldefs++];
		def_to_ddef (def, dd, 0);
		if (!def->nosave
			&& !def->constant
			&& def->type->type != ev_func
			&& def->type->type != ev_field && def->global)
			dd->type |= DEF_SAVEGLOBAL;
	}

	pr.strings->size = (pr.strings->size + 3) & ~3;

	if (options.verbosity >= 0) {
		printf ("%6i strofs\n", pr.strings->size);
		printf ("%6i statements\n", pr.code->size);
		printf ("%6i functions\n", pr.num_functions);
		printf ("%6i global defs\n", numglobaldefs);
		if (!big_function)
			big_function = "";
		printf ("%6i locals size (%s)\n", num_localdefs, big_function);
		printf ("%6i fielddefs\n", numfielddefs);
		printf ("%6i globals\n", pr.near_data->size);
		printf ("%6i entity fields\n", pr.entity_data->size);
	}

	if (!(h = Qopen (options.output_file, "wb")))
		Sys_Error ("%s: %s\n", options.output_file, strerror(errno));
	memset (&progs, 0, sizeof (progs));
	Qwrite (h, &progs, sizeof (progs));

	progs.ofs_strings = Qtell (h);
	progs.numstrings = pr.strings->size;
	Qwrite (h, pr.strings->strings, pr.strings->size);

	progs.ofs_statements = Qtell (h);
	progs.numstatements = pr.code->size;
	for (i = 0; i < pr.code->size; i++) {
		pr.code->code[i].op = LittleShort (pr.code->code[i].op);
		pr.code->code[i].a = LittleShort (pr.code->code[i].a);
		pr.code->code[i].b = LittleShort (pr.code->code[i].b);
		pr.code->code[i].c = LittleShort (pr.code->code[i].c);
	}
	Qwrite (h, pr.code->code, pr.code->size * sizeof (dstatement_t));

	{
		dfunction_t *df;

		progs.ofs_functions = Qtell (h);
		progs.numfunctions = pr.num_functions;
		for (i = 0, df = pr.functions + 1; i < pr.num_functions; i++, df++) {
			df->first_statement = LittleLong (df->first_statement);
			df->parm_start      = LittleLong (df->parm_start);
			df->s_name          = LittleLong (df->s_name);
			df->s_file          = LittleLong (df->s_file);
			df->numparms        = LittleLong (df->numparms);
			df->locals          = LittleLong (df->locals);
		}
		Qwrite (h, pr.functions, pr.num_functions * sizeof (dfunction_t));
	}

	progs.ofs_globaldefs = Qtell (h);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0; i < numglobaldefs; i++) {
		globals[i].type = LittleShort (globals[i].type);
		globals[i].ofs = LittleShort (globals[i].ofs);
		globals[i].s_name = LittleLong (globals[i].s_name);
	}
	Qwrite (h, globals, numglobaldefs * sizeof (ddef_t));

	progs.ofs_fielddefs = Qtell (h);
	progs.numfielddefs = numfielddefs;
	for (i = 0; i < numfielddefs; i++) {
		fields[i].type = LittleShort (fields[i].type);
		fields[i].ofs = LittleShort (fields[i].ofs);
		fields[i].s_name = LittleLong (fields[i].s_name);
	}
	Qwrite (h, fields, numfielddefs * sizeof (ddef_t));

	progs.ofs_globals = Qtell (h);
	progs.numglobals = pr.near_data->size;
	for (i = 0; i < pr.near_data->size; i++)
		G_INT (i) = LittleLong (G_INT (i));
	Qwrite (h, pr.near_data->data, pr.near_data->size * 4);

	if (options.verbosity >= -1)
		printf ("%6i TOTAL SIZE\n", (int) Qtell (h));

	progs.entityfields = pr.entity_data->size;

	progs.version = options.code.progsversion;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < (int) sizeof (progs) / 4; i++)
		((int *) &progs)[i] = LittleLong (((int *) &progs)[i]);

	Qseek (h, 0, SEEK_SET);
	Qwrite (h, &progs, sizeof (progs));
	Qclose (h);

	if (!options.code.debug) {
		return 0;
	}

	if (!(h = Qopen (options.output_file, "rb")))
		Sys_Error ("%s: %s\n", options.output_file, strerror(errno));

	memset (&debug, 0, sizeof (debug));
	debug.version = LittleLong (PROG_DEBUG_VERSION);
	CRC_Init (&debug.crc);
	while ((i = Qgetc (h)) != EOF)
		CRC_ProcessByte (&debug.crc, i);
	Qclose (h);
	debug.crc = LittleShort (debug.crc);
	debug.you_tell_me_and_we_will_both_know = 0;

	if (!(h = Qopen (debugfile, "wb")))
		Sys_Error ("%s: %s\n", options.output_file, strerror(errno));
	Qwrite (h, &debug, sizeof (debug));

	debug.auxfunctions = LittleLong (Qtell (h));
	debug.num_auxfunctions = LittleLong (pr.num_auxfunctions);
	for (i = 0; i < pr.num_auxfunctions; i++) {
		pr.auxfunctions[i].function = LittleLong (pr.auxfunctions[i].function);
		pr.auxfunctions[i].source_line = LittleLong (pr.auxfunctions[i].source_line);
		pr.auxfunctions[i].line_info = LittleLong (pr.auxfunctions[i].line_info);
		pr.auxfunctions[i].local_defs = LittleLong (pr.auxfunctions[i].local_defs);
		pr.auxfunctions[i].num_locals = LittleLong (pr.auxfunctions[i].num_locals);
	}
	Qwrite (h, pr.auxfunctions,
			pr.num_auxfunctions * sizeof (pr_auxfunction_t));

	debug.linenos = LittleLong (Qtell (h));
	debug.num_linenos = LittleLong (pr.num_linenos);
	for (i = 0; i < pr.num_linenos; i++) {
		pr.linenos[i].fa.addr = LittleLong (pr.linenos[i].fa.addr);
		pr.linenos[i].line = LittleLong (pr.linenos[i].line);
	}
	Qwrite (h, pr.linenos, pr.num_linenos * sizeof (pr_lineno_t));

	debug.locals = LittleLong (Qtell (h));
	debug.num_locals = LittleLong (pr.num_locals);
	for (i = 0; i < pr.num_locals; i++) {
		pr.locals[i].type = LittleShort (pr.locals[i].type);
		pr.locals[i].ofs = LittleShort (pr.locals[i].ofs);
		pr.locals[i].s_name = LittleLong (pr.locals[i].s_name);
	}
	Qwrite (h, pr.locals, pr.num_locals * sizeof (ddef_t));

	Qseek (h, 0, SEEK_SET);
	Qwrite (h, &debug, sizeof (debug));
	Qclose (h);
	return 0;
}

static void
begin_compilation (void)
{
	pr.func_tail = &pr.func_head;

	pr.error_count = 0;
}

static void
setup_param_block (void)
{
	size_t      i;
	def_t      *def;
	static struct {
		const char *name;
		type_t     *type;
	} defs[] = {
		{".zero",		&type_zero},
		{".return",		&type_param},
		{".param_0",	&type_param},
		{".param_1",	&type_param},
		{".param_2",	&type_param},
		{".param_3",	&type_param},
		{".param_4",	&type_param},
		{".param_5",	&type_param},
		{".param_6",	&type_param},
		{".param_7",	&type_param},
	};

	for (i = 0; i < sizeof (defs) / sizeof (defs[0]); i++) {
		def = get_def (defs[i].type, defs[i].name, pr.scope, st_global);
		def->nosave = 1;
		def_initialized (def);
	}
}

static qboolean
finish_compilation (void)
{
	def_t      *d;
	qboolean    errors = false;
	function_t *f;
	def_t      *def;
	expr_t      e;
	ex_label_t *l;
	dfunction_t *df;

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
		if (d->external && d->refs) {
			if (strcmp (d->name, ".self") == 0) {
				get_def (d->type, ".self", pr.scope, st_global);
			} else if (strcmp (d->name, ".this") == 0) {
				get_def (d->type, ".this", pr.scope, st_global);
			} else {
				errors = true;
				error (0, "undefined global %s", d->name);
			}
		}
	}
	if (errors)
		return !errors;

	if (options.code.progsversion != PROG_ID_VERSION) {
		e.type = ex_integer;
		e.e.integer_val = type_size (&type_param);
		ReuseConstant (&e, get_def (&type_integer, ".param_size", pr.scope,
					   st_global));
	}

	if (options.code.debug) {
		e.type = ex_string;
		e.e.string_val = debugfile;
		ReuseConstant (&e, get_def (&type_string, ".debug_file", pr.scope,
					   st_global));
	}

	for (def = pr.scope->head; def; def = def->def_next)
		relocate_refs (def->refs, def->ofs);

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
		if (options.code.local_merging) {
			if (f->scope->space->size > num_localdefs) {
				num_localdefs = f->scope->space->size;
				big_function = f->def->name;
			}
			df->parm_start = pr.near_data->size;
		} else {
			df->parm_start = defspace_new_loc (pr.near_data,
											   f->scope->space->size);
			num_localdefs += f->scope->space->size;
		}
		for (def = f->scope->head; def; def = def->def_next) {
			if (!def->local)
				continue;
			def->ofs += df->parm_start;
			relocate_refs (def->refs, def->ofs);
		}
	}
	if (options.code.local_merging) {
		int         ofs;
		for (ofs = defspace_new_loc (pr.near_data, num_localdefs);
			 ofs < pr.near_data->size; ofs++)
			G_INT (ofs) = 0;
	}

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

	yyin = preprocess_file (file, 0);
	if (!yyin)
		return !options.preprocess_only;

	InitData ();
	clear_frame_macros ();
	clear_classes ();
	clear_defs ();
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
		qfo_t      *qfo;
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
		qfo = linker_finish ();
		if (qfo) {
			if (!options.output_file)
				options.output_file = "progs.dat";
			if (options.partial_link) {
				qfo_write (qfo, options.output_file);
			} else {
				int         crc = 0;

				qfo_to_progs (qfo, &pr);
				setup_sym_file (options.output_file);
				finish_compilation ();

				// write progdefs.h
				if (options.progdefs_h)
					crc = WriteProgdefs ("progdefs.h");

				WriteData (crc);
			}
		} else {
			err = 1;
		}
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
	int         crc = 0;
	script_t   *script;
	FILE       *single = 0;

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

	if (!options.compile)
		setup_param_block ();

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

	if (options.compile) {
		qfo_t      *qfo = qfo_from_progs (&pr);
		qfo_write (qfo, options.output_file);
		qfo_delete (qfo);
	} else {
		class_finish_module ();
		if (!finish_compilation ()) {
			fprintf (stderr, "compilation errors\n");
			return 1;
		}

		// write progdefs.h
		if (options.progdefs_h)
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
