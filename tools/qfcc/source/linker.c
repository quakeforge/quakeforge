/*
	linker.c

	qc object file linking

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/3

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pakfile.h"
#include "QF/va.h"

#include "codespace.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "linker.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
#include "type.h"

static void def_error (qfo_def_t *def, const char *fmt, ...)
	__attribute__ ((used, format (printf, 2, 3)));
static void def_warning (qfo_def_t *def, const char *fmt, ...)
	__attribute__ ((used, format (printf, 2, 3)));

typedef struct builtin_sym_s {
	const char *name;
	type_t     *type;
	unsigned    flags;
} builtin_sym_t;

static builtin_sym_t builtin_symbols[] __attribute__ ((used)) = {
	{".zero",		&type_zero,		QFOD_NOSAVE},
	{".return",		&type_param,	QFOD_NOSAVE},
	{".param_0",	&type_param,	QFOD_NOSAVE},
	{".param_1",	&type_param,	QFOD_NOSAVE},
	{".param_2",	&type_param,	QFOD_NOSAVE},
	{".param_3",	&type_param,	QFOD_NOSAVE},
	{".param_4",	&type_param,	QFOD_NOSAVE},
	{".param_5",	&type_param,	QFOD_NOSAVE},
	{".param_6",	&type_param,	QFOD_NOSAVE},
	{".param_7",	&type_param,	QFOD_NOSAVE},
};

static qfo_t *work;
static strpool_t *work_strings;
static codespace_t *work_code;
static defspace_t *work_near_data;
static defspace_t *work_far_data;
static defspace_t *work_entity_data;
static defspace_t *work_type_data;

/**	Add a string to the working qfo string pool.

	If the string is already in the string pool, the already existing string
	index will be returned instead of adding a second copy of the string.

	The strings space in the working qfo is kept up to date.

	\param str		The string to add.
	\return			The string offset in the working qfo string pool.
*/
static string_t
add_string (const char *str)
{
	string_t    new;
	new = strpool_addstr (work_strings, str);
	work->spaces[qfo_strings_space].d.strings = work_strings->strings;
	work->spaces[qfo_strings_space].data_size = work_strings->size;
	return new;
}

static void
add_qfo_strings (qfo_mspace_t *strings)
{
	const char *str = strings->d.strings;

	while (str - strings->d.strings < strings->data_size) {
		add_string (str);
		while (str - strings->d.strings < strings->data_size && *str)
			str++;
		str++;		// advance past the terminating nul
	}
}

static void
add_code (qfo_mspace_t *code)
{
	codespace_addcode (work_code, code->d.code, code->data_size);
	work->spaces[qfo_code_space].d.code = work_code->code;
	work->spaces[qfo_code_space].data_size = work_code->size;
}

static void
add_data (int space, qfo_mspace_t *data)
{
	defspace_t *work_spaces[qfo_num_spaces] = {
		0, 0, 0,
		work_near_data,
		work_far_data,
		work_entity_data,
		0,
	};

	if (space < 0 || space >= qfo_num_spaces || !work_spaces[space])
		internal_error (0, "bad space for add_data (): %d", space);
	defspace_add_data (work_spaces[space], data->d.data, data->data_size);
	work->spaces[space].d.data = work_spaces[space]->data;
	work->spaces[space].data_size = work_spaces[space]->size;
}

#define QFOSTR(q,s)		((q)->spaces[qfo_strings_space].d.strings + (s))
#define WORKSTR(q,s)	QFOSTR (work, s)

static void
update_relocs (qfo_t *qfo)
{
	int         i;
	qfo_reloc_t *reloc;

	for (reloc = qfo->relocs, i = 0; i < qfo->num_relocs; i++, reloc++) {
		if (!reloc->space) {
			// code space is implied
			reloc->offset += work->spaces[qfo_code_space].data_size;
		} else if (reloc->space < 0 || reloc->space >= qfo->num_spaces) {
			//FIXME proper diagnostic
			fprintf (stderr, "bad reloc space: %d", reloc->space);
			reloc->type = rel_none;
		} else if (reloc->space < qfo_num_spaces) {
			reloc->offset += work->spaces[reloc->space].data_size;
		} else {
			reloc->space += work->num_spaces;
		}
		reloc->def += work->num_defs;
	}
}

static void
update_defs (qfo_t *qfo)
{
	int         space;
	int         i;
	qfo_def_t  *def;

	for (space = 0; space < qfo->num_spaces; space++) {
		if (space == qfo_type_space)
			continue;	// complicated. handle separately
		for (def = qfo->spaces[space].defs, i = 0;
			 i < qfo->spaces[space].num_defs; i++, def++) {
			//XXX type handled later
			def->name = add_string (QFOSTR (qfo, def->name));
			if (space < qfo_num_spaces)
				def->offset += work->spaces[space].data_size;
			def->relocs += work->num_relocs;
			def->file = add_string (QFOSTR (qfo, def->file));
		}
	}
}

static void
update_funcs (qfo_t *qfo)
{
	int         i;
	qfo_func_t *func;

	for (func = qfo->funcs, i = 0; i < qfo->num_funcs; i++, func++) {
		func->name = add_string (QFOSTR (qfo, func->name));
		//XXX type handled later
		func->file = add_string (QFOSTR (qfo, func->file));
		if (func->code > 0)
			func->code += work->spaces[qfo_code_space].data_size;
		func->def += work->num_defs;
		if (!func->locals_space) {
			// no locals (builtin function?)
		} else if (func->locals_space < qfo_num_spaces) {
			//FIXME proper diagnostic
			fprintf (stderr, "function with weird locals: setting to none\n");
			func->locals_space = 0;
		} else {
			func->locals_space += work->num_spaces - qfo_num_spaces;
		}
		func->line_info += work->num_lines;
		func->relocs += work->num_relocs;
	}
}

static void
update_lines (qfo_t *qfo)
{
	int         i;
	pr_lineno_t *lineno;

	for (lineno = qfo->lines, i = 0; i < qfo->num_lines; i++, lineno++) {
		if (lineno->line)
			lineno->fa.addr += work->spaces[qfo_code_space].data_size;
		else
			lineno->fa.func += work->num_funcs;
	}
}

static __attribute__ ((used)) void
define_def (const char *name, etype_t basic_type, const char *full_type,
			unsigned flags, int size, int v)
{
}

void
linker_begin (void)
{
	work = qfo_new ();
	work->spaces = calloc (qfo_num_spaces, sizeof (qfo_mspace_t));
	work->num_spaces = qfo_num_spaces;

	// adding data will take care of connecting the work qfo spaces with
	// the actual space data
	work_strings = strpool_new ();
	work_code = codespace_new ();
	work_near_data = new_defspace();
	work_far_data = new_defspace();
	work_entity_data = new_defspace();
	work_type_data = new_defspace();
}

static void
linker_add_qfo (qfo_t *qfo)
{
	update_relocs (qfo);
	update_defs (qfo);
	update_funcs (qfo);
	update_lines (qfo);

	add_qfo_strings (&qfo->spaces[qfo_strings_space]);
	add_code (&qfo->spaces[qfo_code_space]);
	add_data (qfo_near_data_space, &qfo->spaces[qfo_near_data_space]);
	add_data (qfo_far_data_space, &qfo->spaces[qfo_far_data_space]);
	add_data (qfo_entity_space, &qfo->spaces[qfo_entity_space]);
	//FIXME handle type data
}

int
linker_add_object_file (const char *filename)
{
	qfo_t      *qfo;

	qfo = qfo_open (filename);
	if (!qfo)
		return 1;
	if (qfo->num_spaces < qfo_num_spaces
		|| qfo->spaces[qfo_null_space].type != qfos_null
		|| qfo->spaces[qfo_strings_space].type != qfos_string
		|| qfo->spaces[qfo_code_space].type != qfos_code
		|| qfo->spaces[qfo_near_data_space].type != qfos_data
		|| qfo->spaces[qfo_far_data_space].type != qfos_data
		|| qfo->spaces[qfo_entity_space].type != qfos_entity
		|| qfo->spaces[qfo_type_space].type != qfos_type) {
		//FIXME proper diagnostic
		fprintf (stderr, "%s: missing or mangled standard spaces", filename);
		return 1;
	}

	if (options.verbosity >= 2)
		puts (filename);

	linker_add_qfo (qfo);
	
	qfo_delete (qfo);
	return 0;
}

typedef struct path_s {
	struct path_s *next;
	const char *path;
} path_t;

static path_t  *path_head;
static path_t **path_tail = &path_head;

int
linker_add_lib (const char *libname)
{
	pack_t     *pack = 0;
	path_t      start = {path_head, "."};
	path_t     *path = &start;
	const char *path_name = 0;
	int         i, j;
	int         did_something;

	if (strncmp (libname, "-l", 2) == 0) {
		while (path) {
			path_name = va ("%s/lib%s.a", path->path, libname + 2);
			pack = pack_open (path_name);
			if (pack)
				break;
			if (errno != ENOENT) {
				if (errno)
					perror (libname);
				return 1;
			}
			path = path->next;
		}
	} else {
		path_name = libname;
		pack = pack_open (path_name);
	}

	if (!pack) {
		if (errno)
			perror (libname);
		return 1;
	}

	if (options.verbosity > 1)
		puts (path_name);

	do {
		did_something = 0;
		for (i = 0; i < pack->numfiles; i++) {
			QFile      *f;
			qfo_t      *qfo;

			f = Qsubopen (path_name, pack->files[i].filepos,
						  pack->files[i].filelen, 1);
			qfo = qfo_read (f);
			Qclose (f);

			if (!qfo)
				return 1;

			for (j = 0; j < qfo->num_defs; j++) {
//				qfo_def_t  *def = qfo->defs + j;
//				if ((def->flags & QFOD_GLOBAL)
//					&& !(def->flags & QFOD_EXTERNAL)
//					&& Hash_Find (extern_defs, qfo->strings + def->name)) {
//					if (options.verbosity >= 2)
//						printf ("adding %s because of %s\n",
//								pack->files[i].name, qfo->strings + def->name);
//					linker_add_qfo (qfo);
//					did_something = 1;
//					break;
//				}
			}

			qfo_delete (qfo);
		}
	} while (did_something);
	pack_del (pack);
	return 0;
}

static __attribute__ ((used)) void
undefined_def (qfo_def_t *def)
{
#if 0
	qfo_def_t   line_def;
	pr_int_t    i;
	qfo_reloc_t *reloc = relocs.relocs + def->relocs;

	for (i = 0; i < def->num_relocs; i++, reloc++) {
		if ((reloc->type == rel_op_a_def
			 || reloc->type == rel_op_b_def
			 || reloc->type == rel_op_c_def
			 || reloc->type == rel_op_a_def_ofs
			 || reloc->type == rel_op_b_def_ofs
			 || reloc->type == rel_op_c_def_ofs)
			&& lines.lines) {
			qfo_func_t *func = funcs.funcs;
			qfo_func_t *best = func;
			pr_int_t    best_dist = reloc->offset - func->code;
			pr_lineno_t *line;

			while (best_dist && func - funcs.funcs < funcs.num_funcs) {
				if (func->code <= reloc->offset) {
					if (best_dist < 0 || reloc->offset - func->code < best_dist) {
						best = func;
						best_dist = reloc->offset - func->code;
					}
				}
				func++;
			}
			line = lines.lines + best->line_info;
			line_def.file = best->file;
			line_def.line = best->line;
			if (!line->line
				&& line->fa.func == (pr_uint_t) (best - funcs.funcs)) {
				while (line - lines.lines < lines.num_lines - 1 && line[1].line
					   && line[1].fa.addr <= (pr_uint_t) reloc->offset)
					line++;
				line_def.line = line->line + best->line;
			}
			def_error (&line_def, "undefined symbol %s", STRING (def->name));
		} else {
			def_error (def, "undefined symbol %s", STRING (def->name));
		}
	}
#endif
}

qfo_t *
linker_finish (void)
{
#if 0
	defref_t  **undef_defs, **defref;
	qfo_t      *qfo;

	if (!options.partial_link) {
		int         did_self = 0, did_this = 0;

		undef_defs = (defref_t **) Hash_GetList (extern_defs);
		for (defref = undef_defs; *defref; defref++) {
			qfo_def_t  *def = deref_def (*defref, &global_defs);
			const char *name = STRING (def->name);

			if (strcmp (name, ".self") == 0 && !did_self) {
				defref_t   *_d = Hash_Find (defined_defs, "self");
				if (_d) {
//					qfo_def_t  *d = deref_def (_d, &global_defs);
//					if (d->basic_type == ev_entity)
//						def_warning (d, "@self and self used together");
				}
				define_def (".self", ev_entity, "E", 0, 1, 0);
				did_self = 1;
			} else if (strcmp (name, ".this") == 0 && !did_this) {
				entity_base = 0;
				define_def (".this", ev_field, "F@", QFOD_NOSAVE, 1,
							entity->size);
				defspace_add_data (entity, 0, 1);
				did_this = 1;
			}
		}
		free (undef_defs);
		undef_defs = (defref_t **) Hash_GetList (extern_defs);
		for (defref = undef_defs; *defref; defref++) {
			qfo_def_t  *def = deref_def (*defref, &global_defs);
			undefined_def (def);
		}
		free (undef_defs);
		if (pr.error_count)
			return 0;
	}

	merge_defgroups ();

	fixup_relocs ();

	qfo = qfo_new ();
	qfo_add_code (qfo, code->code, code->size);
	qfo_add_data (qfo, data->data, data->size);
	qfo_add_far_data (qfo, far_data->data, far_data->size);
	qfo_add_strings (qfo, strings->strings, strings->size);
	qfo_add_relocs (qfo, final_relocs.relocs, final_relocs.num_relocs);
	qfo_add_defs (qfo, defs.defs, defs.num_defs);
	qfo_add_funcs (qfo, funcs.funcs, funcs.num_funcs);
	qfo_add_lines (qfo, lines.lines, lines.num_lines);
	qfo_add_types (qfo, type_strings->strings, type_strings->size);
//	qfo->entity_fields = entity->size;
	return qfo;
#endif
	return work;
}

void
linker_add_path (const char *path)
{
	path_t     *p = malloc (sizeof (path_t));
	p->next = 0;
	p->path = path;
	*path_tail = p;
	path_tail = &p->next;
}

static void
def_error (qfo_def_t *def, const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	pr.source_file = def->file;
	pr.source_line = def->line;
//	pr.strings = strings;
	error (0, "%s", string->str);
}

static void
def_warning (qfo_def_t *def, const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	pr.source_file = def->file;
	pr.source_line = def->line;
//	pr.strings = strings;
	warning (0, "%s", string->str);
}
