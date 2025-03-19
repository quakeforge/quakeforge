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

#include "tools/qfcc/include/algebra.h"
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

typedef def_t *(*encode_f) (const type_t *type, defspace_t *space);

static pr_string_t
encoding_string (const char *string)
{
	int         str;

	str = ReuseString (string);
	return str;
}

static def_t *
qfo_new_encoding (const type_t *type, int size, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	size += offsetof (qfot_type_t, type);
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
qfo_encode_func (const type_t *type, defspace_t *space)
{
	int         param_count;
	int         size;
	qfot_type_t *enc;
	qfot_func_t *func;
	def_t      *return_type_def;
	def_t     **param_type_defs;
	def_t      *def;
	int         i;

	param_count = type->func.num_params;
	if (param_count < 0)
		param_count = ~param_count;
	param_type_defs = alloca (param_count * sizeof (def_t *));
	return_type_def = qfo_encode_type (type->func.ret_type, space);
	for (i = 0; i < param_count; i++)
		param_type_defs[i] = qfo_encode_type (type->func.param_types[i], space);
	size = offsetof (qfot_func_t, param_types[param_count]);

	def = qfo_new_encoding (type, size, space);
	enc = D_POINTER (qfot_type_t, def);
	func = &enc->func;
	func->type = ev_func;
	ENC_DEF (func->return_type, return_type_def);
	func->num_params = type->func.num_params;
	for (i = 0; i < param_count; i++)
		ENC_DEF (func->param_types[i], param_type_defs[i]);
	return def;
}

static def_t *
qfo_encode_fldptr (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *type_def;

	type_def = qfo_encode_type (type->fldptr.type, space);
	def = qfo_new_encoding (type, sizeof (enc->fldptr), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->fldptr.type = type->type;
	ENC_DEF (enc->fldptr.aux_type, type_def);
	return def;
}

static def_t *
qfo_encode_basic (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	if (type->type == ev_func)
		return qfo_encode_func (type, space);
	else if (type->type == ev_ptr || type->type == ev_field)
		return qfo_encode_fldptr (type, space);

	def = qfo_new_encoding (type, sizeof (enc->basic), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->basic.type = type->type;
	enc->basic.width = type->width;
	enc->basic.columns = type->columns;
	return def;
}

static def_t *
qfo_encode_struct (const type_t *type, defspace_t *space)
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

	sy = sy_offset;
	if (type->meta == ty_enum)
		sy = sy_const;
	if (!type->symtab) {
		def = new_def (type->encoding, 0, pr.type_data, sc_extern);
		return def;
	}
	for (num_fields = 0, sym = type->symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		num_fields++;
	}

	size = offsetof (qfot_struct_t, fields[num_fields]);
	def = qfo_new_encoding (type, size, space);
	enc = D_POINTER (qfot_type_t, def);
	strct = &enc->strct;
	ENC_STR (strct->tag, type->name);
	strct->num_fields = num_fields;

	type_encodings.a[type->id] = def;	// avoid infinite recursion

	field_types = alloca (num_fields * sizeof (def_t *));
	for (i = 0, sym = type->symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		if (i == num_fields)
			internal_error (0, "whoa, what happened?");
		if (type->meta != ty_enum) {
			field_types[i] = qfo_encode_type (sym->type, space);
		} else {
			field_types[i] = type_encodings.a[type_default->id];
		}
		i++;
	}

	for (i = 0, sym = type->symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy)
			continue;
		if (i == num_fields)
			internal_error (0, "whoa, what happened?");
		if (sym->sy_type == sy_const)
			offset = sym->value->int_val;
		else
			offset = sym->offset;
		ENC_DEF (strct->fields[i].type, field_types[i]);
		ENC_STR (strct->fields[i].name, sym->name);
		strct->fields[i].offset = offset;
		i++;
	}
	return def;
}

static def_t *
qfo_encode_array (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *array_type_def;

	array_type_def = qfo_encode_type (type->array.type, space);

	def = qfo_new_encoding (type, sizeof (enc->array), space);
	enc = D_POINTER (qfot_type_t, def);
	ENC_DEF (enc->array.type, array_type_def);
	enc->array.base = type->array.base;
	enc->array.count = type->array.count;
	return def;
}

static def_t *
qfo_encode_class (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	def = qfo_new_encoding (type, sizeof (enc->class), space);
	enc = D_POINTER (qfot_type_t, def);
	ENC_STR (enc->class, type->class->name);
	return def;
}

static def_t *
qfo_encode_alias (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *type_def;
	def_t      *full_def;

	type_def = qfo_encode_type (type->alias.aux_type, space);
	full_def = qfo_encode_type (type->alias.full_type, space);

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

static def_t *
qfo_encode_handle (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;

	def = qfo_new_encoding (type, sizeof (enc->handle), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->handle.type = type->type;
	ENC_STR (enc->handle.tag, type->name);
	return def;
}

static def_t *
qfo_encode_algebra (const type_t *type, defspace_t *space)
{
	qfot_type_t *enc;
	def_t      *def;
	def_t      *algebra_type_def = 0;

	if (type->type != ev_invalid) {
		auto m = (multivector_t *) type->algebra;
		algebra_type_def = qfo_encode_type (m->algebra->type, space);
	}

	def = qfo_new_encoding (type, sizeof (enc->algebra), space);
	enc = D_POINTER (qfot_type_t, def);
	enc->algebra = (qfot_algebra_t) {
		.type = type->type,
		.width = type->width,
	};
	if (algebra_type_def) {
		ENC_DEF (enc->algebra.algebra, algebra_type_def);
	}
	return def;
}

def_t *
qfo_encode_type (const type_t *type, defspace_t *space)
{
	reloc_t    *relocs = 0;

	static encode_f funcs[] = {
		[ty_basic]   = qfo_encode_basic,
		[ty_struct]  = qfo_encode_struct,
		[ty_union]   = qfo_encode_struct,
		[ty_enum]    = qfo_encode_struct,
		[ty_array]   = qfo_encode_array,
		[ty_class]   = qfo_encode_class,
		[ty_alias]   = qfo_encode_alias,
		[ty_handle]  = qfo_encode_handle,
		[ty_algebra] = qfo_encode_algebra,
		[ty_bool]    = qfo_encode_basic,
	};

	auto type_def = &type_encodings.a[type->id];
	if (*type_def && (*type_def)->external) {
		relocs = type_encodings.a[type->id]->relocs;
		type_encodings.a[type->id] = nullptr;
	}
	if (*type_def)
		return *type_def;
	if (type->meta >= sizeof (funcs) / (sizeof (funcs[0])))
		internal_error (0, "bad type meta type");
	*type_def = funcs[type->meta] (type, space);
	reloc_attach_relocs (relocs, &(*type_def)->relocs);
	return *type_def;
}
