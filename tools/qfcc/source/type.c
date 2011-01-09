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
#include "expr.h"
#include "function.h"
#include "options.h"
#include "qfcc.h"
#include "struct.h"
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
type_t     *type_category;
type_t     *type_ivar;
type_t     *type_module;
type_t      type_va_list = { ev_invalid, "@va_list", ty_struct };
type_t      type_param = { ev_invalid, "@param", ty_struct };
type_t      type_zero;

struct_t   *vector_struct;
struct_t   *quaternion_struct;

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
			if (a->t.strct != b->t.strct)
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
	error (0, "we be broke");
	abort ();
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

static hashtab_t *typedef_hash;
static typedef_t *free_typedefs;

static const char *
typedef_get_key (void *t, void *unused)
{
	return ((typedef_t *)t)->name;
}

void
new_typedef (const char *name, type_t *type)
{
	typedef_t  *td;

	if (!typedef_hash)
		typedef_hash = Hash_NewTable (1023, typedef_get_key, 0, 0);
	td = Hash_Find (typedef_hash, name);
	if (td) {
		error (0, "%s redefined", name);
		return;
	}
	ALLOC (1024, typedef_t, typedefs, td);
	td->name = name;
	td->type = type;
	Hash_Add (typedef_hash, td);
}

typedef_t *
get_typedef (const char *name)
{
	if (!typedef_hash)
		return 0;
	return Hash_Find (typedef_hash, name);
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
				case ty_struct:
					{
						const char *tag = "struct";

						if (type->t.strct->stype == str_union)
							tag = "union";
						dasprintf (str, " %s %s", tag, type->t.strct->name);
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
encode_struct_fields (dstring_t *encoding, struct_t *strct, int level)
{
	struct_field_t *field;

	for (field = strct->struct_head; field; field = field->next)
		_encode_type (encoding, field->type, level + 1);
}

static void
encode_class (dstring_t *encoding, type_t *type, int level)
{
	class_t    *class = type->t.class;
	const char *name ="?";

	if (class->name)
		name = class->name;
	dasprintf (encoding, "{%s=", name);	//FIXME loses info about being a class
	if (level < 2 && class->ivars)
		encode_struct_fields (encoding, class->ivars, level);
	dasprintf (encoding, "}");
}

static void
encode_struct (dstring_t *encoding, type_t *type, int level)
{
	struct_t   *strct = type->t.strct;
	const char *name ="?";
	char        su = '=';

	if (strct->name)
		name = strct->name;
	if (strct->stype == str_union)
		su = '-';
	dasprintf (encoding, "{%s%c", name, su);
	if (level < 2)
		encode_struct_fields (encoding, strct, level);
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
				case ty_struct:
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
_parse_type (const char **str)
{
	type_t      new;
	struct_t   *strct;
	dstring_t  *name;
	const char *s;

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
			new.type = ev_invalid;
			new.ty = ty_struct;
			name = dstring_newstr ();
			for (s = *str;
				 *s && *s != '='  && *s != '-' && *s != '#' && *s != '}'; s++)
				;
			if (!*s)
				return 0;
			dstring_appendsubstr (name, *str, s - *str);
			*str = s;
			strct = 0;
			if (name->str[0])
				strct = get_struct (name->str, 0);
			if (strct && strct->struct_head) {
				dstring_delete (name);
				if (**str == '=' || **str == '-') {
					(*str)++;
					while (**str && **str != '}')
						_parse_type (str);
				}
				if (**str != '}')
					return 0;
				(*str)++;
				return strct->type;
			}
			if (**str != '=' && **str != '-' && **str != '}') {
				if (( s = strchr (*str, '}')))
					*str = s + 1;
				dstring_delete (name);
				return 0;
			}
			if (!strct) {
				strct = get_struct (*name->str ? name->str : 0, 1);
				init_struct (strct, new_type (), str_none, 0);
			}
			dstring_delete (name);
			if (**str == '}') {
				(*str)++;
				return strct->type;
			}
			if (**str == '-')
				strct->stype = str_union;
			else
				strct->stype = str_struct;
			(*str)++;
			while (**str && **str != '}')
				new_struct_field (strct, _parse_type (str), 0, vis_public);
			if (**str != '}')
				return 0;
			(*str)++;
			return strct->type;
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
is_struct (type_t *type)
{
	if (type->type == ev_invalid && type->ty == ty_struct)
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
				case ty_struct:
					return type->t.strct->size;
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
	struct_t   *strct;

	strct = calloc (sizeof (struct_t), 1);
	init_struct (strct, &type_zero, str_union, 0);
	new_struct_field (strct, &type_string,   "string_val",   vis_public);
	new_struct_field (strct, &type_float,    "float_val",    vis_public);
	new_struct_field (strct, &type_entity,   "entity_val",   vis_public);
	new_struct_field (strct, &type_field,    "field_val",    vis_public);
	new_struct_field (strct, &type_function, "func_val",     vis_public);
	new_struct_field (strct, &type_pointer,  "pointer_val",  vis_public);
	new_struct_field (strct, &type_integer,  "integer_val",  vis_public);

	strct = calloc (sizeof (struct_t), 1);
	init_struct (strct, &type_param, str_union, 0);
	new_struct_field (strct, &type_string,   "string_val",   vis_public);
	new_struct_field (strct, &type_float,    "float_val",    vis_public);
	new_struct_field (strct, &type_vector,   "vector_val",   vis_public);
	new_struct_field (strct, &type_entity,   "entity_val",   vis_public);
	new_struct_field (strct, &type_field,    "field_val",    vis_public);
	new_struct_field (strct, &type_function, "func_val",     vis_public);
	new_struct_field (strct, &type_pointer,  "pointer_val",  vis_public);
	new_struct_field (strct, &type_integer,  "integer_val",  vis_public);

	strct = vector_struct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_float, "x", vis_public);
	new_struct_field (strct, &type_float, "y", vis_public);
	new_struct_field (strct, &type_float, "z", vis_public);

	if (options.traditional)
		return;

	if (options.code.progsversion != PROG_ID_VERSION) {
		strct = type_zero.t.strct;
		// vector can't be part of .zero for v6 progs because for v6 progs,
		// .zero is only one word wide.
		new_struct_field (strct, &type_vector, "vector_val", vis_public);
		new_struct_field (strct, &type_quaternion, "quaternion_val",
						  vis_public);

		strct = type_param.t.strct;
		new_struct_field (strct, &type_quaternion, "quaternion_val",
						  vis_public);
	}

	strct = quaternion_struct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_float,  "s", vis_public);
	new_struct_field (strct, &type_vector, "v", vis_public);

	strct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_string, "sel_id", vis_public);
	new_struct_field (strct, &type_string, "sel_types", vis_public);
	type_SEL.t.fldptr.type = strct->type;

	strct = get_struct (0, 1);
	init_struct (strct, &type_Method, str_struct, 0);
	new_struct_field (strct, &type_SEL, "method_name", vis_public);
	new_struct_field (strct, &type_string, "method_types", vis_public);
	new_struct_field (strct, &type_IMP, "method_imp", vis_public);

	strct = get_struct (0, 1);
	init_struct (strct, type = new_type (), str_struct, 0);
	type->ty = ty_class;
	type->t.class = &class_Class;
	new_struct_field (strct, &type_Class, "class_pointer", vis_public);
	new_struct_field (strct, &type_Class, "super_class", vis_public);
	new_struct_field (strct, &type_string, "name", vis_public);
	new_struct_field (strct, &type_integer, "version", vis_public);
	new_struct_field (strct, &type_integer, "info", vis_public);
	new_struct_field (strct, &type_integer, "instance_size", vis_public);
	new_struct_field (strct, &type_pointer, "ivars", vis_public);
	new_struct_field (strct, &type_pointer, "methods", vis_public);
	new_struct_field (strct, &type_pointer, "dtable", vis_public);
	new_struct_field (strct, &type_pointer, "subclass_list", vis_public);
	new_struct_field (strct, &type_pointer, "sibling_class", vis_public);
	new_struct_field (strct, &type_pointer, "protocols", vis_public);
	new_struct_field (strct, &type_pointer, "gc_object_type", vis_public);
	type_Class.t.fldptr.type = strct->type;
	class_Class.ivars = strct;

	strct = get_struct (0, 1);
	init_struct (strct, type = &type_Protocol, str_struct, 0);
	type->ty = ty_class;
	type->t.class = &class_Protocol;
	new_struct_field (strct, &type_Class, "class_pointer", vis_public);
	new_struct_field (strct, &type_string, "protocol_name", vis_public);
	new_struct_field (strct, &type_pointer, "protocol_list", vis_public);
	new_struct_field (strct, &type_pointer, "instance_methods", vis_public);
	new_struct_field (strct, &type_pointer, "class_methods", vis_public);
	class_Protocol.ivars = strct;

	strct = get_struct (0, 1);
	init_struct (strct, type = new_type (), str_struct, 0);
	type->ty = ty_class;
	type->t.class = &class_id;
	new_struct_field (strct, &type_Class, "class_pointer", vis_public);
	type_id.t.fldptr.type = strct->type;
	class_id.ivars = strct;

	strct = get_struct (0, 1);
	init_struct (strct, &type_method_description, str_struct, 0);
	new_struct_field (strct, &type_string, "name", vis_public);
	new_struct_field (strct, &type_string, "types", vis_public);

	strct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_string, "category_name", vis_public);
	new_struct_field (strct, &type_string, "class_name", vis_public);
	new_struct_field (strct, &type_pointer, "instance_methods", vis_public);
	new_struct_field (strct, &type_pointer, "class_methods", vis_public);
	new_struct_field (strct, &type_pointer, "protocols", vis_public);
	type_category = strct->type;

	strct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_string, "ivar_name", vis_public);
	new_struct_field (strct, &type_string, "ivar_type", vis_public);
	new_struct_field (strct, &type_integer, "ivar_offset", vis_public);
	type_ivar = strct->type;

	strct = get_struct ("Super", 1);
	init_struct (strct, &type_Super, str_struct, 0);
	new_struct_field (strct, &type_id, "self", vis_public);
	new_struct_field (strct, &type_Class, "class", vis_public);
}

void
chain_initial_types (void)
{
	struct_t   *strct;

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

	strct = calloc (sizeof (struct_t), 1);
	init_struct (strct, &type_va_list, str_struct, 0);
	new_struct_field (strct, &type_integer, "count", vis_public);
	new_struct_field (strct, pointer_type (&type_param), "list", vis_public);

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
	chain_type (type_category);
	chain_type (type_ivar);

	type_supermsg.t.func.param_types[0] = pointer_type (&type_Super);
	chain_type (&type_supermsg);

	strct = get_struct ("obj_module_s", 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_integer, "version", vis_public);
	new_struct_field (strct, &type_integer, "size", vis_public);
	new_struct_field (strct, &type_string, "name", vis_public);
	new_struct_field (strct, &type_pointer, "symtab", vis_public);
	type_module = strct->type;
	new_typedef ("obj_module_t", type_module);
	chain_type (type_module);

	type_obj_exec_class.t.func.param_types[0] = pointer_type (type_module);
	chain_type (&type_obj_exec_class);
}

void
clear_typedefs (void)
{
	if (typedef_hash)
		Hash_FlushTable (typedef_hash);
}
