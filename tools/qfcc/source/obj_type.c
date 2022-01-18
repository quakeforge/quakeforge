/*
	obj_type.c

	object file type encoding support

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/02/18

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

#include "qfalloca.h"

#include "compat.h"

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/value.h"

#define ENC_DEF(dest,def) EMIT_DEF (pr.type_data, dest, def)
#define ENC_STR(dest,str)									\
	do {													\
		def_t       loc;									\
		loc.space = pr.type_data;							\
		loc.offset = POINTER_OFS (pr.type_data, &(dest));	\
		(dest) = encoding_string (str);						\
		reloc_def_string (&loc);							\
	} while (0)

typedef def_t *(*encode_f) (type_t *type, defspace_t *space);

static pr_string_t
encoding_string (const char *string)
{
	int         str;

	str = ReuseString (string);
	return str;
}

static def_t *
qfo_new_encoding (type_t *type, int size, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	size += field_offset (qfot_type_t, type);
	size /= sizeof (pr_type_t);

	def = new_def (type->encoding, 0, space, sc_static);
	def->offset = defspace_alloc_loc (pr.type_data, size);

	enc = D_POINTER (qfot_type_t, def);
	enc->meta = type->meta;
	enc->size = size;
	ENC_STR (enc->encoding, type->encoding);
	return def;
}

static def_t *
qfo_encode_func (type_t *type, defspace_t *space)
{
	int         param_count;
	int         size;
	qfot_type_t *enc;
	qfot_func_t *func;
	def_t      *return_type_def;
	def_t     **param_type_defs;
	def_t      *def;
	int         i;

	param_count = type->t.func.num_params;
	if (param_count < 0)
		param_count = ~param_count;
	param_type_defs = alloca (param_count * sizeof (def_t *));
	return_type_def = qfo_encode_type (type->t.func.type, space);
	for (i = 0; i < param_count; i++)
		param_type_defs[i] = qfo_encode_type (type->t.func.param_types[i],
											  space);
	size = field_offset (qfot_func_t, param_types[param_count]);

	def = qfo_new_encoding (type, size, space);
	enc = D_POINTER (qfot_type_t, def);
	func = &enc->func;
	func->type = ev_func;
	ENC_DEF (func->return_type, return_type_def);
	func->num_params = type->t.func.num_params;
	for (i = 0; i < param_count; i++)
		ENC_DEF (func->param_types[i], param_type_defs[i]);
	return def;
}

static def_t *
qfo_encode_fldptr (type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *type_def;

	type_def = qfo_encode_type (type->t.fldptr.type, space);
	def = qfo_new_encoding (type, sizeof (enc->fldptr), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->fldptr.type = type->type;
	ENC_DEF (enc->fldptr.aux_type, type_def);
	return def;
}

static def_t *
qfo_encode_basic (type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	if (type->type == ev_func)
		return qfo_encode_func (type, space);
	else if (type->type == ev_pointer || type->type == ev_field)
		return qfo_encode_fldptr (type, space);

	def = qfo_new_encoding (type, sizeof (enc->type), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->type = type->type;
	return def;
}

static def_t *
qfo_encode_struct (type_t *type, defspace_t *space)
{
	sy_type_e   sy;
	int         num_fields;
	symbol_t   *sym;
	qfot_type_t *enc;
	qfot_struct_t *strct;
	def_t      *def;
	def_t     **field_types;
	int         i;
	int         size;
	int         offset;

	sy = sy_var;
	if (type->meta == ty_enum)
		sy = sy_const;
	if (!type->t.symtab) {
		def = new_def (type->encoding, 0, pr.type_data, sc_extern);
		return def;
	}
	for (num_fields = 0, sym = type->t.symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		num_fields++;
	}

	size = field_offset (qfot_struct_t, fields[num_fields]);
	def = qfo_new_encoding (type, size, space);
	enc = D_POINTER (qfot_type_t, def);
	strct = &enc->strct;
	ENC_STR (strct->tag, type->name);
	strct->num_fields = num_fields;

	type->type_def = def;	// avoid infinite recursion

	field_types = alloca (num_fields * sizeof (def_t *));
	for (i = 0, sym = type->t.symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		if (i == num_fields)
			internal_error (0, "whoa, what happened?");
		if (type->meta != ty_enum) {
			field_types[i] = qfo_encode_type (sym->type, space);
		} else {
			field_types[i] = type_default->type_def;
		}
		i++;
	}

	for (i = 0, sym = type->t.symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		if (i == num_fields)
			internal_error (0, "whoa, what happened?");
		if (sym->sy_type == sy_const)
			offset = sym->s.value->v.integer_val;
		else
			offset = sym->s.offset;
		ENC_DEF (strct->fields[i].type, field_types[i]);
		ENC_STR (strct->fields[i].name, sym->name);
		strct->fields[i].offset = offset;
		i++;
	}
	return def;
}

static def_t *
qfo_encode_array (type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *array_type_def;

	array_type_def = qfo_encode_type (type->t.array.type, space);

	def = qfo_new_encoding (type, sizeof (enc->array), space);
	enc = D_POINTER (qfot_type_t, def);
	ENC_DEF (enc->array.type, array_type_def);
	enc->array.base = type->t.array.base;
	enc->array.size = type->t.array.size;
	return def;
}

static def_t *
qfo_encode_class (type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	def = qfo_new_encoding (type, sizeof (enc->class), space);
	enc = D_POINTER (qfot_type_t, def);
	ENC_STR (enc->class, type->t.class->name);
	return def;
}

static def_t *
qfo_encode_alias (type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *type_def;
	def_t      *full_def;

	type_def = qfo_encode_type (type->t.alias.aux_type, space);
	full_def = qfo_encode_type (type->t.alias.full_type, space);

	def = qfo_new_encoding (type, sizeof (enc->alias), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->alias.type = type->type;
	ENC_DEF (enc->alias.aux_type, type_def);
	ENC_DEF (enc->alias.full_type, full_def);
	if (type->name) {
		ENC_STR (enc->alias.name, type->name);
	}
	return def;
}

def_t *
qfo_encode_type (type_t *type, defspace_t *space)
{
	reloc_t    *relocs = 0;

	static encode_f funcs[] = {
		qfo_encode_basic,	// ty_basic
		qfo_encode_struct,	// ty_struct
		qfo_encode_struct,	// ty_union
		qfo_encode_struct,	// ty_enum
		qfo_encode_array,	// ty_array
		qfo_encode_class,	// ty_class
		qfo_encode_alias,	// ty_alias
	};

	if (type->type_def && type->type_def->external) {
		relocs = type->type_def->relocs;
		free_def (type->type_def);
		type->type_def = 0;
	}
	if (type->type_def)
		return type->type_def;
	if (type->meta >= sizeof (funcs) / (sizeof (funcs[0])))
		internal_error (0, "bad type meta type");
	if (!type->encoding)
		type->encoding = type_get_encoding (type);
	type->type_def = funcs[type->meta] (type, space);
	reloc_attach_relocs (relocs, &type->type_def->relocs);
	return type->type_def;
}
