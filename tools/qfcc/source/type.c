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

#include <stdlib.h>

#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/dstring.h"

#include "qfcc.h"
#include "function.h"
#include "struct.h"

typedef struct {
	const char *name;
	type_t     *type;
} typedef_t;

// simple types.  function types are dynamically allocated
type_t      type_void = { ev_void };
type_t      type_string = { ev_string };
type_t      type_float = { ev_float };
type_t      type_vector = { ev_vector };
type_t      type_entity = { ev_entity };
type_t      type_field = { ev_field };

// type_function is a void() function used for state defs
type_t      type_function = { ev_func, NULL, &type_void };
type_t      type_pointer = { ev_pointer };
type_t      type_quaternion = { ev_quaternion };
type_t      type_integer = { ev_integer };
type_t      type_uinteger = { ev_uinteger };
type_t      type_short = { ev_short };
type_t      type_struct = { ev_struct };
// these will be built up further
type_t      type_id = { ev_pointer };
type_t      type_Class = { ev_pointer };
type_t      type_Protocol = { ev_pointer };
type_t      type_SEL = { ev_pointer };
type_t      type_IMP = { ev_func, NULL, &type_id, -3, { &type_id, &type_SEL }};
type_t      type_method_list = { ev_pointer };
type_t     *type_method;

type_t      type_floatfield = { ev_field, NULL, &type_float };

def_t       def_void = { &type_void, "temp" };
def_t       def_function = { &type_function, "temp" };

def_t       def_ret, def_parms[MAX_PARMS];

static inline void
chain_type (type_t *type)
{
	type->next = pr.types;
	pr.types = type;
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
			|| check->num_parms != type->num_parms)
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
	check = malloc (sizeof (*check));
	if (!check)
		Sys_Error ("find_type: Memory Allocation Failure\n");
	*check = *type;

	chain_type (check);

	return check;
}

static hashtab_t *typedef_hash;

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
	td = malloc (sizeof (typedef_t));
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
	new.type = ev_pointer;
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
		default:
			printf(" %s", pr_type_name[type->type]);
			break;
	}
}

void
_encode_type (dstring_t *encoding, type_t *type, int level)
{
	struct_field_t *field;

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
			dstring_appendstr (encoding, "?");
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
			//XXX dstring_appendstr (encoding, name);
			dstring_appendstr (encoding, "=");
			for (field = type->struct_head; field; field = field->next)
				_encode_type (encoding, field->type, level + 1);
			dstring_appendstr (encoding, "}");
			break;
		case ev_sel:
			dstring_appendstr (encoding, ":");
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

void
init_types (void)
{
	type_t     *type;

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

	type = type_SEL.aux_type = new_struct ("SEL");
	new_struct_field (type, &type_string, "sel_id", vis_public);
	new_struct_field (type, &type_string, "sel_types", vis_public);
	chain_type (&type_SEL);

	type_method = new_struct ("obj_method");
	new_struct_field (type_method, &type_SEL, "method_name", vis_public);
	new_struct_field (type_method, &type_string, "method_types", vis_public);
	new_struct_field (type_method, &type_IMP, "method_imp", vis_public);
	chain_type (type_method);

	type = type_method_list.aux_type = new_struct ("obj_method_list");
	new_struct_field (type, &type_method_list, "method_next", vis_public);
	new_struct_field (type, &type_integer, "method_count", vis_public);
	new_struct_field (type, array_type (type_method, 1),
					  "method_list", vis_public);
	chain_type (&type_method_list);

	type = type_Class.aux_type = new_struct ("Class");
	new_struct_field (type, &type_Class, "super_class", vis_public);
	new_struct_field (type, &type_Class, "methods", vis_public);
	chain_type (&type_Class);

	type = type_Protocol.aux_type = new_struct ("Protocol");
	chain_type (&type_Protocol);

	type = type_id.aux_type = new_struct ("id");
	new_struct_field (type, &type_Class, "class_pointer", vis_public);
	chain_type (&type_id);
}
