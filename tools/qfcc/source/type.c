/*
	type.c

	type system

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/11

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "class.h"
#include "def.h"
#include "diagnostic.h"
#include "expr.h"
#include "function.h"
#include "options.h"
#include "qfcc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"

// simple types.  function types are dynamically allocated
type_t      type_invalid = { ev_invalid, "invalid" };
type_t      type_void = { ev_void, "void" };
type_t      type_string = { ev_string, "string" };
type_t      type_float = { ev_float, "float" };
type_t      type_vector = { ev_vector, "vector" };
type_t      type_entity = { ev_entity, "entity" };
type_t      type_field = {ev_field, "field", ty_none, {{&type_void}} };

// type_function is a void() function used for state defs
type_t      type_function = { ev_func, "function", ty_none, {{&type_void}} };
type_t      type_pointer = { ev_pointer, "pointer", ty_none, {{&type_void}} };
type_t      type_quaternion = { ev_quat, "quaternion" };
type_t      type_integer = { ev_integer, "integer" };
type_t      type_short = { ev_short, "short" };

type_t     *type_nil;
type_t     *type_default;

// these will be built up further
type_t      type_id = { ev_pointer, "id" };
type_t      type_Class = { ev_pointer, "Class" };
type_t      type_Protocol = { ev_invalid, "Protocol" };
type_t      type_SEL = { ev_pointer, "SEL" };
type_t      type_IMP = { ev_func, "IMP", ty_none,
						 {{&type_id, -3, {&type_id, &type_SEL}}}};
type_t      type_supermsg = { ev_func, ".supermsg", ty_none,
							  {{&type_id, -3, {0, &type_SEL}}}};
type_t      type_obj_exec_class = { ev_func, "function", ty_none,
									{{&type_void, 1, { 0 }}}};
type_t      type_Method = { ev_invalid, "Method" };
type_t      type_Super = { ev_invalid, "Super" };
type_t      type_method_description = { ev_invalid, "obj_method_description",
										ty_struct };
type_t      type_category;
type_t      type_ivar;
type_t      type_module;
type_t      type_va_list = { ev_invalid, "@va_list", ty_struct };
type_t      type_param;
type_t      type_zero;

type_t      type_floatfield = { ev_field, ".float", ty_none, {{&type_float}} };

static type_t *free_types;

static inline void
chain_type (type_t *type)
{
	type->next = pr.types;
	pr.types = type;
}

type_t *
new_type (void)
{
	type_t     *type;
	ALLOC (1024, type_t, types, type);
	return type;
}

type_t *
append_type (type_t *type, type_t *new)
{
	type_t    **t = &type;

	while (*t) {
		switch ((*t)->type) {
			case ev_void:
			case ev_string:
			case ev_float:
			case ev_vector:
			case ev_entity:
			case ev_type_count:
			case ev_quat:
			case ev_integer:
			case ev_short:
				internal_error (0, "append to basic type");
			case ev_field:
			case ev_pointer:
				t = &(*t)->t.fldptr.type;
				break;
			case ev_func:
				t = &(*t)->t.func.type;
				break;
			case ev_invalid:
				if ((*t)->ty == ty_array)
					t = &(*t)->t.array.type;
				else
					internal_error (0, "append to object type");
				break;
		}
	}
	*t = new;
	return type;
}

static int
types_same (type_t *a, type_t *b)
{
	int         i, count;

	if (a->type != b->type || a->ty != b->ty)
		return 0;
	switch (a->ty) {
		case ty_none:
			switch (a->type) {
				case ev_field:
				case ev_pointer:
					if (a->t.fldptr.type != b->t.fldptr.type)
						return 0;
					return 1;
				case ev_func:
					if (a->t.func.type != b->t.func.type
						|| a->t.func.num_params != b->t.func.num_params)
						return 0;
					count = a->t.func.num_params;
					if (count < 0)
						count = ~count;	// param count in one's complement
					for (i = 0; i < count; i++)
						if (a->t.func.param_types[i]
							!= b->t.func.param_types[i])
							return 0;
					return 1;
				default:		// other types don't have aux data
					return 1;
			}
			break;
		case ty_struct:
		case ty_union:
		case ty_enum:
			if (a->t.symtab != b->t.symtab)
				return 0;
			return 1;
		case ty_array:
			if (a->t.array.type != b->t.array.type
				|| a->t.array.base != b->t.array.base
				|| a->t.array.size != b->t.array.size)
				return 0;
			return 1;
		case ty_class:
			if (a->t.class != b->t.class)
				return 0;
			return 1;
	}
	internal_error (0, "we be broke");
}

/*
	find_type

	Returns a preexisting complex type that matches the parm, or allocates
	a new one and copies it out.
*/
type_t *
find_type (type_t *type)
{
	type_t     *check;

	for (check = pr.types; check; check = check->next) {
		if (types_same (check, type))
			return check;
	}

	// allocate a new one
	check = new_type ();
	*check = *type;

	chain_type (check);

	return check;
}

type_t *
field_type (type_t *aux)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_field;
	new.t.fldptr.type = aux;
	return find_type (&new);
}

type_t *
pointer_type (type_t *aux)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_pointer;
	new.t.fldptr.type = aux;
	return find_type (&new);
}

type_t *
array_type (type_t *aux, int size)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_invalid;
	new.ty = ty_array;
	new.t.array.type = aux;
	new.t.array.size = size;
	return find_type (&new);
}

type_t *
based_array_type (type_t *aux, int base, int top)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_invalid;
	new.ty = ty_array;
	new.t.array.type = aux;
	new.t.array.base = base;
	new.t.array.size = top - base + 1;
	return find_type (&new);
}

void
print_type_str (dstring_t *str, type_t *type)
{
	if (!type) {
		dasprintf (str, " (null)");
		return;
	}
	switch (type->type) {
		case ev_field:
			dasprintf (str, ".(");
			print_type_str (str, type->t.fldptr.type);
			dasprintf (str, ")");
			break;
		case ev_func:
			print_type_str (str, type->t.func.type);
			if (type->t.func.num_params == -1) {
				dasprintf (str, "(...)");
			} else {
				int         c, i;
				dasprintf (str, "(");
				if ((c = type->t.func.num_params) < 0)
					c = ~c;		// num_params is one's compliment
				for (i = 0; i < c; i++) {
					if (i)
						dasprintf (str, ", ");
					print_type_str (str, type->t.func.param_types[i]);
				}
				if (type->t.func.num_params < 0)
						dasprintf (str, ", ...");
				dasprintf (str, ")");
			}
			break;
		case ev_pointer:
			if (type == &type_id) {
				dasprintf (str, "id");
				break;
			}
			if (type == &type_SEL) {
				dasprintf (str, "SEL");
				break;
			}
			dasprintf (str, "(*");
			print_type_str (str, type->t.fldptr.type);
			dasprintf (str, ")");
			break;
		case ev_invalid:
			switch (type->ty) {
				case ty_class:
					dasprintf (str, " %s", type->t.class->name);
					break;
				case ty_enum:
				case ty_struct:
				case ty_union:
					{
						const char *tag = "struct";

						if (type->t.symtab->type == stab_union)
							tag = "union";
						else if (type->t.symtab->type == stab_union)
							tag = "enum";
						dasprintf (str, " %s %s", tag, type->name);//FIXME
					}
					break;
				case ty_array:
					print_type_str (str, type->t.array.type);
					if (type->t.array.base) {
						dasprintf (str, "[%d..%d]", type->t.array.base,
								type->t.array.base + type->t.array.size - 1);
					} else {
						dasprintf (str, "[%d]", type->t.array.size);
					}
					break;
				case ty_none:
					break;
			}
			break;
		default:
			dasprintf (str, " %s", pr_type_name[type->type]);
			break;
	}
}

void
print_type (type_t *type)
{
	dstring_t  *str = dstring_newstr ();
	print_type_str (str, type);
	printf ("%s", str->str);
	dstring_delete (str);
}

const char *
encode_params (type_t *type)
{
	const char *ret;
	dstring_t  *encoding = dstring_newstr ();
	int         i, count;

	if (type->t.func.num_params < 0)
		count = -type->t.func.num_params - 1;
	else
		count = type->t.func.num_params;
	for (i = 0; i < count; i++)
		encode_type (encoding, type->t.func.param_types[i]);
	if (type->t.func.num_params < 0)
		dasprintf (encoding, ".");

	ret = save_string (encoding->str);
	dstring_delete (encoding);
	return ret;
}

static void _encode_type (dstring_t *encoding, type_t *type, int level);

static void
encode_struct_fields (dstring_t *encoding, symtab_t *strct, int level)
{
	symbol_t   *field;

	for (field = strct->symbols; field; field = field->next) {
		if (field->sy_type != sy_var)
			continue;
		_encode_type (encoding, field->type, level + 1);
	}
}

static void
encode_class (dstring_t *encoding, type_t *type, int level)
{
	class_t    *class = type->t.class;
	const char *name ="?";

	if (class->name)
		name = class->name;
	dasprintf (encoding, "{%s@}", name);
}

static void
encode_struct (dstring_t *encoding, type_t *type, int level)
{
	symtab_t   *strct = type->t.symtab;
	const char *name ="?";
	char        su = '=';

	if (type->name)		// FIXME
		name = type->name;
	if (strct->type == stab_union)
		su = '-';
	dasprintf (encoding, "{%s%c", name, su);
	if (level < 2)
		encode_struct_fields (encoding, strct, level);
	dasprintf (encoding, "}");
}

static void
encode_enum (dstring_t *encoding, type_t *type, int level)
{
	symtab_t   *enm = type->t.symtab;
	symbol_t   *e;
	const char *name ="?";

	if (type->name)		// FIXME
		name = type->name;
	dasprintf (encoding, "{%s#", name);
	if (level < 2) {
		for (e = enm->symbols; e; e = e->next) {
			dasprintf (encoding, "%s=%d%s", e->name, e->s.value.v.integer_val,
					   e->next ? "," : "");
		}
	}
	dasprintf (encoding, "}");
}

static void
_encode_type (dstring_t *encoding, type_t *type, int level)
{
	if (!type)
		return;
	switch (type->type) {
		case ev_void:
			dasprintf (encoding, "v");
			break;
		case ev_string:
			dasprintf (encoding, "*");
			break;
		case ev_float:
			dasprintf (encoding, "f");
			break;
		case ev_vector:
			dasprintf (encoding, "V");
			break;
		case ev_entity:
			dasprintf (encoding, "E");
			break;
		case ev_field:
			dasprintf (encoding, "F");
			_encode_type (encoding, type->t.fldptr.type, level + 1);
			break;
		case ev_func:
			dasprintf (encoding, "(");
			_encode_type (encoding, type->t.func.type, level + 1);
			dasprintf (encoding, "%s)", encode_params (type));
			break;
		case ev_pointer:
			if (type == &type_id) {
				dasprintf (encoding, "@");
				break;
			}
			if (type == &type_SEL) {
				dasprintf (encoding, ":");
				break;
			}
			if (type == &type_Class) {
				dasprintf (encoding, "#");
				break;
			}
			type = type->t.fldptr.type;
			dasprintf (encoding, "^");
			_encode_type (encoding, type, level + 1);
			break;
		case ev_quat:
			dasprintf (encoding, "Q");
			break;
		case ev_integer:
			dasprintf (encoding, "i");
			break;
		case ev_short:
			dasprintf (encoding, "s");
			break;
		case ev_invalid:
			switch (type->ty) {
				case ty_class:
					encode_class (encoding, type, level);
					break;
				case ty_enum:
					encode_enum (encoding, type, level);
					break;
				case ty_struct:
				case ty_union:
					encode_struct (encoding, type, level);
					break;
				case ty_array:
					dasprintf (encoding, "[");
					dasprintf (encoding, "%d", type->t.array.size);
					if (type->t.array.base)
						dasprintf (encoding, ":%d", type->t.array.base);
					dasprintf (encoding, "=");
					_encode_type (encoding, type->t.array.type, level + 1);
					dasprintf (encoding, "]");
					break;
				case ty_none:
					dasprintf (encoding, "?");
					break;
			}
			break;
		case ev_type_count:
			dasprintf (encoding, "?");
			break;
	}
}

void
encode_type (dstring_t *encoding, type_t *type)
{
	_encode_type (encoding, type, 0);
}

static type_t *
parse_struct (const char **str)
{
	//FIXME
	dstring_t  *name;
	const char *s;

	name = dstring_newstr ();
	for (s = *str; *s && strchr ("=-@#}", *s); s++)
		;
	if (!*s)
		return 0;
	dstring_appendsubstr (name, *str, s - *str);
	*str = s;
	switch (*(*str)++) {
		case '=':
			break;
		case '-':
			break;
		case '@':
			break;
		case '#':
			break;
	}
	return 0;
}

static type_t *
_parse_type (const char **str)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	switch (*(*str)++) {
		case 'v':
			return &type_void;
		case '*':
			return &type_string;
		case 'f':
			return &type_float;
		case 'V':
			return &type_vector;
		case 'E':
			return &type_entity;
		case 'F':
			return field_type (_parse_type (str));
		case '(':
			new.type = ev_func;
			new.t.func.type = _parse_type (str);
			while (**str && **str != ')' && **str != '.')
				new.t.func.param_types[new.t.func.num_params++] = _parse_type (str);
			if (!**str)
				return 0;
			if (**str == '.') {
				(*str)++;
				new.t.func.num_params = -new.t.func.num_params - 1;
			}
			if (**str != ')')
				return 0;
			(*str)++;
			return find_type (&new);
		case '@':
			return &type_id;
		case '#':
			return &type_Class;
		case ':':
			return &type_SEL;
		case '^':
			return pointer_type (_parse_type (str));
		case 'Q':
			return &type_quaternion;
		case 'i':
			return &type_integer;
		case 's':
			return &type_short;
		case '{':
			return parse_struct (str);
		case '[':
			{
				type_t     *type;
				int         base = 0;
				int         size = 0;

				while (isdigit ((byte)**str)) {
					size *= 10;
					size += *(*str)++ - '0';
				}
				if (**str == ':') {
					(*str)++;
					while (isdigit ((byte)**str)) {
						size *= 10;
						size += *(*str)++ - '0';
					}
				}
				if (**str != '=')
					return 0;
				(*str)++;
				type = _parse_type (str);
				if (**str != ']')
					return 0;
				(*str)++;
				return based_array_type (type, base, base + size - 1);
			}
		default:
			return 0;
	}
}

type_t *
parse_type (const char *str)
{
	const char *s = str;
	type_t     *type = _parse_type (&s);
	if (!type) {
		error (0, "internal error: parsing type string `%s'", str);
		abort ();
	}
	return type;
}

int
is_scalar (type_t *type)
{
	etype_t     t = type->type;

	if (t == ev_float || t == ev_integer || t == ev_short)
		return 1;
	return 0;
}

int
is_math (type_t *type)
{
	etype_t     t = type->type;

	return t == ev_vector || t == ev_quat || is_scalar (type);
}

int
is_struct (type_t *type)
{
	if (type->type == ev_invalid
		&& (type->ty == ty_struct || type->type == ty_union))
		return 1;
	return 0;
}

int
is_class (type_t *type)
{
	if (type->type == ev_invalid && type->ty == ty_class)
		return 1;
	return 0;
}

int
is_array (type_t *type)
{
	if (type->type == ev_invalid && type->ty == ty_array)
		return 1;
	return 0;
}

int
type_assignable (type_t *dst, type_t *src)
{
	class_t    *dst_class, *src_class;

	if (dst == src)
		return 1;
	if (dst->type == ev_pointer
		&& src->type == ev_invalid && src->ty == ty_array) {
		if (dst->t.fldptr.type == src->t.array.type)
			return 1;
		return 0;
	}
	if (dst->type != ev_pointer || src->type != ev_pointer)
		return is_scalar (dst) && is_scalar (src);
	dst = dst->t.fldptr.type;
	src = src->t.fldptr.type;
	if (dst->type == ev_void)
		return 1;
	if (src->type == ev_void)
		return 1;
	if (!is_class (dst) || !is_class (src))
		return 0;
	dst_class = dst->t.class;
	src_class = src->t.class;
	//printf ("%s %s\n", dst_class->class_name, src_class->class_name);
	if (!dst_class || dst_class == &class_id)
		return 1;
	if (!src_class || src_class == &class_id)
		return 1;
	while (dst_class != src_class && src_class) {
		src_class = src_class->super_class;
		//if (src_class)
		//	printf ("%s %s\n", dst_class->class_name, src_class->class_name);
	}
	if (dst_class == src_class)
		return 1;
	return 0;
}

int
type_size (type_t *type)
{
	if (!type)
		return 0;
	switch (type->type) {
		case ev_void:
		case ev_string:
		case ev_float:
		case ev_vector:
		case ev_entity:
		case ev_field:
		case ev_func:
		case ev_pointer:
		case ev_quat:
		case ev_integer:
		case ev_short:
		case ev_type_count:
			return pr_type_size[type->type];
		case ev_invalid:
			switch (type->ty) {
				case ty_enum:
					return type_size (&type_integer);
				case ty_struct:
				case ty_union:
					return type->t.symtab->size;
				case ty_class:
					if (!type->t.class->ivars)
						return 0;
					return type->t.class->ivars->size;
				case ty_array:
					return type->t.array.size * type_size (type->t.array.type);
				case ty_none:
					return 0;
			}
			break;
	}
	return 0;
}

void
init_types (void)
{
	type_t     *type;
	static struct_def_t zero_struct[] = {
		{"string_val",       &type_string},
		{"float_val",        &type_float},
		{"entity_val",       &type_entity},
		{"field_val",        &type_field},
		{"func_val",         &type_function},
		{"pointer_val",      &type_pointer},
		{"integer_val",      &type_integer},
		{"vector_val",       &type_vector},
		{"quaternion_val",   &type_quaternion},
		{0, 0}
	};
	static struct_def_t param_struct[] = {
		{"string_val",       &type_string},
		{"float_val",        &type_float},
		{"vector_val",       &type_vector},
		{"entity_val",       &type_entity},
		{"field_val",        &type_field},
		{"func_val",         &type_function},
		{"pointer_val",      &type_pointer},
		{"integer_val",      &type_integer},
		{"quaternion_val",   &type_quaternion},
		{0, 0}
	};
	static struct_def_t vector_struct[] = {
		{"x", &type_float},
		{"y", &type_float},
		{"z", &type_float},
		{0, 0}
	};
	static struct_def_t quaternion_struct[] = {
		{"s", &type_float},
		{"v", &type_vector},
		{0, 0}
	};
	static struct_def_t sel_struct[] = {
		{"sel_id",    &type_string},
		{"sel_types", &type_string},
		{0, 0}
	};
	static struct_def_t method_struct[] = {
		{"method_name",  &type_SEL},
		{"method_types", &type_string},
		{"method_imp",   &type_IMP},
		{0, 0}
	};
	static struct_def_t class_struct[] = {
		{"class_pointer",  &type_Class},
		{"super_class",    &type_Class},
		{"name",           &type_string},
		{"version",        &type_integer},
		{"info",           &type_integer},
		{"instance_size",  &type_integer},
		{"ivars",          &type_pointer},
		{"methods",        &type_pointer},
		{"dtable",         &type_pointer},
		{"subclass_list",  &type_pointer},
		{"sibling_class",  &type_pointer},
		{"protocols",      &type_pointer},
		{"gc_object_type", &type_pointer},
	};
	static struct_def_t protocol_struct[] = {
		{"class_pointer",    &type_Class},
		{"protocol_name",    &type_string},
		{"protocol_list",    &type_pointer},
		{"instance_methods", &type_pointer},
		{"class_methods",    &type_pointer},
		{0, 0}
	};
	static struct_def_t id_struct[] = {
		{"class_pointer", &type_Class},
		{0, 0}
	};
	static struct_def_t method_desc_struct[] = {
		{"name",  &type_string},
		{"types", &type_string},
		{0, 0}
	};
	static struct_def_t category_struct[] = {
		{"category_name",    &type_string},
		{"class_name",       &type_string},
		{"instance_methods", &type_pointer},
		{"class_methods",    &type_pointer},
		{"protocols",        &type_pointer},
		{0, 0}
	};
	static struct_def_t ivar_struct[] = {
		{"ivar_name",   &type_string},
		{"ivar_type",   &type_string},
		{"ivar_offset", &type_integer},
		{0, 0}
	};
	static struct_def_t super_struct[] = {
		{"self", &type_id},
		{"class", &type_Class},
		{0, 0}
	};

	type_nil = &type_quaternion;
	type_default = &type_integer;
	if (options.code.progsversion == PROG_ID_VERSION) {
		// vector can't be part of .zero for v6 progs because for v6 progs,
		// .zero is only one word wide.
		zero_struct[7].name = 0;
		// v6 progs don't have quaternions
		zero_struct[8].name = 0;
		param_struct[8].name = 0;
		type_nil = &type_vector;
		type_default = &type_float;
	}

	make_structure (0, 'u', zero_struct, &type_zero);
	make_structure (0, 'u', param_struct, &type_param);
	make_structure (0, 's', vector_struct, &type_vector);
	type_vector.type = ev_vector;

	if (options.traditional)
		return;

	make_structure (0, 's', quaternion_struct, &type_quaternion);
	type_quaternion.type = ev_quat;

	type_SEL.t.fldptr.type = make_structure (0, 's', sel_struct, 0)->type;

	make_structure (0, 's', method_struct, &type_Method);

	type = make_structure (0, 's', class_struct, 0)->type;
	type_Class.t.fldptr.type = type;
	class_Class.ivars = type->t.symtab;

	type = make_structure (0, 's', protocol_struct, &type_Protocol)->type;
	type->ty = ty_class;
	type->t.class = &class_Protocol;
	class_Protocol.ivars = type->t.symtab;

	type = make_structure (0, 's', id_struct, 0)->type;
	type->ty = ty_class;
	type->t.class = &class_id;
	type_id.t.fldptr.type = type;
	class_id.ivars = type->t.symtab;

	make_structure (0, 's', method_desc_struct, &type_method_description);

	make_structure (0, 's', category_struct, &type_category);

	make_structure (0, 's', ivar_struct, &type_ivar);

	make_structure (0, 's', super_struct, &type_Super);
}

void
chain_initial_types (void)
{
	static struct_def_t va_list_struct[] = {
		{"count", &type_integer},
		{"list",  0},				// type will be filled in at runtime
		{0, 0}
	};
	static struct_def_t module_struct[] = {
		{"version", &type_integer},
		{"size",    &type_integer},
		{"name",    &type_string},
		{"symtab",  &type_pointer},
		{0, 0}
	};

	chain_type (&type_void);
	chain_type (&type_string);
	chain_type (&type_float);
	chain_type (&type_vector);
	chain_type (&type_entity);
	chain_type (&type_field);
	chain_type (&type_function);
	chain_type (&type_pointer);
	chain_type (&type_floatfield);

	chain_type (&type_param);
	chain_type (&type_zero);

	va_list_struct[1].type = pointer_type (&type_param);
	make_structure (0, 's', va_list_struct, &type_va_list);

	if (options.traditional)
		return;

	chain_type (&type_quaternion);
	chain_type (&type_integer);
	chain_type (&type_short);
	chain_type (&type_IMP);
	chain_type (&type_Super);

	chain_type (&type_SEL);
	chain_type (&type_Method);
	chain_type (&type_Class);
	chain_type (&type_Protocol);
	chain_type (&type_id);
	chain_type (&type_method_description);
	chain_type (&type_category);
	chain_type (&type_ivar);

	type_supermsg.t.func.param_types[0] = pointer_type (&type_Super);
	chain_type (&type_supermsg);

	make_structure ("obj_module_s", 's', module_struct, &type_module);
	//new_typedef ("obj_module_t", type_module);
	chain_type (&type_module);

	type_obj_exec_class.t.func.param_types[0] = pointer_type (&type_module);
	chain_type (&type_obj_exec_class);
}
