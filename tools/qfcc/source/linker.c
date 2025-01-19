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

#include "qfalloca.h"

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pakfile.h"
#include "QF/va.h"

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/linker.h"
#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"

static void linker_internal_error (const char *fmt, ...)
	__attribute__ ((format (PRINTF, 1, 2), noreturn));
static void linker_error (const char *fmt, ...)
	__attribute__ ((format (PRINTF, 1, 2)));
static void linker_warning (const char *fmt, ...)
	__attribute__ ((format (PRINTF, 1, 2)));
static void def_error (qfo_def_t *def, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 2, 3)));
static void def_warning (qfo_def_t *def, const char *fmt, ...)
	__attribute__ ((used, format (PRINTF, 2, 3)));

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
static const unsigned num_builtins = sizeof (builtin_symbols)
									 / sizeof (builtin_symbols[0]);

ALLOC_STATE (defref_t, defrefs);

static hashtab_t *extern_data_defs;
static hashtab_t *defined_data_defs;

static hashtab_t *extern_field_defs;
static hashtab_t *defined_field_defs;

static hashtab_t *extern_type_defs;
static hashtab_t *defined_type_defs;
static qfo_mspace_t *qfo_type_defs;

static qfo_t *work;
static int work_base[qfo_num_spaces];
static int work_func_base;
static defref_t **work_defrefs;
static unsigned num_work_defrefs;
static strpool_t *work_strings;
static codespace_t *work_code;
static defspace_t *work_near_data;
static defspace_t *work_far_data;
static defspace_t *work_entity_data;
static defspace_t *work_type_data;
static defspace_t *work_debug_data;
static qfo_reloc_t *work_loose_relocs;
static int work_num_loose_relocs;

static defspace_t **work_spaces[qfo_num_spaces] = {
	0, 0, 0,
	&work_near_data,
	&work_far_data,
	&work_entity_data,
	&work_type_data,
	&work_debug_data,
};

static dstring_t *linker_current_file;

#define QFOSTR(q,s)		QFO_GETSTR (q, s)
#define WORKSTR(s)		QFOSTR (work, s)
#define QFOTYPE(t)		((qfot_type_t *) (char *) \
						 (qfo_type_defs->data + (t)))
#define WORKTYPE(t)		((qfot_type_t *) (char *) \
						 (work_type_data->data + (t)))

/**	Produce an error message for defs with mismatched types.

	\param def		The new def.
	\param prev		The previous definition.
*/
static void
linker_type_mismatch (qfo_def_t *def, qfo_def_t *prev)
{
	const char *def_encoding;
	const char *prev_encoding;

	if ((int) def->type < 0)
		def_encoding = QFO_GETSTR (work, -(int) def->type);
	else
		def_encoding = QFO_TYPESTR (work, def->type);
	if ((int) prev->type < 0)
		prev_encoding = QFO_GETSTR (work, -(int) prev->type);
	else
		prev_encoding = QFO_TYPESTR (work, prev->type);
	def_error (def, "type mismatch for `%s' `%s'", WORKSTR (def->name),
			   def_encoding);
	def_error (prev, "previous definition `%s'",
			   prev_encoding);
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
defs_get_key (const void *_r, void *unused)
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
	pr_string_t new;
	new = strpool_addstr (work_strings, str);
	work->spaces[qfo_strings_space].strings = work_strings->strings;
	work->spaces[qfo_strings_space].data_size = work_strings->size;
	return new;
}

/** Allocate data from the working qfo.

	\param space	The space from which to allocate data.
	\param size		The number of words to allocate.
	\return			The offset of the allocated data.
*/
static int
alloc_data (int space, int size)
{
	int         offset;

	if (space < 0 || space >= qfo_num_spaces || !work_spaces[space])
		linker_internal_error ("bad space for alloc_data (): %d", space);
	if (size <= 0)
		linker_internal_error ("bad size for alloc_data (): %d", space);
	offset = defspace_alloc_loc (*work_spaces[space], size);
	work->spaces[space].data = (*work_spaces[space])->data;
	work->spaces[space].data_size = (*work_spaces[space])->size;
	return offset;
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
	qfot_type_t *ext_type;
	qfot_type_t *def_type;
	if (!(REF (ext)->flags & QFOD_EXTERNAL)) {
		def_error (REF (ext), "%s %x", WORKSTR (REF (ext)->name),
				   REF (ext)->flags);
		linker_internal_error ("ext is not an external def");
	}
	if (REF (def)->flags & (QFOD_EXTERNAL | QFOD_LOCAL)) {
		def_error (REF (def), "%s %x", WORKSTR (REF (def)->name),
				   REF (def)->flags);
		linker_internal_error ("def is an external or local def");
	}
	ext_type = WORKTYPE (REF(ext)->type);
	if (ext_type->meta == ty_alias) {
		ext_type = WORKTYPE (ext_type->alias.aux_type);
	}
	def_type = WORKTYPE (REF (def)->type);
	if (def_type->meta == ty_alias) {
		def_type = WORKTYPE (def_type->alias.aux_type);
	}
	if (ext_type != def_type) {
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
define_def (defref_t *ref, hashtab_t *extern_tab, hashtab_t *defined_tab)
{
	defref_t   *r;
	const char *name;

	if (REF (ref)->flags & (QFOD_EXTERNAL | QFOD_LOCAL)) {
		def_error (REF (ref), "%s %x", WORKSTR (REF (ref)->name),
				   REF (ref)->flags);
		linker_internal_error ("ref is an external or local def");
	}
	name = WORKSTR (REF (ref)->name);
	if ((r = Hash_Find (defined_tab, name))) {
		if (REF (r)->flags & QFOD_SYSTEM) {
			/// System defs may be redefined only once.
			REF (r)->flags &= ~QFOD_SYSTEM;
			// treat the new def as external
			REF (ref)->flags |= QFOD_EXTERNAL;
			resolve_external_def (ref, r);
		} else {
			def_error (REF (ref), "%s redefined", WORKSTR (REF (ref)->name));
			def_error (REF (r), "previous definition");
		}
		return;
	}
	Hash_Add (defined_tab, ref);
	while ((r = Hash_Del (extern_tab, name)))
		resolve_external_def (r, ref);
}

static void
extern_def (defref_t *ref, hashtab_t *extern_tab, hashtab_t *defined_tab)
{
	defref_t   *r;
	const char *name;

	name = WORKSTR (REF (ref)->name);
	r = Hash_Find (defined_tab, name);
	if (!REF (ref)->num_relocs)
		return;
	if (r) {
		resolve_external_def (ref, r);
	} else {
		Hash_Add (extern_tab, ref);
	}
}

static void
process_def (defref_t *ref, qfo_mspace_t *space,
			 hashtab_t  *extern_tab, hashtab_t  *defined_tab)
{
	if (REF (ref)->flags & QFOD_EXTERNAL) {
		extern_def (ref, extern_tab, defined_tab);
	} else {
		if (!(REF (ref)->flags & QFOD_LOCAL))
			REF (ref)->offset += space->data_size;
		if (REF (ref)->flags & QFOD_GLOBAL)
			define_def (ref, extern_tab, defined_tab);
	}
}

static void
process_data_def (defref_t *ref, qfo_mspace_t *space, qfo_def_t *old)
{
	process_def (ref, space, extern_data_defs, defined_data_defs);
}

static void
process_field_def (defref_t *ref, qfo_mspace_t *space, qfo_def_t *old)
{
	process_def (ref, space, extern_field_defs, defined_field_defs);
}

static void
process_type_def (defref_t *ref, qfo_mspace_t *space, qfo_def_t *old)
{
	if (REF (ref)->flags & QFOD_EXTERNAL) {
		// The most common (only?) time external type encodings show up is
		// in pointers to incomplete structs. eg, struct foo *bar; with no
		// struct foo {...};
		extern_def (ref, extern_type_defs, defined_type_defs);
	} else {
		const char *name;
		defref_t   *r;
		qfot_type_t *old_type;
		qfot_type_t *new_type;

		old_type = QFOTYPE(old->offset);
		name = WORKSTR (REF (ref)->name);
		if ((r = Hash_Find (defined_type_defs, name))) {
			// The type has been defined already, so treat this def as if it
			// were external
			REF (ref)->flags |= QFOD_EXTERNAL;
			resolve_external_def (ref, r);
		} else {
			// The type is new to the program, so we need to allocate space
			// for it and copy the type encoding. Relocation records will
			// take care of any cross-references.

			REF (ref)->offset = alloc_data (qfo_type_space, old_type->size);
			new_type = WORKTYPE (REF (ref)->offset);
			memcpy (new_type, old_type, old_type->size * sizeof (pr_type_t));
			define_def (ref, extern_type_defs, defined_type_defs);
		}
		// Save the new address in the old def's type field so relocation
		// records can be updated. Type encoding defs start with no type
		// (type = 0). The old def's offset is not modified because it is used
		// for finding the def when adjusting relocation records that point
		// to fields inside the type encoding.
		old->type = REF (ref)->offset;
		// Mark the old type encoding as having been transfered, and save the
		// new address in the encoding's class field so def and function types
		// can be updated easily.
		old_type->meta = -1;
		old_type->class = REF (ref)->offset;
	}
}

static void
adjust_reloc_offset (qfo_reloc_t *reloc)
{
	if (!reloc->space) {
		//FIXME double check
		reloc->offset += work_base[qfo_code_space];
	} else if (reloc->space < qfo_num_spaces) {
		// Relocs in type space are handled by process_type_space, so don't
		// touch them here.
		if (reloc->space != qfo_type_space)
			reloc->offset += work_base[reloc->space];
	}
}

static int
add_relocs (qfo_t *qfo, int start, int count, int target)
{
	unsigned    size;
	qfo_reloc_t *ireloc;
	qfo_reloc_t *oreloc;

	size = work->num_relocs + count;
	work->relocs = realloc (work->relocs, size * sizeof (qfo_reloc_t));
	ireloc = qfo->relocs + start;
	oreloc = work->relocs + work->num_relocs;
	for  ( ; work->num_relocs < size; ireloc++, oreloc++) {
		*oreloc = *ireloc;
		// Mark the reloc as having been copied and record the new reloc record
		// number in the old reloc's offset
		ireloc->type = -1;
		ireloc->offset = work->num_relocs++;
		if (oreloc->space >= qfo->num_spaces) {
			linker_error ("bad reloc space: %d (%d)", oreloc->space,
						  qfo->num_spaces);
			oreloc->type = rel_none;
			continue;
		}
		oreloc->space = qfo->spaces[oreloc->space].id;
		adjust_reloc_offset (oreloc);
		oreloc->target = target;
	}
	return work->num_relocs - count;
}

static int
add_defs (qfo_t *qfo, qfo_mspace_t *space, qfo_mspace_t *dest_space,
		  void (*process) (defref_t *ref, qfo_mspace_t *space, qfo_def_t *old))
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
		odef->name = linker_add_string (QFOSTR (qfo, idef->name));
		odef->file = linker_add_string (QFOSTR (qfo, idef->file));
		idef->file = -1;					// mark def as copied
		idef->line = num_work_defrefs;		// so def can be found
		// In the first pass, process_type_def sets the type meta to -1 and
		// class to the offset of the copied type, but the null type encodiing
		// is not modified. Other defs are processed in the second pass.
		type = QFOTYPE(idef->type);
		if (idef->type && (int) type->meta != -1) {
			linker_internal_error ("reference to type that has not been "
								   "relocated");
		}
		// Type encodings have no type (type = 0) so setting the type
		// to the idef type class has no effect.
		odef->type = type->class;			// pointer to type in work
		// don't add unused (no attached relocs) external defs to the work
		// defref list so they will not cause unused object files to be
		// pulled in from libraries
		if (odef->flags & QFOD_EXTERNAL && !odef->num_relocs)
			continue;
		ref = get_defref (odef, dest_space);
		work_defrefs[num_work_defrefs++] = ref;
		process (ref, dest_space, idef);
		REF (ref)->relocs = add_relocs (qfo, REF (ref)->relocs,
										REF (ref)->num_relocs,
										REF (ref) - dest_space->defs);
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
	const char *str = strings->strings;

	while ((pr_uint_t) (str - strings->strings) < strings->data_size) {
		linker_add_string (str);
		while ((pr_uint_t) (str - strings->strings) < strings->data_size
			   && *str)
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
	codespace_addcode (work_code, code->code, code->data_size);
	work->spaces[qfo_code_space].code = work_code->code;
	work->spaces[qfo_code_space].data_size = work_code->size;
}

static void
align_data (int space, qfo_mspace_t *data)
{
	if (space < 0 || space >= qfo_num_spaces || !work_spaces[space])
		linker_internal_error ("bad space for align_data (): %d", space);
	defspace_alloc_aligned_highwater (*work_spaces[space], 0,
									  1 << data->alignment);
	work->spaces[space].data = (*work_spaces[space])->data;
	work->spaces[space].data_size = (*work_spaces[space])->size;
}

/**	Add the data in a data space to the working qfo.

	\param space	The space to which the data will be added.
	\param data		A data space of the qfo being linked.
*/
static void
add_data (int space, qfo_mspace_t *data)
{
	if (space < 0 || space >= qfo_num_spaces || !work_spaces[space])
		linker_internal_error ("bad space for add_data (): %d", space);
	if (data->data_size)
		defspace_add_data (*work_spaces[space], data->data, data->data_size);
	work->spaces[space].data = (*work_spaces[space])->data;
	work->spaces[space].data_size = (*work_spaces[space])->size;
}

/**	Add a defspace to the working qfo.

	\param qfo		The qfo being linked.
	\param space	The defspace from \a qfo that will be added to the working
					qfo.
*/
static void
add_data_space (qfo_t *qfo, qfo_mspace_t *space)
{
	qfo_mspace_t *ws;
	if (space->type != qfos_data)
		linker_internal_error ("bad space type for add_data_space (): %d",
							   space->type);
	space->id = work->num_spaces++;	// so the space in work can be found
	work->spaces = realloc (work->spaces,
							work->num_spaces * sizeof (qfo_mspace_t));
	ws = &work->spaces[space->id];
	memset (ws, 0, sizeof (qfo_mspace_t));
	ws->type = space->type;
	if (space->num_defs)
		add_defs (qfo, space, ws, process_data_def);
	if (space->data) {
		int         size = space->data_size * sizeof (pr_type_t);
		ws->data = malloc (size);
		memcpy (ws->data, space->data, size);
	}
	ws->data_size = space->data_size;
	ws->id = space->id;
	ws->alignment = space->alignment;
}

static defref_t *
make_def (int s, const char *name, const type_t *type, unsigned flags,
		  void *val)
{
	qfo_def_t  *def;
	defref_t   *ref;
	qfo_mspace_t *space;
	defspace_t *def_space;

	space = &work->spaces[s];
	def_space = *work_spaces[s];

	space->defs = realloc (space->defs,
						   (space->num_defs + 1) * sizeof (qfo_def_t));
	def = space->defs + space->num_defs++;
	memset (def, 0, sizeof (*def));
	def->name = linker_add_string (name);
	def->type = -linker_add_string (type->encoding);
	ref = Hash_Find (defined_type_defs, WORKSTR (-def->type));
	if (ref)
		def->type = REF (ref)->offset;
	def->offset = alloc_data (s, type_size (type));
	def->flags = flags;
	if (val)
		memcpy (&def_space->data[def->offset], val,
				type_size (type) * sizeof (pr_type_t));
	space->data = def_space->data;
	space->data_size = def_space->size;

	ref = get_defref (def, space);
	work_defrefs = realloc (work_defrefs,
							(num_work_defrefs + 1) * sizeof (defref_t *));
	work_defrefs[num_work_defrefs++] = ref;
	if (s == qfo_entity_space) {
		define_def (ref, extern_field_defs, defined_field_defs);
	} else {
		define_def (ref, extern_data_defs, defined_data_defs);
	}

	return ref;
}

void
linker_add_def (const char *name, const type_t *type, unsigned flags, void *val)
{
	make_def (qfo_near_data_space, name, type, flags, val);
}

qfo_def_t *
linker_find_def (const char *name)
{
	defref_t   *r;

	if ((r = Hash_Find (defined_data_defs, name)))
		return REF (r);
	return 0;
}

/**	Initialize the linker state.
*/
void
linker_begin (void)
{
	unsigned    i;

	linker_current_file = dstring_newstr ();

	extern_data_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);
	defined_data_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);

	extern_field_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);
	defined_field_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);

	extern_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);
	defined_type_defs = Hash_NewTable (16381, defs_get_key, 0, 0, 0);

	work_strings = strpool_new ();
	work_code = codespace_new ();
	work_near_data = defspace_new (ds_backed);
	work_far_data = defspace_new (ds_backed);
	work_entity_data = defspace_new (ds_virtual);
	work_type_data = defspace_new (ds_backed);
	work_debug_data = defspace_new (ds_backed);

	pr.strings = work_strings;

	work = qfo_new ();
	work->spaces = calloc (qfo_num_spaces, sizeof (qfo_mspace_t));
	work->num_spaces = qfo_num_spaces;
	work->spaces[qfo_null_space].type = qfos_null;
	work->spaces[qfo_strings_space].type = qfos_string;
	work->spaces[qfo_strings_space].strings = work_strings->strings;
	work->spaces[qfo_strings_space].data_size = work_strings->size;
	work->spaces[qfo_code_space].type = qfos_code;
	work->spaces[qfo_code_space].code = work_code->code;
	work->spaces[qfo_code_space].data_size = work_code->size;
	work->spaces[qfo_near_data_space].type = qfos_data;
	work->spaces[qfo_near_data_space].data = work_near_data->data;
	work->spaces[qfo_near_data_space].data_size = work_near_data->size;
	work->spaces[qfo_far_data_space].type = qfos_data;
	work->spaces[qfo_far_data_space].data = work_far_data->data;
	work->spaces[qfo_far_data_space].data_size = work_far_data->size;
	work->spaces[qfo_entity_space].type = qfos_entity;
	work->spaces[qfo_entity_space].data = work_entity_data->data;
	work->spaces[qfo_entity_space].data_size = work_entity_data->size;
	work->spaces[qfo_type_space].type = qfos_type;
	work->spaces[qfo_type_space].data = work_type_data->data;
	work->spaces[qfo_type_space].data_size = work_type_data->size;
	work->spaces[qfo_debug_space].type = qfos_debug;
	work->spaces[qfo_debug_space].data = work_type_data->data;
	work->spaces[qfo_debug_space].data_size = work_type_data->size;
	for (i = 0; i < qfo_num_spaces; i++)
		work->spaces[i].id = i;

	alloc_data (qfo_type_space, 4);

	work->lines = calloc (1, sizeof (pr_lineno_t));
	work->num_lines = 1;

	if (!options.partial_link) {
		for (i = 0; i < num_builtins; i++) {
			builtin_sym_t *bi = builtin_symbols + i;
			bi->defref = make_def (qfo_near_data_space, bi->name, bi->type,
								   bi->flags, 0);
		}
	}
}

typedef int (*space_func) (qfo_t *qfo, qfo_mspace_t *space, int pass);

static int
process_null_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (pass != 0)
		return 0;
	if (space->defs || space->num_defs || space->data || space->data_size
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
	if (pass == 0) {
		if (space->id == qfo_near_data_space) {
			align_data (qfo_near_data_space, space);
		} else if (space->id == qfo_far_data_space) {
			align_data (qfo_far_data_space, space);
		} else {
		}
	} else if (pass == 1) {
		if (space->id == qfo_near_data_space) {
			add_defs (qfo, space, work->spaces + qfo_near_data_space,
					  process_data_def);
			add_data (qfo_near_data_space, space);
		} else if (space->id == qfo_far_data_space) {
			add_defs (qfo, space, work->spaces + qfo_far_data_space,
					  process_data_def);
			add_data (qfo_far_data_space, space);
		} else {
			add_data_space (qfo, space);
		}
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
	if (pass == 0) {
		align_data (qfo_entity_space, space);
	} else if (pass == 1) {
		add_defs (qfo, space, work->spaces + qfo_entity_space, process_field_def);
		add_data (qfo_entity_space, space);
	}
	return 0;
}

// NOTE this will likely give weird results for qsort if the defs overlapp.
// However, that should not happen.
static int
type_def_compare (const void *a, const void *b)
{
	qfo_def_t  *ta = (qfo_def_t *) a;
	qfo_def_t  *tb = (qfo_def_t *) b;

	if (ta->offset < tb->offset)
		return -1;
	if (ta->offset >= tb->offset + QFOTYPE (tb->offset)->size)
		return 1;
	return 0;
}

static void
update_type_space_reloc (qfo_mspace_t *space, qfo_reloc_t *reloc)
{
	qfo_def_t    dummy;
	qfo_def_t   *def;

	if (reloc->type == -1) {
		// The reloc has been copied, and the record number of the new reloc
		// is in the old reloc's offset.
		reloc = work->relocs + reloc->offset;
	}
	dummy.offset = reloc->offset;
	def = (qfo_def_t *) bsearch (&dummy, space->defs, space->num_defs,
								 sizeof (qfo_def_t), type_def_compare);
	if (!def) {
		linker_internal_error ("relocation record with invalid address. "
							   "corrupt object file?");
	}
	// The new offset of the type encoding is stored in the def's type field.
	// The old offset is in the def's offset field. The reloc's offset points
	// to somewhere within the type encoding.
	reloc->offset = def->type + (reloc->offset - def->offset);
}

static int
process_type_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	unsigned    i;

	if (pass != 0)
		return 0;
	if (qfo_type_defs) {
		linker_error ("type space already defined");
		return 1;
	}
	qfo_type_defs = space;
	add_defs (qfo, space, work->spaces + qfo_type_space, process_type_def);
	// The defs in qfo are no longer needed by the rest of the linker, so
	// we're free to mess around with them.

	// Sort the defs by addres so they can found using bsearch when adjusting
	// the targets of type encoding relocs. Unfortunately, they will usually
	// be in order, so qsort will likely be pessimistic, but oh well.
	qsort (space->defs, space->num_defs, sizeof (qfo_def_t), type_def_compare);

	// Update the offsets of all relocation records that point into the type
	// encoding space.
	for (i = 0; i < qfo->num_relocs; i++) {
		qfo_reloc_t *reloc = qfo->relocs + i;

		if (reloc->space != space->id)
			continue;
		update_type_space_reloc (space, reloc);
		// while we're at it, relocate all references in the type encoding
		// space so the type encodings are always correct.
		if (reloc->type == rel_def_string) {
			pr_string_t str;
			str = linker_add_string (QFOSTR (qfo, reloc->target));
			QFO_STRING (work, reloc->space, reloc->offset) = str;
		} else if (reloc->type == rel_def_def || reloc->type == -1) {
			qfo_def_t  *def;
			if (reloc->type == -1) {
				// The reloc has been copied, and the record number of the new
				// reloc is in the old reloc's offset.
				reloc = work->relocs + reloc->offset;
			}
			if (reloc->target >= work->spaces[reloc->space].num_defs) {
				linker_error ("Invalid reloc target def %d / %d.\n",
							  reloc->target, qfo->num_defs);
				continue;
			}
			def = work->spaces[reloc->space].defs + reloc->target;
			QFO_INT (work, reloc->space, reloc->offset) = def->offset;
		}
	}
	for (i = 0; i < num_builtins; i++) {
		builtin_sym_t *bi = builtin_symbols + i;
		qfo_def_t  *def;
		defref_t   *ref;

		if (!bi->defref)
			continue;
		def = REF (bi->defref);
		if ((int) def->type >= 0)
			continue;
		ref = Hash_Find (defined_type_defs, WORKSTR (-def->type));
		if (!ref)
			continue;
		def->type = REF (ref)->offset;
	}
	return 0;
}

static int
process_debug_space (qfo_t *qfo, qfo_mspace_t *space, int pass)
{
	if (space->type != qfos_debug) {
		linker_internal_error ("bad space type for process_debug_space (): %d",
							   space->type);
	}
	if (pass == 0) {
		align_data (qfo_debug_space, space);
	} else if (pass == 1) {
		add_defs (qfo, space, work->spaces + qfo_debug_space, process_data_def);
		add_data (qfo_debug_space, space);
	}
	return 0;
}

static void
process_funcs (qfo_t *qfo)
{
	unsigned    size;
	qfo_func_t *func;
	qfot_type_t *type;

	size = work->num_funcs + qfo->num_funcs;
	work->funcs = realloc (work->funcs, size * sizeof (qfo_func_t));
	memcpy (work->funcs + work->num_funcs, qfo->funcs,
			qfo->num_funcs * sizeof (qfo_func_t));
	while (work->num_funcs < size) {
		func = work->funcs + work->num_funcs++;
		type = QFOTYPE(func->type);
		func->type = type->class;
		func->name = linker_add_string (QFOSTR (qfo, func->name));
		func->file = linker_add_string (QFOSTR (qfo, func->file));
		if (func->code > 0)
			func->code += work_base[qfo_code_space];
		func->def = qfo->defs[func->def].line;	// defref index
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
	unsigned    size;
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
		if (reloc->space >= qfo->num_spaces) {
			linker_error ("bad reloc space");
			reloc->type = rel_none;
			continue;
		}
		reloc->space = qfo->spaces[reloc->space].id;
		if (reloc->type == rel_def_string) {
			const char *str;

			if (reloc->target >= qfo->spaces[qfo_strings_space].data_size) {
				linker_error ("bad string reloc at %d:%x", reloc->space,
							  reloc->offset);
				reloc->target = 0;
			}
			str = QFOSTR (qfo, reloc->target);
			reloc->target = linker_add_string (str);
		}
		if (reloc->type == rel_def_op)
			reloc->target += work_base[qfo_code_space];
		adjust_reloc_offset (reloc);
		if (reloc->type == rel_def_string) {
			QFO_STRING (work, reloc->space, reloc->offset) = reloc->target;
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
		process_debug_space,
	};
	unsigned    i;
	int         pass;
	qfo_mspace_t *space;

	qfo_type_defs = 0;
	work_func_base = work->num_funcs;
	for (pass = 0; pass < 2; pass++) {
		for (i = 0, space = qfo->spaces; i < qfo->num_spaces; i++, space++) {
			if ((int) space->type < 0 || space->type > qfos_debug) {
				linker_error ("bad space type");
				return 1;
			}
			if (funcs[space->type] (qfo, space, pass))
				return 1;
		}
		for (i = 0; i < qfo_num_spaces; i++) {
			if (pass == 0) {
				work_base[i] = work->spaces[i].data_size;
			}
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
	int         i;
	unsigned    j;
	int         did_something;

	if (strncmp (libname, "-l", 2) == 0) {
		while (path) {
			path_name = va (0, "%s/lib%s.a", path->path, libname + 2);
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
			if (qfo->progs_version != options.code.progsversion) {
				linker_error ("qfo progs version does not match target");
				return 1;
			}

			for (j = 0; j < qfo->num_defs; j++) {
				qfo_def_t  *def = qfo->defs + j;
				if ((def->flags & QFOD_GLOBAL)
					&& !(def->flags & QFOD_EXTERNAL)
					&& Hash_Find (extern_data_defs, QFOSTR (qfo, def->name))) {
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
	pr_uint_t   i;
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
			qfo_func_t *best = 0;
			pr_uint_t    best_dist;
			pr_lineno_t *line;

			while (func - work->funcs < (ptrdiff_t) work->num_funcs) {
				if (func->code >= 0
					&& (pr_uint_t) func->code <= reloc->offset) {
					if (!best || reloc->offset - func->code < best_dist) {
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
				while (line - work->lines < (ptrdiff_t) work->num_lines - 1
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

	undef_defs = (defref_t **) Hash_GetList (extern_data_defs);
	for (defref = undef_defs; *defref; defref++) {
		qfo_def_t  *def = REF (*defref);
		const char *name = WORKSTR (def->name);

		if (strcmp (name, ".self") == 0 && !did_self) {
			defref_t   *_d = Hash_Find (defined_data_defs, "self");
			if (_d) {
				qfo_def_t  *d = REF (_d);
				if (QFO_TYPEMETA (work, d->type) == ty_basic
					&& QFO_TYPETYPE (work, d->type) == ev_entity)
					def_warning (d, "@self and self used together");
			}
			linker_add_def (".self", &type_entity, QFOD_GLOBAL, 0);
			did_self = 1;
		} else if (strcmp (name, ".this") == 0 && !did_this) {
			int         flags;
			defref_t   *this_ref;

			if (!obj_initialized)
				class_init ();
			flags = QFOD_GLOBAL | QFOD_NOSAVE;
			this_ref = make_def (qfo_entity_space, name, &type_id, flags, 0);
			flags |= QFOD_CONSTANT | QFOD_INITIALIZED;
			auto type = field_type (&type_id);
			linker_add_def (".this", type, flags, &REF (this_ref)->offset);
			did_this = 1;
		}
	}
	free (undef_defs);
	undef_defs = (defref_t **) Hash_GetList (extern_data_defs);
	for (defref = undef_defs; *defref; defref++) {
		qfo_def_t  *def = REF (*defref);
		undefined_def (def);
	}
	free (undef_defs);
}

static void
sort_defs (qfo_mspace_t *space)
{
	qfo_def_t  *defs = space->defs;
	for (pr_uint_t i = 1; i < space->num_defs; i++) {
		qfo_def_t   d = defs[i];
		for (pr_uint_t j = i; j-- > 0 && d.offset < defs[j].offset; ) {
			defs[j + 1] = defs[j];
			defs[j] = d;
		}
	}
}

static qfo_t *
build_qfo (void)
{
	qfo_t      *qfo;
	int         size;
	unsigned    i, j;
	qfo_reloc_t *reloc;
	qfo_def_t **defs;

	qfo = qfo_new ();
	qfo->spaces = calloc (work->num_spaces, sizeof (qfo_mspace_t));
	qfo->num_spaces = work->num_spaces;
	for (i = qfo_near_data_space; i <= qfo_entity_space; i++) {
		sort_defs (&qfo->spaces[i]);
	}
	for (i = 0; i < work->num_spaces; i++) {
		qfo->spaces[i].type = work->spaces[i].type;
		qfo->spaces[i].id = work->spaces[i].id;
		qfo->spaces[i].data = work->spaces[i].data;
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

	pr.loc = (rua_loc_t) {
		.line = def->line,
		.column = 1,
		.last_line = def->line,
		.last_column = 1,
		.file = def->file,
	};
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

	pr.loc = (rua_loc_t) {
		.line = def->line,
		.column = 1,
		.last_line = def->line,
		.last_column = 1,
		.file = def->file,
	};
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

static void
linker_internal_error (const char *fmt, ...)
{
	va_list     args;

	fprintf (stderr, "%s: ", linker_current_file->str);

	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);

	fputs ("\n", stderr);

	abort ();
}
