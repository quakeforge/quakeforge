/*
	dump_globals.c

	Dump progs globals information.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/16

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/va.h"

#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/qfprogs.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/strpool.h"

static int
cmp (const void *_a, const void *_b)
{
	const pr_def_t *a = (const pr_def_t *)_a;
	const pr_def_t *b = (const pr_def_t *)_b;

	return a->ofs - b->ofs;
}

static void
dump_def (progs_t *pr, pr_def_t *def, int indent)
{
	const char *name;
	const char *type;
	pr_uint_t   offset;
	const char *comment;
	int         saveglobal;

	if (!def->type && !def->ofs && !def->name)
		return;

	name = PR_GetString (pr, def->name);
	type = pr_type_name[def->type & ~DEF_SAVEGLOBAL];
	saveglobal = (def->type & DEF_SAVEGLOBAL) != 0;
	offset = def->ofs;

	comment = " invalid offset";

	if (offset < pr->progs->globals.count) {
		static dstring_t *value_dstr;
		if (!value_dstr) {
			value_dstr = dstring_newstr ();
		}
		dstring_clearstr (value_dstr);
		qfot_type_t *ty = &G_STRUCT (pr, qfot_type_t, def->type_encoding);
		comment = PR_Debug_ValueString (pr, offset, ty, value_dstr);
	}
	printf ("%*s %x:%d %d %s %s:%x %s\n", indent * 12, "",
			offset, def->size, saveglobal, name, type, def->type_encoding,
			comment);
}

void
dump_globals (progs_t *pr)
{
	unsigned int i;
	pr_def_t   *global_defs = pr->pr_globaldefs;

	if (sorted) {
		global_defs = malloc (pr->progs->globaldefs.count * sizeof (ddef_t));
		memcpy (global_defs, pr->pr_globaldefs,
				pr->progs->globaldefs.count * sizeof (ddef_t));
		qsort (global_defs, pr->progs->globaldefs.count, sizeof (ddef_t), cmp);
	}
	for (i = 0; i < pr->progs->globaldefs.count; i++) {
		pr_def_t   *def = &global_defs[i];
		dump_def (pr, def, 0);
	}
}

void
dump_fields (progs_t *pr)
{
	unsigned int i;
	const char *name;
	const char *type;
	int         offset;
	const char *comment;

	for (i = 0; i < pr->progs->fielddefs.count; i++) {
		pr_def_t   *def = &pr->pr_fielddefs[i];

		name = PR_GetString (pr, def->name);
		type = pr_type_name[def->type & ~DEF_SAVEGLOBAL];
		offset = def->ofs;

		comment = "";

		printf ("%d %s %s%s\n", offset, name, type, comment);
	}
}

void
qfo_fields (qfo_t *qfo)
{
	unsigned int i;
	const char *name;
	const char *typestr;
	qfot_type_t *type;
	int         offset;
	const char *comment;
	qfo_mspace_t *space = &qfo->spaces[qfo_entity_space];

	if (qfo_entity_space >= qfo->num_spaces) {
		printf ("no entity space\n");
		return;
	}
	if (!space->num_defs) {
		printf ("no fields\n");
		return;
	}

	for (i = 0; i < space->num_defs; i++) {
		qfo_def_t  *def = space->defs + i;

		name = QFO_GETSTR (qfo, def->name);
		//FIXME check type
		type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, def->type);
		typestr = QFO_GETSTR (qfo, type->encoding);
		offset = def->offset;

		comment = "";

		printf ("%d %s %s%s\n", offset, name, typestr, comment);
	}
}

void
dump_functions (progs_t *pr)
{
	pr_uint_t   i, j, count;
	const char *name;
	int         start;
	const char *comment;
	pr_def_t   *encodings_def;
	pr_ptr_t    type_encodings = 0;

	encodings_def = PR_FindGlobal (pr, ".type_encodings");
	if (encodings_def) {
		type_encodings = encodings_def->ofs;
	}

	for (i = 0; i < pr->progs->functions.count; i++) {
		dfunction_t *func = &pr->pr_functions[i];

		name = PR_GetString (pr, func->name);

		start = func->first_statement;
		if (start > 0)
			comment = va (0, " @ %x", start);
		else
			comment = va (0, " = #%d", -start);

		printf ("%-5d %s%s: %d (", i, name, comment, func->numparams);
		if (func->numparams < 0)
			count = -func->numparams - 1;
		else
			count = func->numparams;
		for (j = 0; j < count; j++)
			printf (" %d:%d", func->param_size[j].alignment,
					func->param_size[j].size);
		printf (") %d @ %x", func->locals, func->params_start);
		puts ("");
		if (type_encodings) {
			pr_auxfunction_t *aux = PR_Debug_MappedAuxFunction (pr, i);
			if (!aux) {
				continue;
			}
			printf ("        %d %s:%d %d %d %d %x\n", aux->function,
					PR_GetString (pr, func->file), aux->source_line,
					aux->line_info,
					aux->local_defs, aux->num_locals,
					aux->return_type);
			pr_def_t   *local_defs = PR_Debug_LocalDefs (pr, aux);
			if (!local_defs) {
				continue;
			}
			for (j = 0; j < aux->num_locals; j++) {
				dump_def (pr, local_defs + j, 1);
			}
		}
	}
}

static const char *
flags_string (pr_uint_t flags)
{
	static dstring_t *str;
	if (!str)
		str = dstring_newstr ();
	dstring_clearstr (str);
	dstring_appendstr (str, (flags & QFOD_INITIALIZED) ? "I" : "-");
	dstring_appendstr (str, (flags & QFOD_CONSTANT)    ? "C" : "-");
	dstring_appendstr (str, (flags & QFOD_ABSOLUTE)    ? "A" : "-");
	dstring_appendstr (str, (flags & QFOD_GLOBAL)      ? "G" : "-");
	dstring_appendstr (str, (flags & QFOD_EXTERNAL)    ? "E" : "-");
	dstring_appendstr (str, (flags & QFOD_LOCAL)       ? "L" : "-");
	dstring_appendstr (str, (flags & QFOD_SYSTEM)      ? "S" : "-");
	dstring_appendstr (str, (flags & QFOD_NOSAVE)      ? "N" : "-");
	dstring_appendstr (str, (flags & QFOD_PARAM)       ? "P" : "-");
	return str->str;
}

void
qfo_globals (qfo_t *qfo)
{
	qfo_def_t  *def;
	unsigned    i;
	unsigned    space;
	int         count = 0;

	for (space = 0; space < qfo->num_spaces; space++) {
		for (i = 0; i < qfo->spaces[space].num_defs; i++, count++) {
			def = &qfo->spaces[space].defs[i];
			printf ("%-5d %2d:%-5x %s %s %x %s", count, space, def->offset,
					flags_string (def->flags),
					QFO_GETSTR (qfo, def->name),
					def->type,
					QFO_TYPESTR (qfo, def->type));
			if (!(def->flags & QFOD_EXTERNAL) && qfo->spaces[space].data)
				printf (" %d",
						qfo->spaces[space].data[def->offset].value);
			puts ("");
		}
	}
}

void
qfo_relocs (qfo_t *qfo)
{
	qfo_reloc_t  *reloc;
	qfo_def_t    *def;
	qfo_func_t   *func;
	int           opind;
	dstatement_t *statement;
	unsigned      i;

	for (i = 0; i < qfo->num_relocs; i++) {
		if (i == qfo->num_relocs - qfo->num_loose_relocs) {
			printf ("---- unbound relocs ----\n");
		}
		reloc = qfo->relocs + i;
		if ((unsigned) reloc->type > rel_def_field_ofs) {
			printf ("%d unknown reloc: %d\n", i, reloc->type);
			continue;
		}
		printf ("%s", reloc_names[reloc->type]);
		def = 0;
		func = 0;
		switch ((reloc_type) reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
			case rel_op_b_def:
			case rel_op_c_def:
				def = qfo->defs + reloc->target;
				printf (" op.%c@%x def#%d %s",
						reloc->type - rel_op_a_def + 'a',
						reloc->offset, reloc->target,
						QFO_GETSTR (qfo, def->name));
				break;
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
				printf (" op.%c op@%x", reloc->type - rel_op_a_def + 'a',
						reloc->offset);
				break;
			case rel_def_op:
				printf (" def@%d:%x op@%d", reloc->space, reloc->offset,
						reloc->target);
				break;
			case rel_def_def:
				def = qfo->defs + reloc->target;
				printf (" def@%d:%x def#%d %s", reloc->space, reloc->offset,
						reloc->target, QFO_GETSTR (qfo, def->name));
				break;
			case rel_def_func:
				func = qfo->funcs + reloc->target;
				//func = qfo->funcs + QFO_FUNCTION (qfo, reloc->space,
				//								  reloc->offset);
				printf (" def@%d:%x func#%d %s %x",
						reloc->space, reloc->offset,
						QFO_FUNCTION (qfo, reloc->space, reloc->offset),
						QFO_GETSTR (qfo, func->name),
						reloc->target);
				break;
			case rel_def_string:
				printf (" def@%d:%x string:%x `%s'",
						reloc->space, reloc->offset, reloc->target,
						quote_string (QFO_GETSTR (qfo, reloc->target)));
				break;
			case rel_def_field:
				def = qfo->defs + reloc->target;
				printf (" def@%d:%x def#%d %s", reloc->space, reloc->offset,
						reloc->target, QFO_GETSTR (qfo, def->name));
				break;
			case rel_op_a_def_ofs:
			case rel_op_b_def_ofs:
			case rel_op_c_def_ofs:
				def = qfo->defs + reloc->target;
				opind = reloc->type - rel_op_a_def_ofs;
				statement = QFO_STATEMENT (qfo, reloc->offset);
				printf (" op.%c@%x def#%d %s+%d",
						opind + 'a',
						reloc->offset, reloc->target,
						QFO_GETSTR (qfo, def->name),
						((pr_ushort_t *)statement)[opind + 1]);
				break;
			case rel_def_def_ofs:
				def = qfo->defs + reloc->target;
				printf (" def@%d:%x def#%d+%d %s+%d",
						reloc->space, reloc->offset, reloc->target,
						QFO_INT (qfo, reloc->space, reloc->offset),
						QFO_GETSTR (qfo, def->name),
						QFO_INT (qfo, reloc->space, reloc->offset));
				break;
			case rel_def_field_ofs:
				def = qfo->defs + reloc->target;
				printf (" def@%d:%x def#%d+%d %s(%d)+%d",
						reloc->space, reloc->offset, reloc->target,
						QFO_INT (qfo, reloc->space, reloc->offset),
						QFO_GETSTR (qfo, def->name), def->offset,
						QFO_INT (qfo, reloc->space, reloc->offset));
				break;
		}
		if (def && def->flags & QFOD_EXTERNAL)
			printf (" external");
		if (func && qfo->defs[func->def].flags & QFOD_EXTERNAL)
			printf (" external");
		if (def && (i < def->relocs || i >= def->relocs + def->num_relocs))
			printf (" BOGUS def reloc! %d %d %d",
					i, def->relocs, def->num_relocs);
		if (func && (i < func->relocs || i >= func->relocs + func->num_relocs))
			printf (" BOGUS func reloc! %d %d %d",
					i, func->relocs, func->num_relocs);
		puts ("");
	}
}

void
qfo_functions (qfo_t *qfo)
{
	qfo_def_t  *def;
	qfo_func_t *func;
	unsigned    i, j, d;
	unsigned    space;
	qfo_mspace_t *locals;

	for (i = 0; i < qfo->num_funcs; i++) {
		func = &qfo->funcs[i];
		def = &qfo->defs[func->def];
		for (space = 0; space < qfo->num_spaces; space++) {
			if (!qfo->spaces[space].num_defs)
				continue;
			d = qfo->spaces[space].defs - qfo->defs;
			if (func->def >= d && func->def < d + qfo->spaces[space].num_defs)
				break;
		}
		if (space == qfo->num_spaces)
			space = qfo_near_data_space;
		printf ("%-5d %-5d %s %s %d %s", i, def->offset,
				flags_string (def->flags),
				QFO_GETSTR (qfo, func->name), func->def,
				QFO_GETSTR (qfo, def->name));
		if (!(def->flags & QFOD_EXTERNAL))
			printf (" %d", QFO_FUNCTION (qfo, space, def->offset));
		if (func->code > 0)
			printf (" @ %x", func->code);
		else
			printf (" = #%d", -func->code);
		printf (" loc: %d params: %d\n",
				func->locals_space, func->params_start);
		if (func->locals_space) {
			locals = &qfo->spaces[func->locals_space];
			printf ("%*s%d %p %d %p %d %d\n", 16, "", locals->type,
					locals->defs, locals->num_defs,
					locals->data, locals->data_size, locals->id);
			for (j = 0; j < locals->num_defs; j++) {
				qfo_def_t  *def = locals->defs + j;
				int         offset;
				const char *typestr;
				const char *name;
				qfot_type_t *type;

				name = QFO_GETSTR (qfo, def->name);
				//FIXME check type
				type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t,
									def->type);
				typestr = QFO_GETSTR (qfo, type->encoding);
				offset = def->offset;

				printf ("%*s%d %s %s\n", 20, "", offset, name, typestr);
			}
		}
	}
}

static const char *ty_meta_names[] = {
	"ty_basic",
	"ty_struct",
	"ty_union",
	"ty_enum",
	"ty_array",
	"ty_class",
	"ty_alias",
	"ty_handle",
	"ty_algebra",
	"ty_bool",
};
#define NUM_META ((int)(sizeof (ty_meta_names) / sizeof (ty_meta_names[0])))
const int vector_types =  (1 << ev_float)
						| (1 << ev_int)
						| (1 << ev_uint)
						| (1 << ev_double)
						| (1 << ev_long)
						| (1 << ev_ulong);

static const char *
get_ev_type_name (etype_t type)
{
	return ((unsigned) type >= ev_type_count)
		  ? "invalid type"
		  : pr_type_name[type];
}

static void
dump_qfo_types (qfo_t *qfo, int base_address)
{
	pr_ptr_t    type_ptr;
	qfot_type_t *type;
	const char *meta;
	int         i, count;

	for (type_ptr = 4; type_ptr < qfo->spaces[qfo_type_space].data_size;
		 type_ptr += type->size) {
		type = &QFO_STRUCT (qfo, qfo_type_space, qfot_type_t, type_ptr);
		if (qfo->spaces[qfo_type_space].data_size - type_ptr < 2) {
			printf ("%-5x overflow, can't check size. %x\n",
					type_ptr + base_address,
					qfo->spaces[qfo_type_space].data_size);
		}
		if (type_ptr + type->size > qfo->spaces[qfo_type_space].data_size) {
			printf ("%-5x overflow by %d words. %x\n", type_ptr + base_address,
					(type_ptr + type->size
					 - qfo->spaces[qfo_type_space].data_size),
					qfo->spaces[qfo_type_space].data_size);
			continue;
		}
		if ((unsigned) type->meta >= NUM_META)
			meta = va (0, "invalid meta: %d", type->meta);
		else
			meta = ty_meta_names[type->meta];
		printf ("%-5x %-9s %-20s", type_ptr + base_address, meta,
				QFO_TYPESTR (qfo, type_ptr));
		if ((unsigned) type->meta >= NUM_META) {
			puts ("");
			break;
		}
		switch ((ty_meta_e) type->meta) {
			case ty_bool:
			case ty_basic:
				printf (" %-10s", get_ev_type_name (type->type));
				if (type->type == ev_func) {
					printf (" %4x %d", type->func.return_type,
							count = type->func.num_params);
					if (count < 0)
						count = ~count;	//ones complement
					for (i = 0; i < count; i++)
						printf (" %x", type->func.param_types[i]);
				} else if (type->type == ev_ptr
						   || type->type == ev_field) {
					printf (" %4x", type->fldptr.aux_type);
				} else if ((1 << type->type) & vector_types) {
					if (type->basic.columns > 1) {
						printf ("[%d]", type->basic.columns);
					}
					if (type->basic.width > 1) {
						printf ("[%d]", type->basic.width);
					}
				}
				printf ("\n");
				break;
			case ty_struct:
			case ty_union:
			case ty_enum:
				printf (" %s\n", QFO_GETSTR (qfo, type->strct.tag));
				for (i = 0; i < type->strct.num_fields; i++) {
					printf ("        %-5x %4x %s\n",
							type->strct.fields[i].type,
							type->strct.fields[i].offset,
							QFO_GETSTR (qfo, type->strct.fields[i].name));
				}
				break;
			case ty_array:
				printf (" %-5x %d %d\n", type->array.type,
						type->array.base, type->array.size);
				break;
			case ty_class:
				printf (" %-5x\n", type->class);
				break;
			case ty_alias:
				printf (" %s %d %5x %5x\n",
						QFO_GETSTR (qfo, type->alias.name),
						type->alias.type, type->alias.aux_type,
						type->alias.full_type);
				break;
			case ty_handle:
				printf (" %-5x\n", type->handle.tag);
				break;
			case ty_algebra:
				printf (" %s[%d] %5x %d\n",
						get_ev_type_name (type->type), type->algebra.width,
						type->algebra.algebra, type->algebra.element);
			case ty_meta_count:
				break;
		}
	}
}

void
qfo_types (qfo_t *qfo)
{
	dump_qfo_types (qfo, 0);
}

void
dump_types (progs_t *pr)
{
	qfo_mspace_t spaces[qfo_num_spaces];
	qfo_t       qfo;
	pr_def_t   *encodings_def;
	qfot_type_encodings_t *encodings;

	encodings_def = PR_FindGlobal (pr, ".type_encodings");
	if (!encodings_def) {
		printf ("no type encodings found");
		return;
	}
	if (encodings_def->ofs > pr->globals_size - 2) {
		printf ("invalid .type_encodings address");
		return;
	}
	encodings = &G_STRUCT (pr, qfot_type_encodings_t, encodings_def->ofs);
	if (encodings->types > pr->globals_size) {
		printf ("invalid encodings block start");
		return;
	}
	if (encodings->types + encodings->size > pr->globals_size) {
		printf ("truncating encodings block size");
		encodings->size = pr->globals_size - encodings->types;
	}
	memset (spaces, 0, sizeof (spaces));
	spaces[qfo_strings_space].type = qfos_string;
	spaces[qfo_strings_space].strings = pr->pr_strings;
	spaces[qfo_strings_space].data_size = pr->pr_stringsize;
	spaces[qfo_type_space].type = qfos_type;
	spaces[qfo_type_space].data = pr->pr_globals + encodings->types;
	spaces[qfo_type_space].data_size = encodings->size;
	memset (&qfo, 0, sizeof (qfo));
	qfo.spaces = spaces;
	qfo.num_spaces = qfo_num_spaces;
	dump_qfo_types (&qfo, encodings->types);
}
