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

static __attribute__ ((unused)) const char rcsid[] =
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
type_t      type_void = { ev_void, "void" };
type_t      type_string = { ev_string, "string" };
type_t      type_float = { ev_float, "float" };
type_t      type_vector = { ev_vector, "vector" };
type_t      type_entity = { ev_entity, "entity" };
type_t      type_field = { ev_field, "field", NULL, &type_void };

// type_function is a void() function used for state defs
type_t      type_function = { ev_func, "function", NULL, &type_void };
type_t      type_pointer = { ev_pointer, "pointer", NULL, &type_void };
type_t      type_quaternion = { ev_quat, "quaternion" };
type_t      type_integer = { ev_integer, "integer" };
type_t      type_uinteger = { ev_uinteger, "uiniteger" };
type_t      type_short = { ev_short, "short" };
type_t      type_struct = { ev_struct, "struct" };
// these will be built up further
type_t      type_id = { ev_pointer, "id" };
type_t      type_Class = { ev_pointer, "Class" };
type_t      type_Protocol = { ev_pointer, "Protocol" };
type_t      type_SEL = { ev_pointer, "SEL" };
type_t      type_IMP = { ev_func, "IMP", NULL, &type_id, -3, { &type_id,
															   &type_SEL }};
type_t      type_supermsg = { ev_func, ".supermsg", NULL, &type_id, -3,
							  { 0, &type_SEL }};
type_t      type_obj_exec_class = { ev_func, "function", NULL, &type_void, 1, { 0 }};
type_t      type_Method = { ev_pointer, "Method" };
type_t      type_Super = { ev_pointer, "Super" };
type_t      type_method_description = { ev_struct, "obj_method_description" };
type_t     *type_category;
type_t     *type_ivar;
type_t     *type_module;
type_t      type_va_list = { ev_struct, "@va_list" };
type_t      type_param = { ev_struct, "@param" };
type_t      type_zero;

struct_t   *vector_struct;
struct_t   *quaternion_struct;

type_t      type_floatfield = { ev_field, ".float", NULL, &type_float };

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

/*
	find_type

	Returns a preexisting complex type that matches the parm, or allocates
	a new one and copies it out.
*/
type_t *
find_type (type_t *type)
{
	type_t     *check;
	int         c, i;

	for (check = pr.types; check; check = check->next) {
		if (check->type != type->type
			|| check->aux_type != type->aux_type
			|| check->num_parms != type->num_parms
			|| check->s.class != type->s.class)
			continue;

		if (check->type != ev_func)
			return check;

		if (check->num_parms == -1)
			return check;

		if ((c = check->num_parms) < 0)
			c = -c - 1;

		for (i = 0; i < c; i++)
			if (check->parm_types[i] != type->parm_types[i])
				break;

		if (i == c)
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
	new.aux_type = aux;
	return find_type (&new);
}

type_t *
pointer_type (type_t *aux)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_pointer;
	new.aux_type = aux;
	return find_type (&new);
}

type_t *
array_type (type_t *aux, int size)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_array;
	new.aux_type = aux;
	new.num_parms = size;
	return find_type (&new);
}

void
print_type (type_t *type)
{
	if (!type) {
		printf (" (null)");
		return;
	}
	switch (type->type) {
		case ev_field:
			printf (" .(");
			print_type (type->aux_type);
			printf (")");
			break;
		case ev_func:
			print_type (type->aux_type);
			if (type->num_parms == -1) {
				printf ("(...)");
			} else {
				int         c, i;
				printf ("(");
				if ((c = type->num_parms) < 0)
					c = -c - 1;
				for (i = 0; i < c; i++) {
					if (i)
						printf (", ");
					print_type (type->parm_types[i]);
				}
				if (type->num_parms < 0)
						printf (", ...");
				printf (")");
			}
			break;
		case ev_pointer:
			print_type (type->aux_type);
			if (type->num_parms)
				printf ("[%d]", type->num_parms);
			else
				printf ("[]");
			break;
		case ev_object:
		case ev_class:
			printf (" %s", type->s.class->name);
			break;
		case ev_struct:
			printf (" %s %s", pr_type_name[type->type], type->s.strct->name);
			break;
		default:
			printf(" %s", pr_type_name[type->type]);
			break;
	}
}

const char *
encode_params (type_t *type)
{
	const char *ret;
	dstring_t  *encoding = dstring_newstr ();
	int         i, count;

	if (type->num_parms < 0)
		count = -type->num_parms - 1;
	else
		count = type->num_parms;
	for (i = 0; i < count; i++)
		encode_type (encoding, type->parm_types[i]);
	if (type->num_parms < 0)
		dstring_appendstr (encoding, ".");

	ret = save_string (encoding->str);
	dstring_delete (encoding);
	return ret;
}

static void
_encode_type (dstring_t *encoding, type_t *type, int level)
{
	struct_field_t *field;
	struct_t   *strct;

	if (!type)
		return;
	switch (type->type) {
		case ev_void:
			dstring_appendstr (encoding, "v");
			break;
		case ev_string:
			dstring_appendstr (encoding, "*");
			break;
		case ev_float:
			dstring_appendstr (encoding, "f");
			break;
		case ev_vector:
			dstring_appendstr (encoding, "V");
			break;
		case ev_entity:
			dstring_appendstr (encoding, "E");
			break;
		case ev_field:
			dstring_appendstr (encoding, "F");
			_encode_type (encoding, type->aux_type, level + 1);
			break;
		case ev_func:
			dstring_appendstr (encoding, "(");
			_encode_type (encoding, type->aux_type, level + 1);
			dstring_appendstr (encoding, encode_params (type));
			dstring_appendstr (encoding, ")");
			break;
		case ev_pointer:
			type = type->aux_type;
			switch (type->type) {
				case ev_object:
					dstring_appendstr (encoding, "@");
					break;
				case ev_class:
					dstring_appendstr (encoding, "#");
					break;
				case ev_struct:
					if (type == type_SEL.aux_type) {
						dstring_appendstr (encoding, ":");
						break;
					}
					// fall through
				default:
					dstring_appendstr (encoding, "^");
					_encode_type (encoding, type, level + 1);
					break;
			}
			break;
		case ev_quat:
			dstring_appendstr (encoding, "Q");
			break;
		case ev_integer:
			dstring_appendstr (encoding, "i");
			break;
		case ev_uinteger:
			dstring_appendstr (encoding, "I");
			break;
		case ev_short:
			dstring_appendstr (encoding, "s");
			break;
		case ev_struct:
		case ev_object:
		case ev_class:
			if (type->type == ev_struct) {
				strct = type->s.strct;
			} else {
				strct = type->s.class->ivars;
			}
			dstring_appendstr (encoding, "{");
			if (strct->name) {
				dstring_appendstr (encoding, strct->name);
			} else if (type->type == ev_object || type->type == ev_class) {
				dstring_appendstr (encoding, type->s.class->name);
			}
			if (level < 2) {
				struct_t   *s;
				if (type->type == ev_struct
					&& type->s.strct->stype == str_union)
					dstring_appendstr (encoding, "-");
				else
					dstring_appendstr (encoding, "=");
				if (type->type == ev_struct) {
					s = type->s.strct;
				} else {
					s = type->s.class->ivars;
				}
				for (field = s->struct_head; field; field = field->next)
					_encode_type (encoding, field->type, level + 1);
			}
			dstring_appendstr (encoding, "}");
			break;
		case ev_sel:
			dstring_appendstr (encoding, ":");
			break;
		case ev_array:
			dstring_appendstr (encoding, "[");
			dstring_appendstr (encoding, va ("%d ", type->num_parms));
			_encode_type (encoding, type->aux_type, level + 1);
			dstring_appendstr (encoding, "]");
			break;
		case ev_type_count:
			dstring_appendstr (encoding, "?");
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
			new.aux_type = _parse_type (str);
			while (**str && **str != ')' && **str != '.')
				new.parm_types[new.num_parms++] = _parse_type (str);
			if (!**str)
				return 0;
			if (**str == '.') {
				(*str)++;
				new.num_parms = -new.num_parms - 1;
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
		case 'I':
			return &type_uinteger;
		case 's':
			return &type_short;
		case '{':
			new.type = ev_struct;
			name = dstring_newstr ();
			for (s = *str; *s && *s != '='  && *s != '-' && *s !='}'; s++)
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
			new.type = ev_array;
			while (isdigit ((byte)**str)) {
				new.num_parms *= 10;
				new.num_parms += *(*str)++ - '0';
			}
			while (isspace ((byte)**str))
				(*str)++;
			new.aux_type = _parse_type (str);
			if (**str != ']')
				return 0;
			(*str)++;
			return find_type (&new);
		default:
			return 0;
	}
}

type_t *
parse_type (const char *str)
{
	return _parse_type (&str);
}

static int
is_scalar (type_t *type)
{
	etype_t     t = type->type;

	if (t == ev_float || t == ev_integer || t == ev_uinteger || t == ev_short)
		return 1;
	return 0;
}

int
type_assignable (type_t *dst, type_t *src)
{
	class_t    *dst_class, *src_class;

	if (dst == src)
		return 1;
	if (dst->type != ev_pointer || src->type != ev_pointer)
		return is_scalar (dst) && is_scalar (src);
	dst = dst->aux_type;
	src = src->aux_type;
	if (dst->type == ev_void)
		return 1;
	if (src->type == ev_void)
		return 1;
	if ((dst->type != ev_object && dst->type != ev_class)
		|| (src->type != ev_object && src->type != ev_class))
		return 0;
	dst_class = dst->s.class;
	src_class = src->s.class;
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
		case ev_uinteger:
		case ev_short:
		case ev_sel:
		case ev_type_count:
			return pr_type_size[type->type];
		case ev_struct:
			return type->s.strct->size;
		case ev_object:
		case ev_class:
			if (!type->s.class->ivars)
				return 0;
			return type->s.class->ivars->size;
		case ev_array:
			return type->num_parms * type_size (type->aux_type);
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
	new_struct_field (strct, &type_uinteger, "uinteger_val", vis_public);

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
	new_struct_field (strct, &type_uinteger, "uinteger_val", vis_public);

	strct = vector_struct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_float, "x", vis_public);
	new_struct_field (strct, &type_float, "y", vis_public);
	new_struct_field (strct, &type_float, "z", vis_public);

	if (options.traditional)
		return;

	strct = type_zero.s.strct;
	new_struct_field (strct, &type_vector,     "vector_val",     vis_public);
	new_struct_field (strct, &type_quaternion, "quaternion_val", vis_public);

	strct = type_param.s.strct;
	new_struct_field (strct, &type_quaternion, "quaternion_val", vis_public);

	strct = quaternion_struct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_float,  "s", vis_public);
	new_struct_field (strct, &type_vector, "v", vis_public);

	strct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_string, "sel_id", vis_public);
	new_struct_field (strct, &type_string, "sel_types", vis_public);
	type_SEL.aux_type = strct->type;

	strct = get_struct (0, 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_SEL, "method_name", vis_public);
	new_struct_field (strct, &type_string, "method_types", vis_public);
	new_struct_field (strct, &type_IMP, "method_imp", vis_public);
	type_Method.aux_type = strct->type;

	strct = get_struct (0, 1);
	init_struct (strct, type = new_type (), str_struct, 0);
	type->type = ev_class;
	type->s.class = &class_Class;
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
	type_Class.aux_type = strct->type;
	class_Class.ivars = strct;

	strct = get_struct (0, 1);
	init_struct (strct, type = new_type (), str_struct, 0);
	type->type = ev_class;
	type->s.class = &class_Protocol;
	new_struct_field (strct, &type_Class, "class_pointer", vis_public);
	new_struct_field (strct, &type_string, "protocol_name", vis_public);
	new_struct_field (strct, &type_pointer, "protocol_list", vis_public);
	new_struct_field (strct, &type_pointer, "instance_methods", vis_public);
	new_struct_field (strct, &type_pointer, "class_methods", vis_public);
	type_Protocol.aux_type = strct->type;
	class_Protocol.ivars = strct;

	strct = get_struct (0, 1);
	init_struct (strct, type = new_type (), str_struct, 0);
	type->type = ev_object;
	type->s.class = &class_id;
	new_struct_field (strct, &type_Class, "class_pointer", vis_public);
	type_id.aux_type = strct->type;
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
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_id, "self", vis_public);
	new_struct_field (strct, &type_Class, "class", vis_public);
	type_Super.aux_type = strct->type;
#if 0
	type = type_module = new_struct ("obj_module_t");
	new_struct_field (strct, &type_integer, "version", vis_public);
	new_struct_field (strct, &type_integer, "size", vis_public);
	new_struct_field (strct, &type_string, "name", vis_public);
	new_struct_field (strct, &type_pointer, "symtab", vis_public);
#endif
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

	if (options.traditional)
		return;

	chain_type (&type_quaternion);
	chain_type (&type_integer);
	chain_type (&type_uinteger);
	chain_type (&type_short);
	chain_type (&type_struct);
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

	type_supermsg.parm_types[0] = &type_Super;
	chain_type (&type_supermsg);

	strct = calloc (sizeof (struct_t), 1);
	init_struct (strct, &type_va_list, str_struct, 0);
	new_struct_field (strct, &type_integer, "count", vis_public);
	new_struct_field (strct, pointer_type (&type_param), "list", vis_public);

	strct = get_struct ("obj_module_s", 1);
	init_struct (strct, new_type (), str_struct, 0);
	new_struct_field (strct, &type_integer, "version", vis_public);
	new_struct_field (strct, &type_integer, "size", vis_public);
	new_struct_field (strct, &type_string, "name", vis_public);
	new_struct_field (strct, &type_pointer, "symtab", vis_public);
	type_module = strct->type;
	new_typedef ("obj_module_t", type_module);
	chain_type (type_module);

	type_obj_exec_class.parm_types[0] = pointer_type (type_module);
	chain_type (&type_obj_exec_class);
}

void
clear_typedefs (void)
{
	if (typedef_hash)
		Hash_FlushTable (typedef_hash);
}
