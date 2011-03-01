/*
	linker.c

	qc object file linking

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>
	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/3
	Date: 2011/2/24

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
#include "obj_type.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
#include "type.h"

static void linker_error (const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
static void linker_warning (const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
static void def_error (qfo_def_t *def, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
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

typedef struct defref_s {
	struct defref_s *next;
	qfo_def_t **def_list;
	int         def;
	int         space;
} defref_t;

#define REF(r) ((*(r)->def_list) + (r)->def)

static defref_t *free_defrefs;

static hashtab_t *extern_defs;
static hashtab_t *defined_defs;

static hashtab_t *extern_type_defs;
static hashtab_t *defined_type_defs;
static qfo_mspace_t *qfo_type_defs;

static qfo_t *work;
static defref_t **work_defrefs;
static int num_work_defrefs;
static strpool_t *work_strings;
static codespace_t *work_code;
static defspace_t *work_near_data;
static defspace_t *work_far_data;
static defspace_t *work_entity_data;
static defspace_t *work_type_data;

static dstring_t *linker_current_file;

#define QFOSTR(q,s)		QFO_GETSTR (q, s)
#define WORKSTR(s)		QFOSTR (work, s)

/**	Produce an error message for defs with mismatched types.

	\param def		The new def.
	\param prev		The previous definition.
*/
static void
linker_type_mismatch (qfo_def_t *def, qfo_def_t *prev)
{
	def_error (def, "type mismatch for `%s' `%s'",
			   WORKSTR (def->name),
			   QFO_TYPESTR (work, def->type));
	def_error (prev, "previous definition `%s'",
			   QFO_TYPESTR (work, prev->type));
}

/**	Create a new def reference.

	References are required to avoid problems with realloc moving things
	around.

	The \a def must be in the \a space, and the \a space must be in the
	\c work qfo.

	\param def		The def for which the reference will be created.
	\param space	The defspace in \c work which holds \a def.
	\param def_list	The list which currently holds the def.
	\return			The new reference.
*/
static defref_t *
get_defref  (qfo_def_t *def, qfo_mspace_t *space, qfo_def_t **def_list)
{
	defref_t   *defref;

	ALLOC (16384, defref_t, defrefs, defref);
	defref->def_list = def_list;
	defref->def = def - *def_list;
	defref->space = space - work->spaces;
	return defref;
}

static const char *
defs_get_key (void *_r, void *unused)
{
	defref_t   *r = (defref_t *)_r;
	qfo_def_t  *d = REF (r);
	return WORKSTR (d->name);
}

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

/**	Resolve an external def with its global definition.

	The types of the external def and the global def must match.
	The offset and flags of the global def are copied to the external def.
	\fixme handle cross-space resolutions.
	\fixme copy relocs from \a ext to \a def and "delete" \a ext?

	\param ext		The external def.
	\param def		The global definition.
*/
static void
resolve_external_def (defref_t *ext, defref_t *def)
{
	if (REF (ext)->type != REF (def)->type) {
		linker_type_mismatch (REF (ext), REF (def));
		return;
	}
	if (def->space != ext->space)
		internal_error (0, "help, help!");
	REF (ext)->offset = REF (def)->offset;
	REF (ext)->flags = REF (def)->flags;
}

static void
process_def (defref_t *ref, qfo_mspace_t *space)
{
	qfo_def_t  *def;
	defref_t   *r;
	const char *name;

	def = REF (ref);
	name = WORKSTR (def->name);
	r = Hash_Find (defined_defs, name);
	if (def->flags & QFOD_EXTERNAL) {
		if (!def->num_relocs)
			return;
		if (r) {
			resolve_external_def (ref, r);
		} else {
			Hash_Add (extern_defs, ref);
		}
	} else if (def->flags & QFOD_GLOBAL) {
		if (r) {
			if (REF (r)->flags & QFOD_SYSTEM) {
				if (def->type != REF (r)->type) {
					linker_type_mismatch (def, REF (r));
					return;
				}
				/// System defs may be redefined only once.
				REF (r)->flags &= ~QFOD_SYSTEM;
				if (ref->space != r->space)
					internal_error (0, "help, help!");
				//FIXME copy stuff from new def to existing def???
				def->offset = REF(r)->offset;
				def->flags = REF(r)->flags;
			} else {
				def_error (def, "%s redefined", WORKSTR (def->name));
				def_error (REF (r), "previous definition");
			}
			return;
		}
		Hash_Add (defined_defs, ref);
		if (!(def->flags & QFOD_LOCAL))
			def->offset += space->data_size;
		while ((r = Hash_Del (extern_defs, name)))
			resolve_external_def (r, ref);
	}
}

static int
add_defs (qfo_t *qfo, qfo_mspace_t *space, qfo_mspace_t *dest_space)
{
	int         count = space->num_defs;
	int         size;
	int         i;
	qfo_def_t  *idef;
	qfo_def_t  *odef;
	defref_t   *ref;
	qfot_type_t *type;

	size = (num_work_defrefs + count) * sizeof (defref_t *);
	work_defrefs = realloc (work_defrefs, size);
	size = (dest_space->num_defs + count) * sizeof (qfo_def_t);
	dest_space->defs = realloc (dest_space->defs, size);
	idef = space->defs;
	odef = dest_space->defs + dest_space->num_defs;
	for (i = 0; i < count; i++, idef++, odef++) {
		*odef = *idef;
		idef->offset = num_work_defrefs;	// so def can be found
		odef->name = add_string (QFOSTR (qfo, idef->name));
		odef->file = add_string (QFOSTR (qfo, idef->file));
		type = (qfot_type_t *) (char *) (qfo_type_defs->d.data + idef->type);
		odef->type = type->t.class;
		ref = get_defref (odef, dest_space, &dest_space->defs);
		work_defrefs[num_work_defrefs++] = ref;
		process_def (ref, dest_space);
	}
	dest_space->num_defs += count;
	return count;
}

/**	Add all the strings in the strings space to the working qfo.

	\param strings	The strings space of the qfo being linked.
*/
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

/**	Add the code in the code space to the working qfo.

	\param code		The code space of the qfo being linked.
*/
static void
add_code (qfo_mspace_t *code)
{
	codespace_addcode (work_code, code->d.code, code->data_size);
	work->spaces[qfo_code_space].d.code = work_code->code;
	work->spaces[qfo_code_space].data_size = work_code->size;
}

/**	Add the data in a data space to the working qfo.

	\param data		A data space of the qfo being linked.
*/
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

/**	Add a defspace to the working qfo.

	\param qfo		The qfo being linked.
	\param space	The defspace from \a qfo that will be added to the working
					qfo.
*/
static void
add_space (qfo_t *qfo, qfo_mspace_t *space)
{
	qfo_mspace_t *ws;
	if (space->type != qfos_data)
		internal_error (0, "bad space type for add_space (): %d", space->type);
	space->id = work->num_spaces++;	// so the space in work can be found
	work->spaces = realloc (work->spaces,
							work->num_spaces * sizeof (qfo_mspace_t));
	ws = &work->spaces[space->id];
	memset (ws, 0, sizeof (qfo_mspace_t));
	ws->type = space->type;
	if (space->num_defs)
		add_defs (qfo, space, ws);
	if (space->d.data) {
		int         size = space->data_size * sizeof (pr_type_t);
		ws->d.data = malloc (size);
		memcpy (ws->d.data, space->d.data, size);
	}
	ws->data_size = space->data_size;
	ws->id = space->id;
}

static __attribute__ ((used)) void
define_def (const char *name, etype_t basic_type, const char *full_type,
			unsigned flags, int size, int v)
{
}

/**	Initialize the linker state.
*/
void
linker_begin (void)
{
	linker_current_file = dstring_newstr ();

	extern_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_defs = Hash_NewTable (16381, defs_get_key, 0, 0);

	extern_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0);

	work = qfo_new ();
	work->spaces = calloc (qfo_num_spaces, sizeof (qfo_mspace_t));
	work->num_spaces = qfo_num_spaces;
	work->spaces[qfo_null_space].type = qfos_null;
	work->spaces[qfo_strings_space].type = qfos_string;
	work->spaces[qfo_code_space].type = qfos_code;
	work->spaces[qfo_near_data_space].type = qfos_data;
	work->spaces[qfo_far_data_space].type = qfos_data;
	work->spaces[qfo_entity_space].type = qfos_entity;
	work->spaces[qfo_type_space].type = qfos_type;

	// adding data will take care of connecting the work qfo spaces with
	// the actual space data
	work_strings = strpool_new ();
	work_code = codespace_new ();
	work_near_data = defspace_new ();
	work_far_data = defspace_new ();
	work_entity_data = defspace_new ();
	work_type_data = defspace_new ();
}

typedef int (*space_func) (qfo_t *qfo, qfo_mspace_t *space, int pass);

static int
process_null (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 0)
		return 0;
	if (space->defs || space->num_defs || space->d.data || space->data_size
		|| space->id) {
		linker_error ("non-null null space");
		return 1;
	}
	return 0;
}

static int
process_code (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 1)
		return 0;
	if (space->defs || space->num_defs) {
		linker_error ("defs in code space");
		return 1;
	}
	if (space->id != qfo_code_space)
		linker_warning ("hmm, unexpected code space. *shrug*");
	add_code (space);
	return 0;
}

static int
process_data (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 1)
		return 0;
	if (space->id == qfo_near_data_space) {
		add_data (qfo_near_data_space, space);
	} else if (space->id == qfo_far_data_space) {
		add_data (qfo_far_data_space, space);
	} else {
		add_space (qfo, space);
	}
	return 0;
}

static int
process_strings (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 0)
		return 0;
	if (space->defs || space->num_defs) {
		linker_error ("defs in strings space");
		return 1;
	}
	add_qfo_strings (space);
	return 0;
}

static int
process_entity (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 1)
		return 0;
	add_data (qfo_entity_space, space);
	return 0;
}

static pointer_t
transfer_type (qfo_t *qfo, qfo_mspace_t *space, pointer_t type_offset)
{
	qfot_type_t *type;
	int         i;

	type = (qfot_type_t *) (char *) &space->d.data[type_offset];
	if (type->ty < 0)
		return type->t.class;
	switch ((ty_type_e) type->ty) {
		case ty_none:
			if (type->t.type == ev_func) {
				int         count;
				qfot_func_t *f = &type->t.func;
				pointer_t  *p;
				count = f->num_params;
				if (count < 0)
					count = ~count;	//ones complement
				f->return_type = transfer_type (qfo, space, f->return_type);
				for (i = 0, p = f->param_types; i < count; i++, p++)
					*p = transfer_type (qfo, space, *p);
			} else if (type->t.type == ev_pointer
					   || type->t.type == ev_field) {
				qfot_fldptr_t *fp = &type->t.fldptr;
				fp->aux_type = transfer_type (qfo, space, fp->aux_type);
			}
			break;
		case ty_struct:
		case ty_union:
		case ty_enum:
			type->t.strct.tag = add_string (QFOSTR (qfo, type->t.strct.tag));
			for (i = 0; i < type->t.strct.num_fields; i++) {
				qfot_var_t *field = &type->t.strct.fields[i];
				field->type = transfer_type (qfo, space, field->type);
				field->name = add_string (QFOSTR (qfo, field->name));
			}
			break;
		case ty_array:
			type->t.array.type = transfer_type (qfo, space,
												type->t.array.type);
			break;
		case ty_class:
			//FIXME this is broken
			break;
	}
	type_offset = defspace_alloc_loc (work_type_data, type->size);
	memcpy (work_type_data->data + type_offset, type,
			type->size * sizeof (pr_type_t));
	type->ty = -1;
	type->t.class = type_offset;
	return type_offset;
}

static int
process_type (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	int         i;
	int         size;
	qfo_def_t  *def;
	qfo_def_t  *type_def;
	qfo_mspace_t *type_space;
	qfot_type_t *type;
	const char *name;
	defref_t   *ref;
	pointer_t   offset;

	if (pass != 0)
		return 0;
	if (qfo_type_defs) {
		linker_error ("type space already defined");
		return 1;
	}
	qfo_type_defs = space;
	type_space = &work->spaces[qfo_type_space];
	size = (type_space->num_defs + space->num_defs) * sizeof (qfo_def_t);
	type_space->defs = realloc (type_space->defs, size);
	// first pass: check for already defined types
	for (i = 0, def = space->defs; i < space->num_defs; i++, def++) {
		name = QFOSTR (qfo, def->name);
		type = (qfot_type_t *) (char *) &space->d.data[def->offset];
		if ((ref = Hash_Find (defined_type_defs, name))) {
			type->ty = -1;
			type->t.class = REF (ref)->offset;
			continue;
		}
	}
	// second pass: transfer any new types
	for (i = 0, def = space->defs; i < space->num_defs; i++, def++) {
		name = QFOSTR (qfo, def->name);
		type = (qfot_type_t *) (char *) &space->d.data[def->offset];
		if (type->ty < 0)
			continue;
		offset = transfer_type (qfo, space, def->offset);
		type_def = type_space->defs + type_space->num_defs++;
		memset (type_def, 0, sizeof (*type_def));
		type_def->name = add_string (name);
		type_def->offset = offset;
		ref = get_defref (type_def, type_space, &type_space->defs);
		Hash_Add (defined_type_defs, ref);
		while ((ref = Hash_Del (extern_type_defs, name))) {
			REF (ref)->flags = 0;
			REF (ref)->offset = type_def->offset;
		}
	}
	size = type_space->num_defs * sizeof (qfo_def_t);
	type_space->defs = realloc (type_space->defs, size);

	type_space->d.data = work_type_data->data;
	type_space->data_size = work_type_data->size;
	return 0;
}

static int
linker_add_qfo (qfo_t *qfo)
{
	static space_func funcs[] = {
		process_null,
		process_code,
		process_data,
		process_strings,
		process_entity,
		process_type,
	};
	int         i;
	int         pass;
	qfo_mspace_t *space;

	qfo_type_defs = 0;
	for (pass = 0; pass < 2; pass++) {
		for (i = 0, space = qfo->spaces; i < qfo->num_spaces; i++, space++) {
			if (space->type < 0 || space->type > qfos_type) {
				linker_error ("bad space type");
				return 1;
			}
			if (funcs[space->type] (qfo, space, pass))
				return 1;
		}
	}
	return 0;
}

int
linker_add_object_file (const char *filename)
{
	qfo_t      *qfo;

	dsprintf (linker_current_file, "%s", filename);

	qfo = qfo_open (filename);
	if (!qfo) {
		linker_error ("error opening");
		return 1;
	}
	if (qfo->num_spaces < qfo_num_spaces
		|| qfo->spaces[qfo_null_space].type != qfos_null
		|| qfo->spaces[qfo_strings_space].type != qfos_string
		|| qfo->spaces[qfo_code_space].type != qfos_code
		|| qfo->spaces[qfo_near_data_space].type != qfos_data
		|| qfo->spaces[qfo_far_data_space].type != qfos_data
		|| qfo->spaces[qfo_entity_space].type != qfos_entity
		|| qfo->spaces[qfo_type_space].type != qfos_type) {
		linker_error ("missing or mangled standard spaces");
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

			dsprintf (linker_current_file, "%s(%s)", path_name,
					  pack->files[i].name);
			f = Qsubopen (path_name, pack->files[i].filepos,
						  pack->files[i].filelen, 1);
			qfo = qfo_read (f);
			Qclose (f);

			if (!qfo) {
				linker_error ("error opening");
				return 1;
			}

			for (j = 0; j < qfo->num_defs; j++) {
				qfo_def_t  *def = qfo->defs + j;
				if ((def->flags & QFOD_GLOBAL)
					&& !(def->flags & QFOD_EXTERNAL)
					&& Hash_Find (extern_defs, QFOSTR (qfo, def->name))) {
					if (options.verbosity >= 2)
						printf ("adding %s because of %s\n",
								pack->files[i].name, QFOSTR (qfo, def->name));
					linker_add_qfo (qfo);
					did_something = 1;
					break;
				}
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

static void
linker_warning (const char *fmt, ...)
{
	va_list     args;

	fprintf (stderr, "%s: warning: ", linker_current_file->str);

	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);

	fputs ("\n", stderr);

	if (options.warnings.promote)
		pr.error_count++;
}

static void
linker_error (const char *fmt, ...)
{
	va_list     args;

	fprintf (stderr, "%s: ", linker_current_file->str);

	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);

	fputs ("\n", stderr);

	pr.error_count++;
}
