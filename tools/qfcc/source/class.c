/*
	class.c

	QC class support code

	Copyright (C) 2002 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/7

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
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pr_obj.h"
#include "QF/va.h"

#include "qfcc.h"

#include "codespace.h"
#include "class.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "shared.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

static hashtab_t *class_hash;
static hashtab_t *category_hash;
static hashtab_t *protocol_hash;
static hashtab_t *static_instances;
static hashtab_t *static_instance_classes;

// these will be built up further
type_t      type_selector = { ev_invalid, 0, 0, ty_struct};
type_t      type_SEL = { ev_pointer, "SEL", 1, ty_basic, {{&type_selector}}};
type_t     *IMP_params[] = {&type_id, &type_SEL};
type_t      type_IMP = { ev_func, "IMP", 1, ty_basic,
						 {{&type_id, -3, IMP_params}}};
type_t      type_super = { ev_invalid, 0, 0 };
type_t      type_SuperPtr = { ev_pointer, 0, 1, ty_basic, {{&type_super}}};
type_t     *supermsg_params[] = {&type_SuperPtr, &type_SEL};
type_t      type_supermsg = { ev_func, ".supermsg", 1, ty_basic,
							  {{&type_id, -3, supermsg_params}}};
type_t      type_method = { ev_invalid, 0, 0, ty_struct };
type_t      type_method_description = { ev_invalid, 0, 0, ty_struct };
type_t      type_category = { ev_invalid, 0, 0, ty_struct};
type_t      type_ivar = { ev_invalid, 0, 0, ty_struct};
type_t      type_module = { ev_invalid, 0, 0, ty_struct};
type_t      type_moduleptr = { ev_pointer, 0, 1, ty_basic, {{&type_module}}};
type_t     *obj_exec_class_params[] = { &type_moduleptr };
type_t      type_exec_class = { ev_func, 0, 1, ty_basic,
								{{&type_void, 1, obj_exec_class_params}}};
// the cast of 1 in the init is to ensure pointers to incomplete types
// are never misidentified as id. It will be set to the correct value
// when the obj system is initialized.
type_t      type_object = {ev_invalid, 0, 0, ty_struct, {{(type_t *)1}}};
type_t      type_id = { ev_pointer, "id", 1, ty_basic, {{&type_object}}};
type_t      type_class = { ev_invalid, 0, 0, ty_struct};
type_t      type_Class = { ev_pointer, 0, 1, ty_basic, {{&type_class}}};
type_t      type_protocol = { ev_invalid, 0, 0, ty_struct};

int         obj_initialized = 0;

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

static struct_def_t module_struct[] = {
	{"version", &type_integer},
	{"size",    &type_integer},
	{"name",    &type_string},
	{"symtab",  &type_pointer},
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
	{0, 0}
};

static struct_def_t protocol_struct[] = {
	{"class_pointer",    &type_Class},
	{"protocol_name",    &type_string},
	{"protocol_list",    &type_pointer},
	{"instance_methods", &type_pointer},
	{"class_methods",    &type_pointer},
	{0, 0}
};

static struct_def_t object_struct[] = {
	{"class_pointer", &type_Class},
	{0, 0}
};

static const char *
static_instance_get_key (const void *instance, void *unused)
{
	return ((static_instance_t *) instance)->class;
}

static void
add_static_instance (const char *class, def_t *instance_def)
{
	static_instance_t *instance = malloc (sizeof (*instance));

	if (!static_instances) {
		static_instances = Hash_NewTable (1021, static_instance_get_key,
										  0, 0, 0);
		static_instance_classes = Hash_NewTable (1021, static_instance_get_key,
												 0, 0, 0);
	}

	instance->class = save_string (class);
	instance->instance = instance_def;
	Hash_Add (static_instances, instance);

	// uniqued set of class names for all static instances
	if (!Hash_Find (static_instance_classes, class)) {
		Hash_Add (static_instance_classes, instance);
	}
}
typedef struct {
	const char *class_name;
	int         num_instances;
	static_instance_t **instances;
} obj_static_instances_data_t;

static void
emit_instance_classname (def_t *def, void *data, int index)
{
	obj_static_instances_data_t *da = (obj_static_instances_data_t *)data;

	if (!is_string(def->type))
		internal_error (0, "%s: expected string def", __FUNCTION__);
	EMIT_STRING (def->space, D_STRING (def), da->class_name);
}

static void
emit_instance_defs (def_t *def, void *data, int index)
{
	obj_static_instances_data_t *da = (obj_static_instances_data_t *)data;

	if (!is_array (def->type) || def->type->t.array.type->type != ev_pointer)
		internal_error (0, "%s: expected array of pointers def", __FUNCTION__);
	if (index < 0 || index >= da->num_instances + 1)
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index, da->num_instances + 1);
	D_INT (def) = 0;
	if (index < da->num_instances) {
		EMIT_DEF (def->space, D_INT (def), da->instances[index]->instance);
	}
}

static def_t *
emit_static_instances (const char *classname)
{
	static struct_def_t instances_struct[] = {
		{"class_name",	&type_string, emit_instance_classname},
		{"instances",	0,            emit_instance_defs},
		{0, 0}
	};
	obj_static_instances_data_t data = {};
	def_t      *instances_def;

	data.class_name = classname;
	data.instances = (static_instance_t **) Hash_FindList (static_instances,
														   classname);
	for (static_instance_t **inst = data.instances; *inst; inst++) {
		data.num_instances++;
	}
	instances_struct[1].type = array_type (&type_pointer,
										   data.num_instances + 1);
	instances_def = emit_structure (va ("_OBJ_STATIC_INSTANCES_%s", classname),
									's', instances_struct, 0, &data,
									sc_static);
	free (data.instances);
	return instances_def;
}

static def_t *
emit_static_instances_list (void)
{
	static_instance_t **classes;
	int         num_classes = 0;
	def_t     **instance_lists;
	type_t     *instance_lists_type;
	symbol_t   *instance_lists_sym;
	def_t      *instance_lists_def;
	pointer_t  *list;
	defspace_t *space;

	if (!static_instance_classes || !static_instances) {
		return 0;
	}

	classes = (static_instance_t **) Hash_GetList (static_instance_classes);
	for (static_instance_t **c = classes; *c; c++) {
		num_classes++;
	}
	if (!num_classes) {
		free (classes);
		return 0;
	}
	instance_lists = alloca (num_classes * sizeof (*instance_lists));
	for (int i = 0; i < num_classes; i++) {
		instance_lists[i] = emit_static_instances (classes[i]->class);
	}
	free (classes);

	// +1 for terminating null
	instance_lists_type = array_type (&type_pointer, num_classes + 1);
	instance_lists_sym = make_symbol ("_OBJ_STATIC_INSTANCES",
									  instance_lists_type,
									  pr.far_data, sc_static);
	if (!instance_lists_sym->table) {
		symtab_addsymbol (pr.symtab, instance_lists_sym);
	}
	instance_lists_def = instance_lists_sym->s.def;
	instance_lists_def->initialized = instance_lists_def->constant = 1;
	instance_lists_def->nosave = 1;

	list = D_POINTER (pointer_t, instance_lists_def);
	space = instance_lists_def->space;
	for (int i = 0; i < num_classes; i++, list++) {
		EMIT_DEF (space, *list, instance_lists[i]);
	}
	*list = 0;
	return instance_lists_def;
}

int
is_id (const type_t *type)
{
	if (type == &type_id)
		return 1;
	// type may be a qualified id, in which case it will be a pointer to
	// a qualified obj_object struct
	if (type->type != ev_pointer)
		return 0;
	if (!is_struct (type->t.fldptr.type))
		return 0;
	// if the the symtabs match, then type is id in disguise
	if (type->t.fldptr.type->t.symtab == type_object.t.symtab)
		return 1;
	return 0;
}

int
is_class (const type_t *type)
{
	if (type->type == ev_invalid && type->meta == ty_class)
		return 1;
	return 0;
}

int
is_Class (const type_t *type)
{
	if (type == &type_Class)
		return 1;
	return 0;
}

int
is_classptr (const type_t *type)
{
	// easy cases first :)
	if (is_id (type) || is_Class (type))
		return 1;
	if (type->type != ev_pointer)
		return 0;
	type = type->t.fldptr.type;
	if (is_class (type))
		return 1;
	return 0;
}

int
is_SEL (const type_t *type)
{
	return type == &type_SEL;
}

int
is_object (const type_t *type)
{
	return type == &type_object;
}

int
is_method (const type_t *type)
{
	return type == &type_method;
}

int
is_method_description (const type_t *type)
{
	return type == &type_method_description;
}

static protocollist_t *
obj_get_class_protos (const type_t *type)
{
	if (is_pointer (type))
		type = type->t.fldptr.type;
	if (is_class (type))
		return type->t.class->protocols;
	return 0;
}

static protocollist_t *
obj_get_protos (const type_t *type)
{
	if (is_pointer (type))
		type = type->t.fldptr.type;
	return type->protos;
}

static category_t *
obj_get_categories (const type_t *type)
{
	if (is_pointer (type))
		type = type->t.fldptr.type;
	if (is_class (type))
		return type->t.class->categories;
	return 0;
}

static const char *
obj_classname (const type_t *type)
{
	static dstring_t *str;
	protocollist_t *protos;

	if (!str)
		str = dstring_new ();
	dstring_clearstr (str);
	if (is_id (type)) {
		dstring_copystr (str, "id");
	} else if (is_Class (type)) {
		dstring_copystr (str, "Class");
	} else {
		if (is_pointer (type))
			type = type->t.fldptr.type;
		if (is_class (type))
			dstring_copystr (str, type->t.class->name);
	}
	if ((protos = obj_get_protos (type)))
		print_protocollist (str, protos);
	return str->str;
}

static __attribute__((pure)) int
category_implements (category_t *cat, protocol_t *protocol)
{
	for (; cat; cat = cat->next) {
		if (!cat->protocols)
			continue;
		if (procollist_find_protocol (cat->protocols, protocol))
			return 1;
	}
	return 0;
}

int
obj_types_assignable (const type_t *dst, const type_t *src)
{
	class_t    *dst_class, *src_class;
	category_t *cat;
	int         dst_is_proto, src_is_proto;
	protocollist_t *dst_protos = 0, *src_protos = 0;
	int         i;

	//puts ("%$$\"$#%");
	if (!is_classptr (src)) {
		// if dst is a class pointer, then the types are not compatible,
		// otherwise unknown
		return is_classptr (dst) - 1;
	}
	if (!is_classptr (dst)) {
		return -1;
	}

	dst_is_proto = is_id (dst) && (dst_protos = obj_get_protos (dst));
	src_is_proto = is_id (src) && (src_protos = obj_get_protos (src));

	if (dst_is_proto) {
		if (src_is_proto) {
			// id <protos> = id <protos>
			for (i = 0; i < dst_protos->count; i++) {
				if (!procollist_find_protocol (src_protos,
											   dst_protos->list[i])) {
					warning (0, "type %s does not conform to the %s protocol",
							 obj_classname (src), dst_protos->list[i]->name);
					return 1;
				}
			}
		} else if (!is_id (src)) {
			src_protos = obj_get_class_protos (src);
			for (i = 0; i < dst_protos->count; i++) {
				if (procollist_find_protocol (src_protos, dst_protos->list[i]))
					continue;
				cat = obj_get_categories (src);
				if (cat && category_implements (cat, dst_protos->list[i]))
					continue;
				warning (0, "class %s does not implement to the %s protocol",
						 obj_classname (src), dst_protos->list[i]->name);
				return 1;
			}
			return 1;
		}
	} else if (src_is_proto) {
	} else {
	}
	if (is_id (dst) || is_id (src))
		return 1;

	// check dst is a base class of src
	dst_class = dst->t.fldptr.type->t.class;
	src_class = src->t.fldptr.type->t.class;
	//printf ("%s %s\n", dst_class->name, src_class->name);
	while (dst_class != src_class && src_class) {
		src_class = src_class->super_class;
		//if (src_class)
		//	printf ("%s %s\n", dst_class->name, src_class->name);
	}
	if (dst_class == src_class)
		return 1;
	return 0;
}

static const char *
class_get_key (const void *class, void *unused)
{
	return ((class_t *) class)->name;
}

static const char *
protocol_get_key (const void *protocol, void *unused)
{
	return ((protocol_t *) protocol)->name;
}

const char *
get_class_name (class_type_t *class_type, int pretty)
{
	switch (class_type->type) {
		case ct_class:
			if (pretty)
				return class_type->c.class->name;
			else
				return va ("%s_", class_type->c.class->name);
		case ct_category:
			if (pretty)
				return va ("%s (%s)", class_type->c.category->class->name,
						   class_type->c.category->name);
			else
				return va ("%s_%s", class_type->c.category->class->name,
						   class_type->c.category->name);
		case ct_protocol:
			return va ("<%s>", class_type->c.protocol->name);
	}
	return "???";
}

symbol_t *
class_symbol (class_type_t *class_type, int external)
{
	const char *name = 0;
	type_t     *type = 0;
	symbol_t   *sym;

	switch (class_type->type) {
		case ct_category:
			name = va ("_OBJ_CATEGORY_%s_%s",
					   class_type->c.category->class->name,
					   class_type->c.category->name);
			type = &type_category;
			break;
		case ct_class:
			name = va ("_OBJ_CLASS_%s", class_type->c.class->name);
			type = &type_class;
			break;
		case ct_protocol:
			return 0;		// probably in error recovery
	}
	sym = make_symbol (name, type, pr.far_data,
					   external ? sc_extern : sc_global);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return sym;
}

static class_t *
_get_class (symbol_t *sym, int create)
{
	class_t    *c;

	if (!class_hash)
		class_hash = Hash_NewTable (1021, class_get_key, 0, 0, 0);
	if (sym) {
		c = Hash_Find (class_hash, sym->name);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->methods = new_methodlist ();
	c->class_type.type = ct_class;
	c->class_type.c.class = c;
	if (sym) {
		c->name = sym->name;
		Hash_Add (class_hash, c);
		sym->sy_type = sy_class;
	}

	sym = class_symbol (&c->class_type, 1);
	c->def = sym->s.def;

	return c;
}

class_t *
get_class (symbol_t *sym, int create)
{
	class_t    *c;
	type_t      new;

	if (!(c = _get_class (sym, create)))
		return c;

	memset (&new, 0, sizeof (new));
	new.type = ev_invalid;
	new.name = c->name;
	new.meta = ty_class;
	new.t.class = c;
	c->type = find_type (&new);
	if (sym)
		sym->type = c->type;
	return c;
}

static void
set_self_type (class_t *class, method_t *method)
{
	if (method->instance)
		method->params->type = pointer_type (class->type);
	else
		method->params->type = &type_Class;
}

static void
methods_set_self_type (class_t *class, methodlist_t *methods)
{
	method_t   *method;

	for (method = methods->head; method; method = method->next)
		set_self_type (class, method);
}

void
class_add_methods (class_t *class, methodlist_t *methods)
{
	if (!methods)
		return;

	merge_method_lists (class->methods, methods);

	methods_set_self_type (class, class->methods);
}

void
class_add_protocols (class_t *class, protocollist_t *protocols)
{
	int         i;
	protocol_t *p;
	methodlist_t *methods;
	methodset_t *except;
	class_t    *super;

	if (!protocols)
		return;

	methods = class->methods;

	except = new_methodset ();
	for (super = class->super_class; super; super = super->super_class) {
		methodset_add_methods (except, super->methods);
	}

	for (i = 0; i < protocols->count; i++) {
		p = protocols->list[i];
		if (p->methods) {
			copy_methods (methods, p->methods, except);
		} else {
			warning (0, "definition of protocol `%s' not found", p->name);
		}
		if (p->protocols)
			class_add_protocols (class, p->protocols);
	}
	class->protocols = protocols;
}

static void
begin_category (category_t *category)
{
	pr_category_t *pr_category;
	class_t    *class = category->class;
	symbol_t   *sym;
	def_t      *def;
	defspace_t *space;

	current_class = &category->class_type;
	sym = class_symbol (current_class, 0);
	category->def = def = sym->s.def;
	def->initialized = def->constant = def->nosave = 1;
	space = def->space;

	pr_category = &D_STRUCT (pr_category_t, def);
	EMIT_STRING (space, pr_category->category_name, category->name);
	EMIT_STRING (space, pr_category->class_name, class->name);
	EMIT_DEF (space, pr_category->protocols,
			  emit_protocol_list (category->protocols,
								  va ("%s_%s", class->name, category->name)));
}

typedef struct {
	int         count;
	symbol_t   *ivars;
	dstring_t  *encoding;
} ivar_data_t;

static void
emit_ivar_count (def_t *def, void *data, int index)
{
	ivar_data_t *ivar_data = (ivar_data_t *) data;

	if (!is_integer(def->type))
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = ivar_data->count;
}

static void
emit_ivar_list_item (def_t *def, void *data, int index)
{
	ivar_data_t *ivar_data = (ivar_data_t *) data;
	symbol_t   *ivar_sym;
	pr_ivar_t  *ivar;
	defspace_t *space;

#if 0
	//FIXME the type is dynamic, so need a way to pass it before it can be
	//checked
	if (def->type != &XXX)
		internal_error (0, "%s: expected XXX def",
						__FUNCTION__);
#endif
	if (index < 0 || index >= ivar_data->count)
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index, ivar_data->count);

	for (ivar_sym = ivar_data->ivars; ivar_sym; ivar_sym = ivar_sym->next) {
		if (ivar_sym->sy_type != sy_var)
			continue;
		if (!index--)
			break;
	}

	ivar = D_POINTER (pr_ivar_t, def);
	space = def->space;
	dstring_clearstr (ivar_data->encoding);

	EMIT_STRING (space, ivar->ivar_name, ivar_sym->name);
	encode_type (ivar_data->encoding, ivar_sym->type);
	EMIT_STRING (space, ivar->ivar_type, ivar_data->encoding->str);
	ivar->ivar_offset = ivar_sym->s.offset;
}

static def_t *
emit_ivars (symtab_t *ivars, const char *name)
{
	static struct_def_t ivar_list_struct[] = {
		{"ivar_count", &type_integer, emit_ivar_count},
		{"ivar_list",  0,             emit_ivar_list_item},
		{0, 0}
	};
	ivar_data_t ivar_data = {0, 0, 0};
	symbol_t   *s;
	def_t      *def;

	ivar_data.encoding = dstring_newstr ();
	if (ivars) {
		ivar_data.ivars = ivars->symbols;
		for (s = ivars->symbols; s; s = s->next)
			if (s->sy_type == sy_var)
				ivar_data.count++;
	}
	ivar_list_struct[1].type = array_type (&type_ivar, ivar_data.count);

	def = emit_structure (va ("_OBJ_INSTANCE_VARIABLES_%s", name), 's',
						  ivar_list_struct, 0, &ivar_data, sc_static);

	dstring_delete (ivar_data.encoding);
	return def;
}

static void
begin_class (class_t *class)
{
	def_t      *meta_def;
	pr_class_t *meta;
	pr_class_t *pr_class;
	symbol_t   *sym;
	def_t      *def;
	defspace_t *space;

	sym = make_symbol (va ("_OBJ_METACLASS_%s", class->name),
					   &type_class, pr.far_data, sc_static);
	meta_def = sym->s.def;
	meta_def->initialized = meta_def->constant = meta_def->nosave = 1;
	space = meta_def->space;
	meta = &D_STRUCT (pr_class_t, meta_def);
	EMIT_STRING (space, meta->class_pointer, class->name);
	if (class->super_class)
		EMIT_STRING (space, meta->super_class, class->super_class->name);
	EMIT_STRING (space, meta->name, class->name);
	meta->info = _PR_CLS_META;
	meta->instance_size = type_size (&type_class);
	if (!class->super_class) {
		// The ivars list for the meta class struct get emitted only for the
		// root class of the hierachy.
		// NOTE: type_class is not actually a class
		EMIT_DEF (space, meta->ivars,
				  emit_ivars (type_class.t.symtab, "Class"));
	} else {
		meta->ivars = 0;
	}
	current_class = &class->class_type;
	sym = class_symbol (current_class, 0);
	class->def = def = sym->s.def;
	def->initialized = def->constant = def->nosave = 1;
	space = def->space;

	pr_class = &D_STRUCT (pr_class_t, def);
	EMIT_DEF (space, pr_class->class_pointer, meta_def);
	if (class->super_class) {
		class_type_t class_type = {ct_class, {0}};
		class_type.c.class = class->super_class;
		EMIT_STRING (space, pr_class->super_class, class->super_class->name);
		class_symbol (&class_type, 1);
	}
	EMIT_STRING (space, pr_class->name, class->name);
	pr_class->info = _PR_CLS_CLASS;
	EMIT_DEF (space, pr_class->protocols,
			  emit_protocol_list (class->protocols, class->name));
}

void
class_begin (class_type_t *class_type)
{
	if (current_class) {
		warning (0, "‘@end’ missing in implementation context");
		class_finish (current_class);
		current_class = 0;
	}
	switch (class_type->type) {
		case ct_category:
			begin_category (class_type->c.category);
			break;
		case ct_class:
			begin_class (class_type->c.class);
			break;
		case ct_protocol:
			return;			// probably error recovery
	}
}

void
emit_class_ref (const char *class_name)
{
	symbol_t   *ref_sym;
	symbol_t   *name_sym;
	def_t      *ref_def;
	def_t      *name_def;

	ref_sym = make_symbol (va (".obj_class_ref_%s", class_name), &type_pointer,
						   pr.far_data, sc_static);
	if (!ref_sym->table)
		symtab_addsymbol (pr.symtab, ref_sym);
	ref_def = ref_sym->s.def;
	if (ref_def->initialized)
		return;
	ref_def->initialized = ref_def->constant = ref_def->nosave = 1;
	name_sym = make_symbol (va (".obj_class_name_%s", class_name),
							&type_pointer, pr.far_data, sc_extern);
	if (!name_sym->table)
		symtab_addsymbol (pr.symtab, name_sym);
	name_def = name_sym->s.def;
	if (!name_def->external)
		D_INT (ref_def) = name_def->offset;
	reloc_def_def (name_def, ref_def);
}

static void
emit_class_name (const char *class_name)
{
	symbol_t   *name_sym;
	def_t      *name_def;

	name_sym = make_symbol (va (".obj_class_name_%s", class_name),
							&type_pointer, pr.far_data, sc_global);
	if (!name_sym->table)
		symtab_addsymbol (pr.symtab, name_sym);
	name_def = name_sym->s.def;
	if (name_def->initialized)
		return;
	name_def->initialized = name_def->constant = 1;
	name_def->nosave = 1;
	D_INT (name_def) = 0;
}

void
emit_category_ref (const char *class_name, const char *category_name)
{
	symbol_t   *ref_sym;
	symbol_t   *name_sym;
	def_t      *ref_def;
	def_t      *name_def;

	ref_sym = make_symbol (va (".obj_category_ref_%s_%s",
						   class_name, category_name),
					   &type_pointer, pr.far_data, sc_static);
	if (!ref_sym->table)
		symtab_addsymbol (pr.symtab, ref_sym);
	ref_def = ref_sym->s.def;
	if (ref_def->initialized)
		return;
	ref_def->initialized = ref_def->constant = 1;
	ref_def->nosave = 1;
	name_sym = make_symbol (va (".obj_category_name_%s_%s",
						   class_name, category_name),
					   &type_pointer, pr.far_data, sc_extern);
	if (!name_sym->table)
		symtab_addsymbol (pr.symtab, name_sym);
	name_def = name_sym->s.def;
	if (!name_def->external)
		D_INT (ref_def) = name_def->offset;
	reloc_def_def (name_def, ref_def);
}

static void
emit_category_name (const char *class_name, const char *category_name)
{
	symbol_t   *name_sym;
	def_t      *name_def;

	name_sym = make_symbol (va (".obj_category_name_%s_%s",
							    class_name, category_name),
							&type_pointer, pr.far_data, sc_global);
	if (!name_sym->table)
		symtab_addsymbol (pr.symtab, name_sym);
	name_def = name_sym->s.def;
	if (name_def->initialized)
		return;
	name_def->initialized = name_def->constant = 1;
	name_def->nosave = 1;
	D_INT (name_def) = 0;
}

static void
finish_category (category_t *category)
{
	pr_category_t *pr_category;
	class_t    *class = category->class;
	char       *name;
	defspace_t *space;

	if (!category->def)	// probably in error recovery
		return;
	name = nva ("%s_%s", class->name, category->name);
	pr_category = &D_STRUCT (pr_category_t, category->def);
	space = category->def->space;
	EMIT_DEF (space, pr_category->instance_methods,
			emit_methods (category->methods, name, 1));
	EMIT_DEF (space, pr_category->class_methods,
			emit_methods (category->methods, name, 0));
	free (name);
	emit_class_ref (class->name);
	emit_category_name (class->name, category->name);
}

static void
finish_class (class_t *class)
{
	pr_class_t *meta;
	pr_class_t *cls;
	defspace_t *space;

	if (pr.error_count)	// probably in error recovery
		return;
	space = class->def->space;
	cls = &D_STRUCT (pr_class_t, class->def);

	meta = &G_STRUCT (space, pr_class_t, cls->class_pointer);

	EMIT_DEF (space, meta->methods, emit_methods (class->methods,
												  class->name, 0));

	cls->instance_size = type_size (class->type);
	EMIT_DEF (space, cls->ivars, emit_ivars (class->ivars, class->name));
	EMIT_DEF (space, cls->methods, emit_methods (class->methods,
												 class->name, 1));
	if (class->super_class)
		emit_class_ref (class->super_class->name);
	emit_class_name (class->name);
}

void
class_finish (class_type_t *class_type)
{
	switch (class_type->type) {
		case ct_category:
			finish_category (class_type->c.category);
			break;
		case ct_class:
			finish_class (class_type->c.class);
			break;
		case ct_protocol:
			return;			// probably in error recovery
	}
}

class_t *
extract_class (class_type_t *class_type)
{
	switch (class_type->type) {
		case ct_class:
			return current_class->c.class;
		case ct_category:
			return current_class->c.category->class;
		case ct_protocol:
			return 0;		// probably in error recovery
	}
	return 0;				// should not happen
}

int
class_access (class_type_t *class_type, class_t *class)
{
	class_t    *cur;
	if (class_type) {
		if (!(cur = extract_class (class_type)))
			return vis_private;
		if (cur == class)
			return vis_private;
		cur = cur->super_class;
		while (cur) {
			if (cur == class)
				return vis_protected;
			cur = cur->super_class;
		}
	}
	return vis_public;
}

symbol_t *
class_find_ivar (class_t *class, int vis, const char *name)
{
	symbol_t   *ivar;

	if (!class->ivars) {
		if (!class->interface_declared) {
			class->interface_declared = 1;
			error (0, "accessing incomplete type %s", class->name);
		}
		return 0;
	}
	ivar = symtab_lookup (class->ivars, name);
	if (ivar) {
		if (ivar->visibility > (vis_t) vis
			|| (ivar->table->class != class
				&& ivar->visibility > vis_protected)) {
			goto access_error;
		}
		return ivar;
	}
	error (0, "%s.%s does not exist", class->name, name);
	return 0;
access_error:
	error (0, "%s.%s is not accessable here", class->name, name);
	return 0;
}

method_t *
class_find_method (class_type_t *class_type, method_t *method)
{
	methodlist_t *methods = 0, *start_methods;
	method_t   *m;
	dstring_t  *sel;
	class_t    *class = 0, *start_class;

	switch (class_type->type) {
		case ct_category:
			methods = class_type->c.category->methods;
			class = class_type->c.category->class;
			break;
		case ct_class:
			class = class_type->c.class;
			methods = class->methods;
			break;
		case ct_protocol:
			return 0;		// probably in error recovery
	}
	start_methods = methods;
	start_class = class;
	while (class) {
		for (m = methods->head; m; m = m->next) {
			if (method_compare (method, m)) {
				if (m->type != method->type)
					error (0, "method type mismatch");
				if (methods != start_methods) {
					m = copy_method (m);
					set_self_type (start_class, m);
					add_method (start_methods, m);
				}
				method_set_param_names (m, method);
				return m;
			}
		}
		if (class->methods == methods)
			class = class->super_class;
		else
			methods = class->methods;
	}
	sel = dstring_newstr ();
	selector_name (sel, (keywordarg_t *)method->selector);
	set_self_type (start_class, method);
	add_method (start_methods, method);
	dstring_delete (sel);
	return method;
}

static method_t *
cls_find_method (methodlist_t *methodlist, selector_t *selector,
				   int class_msg, int is_root)
{
	method_t   *m = 0;
	m = methodlist_find_method (methodlist, selector, !class_msg);
	if (!m && is_root && class_msg
		&& (m = methodlist_find_method (methodlist, selector, 1))) {
		return m;
	}
	return m;
}

method_t *
class_message_response (type_t *clstype, int class_msg, expr_t *sel)
{
	selector_t *selector;
	method_t   *m;
	class_t    *c;
	class_t    *class = 0;
	category_t *cat;
	dstring_t  *dstr;

	selector = get_selector (sel);
	if (!selector)
		return 0;

	if (!is_classptr (clstype) && !is_class (clstype)) {
		error (0, "neither class nor object");
		return 0;
	}
	if (is_id (clstype)) {
		protocollist_t *protos = clstype->t.fldptr.type->protos;
		if (protos) {
			if ((m = protocollist_find_method (protos, selector, !class_msg))) {
				return m;
			}
			dstr = dstring_new ();
			print_protocollist (dstr, protos);
			warning (sel, "id%s may not respond to %c%s", dstr->str,
					 class_msg ? '+' : '-', selector->name);
			dstring_delete (dstr);
		}
	} else {
		if (is_class (clstype)) {
			class = clstype->t.class;
		} else if (is_class (clstype->t.fldptr.type)) {
			class = clstype->t.fldptr.type->t.class;
		}
		if (class && !is_object(class->type)) {
			if (!class->interface_declared) {
				class->interface_declared = 1;
				warning (0, "cannot find interface declaration for `%s'",
						 class->name);
			}
			c = class;
			while (c) {
				for (cat = c->categories; cat; cat = cat->next) {
					if ((m = cls_find_method (cat->methods, selector,
											  class_msg,
											  !c->super_class))) {
						return m;
					}
				}
				if ((m = cls_find_method (c->methods, selector, class_msg,
										  !c->super_class))) {
					return m;
				}
				c = c->super_class;
			}
			warning (sel, "%s may not respond to %c%s", class->name,
					 class_msg ? '+' : '-', selector->name);
		}
	}
	m = find_method (selector->name);
	if (!m && (!class || is_object(class->type))) {
		warning (sel, "could not find method for %c%s",
				 class_msg ? '+' : '-', selector->name);
	}
	return m;
}

static uintptr_t
category_get_hash (const void *_c, void *unused)
{
	category_t *c = (category_t *) _c;
	return Hash_String (c->name) ^ Hash_String (c->class->name);
}

static int
category_compare (const void *_c1, const void *_c2, void *unused)
{
	category_t *c1 = (category_t *) _c1;
	category_t *c2 = (category_t *) _c2;
	return strcmp (c1->name, c2->name) == 0
		&& strcmp (c1->class->name, c2->class->name) == 0;
}

symtab_t *
class_new_ivars (class_t *class)
{
	symtab_t   *ivars;
	symtab_t   *super_ivars = 0;

	if (class->super_class)
		super_ivars = class->super_class->ivars;
	ivars = new_symtab (super_ivars, stab_local);
	ivars->class = class;
	return ivars;
}

void
class_add_ivars (class_t *class, symtab_t *ivars)
{
	int         base = 0;
	symbol_t   *sym;

	if (class->super_class)
		base = type_size (class->super_class->type);
	for (sym = ivars->symbols; sym; sym = sym->next)
		sym->s.offset += base;
	class->ivars = ivars;
}

static int
compare_symbols (symbol_t *s1, symbol_t *s2)
{
	if (s1->visibility != s2->visibility)
		return 0;
	if (s1->name != s2->name)	// works thanks to save_string
		return 0;
	if (s1->sy_type != s2->sy_type)
		return 0;
	if (s1->type != s2->type)
		return 0;
	return 1;
}

void
class_check_ivars (class_t *class, symtab_t *ivars)
{
	symbol_t   *civ, *iv;
	int         missmatch = 0;

	if (!class->ivars != !ivars) {
		missmatch = 1;
	} else if (ivars) {
		for (civ = class->ivars->symbols, iv = ivars->symbols;
			 civ && iv; civ = civ->next, iv = iv->next) {
			if (!compare_symbols (civ, iv)) {
				missmatch = 1;
				break;
			}
		}
	}
	//FIXME right option?
	if (missmatch && options.warnings.interface_check)
		warning (0, "instance variable missmatch for %s", class->name);
	class_add_ivars (class, ivars);
}

category_t *
get_category (symbol_t *class_name, const char *category_name, int create)
{
	category_t *category;
	class_t    *class;

	if (!category_hash) {
		category_hash = Hash_NewTable (1021, 0, 0, 0, 0);
		Hash_SetHashCompare (category_hash,
				category_get_hash, category_compare);
	}
	class = get_class (class_name, 0);
	if (!class) {
		error (0, "undefined class %s", class_name->name);
		return 0;
	}
	if (class_name && category_name) {
		category_t  _c = {0, category_name, class};

		category = Hash_FindElement (category_hash, &_c);
		if (category || !create)
			return category;
	}

	category = calloc (sizeof (category_t), 1);
	category->next = class->categories;
	class->categories = category;
	category->name = category_name;
	category->class = class;
	category->methods = new_methodlist ();
	category->class_type.type = ct_category;
	category->class_type.c.category = category;
	if (class_name && category_name)
		Hash_AddElement (category_hash, category);
	return category;
}

void
category_add_methods (category_t *category, methodlist_t *methods)
{
	if (!methods)
		return;
	merge_method_lists (category->methods, methods);

	methods_set_self_type (category->class, category->methods);
}

void
category_add_protocols (category_t *category, protocollist_t *protocols)
{
	int         i;
	protocol_t *p;
	methodlist_t *methods;
	methodset_t *except;
	class_t    *class;

	if (!protocols)
		return;

	methods = category->methods;

	except = new_methodset ();
	for (class = category->class; class; class = class->super_class) {
		methodset_add_methods (except, class->methods);
	}

	for (i = 0; i < protocols->count; i++) {
		p = protocols->list[i];
		copy_methods (methods, p->methods, except);
		if (p->protocols)
			category_add_protocols (category, p->protocols);
	}
	category->protocols = protocols;
}

symbol_t *
class_pointer_symbol (class_t *class)
{
	def_t      *def;
	symbol_t   *sym;
	class_type_t class_type = {ct_class, {0}};

	class_type.c.class = class;

	sym = make_symbol (va ("_OBJ_CLASS_POINTER_%s", class->name),
					   &type_Class,
					   pr.near_data, sc_static);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	def = sym->s.def;
	if (def->initialized)
		return sym;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	if (!class->def)
		class->def = class_symbol (&class_type, 1)->s.def;
	if (!class->def->external)
		D_INT (def) = class->def->offset;
	reloc_def_def (class->def, def);
	return sym;
}


typedef struct {
	def_t      *refs;
	class_t   **classes;
	int         cls_def_cnt;
	category_t **categories;
	int         cat_def_cnt;
	def_t      *instances_list;
} obj_symtab_data_t;

static void
emit_symtab_ref_cnt (def_t *def, void *data, int index)
{
	obj_symtab_data_t *da = (obj_symtab_data_t *)data;

	if (!is_integer(def->type))
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = 0;
	if (da->refs)
		D_INT (def) = da->refs->type->t.array.size;
}

static void
emit_symtab_refs (def_t *def, void *data, int index)
{
	obj_symtab_data_t *da = (obj_symtab_data_t *)data;

	if (!is_SEL(def->type))
		internal_error (0, "%s: expected SEL def", __FUNCTION__);
	D_INT (def) = 0;
	if (da->refs)
		EMIT_DEF (def->space, D_INT (def), da->refs);
}

static void
emit_symtab_cls_def_cnt (def_t *def, void *data, int index)
{
	obj_symtab_data_t *da = (obj_symtab_data_t *)data;

	if (!is_integer(def->type))
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = da->cls_def_cnt;
}

static void
emit_symtab_cat_def_cnt (def_t *def, void *data, int index)
{
	obj_symtab_data_t *da = (obj_symtab_data_t *)data;

	if (!is_integer(def->type))
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = da->cat_def_cnt;
}

static void
emit_symtab_defs (def_t *def, void *data, int index)
{
	obj_symtab_data_t *da = (obj_symtab_data_t *)data;

	if (!is_array (def->type) || def->type->t.array.type->type != ev_pointer)
		internal_error (0, "%s: expected array of pointers def", __FUNCTION__);
	if (index < 0 || index >= da->cls_def_cnt + da->cat_def_cnt + 1)
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index,
						da->cls_def_cnt + da->cat_def_cnt + 1);

	if (index < da->cls_def_cnt) {
		class_t   **cl;
		for (cl = da->classes; *cl; cl++)
			if ((*cl)->def && !(*cl)->def->external)
				if (!index--)
					break;
		EMIT_DEF (def->space, D_INT (def), (*cl)->def);
	} else if (index < da->cls_def_cnt + da->cat_def_cnt) {
		category_t **ca;
		index -= da->cls_def_cnt;
		for (ca = da->categories; *ca; ca++)
			if ((*ca)->def && !(*ca)->def->external)
				if (!index--)
					break;
		EMIT_DEF (def->space, D_INT (def), (*ca)->def);
	} else {
		D_INT (def) = 0;
		if (da->instances_list) {
			EMIT_DEF (def->space, D_INT (def), da->instances_list);
		}
	}
}

void
class_finish_module (void)
{
	static struct_def_t symtab_struct[] = {
		{"sel_ref_cnt",	&type_integer, emit_symtab_ref_cnt},
		{"refs",		&type_SEL,     emit_symtab_refs},
		{"cls_def_cnt",	&type_integer, emit_symtab_cls_def_cnt},
		{"cat_def_cnt",	&type_integer, emit_symtab_cat_def_cnt},
		{"defs",		0,             emit_symtab_defs},
		{0, 0}
	};

	obj_symtab_data_t data = {};

	class_t   **cl;
	category_t **ca;
	def_t      *symtab_def;
	symbol_t   *module_sym;
	expr_t     *module_expr;
	pr_module_t *module;
	symbol_t   *exec_class_sym;
	symbol_t   *init_sym;
	expr_t     *init_expr;
	storage_class_t save_storage;

	data.refs = emit_selectors ();
	if (class_hash) {
		data.classes = (class_t **) Hash_GetList (class_hash);
		for (cl = data.classes; *cl; cl++)
			if ((*cl)->def && !(*cl)->def->external)
				data.cls_def_cnt++;
	}
	if (category_hash) {
		data.categories = (category_t **) Hash_GetList (category_hash);
		for (ca = data.categories; *ca; ca++)
			if ((*ca)->def && !(*ca)->def->external)
				data.cat_def_cnt++;
	}
	data.instances_list = emit_static_instances_list ();
	if (!data.refs && !data.cls_def_cnt && !data.cat_def_cnt
		&& !data.instances_list)
		return;
	symtab_struct[4].type = array_type (&type_pointer,
										data.cls_def_cnt
										+ data.cat_def_cnt
										+ 1);
	symtab_def = emit_structure ("_OBJ_SYMTAB", 's', symtab_struct, 0, &data,
								 sc_static);
	free (data.classes);
	free (data.categories);

	module_sym = make_symbol ("_OBJ_MODULE", &type_module, pr.far_data,
							  sc_static);
	symtab_addsymbol (current_symtab, module_sym);
	module = &D_STRUCT (pr_module_t, module_sym->s.def);
	module->size = type_size (&type_module);
	EMIT_STRING (module_sym->s.def->space, module->name,
				 GETSTR (pr.source_file));
	EMIT_DEF (module_sym->s.def->space, module->symtab, symtab_def);

	exec_class_sym = symtab_lookup (pr.symtab, "__obj_exec_class");
	if (!exec_class_sym) {
		exec_class_sym = new_symbol_type ("__obj_exec_class",
										  &type_exec_class);
		exec_class_sym = function_symbol (exec_class_sym, 0, 1);
		make_function (exec_class_sym, 0, exec_class_sym->table->space,
					   sc_extern);
	}

	init_sym = new_symbol_type (".ctor", &type_function);
	init_sym = function_symbol (init_sym, 0, 1);

	module_expr = address_expr (new_symbol_expr (module_sym), 0, 0);

	init_expr = new_block_expr ();
	append_expr (init_expr,
				 build_function_call (new_symbol_expr (exec_class_sym),
									  exec_class_sym->type, module_expr));

	save_storage = current_storage;
	current_storage = sc_static;
	current_func = begin_function (init_sym, 0, current_symtab, 1);
	build_code_function (init_sym, 0, init_expr);;
	current_func = 0;
	current_storage = save_storage;
}

protocol_t *
get_protocol (const char *name, int create)
{
	protocol_t    *p;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0, 0);

	if (name) {
		p = Hash_Find (protocol_hash, name);
		if (p || !create)
			return p;
	}

	p = calloc (sizeof (protocol_t), 1);
	p->name = name;
	p->methods = 0;
	p->class_type.type = ct_protocol;
	p->class_type.c.protocol = p;
	if (name)
		Hash_Add (protocol_hash, p);
	return p;
}

void
protocol_add_methods (protocol_t *protocol, methodlist_t *methods)
{
	if (!methods)
		return;
	merge_method_lists (protocol->methods, methods);
}

void
protocol_add_protocols (protocol_t *protocol, protocollist_t *protocols)
{
	protocol->protocols = protocols;
}

def_t *
protocol_def (protocol_t *protocol)
{
	if (!protocol->def) {
		protocol->def = emit_protocol (protocol);
		add_static_instance ("Protocol", protocol->def);
	}
	return protocol->def;
}

protocollist_t *
new_protocol_list (void)
{
	protocollist_t *protocollist = malloc (sizeof (protocollist_t));

	protocollist->count = 0;
	protocollist->list = 0;
	return protocollist;
}

protocollist_t *
add_protocol (protocollist_t *protocollist, const char *name)
{
	protocol_t *protocol = get_protocol (name, 0);

	if (!protocol) {
		error (0, "undefined protocol `%s'", name);
		return protocollist;
	}
	protocollist->count++;
	protocollist->list = realloc (protocollist->list,
								  sizeof (protocol_t *) * protocollist->count);
	protocollist->list[protocollist->count - 1] = protocol;
	return protocollist;
}

int
procollist_find_protocol (protocollist_t *protocollist, protocol_t *proto)
{
	int         i;
	if (!protocollist)
		return 0;
	for (i = 0; i < protocollist->count; i++)
		if (protocollist->list[i] == proto)
			return 1;
	return 0;
}

static method_t *
protocol_find_method (protocol_t *protocol, selector_t *selector, int instance)
{
	method_t   *m = 0;
	if (protocol->methods) {
		m = methodlist_find_method (protocol->methods, selector, instance);
	}
	if (!m && protocol->protocols) {
		return protocollist_find_method (protocol->protocols, selector,
										 instance);
	}
	return m;
}

method_t *
protocollist_find_method (protocollist_t *protocollist, selector_t *selector,
						  int instance)
{
	method_t   *m;
	for (int i = 0; i < protocollist->count; i++) {
		if ((m = protocol_find_method (protocollist->list[i], selector,
									   instance))) {
			return m;
		}
	}
	return 0;
}

int
compare_protocols (protocollist_t *protos1, protocollist_t *protos2)
{
	int         i;

	if (protos1 == protos2)
		return 1;
	if (!protos1 || !protos2)
		return 0;
	if (protos1->count != protos2->count)
		return 0;
	for (i = 0; i < protos1->count; i++)
		if (!procollist_find_protocol (protos2, protos1->list[i]))
			return 0;
	return 1;
}

void
print_protocollist (dstring_t *dstr, protocollist_t *protocollist)
{
	int         i;
	dstring_appendstr (dstr, "<");
	for (i = 0; i < protocollist->count; i++)
		dasprintf (dstr, "%s%s", i ? "," : "", protocollist->list[i]->name);
	dstring_appendstr (dstr, ">");
}

def_t *
emit_protocol (protocol_t *protocol)
{
	def_t      *proto_def;
	pr_protocol_t *proto;
	defspace_t *space;

	proto_def = make_symbol (va ("_OBJ_PROTOCOL_%s", protocol->name),
							 &type_protocol, pr.far_data, sc_static)->s.def;
	if (proto_def->initialized)
		return proto_def;
	proto_def->initialized = proto_def->constant = 1;
	proto_def->nosave = 1;
	space = proto_def->space;
	proto = &D_STRUCT (pr_protocol_t, proto_def);
	proto->class_pointer = 0;
	EMIT_STRING (space, proto->protocol_name, protocol->name);
	EMIT_DEF (space, proto->protocol_list,
			  emit_protocol_list (protocol->protocols,
								  va ("PROTOCOL_%s", protocol->name)));
	EMIT_DEF (space, proto->instance_methods,
			  emit_method_descriptions (protocol->methods, protocol->name, 1));
	EMIT_DEF (space, proto->class_methods,
			  emit_method_descriptions (protocol->methods, protocol->name, 0));
	emit_class_ref ("Protocol");
	return proto_def;
}

static void
emit_protocol_next (def_t *def, void *data, int index)
{
	if (!is_pointer(def->type)) {
		internal_error (0, "%s: expected pointer def", __FUNCTION__);
	}
	D_INT (def) = 0;
}

static void
emit_protocol_count (def_t *def, void *data, int index)
{
	protocollist_t *protocols = (protocollist_t *) data;

	if (!is_integer(def->type)) {
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	}
	D_INT (def) = protocols->count;
}

static void
emit_protocol_list_item (def_t *def, void *data, int index)
{
	protocollist_t *protocols = (protocollist_t *) data;
	protocol_t *protocol = protocols->list[index];

	if (!is_array (def->type) || !is_pointer(def->type->t.array.type)) {
		internal_error (0, "%s: expected array of pointer def", __FUNCTION__);
	}
	if (index < 0 || index >= protocols->count) {
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index, protocols->count);
	}
	EMIT_DEF (def->space, D_INT(def), protocol_def (protocol));
}

def_t *
emit_protocol_list (protocollist_t *protocols, const char *name)
{
	static struct_def_t proto_list_struct[] = {
		{"next",  &type_pointer, emit_protocol_next},
		{"count", &type_integer, emit_protocol_count},
		{"list",  0,             emit_protocol_list_item},
		{0, 0},
	};

	if (!protocols)
		return 0;
	proto_list_struct[2].type = array_type (&type_pointer, protocols->count);
	return emit_structure (va ("_OBJ_PROTOCOLS_%s", name), 's',
						   proto_list_struct, 0, protocols, sc_static);
}

void
clear_classes (void)
{
	if (class_hash)
		Hash_FlushTable (class_hash);
	if (protocol_hash)
		Hash_FlushTable (protocol_hash);
	if (category_hash)
		Hash_FlushTable (category_hash);
	if (static_instances)
		Hash_FlushTable (static_instances);
	if (static_instance_classes)
		Hash_FlushTable (static_instance_classes);
	obj_initialized = 0;
}

symtab_t *
class_to_struct (class_t *class, symtab_t *symtab)
{
	symtab_t   *parent = symtab->parent;
	symtab_t   *ivars = class->ivars;
	symtab_t   *ancestor;

	if (!ivars)
		return symtab;

	// disconnect the struct symbol table from the scope
	symtab->parent = 0;
	// find the ancestor of the ivars symbol table chain
	for (ancestor = ivars; ancestor->parent; ancestor = ancestor->parent)
		;
	// connect the ivars symbol table chain to the struct symbol table
	ancestor->parent = symtab;
	// create a new struct symbol table from the ivars symbol table chain
	symtab = symtab_flat_copy (ivars, 0);
	// disconnect the ivars symbol table chain
	ancestor->parent = 0;
	// connect the new struct symbol table to the scope
	symtab->parent = parent;
	return symtab;
}

symtab_t *
class_ivar_scope (class_type_t *class_type, symtab_t *parent)
{
	class_t    *class = extract_class (class_type);
	if (!class->ivars)
		return 0;
	return symtab_flat_copy (class->ivars, parent);
}

static expr_t *
class_dereference_ivar (symbol_t *sym, void *_self)
{
	expr_t     *self = (expr_t *) _self;

	return field_expr (copy_expr (self),
					   new_symbol_expr (new_symbol (sym->name)));
}

void
class_finish_ivar_scope (class_type_t *class_type, symtab_t *ivar_scope,
						 symtab_t *param_scope)
{
	class_t    *class = extract_class (class_type);
	type_t     *class_ptr = pointer_type (class->type);
	symbol_t   *sym;
	symbol_t   *self;
	expr_t     *self_expr;

	if (!ivar_scope)
		return;
	self = symtab_lookup (param_scope, "self");
	if (!self) {
		internal_error (0, "I've lost my self!");
	}
	self_expr = new_symbol_expr (self);
	if (self->type != class_ptr) {
		debug (0, "class method scope");
		//FIXME should generate a warning on access
		self_expr = cast_expr (class_ptr, self_expr);
	}
	for (sym = ivar_scope->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy_var)
			continue;
		sym->sy_type = sy_convert;
		sym->s.convert.conv = class_dereference_ivar;
		sym->s.convert.data = self_expr;
	}
}

static void
init_objective_structs (void)
{
	make_structure ("obj_selector", 's', sel_struct, &type_selector);
	chain_type (&type_selector);
	chain_type (&type_SEL);
	chain_type (&type_IMP);

	make_structure ("obj_method", 's', method_struct, &type_method);
	chain_type (&type_method);

	make_structure ("obj_method_description", 's', method_desc_struct,
					&type_method_description);
	chain_type (&type_method_description);

	make_structure ("obj_category", 's', category_struct, &type_category);
	chain_type (&type_category);

	make_structure ("obj_ivar", 's', ivar_struct, &type_ivar);
	chain_type (&type_ivar);

	make_structure ("obj_super", 's', super_struct, &type_super);
	chain_type (&type_super);
	chain_type (&type_SuperPtr);
	chain_type (&type_supermsg);

	symtab_addsymbol (current_symtab, new_symbol ("id"));
}

static void
init_classes (void)
{
	make_structure ("obj_class", 's', class_struct, &type_class);
	chain_type (&type_class);
	chain_type (&type_Class);
	make_structure ("obj_object", 's', object_struct, &type_object);
	chain_type (&type_object);
	chain_type (&type_id);
	make_structure ("obj_protocol", 's', protocol_struct, &type_protocol);
	chain_type (&type_protocol);
}

static void
class_init_obj_module (void)
{
	make_structure ("obj_module", 's', module_struct, &type_module);

	chain_type (&type_module);
	chain_type (&type_moduleptr);
	chain_type (&type_exec_class);
}

void
class_init (void)
{
	symtab_t   *cs = current_symtab;

	current_symtab = pr.symtab;
	class_init_obj_module ();
	init_classes ();
	init_objective_structs ();
	obj_initialized = 1;

	current_symtab = cs;
}
