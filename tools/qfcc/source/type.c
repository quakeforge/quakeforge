/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qfcc.h"
#include "def.h"
#include "expr.h"
#include "class.h"
#include "function.h"
#include "struct.h"
#include "type.h"

typedef struct typedef_s {
	struct typedef_s *next;
	const char *name;
	type_t     *type;
} typedef_t;

// simple types.  function types are dynamically allocated
type_t      type_void = { ev_void, "void" };
type_t      type_string = { ev_string, "string" };
type_t      type_float = { ev_float, "float" };
type_t      type_vector = { ev_vector, "vector" };
type_t      type_entity = { ev_entity, "entity" };
type_t      type_field = { ev_field, "field" };

// type_function is a void() function used for state defs
type_t      type_function = { ev_func, "function", NULL, &type_void };
type_t      type_pointer = { ev_pointer, "pointer", NULL, &type_void };
type_t      type_quaternion = { ev_quaternion, "quaternion" };
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
type_t      type_obj_exec_class = { ev_func, "function", NULL, &type_void, 1, { 0 }};
type_t      type_Method = { ev_pointer, "Method" };
type_t     *type_category;
type_t     *type_ivar;
type_t     *type_module;

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
			|| check->class != type->class)
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

type_t *
get_typedef (const char *name)
{
	typedef_t  *td;

	if (!typedef_hash)
		return 0;
	td = Hash_Find (typedef_hash, name);
	if (!td)
		return 0;
	return td->type;
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
	class_t    *class;

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
			class = type->class;
			printf (" %s%s", 
					class->class_name,
					class->category_name ? va (" (%s)", class->category_name)
										 : "");
			break;
		case ev_struct:
			printf (" %s %s", pr_type_name[type->type], type->name);
			break;
		default:
			printf(" %s", pr_type_name[type->type]);
			break;
	}
}

void
_encode_type (dstring_t *encoding, type_t *type, int level)
{
	struct_field_t *field;
	int         i, count;

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
			dstring_appendstr (encoding, "?");
			break;
		case ev_entity:
			dstring_appendstr (encoding, "?");
			break;
		case ev_field:
			dstring_appendstr (encoding, "F");
			break;
		case ev_func:
			dstring_appendstr (encoding, "(");
			_encode_type (encoding, type->aux_type, level + 1);
			if (type->num_parms < 0)
				count = -type->num_parms - 1;
			else
				count = type->num_parms;
			for (i = 0; i < count; i++)
				_encode_type (encoding, type->parm_types[i], level + 1);
			if (type->num_parms < 0)
				dstring_appendstr (encoding, ".");
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
		case ev_quaternion:
			dstring_appendstr (encoding, "?");
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
			dstring_appendstr (encoding, "{");
			if (type->name) {
				dstring_appendstr (encoding, type->name);
			} else if (type->type == ev_object || type->type == ev_class) {
				dstring_appendstr (encoding, type->class->class_name);
			}
			if (level < 2) {
				dstring_appendstr (encoding, "=");
				for (field = type->struct_head; field; field = field->next)
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

int
type_assignable (type_t *dst, type_t *src)
{
	class_t    *dst_class, *src_class;

	if (dst == src)
		return 1;
	if (dst->type != ev_pointer || src->type != ev_pointer)
		return 0;
	dst = dst->aux_type;
	src = src->aux_type;
	if ((dst->type != ev_object && dst->type != ev_class)
		|| (src->type != ev_object && src->type != ev_class))
		return 0;
	dst_class = dst->class;
	src_class = src->class;
	//printf ("%s %s\n", dst_class->class_name, src_class->class_name);
	if (!dst_class || dst_class == &class_id)
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
	struct_field_t *field;
	int         size;

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
		case ev_quaternion:
		case ev_integer:
		case ev_uinteger:
		case ev_short:
		case ev_sel:
		case ev_type_count:
			return pr_type_size[type->type];
		case ev_struct:
		case ev_object:
		case ev_class:
			for (size = 0, field = type->struct_head;
				 field;
				 field = field->next)
				size += type_size (field->type);
			return size;
		case ev_array:
			return type->num_parms * type_size (type->aux_type);
	}
	return 0;
}

void
init_types (void)
{
	type_t     *type;

	type = type_SEL.aux_type = new_struct (0);
	new_struct_field (type, &type_string, "sel_id", vis_public);
	new_struct_field (type, &type_string, "sel_types", vis_public);

	type = type_Method.aux_type = new_struct (0);
	new_struct_field (type, type_SEL.aux_type, "method_name", vis_public);
	new_struct_field (type, &type_string, "method_types", vis_public);
	new_struct_field (type, &type_IMP, "method_imp", vis_public);

	type = type_Class.aux_type = new_struct (0);
	type->type = ev_class;
	type->class = &class_Class;
	class_Class.ivars = type_Class.aux_type;
	new_struct_field (type, &type_Class, "class_pointer", vis_public);
	new_struct_field (type, &type_Class, "super_class", vis_public);
	new_struct_field (type, &type_string, "name", vis_public);
	new_struct_field (type, &type_integer, "version", vis_public);
	new_struct_field (type, &type_integer, "info", vis_public);
	new_struct_field (type, &type_integer, "instance_size", vis_public);
	new_struct_field (type, &type_pointer, "ivars", vis_public);
	new_struct_field (type, &type_pointer, "methods", vis_public);
	new_struct_field (type, &type_pointer, "dtable", vis_public);
	new_struct_field (type, &type_pointer, "subclass_list", vis_public);
	new_struct_field (type, &type_pointer, "sibling_class", vis_public);
	new_struct_field (type, &type_pointer, "protocols", vis_public);
	new_struct_field (type, &type_pointer, "gc_object_type", vis_public);

	type = type_Protocol.aux_type = new_struct (0);
	type->type = ev_class;
	type->class = &class_Protocol;
	class_Protocol.ivars = type_Protocol.aux_type;
	new_struct_field (type, &type_Class, "class_pointer", vis_public);
	new_struct_field (type, &type_string, "protocol_name", vis_public);
	new_struct_field (type, &type_pointer, "protocol_list", vis_public);
	new_struct_field (type, &type_pointer, "instance_methods", vis_public);
	new_struct_field (type, &type_pointer, "class_methods", vis_public);

	type = type_id.aux_type = new_struct ("id");
	type->type = ev_object;
	type->class = &class_id;
	class_id.ivars = type_id.aux_type;
	new_struct_field (type, &type_Class, "class_pointer", vis_public);

	type = type_category = new_struct (0);
	new_struct_field (type, &type_string, "category_name", vis_public);
	new_struct_field (type, &type_string, "class_name", vis_public);
	new_struct_field (type, &type_pointer, "instance_methods", vis_public);
	new_struct_field (type, &type_pointer, "class_methods", vis_public);
	new_struct_field (type, &type_pointer, "protocols", vis_public);

	type = type_ivar = new_struct (0);
	new_struct_field (type, &type_string, "ivar_name", vis_public);
	new_struct_field (type, &type_string, "ivar_type", vis_public);
	new_struct_field (type, &type_integer, "ivar_offset", vis_public);

	type = type_module = new_struct (0);
	new_struct_field (type, &type_integer, "version", vis_public);
	new_struct_field (type, &type_integer, "size", vis_public);
	new_struct_field (type, &type_string, "name", vis_public);
	new_struct_field (type, &type_pointer, "symtab", vis_public);

	type_obj_exec_class.parm_types[0] = pointer_type (type_module);
}

void
chain_initial_types (void)
{
	chain_type (&type_void);
	chain_type (&type_string);
	chain_type (&type_float);
	chain_type (&type_vector);
	chain_type (&type_entity);
	chain_type (&type_field);
	chain_type (&type_function);
	chain_type (&type_pointer);
	chain_type (&type_floatfield);
	chain_type (&type_quaternion);
	chain_type (&type_integer);
	chain_type (&type_uinteger);
	chain_type (&type_short);
	chain_type (&type_struct);
	chain_type (&type_IMP);

	chain_type (&type_SEL);
	chain_type (&type_Method);
	chain_type (&type_Class);
	chain_type (&type_Protocol);
	chain_type (&type_id);
	chain_type (type_category);
	chain_type (type_ivar);
	chain_type (type_module);
	chain_type (&type_obj_exec_class);
}

void
clear_typedefs (void)
{
	if (typedef_hash)
		Hash_FlushTable (typedef_hash);
}
