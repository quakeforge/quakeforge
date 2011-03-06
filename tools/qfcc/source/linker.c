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

#include "class.h"
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

/**	Safe handling of defs in hash tables and other containers.

	As defs are stored in dynamic arrays, storing pointers to the defs is
	a recipe for disaster when using realloc(). By storing the address of
	the pointer to the array and the index into that that array, realloc
	is allowed to do its magic.
*/
typedef struct defref_s {
	struct defref_s *next;			///< if \c merge is true, the next def to
									///< be merged with the main def
	int         def;				///< the index of this def
	int         space;				///< the space in which this def resides
	int         merge;				///< merge def's relocs with the main def
	struct defref_s *merge_list;	///< list of defs to be merged with this
									///< def
} defref_t;

/**	Retreive a def from a defref.

	\param r		The defref pointing to the def (defref_t *).
	\return			A pointer to the def (qfo_def_t *).
*/
#define REF(r) (work->spaces[(r)->space].defs + (r)->def)

typedef struct builtin_sym_s {
	const char *name;
	type_t     *type;
	unsigned    flags;
	defref_t   *defref;
} builtin_sym_t;

static builtin_sym_t builtin_symbols[] __attribute__ ((used)) = {
	{".zero",		&type_zero,		QFOD_NOSAVE | QFOD_GLOBAL},
	{".return",		&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_0",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_1",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_2",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_3",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_4",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_5",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_6",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
	{".param_7",	&type_param,	QFOD_NOSAVE | QFOD_GLOBAL},
};
static const int num_builtins = sizeof (builtin_symbols)
								/ sizeof (builtin_symbols[0]);

static defref_t *free_defrefs;

static hashtab_t *extern_defs;
static hashtab_t *defined_defs;

static hashtab_t *extern_type_defs;
static hashtab_t *defined_type_defs;
static qfo_mspace_t *qfo_type_defs;

static qfo_t *work;
static int work_base[qfo_num_spaces];
static int work_func_base;
static defref_t **work_defrefs;
static int num_work_defrefs;
static strpool_t *work_strings;
static codespace_t *work_code;
static defspace_t *work_near_data;
static defspace_t *work_far_data;
static defspace_t *work_entity_data;
static defspace_t *work_type_data;
static qfo_reloc_t *work_loose_relocs;
static int work_num_loose_relocs;

static defspace_t **work_spaces[qfo_num_spaces] = {
	0, 0, 0,
	&work_near_data,
	&work_far_data,
	&work_entity_data,
	0,
};

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
	\return			The new reference.
*/
static defref_t *
get_defref  (qfo_def_t *def, qfo_mspace_t *space)
{
	defref_t   *defref;

	ALLOC (16384, defref_t, defrefs, defref);
	defref->def = def - space->defs;
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
int
linker_add_string (const char *str)
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

	\param ext		The external def.
	\param def		The global definition.
*/
static void
resolve_external_def (defref_t *ext, defref_t *def)
{
	if (!(REF (ext)->flags & QFOD_EXTERNAL))
		internal_error (0, "ext is not an external def");
	if (!(REF (def)->flags & QFOD_GLOBAL))
		internal_error (0, "def is not a global def");
	if (REF (ext)->type != REF (def)->type) {
		linker_type_mismatch (REF (ext), REF (def));
		return;
	}
	REF (ext)->offset = REF (def)->offset;
	REF (ext)->flags = REF (def)->flags;
	ext->merge = 1;
	ext->space = def->space;
	ext->next = def->merge_list;
	def->merge_list = ext;
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
	} else {
		if (!(def->flags & QFOD_LOCAL))
			def->offset += space->data_size;
		if (def->flags & QFOD_GLOBAL) {
			if (r) {
				if (REF (r)->flags & QFOD_SYSTEM) {
					/// System defs may be redefined only once.
					REF (r)->flags &= ~QFOD_SYSTEM;
					// treat the new def as external
					resolve_external_def (ref, r);
					//FIXME copy stuff from new def to existing def???
				} else {
					def_error (def, "%s redefined", WORKSTR (def->name));
					def_error (REF (r), "previous definition");
				}
				return;
			}
			Hash_Add (defined_defs, ref);
			while ((r = Hash_Del (extern_defs, name)))
				resolve_external_def (r, ref);
		}
	}
}

static int
add_relocs (qfo_t *qfo, int start, int count, int target)
{
	int         size;
	qfo_reloc_t *reloc;

	size = work->num_relocs + count;
	work->relocs = realloc (work->relocs, size * sizeof (qfo_reloc_t));
	memcpy (work->relocs + work->num_relocs, qfo->relocs + start,
			count * sizeof (qfo_reloc_t));
	while (work->num_relocs < size) {
		reloc = work->relocs + work->num_relocs++;
		if (reloc->space < 0 || reloc->space >= qfo->num_spaces) {
			linker_error ("bad reloc space: %d (%d)", reloc->space,
						  qfo->num_spaces);
			reloc->type = rel_none;
			continue;
		}
		reloc->space = qfo->spaces[reloc->space].id;
		if (!reloc->space) {
			//FIXME double check
			reloc->offset += work_base[qfo_code_space];
		} else if (reloc->space < qfo_num_spaces) {
			reloc->offset += work_base[reloc->space];
		}
		reloc->target = target;
	}
	return work->num_relocs - count;
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
		*odef = *idef;						// copy the def data
		idef->offset = num_work_defrefs;	// so def can be found
		odef->name = linker_add_string (QFOSTR (qfo, idef->name));
		odef->file = linker_add_string (QFOSTR (qfo, idef->file));
		type = (qfot_type_t *) (char *) (qfo_type_defs->d.data + idef->type);
		odef->type = type->t.class;			// pointer to type in work
		if (odef->flags & QFOD_EXTERNAL && !odef->num_relocs)
			continue;
		ref = get_defref (odef, dest_space);
		work_defrefs[num_work_defrefs++] = ref;
		process_def (ref, dest_space);
		odef->relocs = add_relocs (qfo, odef->relocs, odef->num_relocs,
								   odef - dest_space->defs);
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
		linker_add_string (str);
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
	if (space < 0 || space >= qfo_num_spaces || !work_spaces[space])
		internal_error (0, "bad space for add_data (): %d", space);
	defspace_add_data (*work_spaces[space], data->d.data, data->data_size);
	work->spaces[space].d.data = (*work_spaces[space])->data;
	work->spaces[space].data_size = (*work_spaces[space])->size;
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

static pointer_t
transfer_type (qfo_t *qfo, qfo_mspace_t *space, pointer_t type_offset)
{
	qfot_type_t *type;
	int         i;
	const char *str;

	type = (qfot_type_t *) (char *) &space->d.data[type_offset];
	if (type->ty < 0)
		return type->t.class;
	type->encoding = linker_add_string (QFOSTR (qfo, type->encoding));
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
			str = QFOSTR (qfo, type->t.strct.tag);
			type->t.strct.tag = linker_add_string (str);
			for (i = 0; i < type->t.strct.num_fields; i++) {
				qfot_var_t *field = &type->t.strct.fields[i];
				field->type = transfer_type (qfo, space, field->type);
				field->name = linker_add_string (QFOSTR (qfo, field->name));
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

static defref_t *
define_def (const char *name, type_t *type, unsigned flags, int v)
{
	qfo_def_t  *def;
	defref_t   *ref;
	qfo_mspace_t *space;
	defspace_t *def_space;

	space = &work->spaces[qfo_near_data_space];
	def_space = work_near_data;

	space->defs = realloc (space->defs,
						   (space->num_defs + 1) * sizeof (qfo_def_t));
	def = space->defs + space->num_defs++;
	memset (def, 0, sizeof (*def));
	def->name = linker_add_string (name);
	def->type = -linker_add_string (type->encoding);// this will be fixed later
	def->offset = defspace_alloc_loc (def_space, type_size (type));
	def->flags = flags;
	def_space->data[def->offset].integer_var = v;
	space->d.data = def_space->data;
	space->data_size = def_space->size;

	ref = get_defref (def, space);
	work_defrefs = realloc (work_defrefs,
							(num_work_defrefs + 1) * sizeof (defref_t *));
	work_defrefs[num_work_defrefs++] = ref;
	Hash_Add (defined_defs, ref);

	return ref;
}

void
linker_add_def (const char *name, type_t *type, unsigned flags, int v)
{
	define_def (name, type, flags, v);
}

/**	Initialize the linker state.
*/
void
linker_begin (void)
{
	int         i;

	linker_current_file = dstring_newstr ();

	extern_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_defs = Hash_NewTable (16381, defs_get_key, 0, 0);

	extern_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0);
	defined_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0);

	work_strings = strpool_new ();
	work_code = codespace_new ();
	work_near_data = defspace_new ();
	work_far_data = defspace_new ();
	work_entity_data = defspace_new ();
	work_type_data = defspace_new ();
	defspace_alloc_loc (work_type_data, 4);

	pr.strings = work_strings;

	work = qfo_new ();
	work->spaces = calloc (qfo_num_spaces, sizeof (qfo_mspace_t));
	work->num_spaces = qfo_num_spaces;
	work->spaces[qfo_null_space].type = qfos_null;
	work->spaces[qfo_strings_space].type = qfos_string;
	work->spaces[qfo_strings_space].d.strings = work_strings->strings;
	work->spaces[qfo_strings_space].data_size = work_strings->size;
	work->spaces[qfo_code_space].type = qfos_code;
	work->spaces[qfo_code_space].d.code = work_code->code;
	work->spaces[qfo_code_space].data_size = work_code->size;
	work->spaces[qfo_near_data_space].type = qfos_data;
	work->spaces[qfo_near_data_space].d.data = work_near_data->data;
	work->spaces[qfo_near_data_space].data_size = work_near_data->size;
	work->spaces[qfo_far_data_space].type = qfos_data;
	work->spaces[qfo_far_data_space].d.data = work_far_data->data;
	work->spaces[qfo_far_data_space].data_size = work_far_data->size;
	work->spaces[qfo_entity_space].type = qfos_entity;
	work->spaces[qfo_entity_space].d.data = work_entity_data->data;
	work->spaces[qfo_entity_space].data_size = work_entity_data->size;
	work->spaces[qfo_type_space].type = qfos_type;
	work->spaces[qfo_type_space].d.data = work_type_data->data;
	work->spaces[qfo_type_space].data_size = work_type_data->size;
	for (i = 0; i < qfo_num_spaces; i++)
		work->spaces[i].id = i;

	work->lines = calloc (1, sizeof (pr_lineno_t));
	work->num_lines = 1;

	if (!options.partial_link) {
		for (i = 0; i < num_builtins; i++) {
			builtin_sym_t *bi = builtin_symbols + i;
			bi->defref = define_def (bi->name, bi->type, bi->flags, 0);
		}
	}
}

typedef int (*space_func) (qfo_t *qfo, qfo_mspace_t *space, int pass);

static int
process_null_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
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
process_code_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
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
process_data_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 1)
		return 0;
	if (space->id == qfo_near_data_space) {
		add_defs (qfo, space, work->spaces + qfo_near_data_space);
		add_data (qfo_near_data_space, space);
	} else if (space->id == qfo_far_data_space) {
		add_defs (qfo, space, work->spaces + qfo_far_data_space);
		add_data (qfo_far_data_space, space);
	} else {
		add_space (qfo, space);
	}
	return 0;
}

static int
process_strings_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
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
process_entity_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 1)
		return 0;
	add_defs (qfo, space, work->spaces + qfo_entity_space);
	add_data (qfo_entity_space, space);
	return 0;
}

static int
process_type_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
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
		type_def->name = linker_add_string (name);
		type_def->offset = offset;
		ref = get_defref (type_def, type_space);
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

	for (i = 0; i < num_builtins; i++) {
		builtin_sym_t *bi = builtin_symbols + i;
		if (!bi->defref)
			continue;
		def = REF (bi->defref);
		if (def->type >= 0)
			continue;
		ref = Hash_Find (defined_type_defs, WORKSTR (-def->type));
		if (!ref)
			continue;
		def->type = REF (ref)->offset;
	}
	return 0;
}

static void
process_funcs (qfo_t *qfo)
{
	int         size;
	qfo_func_t *func;
	qfot_type_t *type;

	size = work->num_funcs + qfo->num_funcs;
	work->funcs = realloc (work->funcs, size * sizeof (qfo_func_t));
	memcpy (work->funcs + work->num_funcs, qfo->funcs,
			qfo->num_funcs * sizeof (qfo_func_t));
	while (work->num_funcs < size) {
		func = work->funcs + work->num_funcs++;
		type = (qfot_type_t *) (char *) (qfo_type_defs->d.data + func->type);
		func->type = type->t.class;
		func->name = linker_add_string (QFOSTR (qfo, func->name));
		func->file = linker_add_string (QFOSTR (qfo, func->file));
		if (func->code > 0)
			func->code += work_base[qfo_code_space];
		func->def = qfo->defs[func->def].offset;	// defref index
		func->locals_space = qfo->spaces[func->locals_space].id;
		if (func->line_info)
			func->line_info += work->num_lines - 1;		//FIXME order dependent
		func->relocs = add_relocs (qfo, func->relocs, func->num_relocs,
								   func - work->funcs);
	}
}

static void
process_lines (qfo_t *qfo)
{
	int         size;
	pr_lineno_t *line;

	if (!qfo->num_lines)
		return;
	size = work->num_lines + qfo->num_lines - 1;
	work->lines = realloc (work->lines, size * sizeof (pr_lineno_t));
	memcpy (work->lines + work->num_lines, qfo->lines + 1,
			(qfo->num_lines - 1) * sizeof (pr_lineno_t));
	while (work->num_lines < size) {
		line = work->lines + work->num_lines++;
		if (line->line)
			line->fa.addr += work_base[qfo_code_space];
		else
			line->fa.func += work_func_base;
	}
}

static void
process_loose_relocs (qfo_t *qfo)
{
	int         size;
	qfo_reloc_t *reloc;

	size = work_num_loose_relocs + qfo->num_loose_relocs;
	work_loose_relocs = realloc (work_loose_relocs,
								 size * sizeof (qfo_reloc_t));
	memcpy (work_loose_relocs + work_num_loose_relocs,
			qfo->relocs + qfo->num_relocs - qfo->num_loose_relocs,
			qfo->num_loose_relocs * sizeof (qfo_reloc_t));
	while (work_num_loose_relocs < size) {
		reloc = work_loose_relocs + work_num_loose_relocs++;
		if (reloc->space < 0 || reloc->space >= qfo->num_spaces) {
			linker_error ("bad reloc space");
			reloc->type = rel_none;
			continue;
		}
		if (reloc->space == qfo_type_defs - qfo->spaces) {
			// loose relocs in the type space become invalid
			reloc->type = rel_none;
			continue;
		}
		reloc->space = qfo->spaces[reloc->space].id;
		if (reloc->type == rel_def_string) {
			const char *str;
			
			if (reloc->target < 0
				|| reloc->target >= qfo->spaces[qfo_strings_space].data_size) {
				linker_error ("bad string reloc at %d:%x", reloc->space,
							  reloc->offset);
				reloc->target = 0;
			}
			str = QFOSTR (qfo, reloc->target);
			reloc->target = linker_add_string (str);
		}
		if (!reloc->space) {
			//FIXME double check
			reloc->offset += work_base[qfo_code_space];
		} else if (reloc->space < qfo_num_spaces) {
			reloc->offset += work_base[reloc->space];
		}
	}
}

int
linker_add_qfo (qfo_t *qfo)
{
	static space_func funcs[] = {
		process_null_space,
		process_code_space,
		process_data_space,
		process_strings_space,
		process_entity_space,
		process_type_space,
	};
	int         i;
	int         pass;
	qfo_mspace_t *space;

	qfo_type_defs = 0;
	for (i = 0; i < qfo_num_spaces; i++) {
		work_base[i] = work->spaces[i].data_size;
	}
	work_func_base = work->num_funcs;
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
	process_funcs (qfo);
	process_lines (qfo);
	process_loose_relocs (qfo);
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
		fprintf (stderr, "%s\n", filename);

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
						fprintf (stderr, "adding %s because of %s\n",
								 pack->files[i].name,
								 QFOSTR (qfo, def->name));
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
	qfo_def_t   line_def;
	pr_int_t    i;
	qfo_reloc_t *reloc = work->relocs + def->relocs;

	for (i = 0; i < def->num_relocs; i++, reloc++) {
		if ((reloc->type == rel_op_a_def
			 || reloc->type == rel_op_b_def
			 || reloc->type == rel_op_c_def
			 || reloc->type == rel_op_a_def_ofs
			 || reloc->type == rel_op_b_def_ofs
			 || reloc->type == rel_op_c_def_ofs)
			&& work->lines) {
			qfo_func_t *func = work->funcs;
			qfo_func_t *best = func;
			pr_int_t    best_dist = reloc->offset - func->code;
			pr_lineno_t *line;

			while (best_dist && func - work->funcs < work->num_funcs) {
				if (func->code <= reloc->offset) {
					if (best_dist < 0
						|| reloc->offset - func->code < best_dist) {
						best = func;
						best_dist = reloc->offset - func->code;
					}
				}
				func++;
			}
			line = work->lines + best->line_info;
			line_def.file = best->file;
			line_def.line = best->line;
			if (!line->line
				&& line->fa.func == (pr_uint_t) (best - work->funcs)) {
				while (line - work->lines < work->num_lines - 1
					   && line[1].line
					   && line[1].fa.addr <= (pr_uint_t) reloc->offset)
					line++;
				line_def.line = line->line + best->line;
			}
			def_error (&line_def, "undefined symbol %s", WORKSTR (def->name));
		} else {
			def_error (def, "undefined symbol %s", WORKSTR (def->name));
		}
	}
}

static void
check_defs (void)
{
	defref_t  **undef_defs, **defref;
	int         did_self = 0, did_this = 0;

	undef_defs = (defref_t **) Hash_GetList (extern_defs);
	for (defref = undef_defs; *defref; defref++) {
		qfo_def_t  *def = REF (*defref);
		const char *name = WORKSTR (def->name);

		if (strcmp (name, ".self") == 0 && !did_self) {
			defref_t   *_d = Hash_Find (defined_defs, "self");
			if (_d) {
				qfo_def_t  *d = REF (_d);
				if (QFO_TYPEMETA (work, d->type) == ty_none
					&& QFO_TYPETYPE (work, d->type) == ev_entity)
					def_warning (d, "@self and self used together");
			}
			linker_add_def (".self", &type_entity, QFOD_GLOBAL, 0);
			did_self = 1;
		} else if (strcmp (name, ".this") == 0 && !did_this) {
			pointer_t   offset;
			type_t     *type;
			int         flags;

			if (!class_Class.super_class)
				class_init ();
			type = field_type (&type_ClassPtr);
			offset = defspace_alloc_loc (work_entity_data, 1);
			flags = (QFOD_GLOBAL | QFOD_CONSTANT
					 | QFOD_INITIALIZED | QFOD_NOSAVE);
			linker_add_def (".this", type, flags, offset);
			did_this = 1;
		}
	}
	free (undef_defs);
	undef_defs = (defref_t **) Hash_GetList (extern_defs);
	for (defref = undef_defs; *defref; defref++) {
		qfo_def_t  *def = REF (*defref);
		undefined_def (def);
	}
	free (undef_defs);
}

static qfo_t *
build_qfo (void)
{
	qfo_t      *qfo;
	int         size;
	int         i, j;
	qfo_mspace_t *space;
	qfo_reloc_t *reloc;
	qfo_def_t **defs;

	qfo = qfo_new ();
	qfo->spaces = calloc (work->num_spaces, sizeof (qfo_mspace_t));
	qfo->num_spaces = work->num_spaces;
	for (i = 0; i < work->num_spaces; i++) {
		space = &work->spaces[i];
		qfo->spaces[i].type = work->spaces[i].type;
		qfo->spaces[i].id = work->spaces[i].id;
		qfo->spaces[i].d = work->spaces[i].d;
		qfo->spaces[i].data_size = work->spaces[i].data_size;
	}
	// allocate space for all relocs and copy in the loose relocs. bound
	// relocs will be handled with defs and funcs.
	size = work->num_relocs + work_num_loose_relocs;
	qfo->relocs = malloc (size * sizeof (qfo_reloc_t));
	reloc = qfo->relocs;
	memcpy (qfo->relocs + work->num_relocs, work_loose_relocs,
			work_num_loose_relocs * sizeof (qfo_reloc_t));
	qfo->num_relocs = size;
	qfo->num_loose_relocs = work_num_loose_relocs;
	qfo->funcs = work->funcs;
	qfo->num_funcs = work->num_funcs;
	qfo->lines = work->lines;
	qfo->num_lines = work->num_lines;
	// count final defs
	for (i = 0; i < num_work_defrefs; i++) {
		if (work_defrefs[i]->merge)
			continue;
		qfo->num_defs++;
		qfo->spaces[work_defrefs[i]->space].num_defs++;
	}
	qfo->defs = malloc (qfo->num_defs * sizeof (qfo_def_t));
	defs = alloca (qfo->num_spaces * sizeof (qfo_def_t *));
	defs[0] = qfo->defs;
	for (i = 1; i < qfo->num_spaces; i++) {
		defs[i] = defs[i - 1] + qfo->spaces[i - 1].num_defs;
		if (qfo->spaces[i].num_defs)
			qfo->spaces[i].defs = defs[i];
	}
	for (i = 0; i < num_work_defrefs; i++) {
		defref_t   *r = work_defrefs[i];
		qfo_def_t  *def;
		qfo_def_t   d;
		int         space;
		if (r->merge)
			continue;
		space = r->space;
		def = REF (r);
		d = *def;
		d.relocs = reloc - qfo->relocs;
		memcpy (reloc, work->relocs + def->relocs,
				def->num_relocs * sizeof (qfo_reloc_t));
		r->def = defs[space] - qfo->defs;
		for (j = 0; j < def->num_relocs; j++) {
			reloc->target = r->def;
			reloc++;
		}
		// copy relocs from merged defs
		for (r = r->merge_list; r; r = r->next) {
			def = REF (r);
			memcpy (reloc, work->relocs + def->relocs,
					def->num_relocs * sizeof (qfo_reloc_t));
			d.num_relocs += def->num_relocs;
			r->space = space;
			r->def = defs[space] - qfo->defs;
			for (j = 0; j < def->num_relocs; j++) {
				reloc->target = r->def;
				reloc++;
			}
		}
		*defs[space]++ = d;
	}
	for (i = 0; i < qfo->num_funcs; i++) {
		qfo_func_t *f = &qfo->funcs[i];
		f->def = work_defrefs[f->def]->def;
		memcpy (reloc, work->relocs + f->relocs,
				f->num_relocs * sizeof (qfo_reloc_t));
		f->relocs = reloc - qfo->relocs;
		for (j = 0; j < f->num_relocs; j++) {
			reloc->target = i;
			reloc++;
		}
	}
	return qfo;
}

qfo_t *
linker_finish (void)
{
	int         i;

	if (!options.partial_link) {
		check_defs ();
		if (pr.error_count)
			return 0;
	}

	for (i = 0; i < work_num_loose_relocs; /**/) {
		if (work_loose_relocs[i].type != rel_none) {
			i++;
			continue;
		}
		memmove (work_loose_relocs + i, work_loose_relocs + i + 1,
				 (--work_num_loose_relocs - i) * sizeof (qfo_reloc_t));
	}
	return build_qfo ();
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
