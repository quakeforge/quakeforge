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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

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

#include "class.h"
#include "def.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "type.h"

static hashtab_t *class_hash;
static hashtab_t *category_hash;
static hashtab_t *protocol_hash;

class_t     class_id = {1, "id", 0, 0, 0, 0, 0, 0, &type_id};
class_t     class_Class = {1, "Class", 0, 0, 0, 0, 0, 0, &type_Class};
class_t     class_Protocol = {1, "Protocl", 0, 0, 0, 0, 0, 0, &type_Protocol};

static const char *
class_get_key (void *class, void *unused)
{
	return ((class_t *) class)->name;
}

static const char *
protocol_get_key (void *protocol, void *unused)
{
	return ((protocol_t *) protocol)->name;
}

void
class_init (void)
{
	class_Class.super_class = get_class ("Object", 1);
}

def_t *
class_def (class_type_t *class_type, int external)
{
	const char *name;
	type_t     *type;
	storage_class_t storage = external ? st_extern : st_global;

	if (!class_type->is_class) {
		name = va ("_OBJ_CATEGORY_%s_%s", class_type->c.category->class->name,
				   class_type->c.category->name);
		type = type_category;
	} else {
		name = va ("_OBJ_CLASS_%s", class_type->c.class->name);
		type = type_Class.aux_type;
	}
	return get_def (type, name, pr.scope, storage);
}

class_t *
get_class (const char *name, int create)
{
	class_t    *c;
	type_t      new;

	if (!class_hash)
		class_hash = Hash_NewTable (1021, class_get_key, 0, 0);
	if (name) {
		c = Hash_Find (class_hash, name);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->name = name;
	new = *type_Class.aux_type;
	new.class = c;
	c->type = pointer_type (find_type (&new));
	c->class_type.is_class = 1;
	c->class_type.c.class = c;
	if (name)
		Hash_Add (class_hash, c);
	return c;
}

void
class_add_methods (class_t *class, methodlist_t *methods)
{
	if (!methods)
		return;

	if (!class->methods)
		class->methods = new_methodlist ();
	*class->methods->tail = methods->head;
	class->methods->tail = methods->tail;
	free (methods);
}

void
class_add_protocol_methods (class_t *class, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0);

	if (!class->methods)
		class->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		methodlist_t *methods = class->methods;
		method_t   **m = methods->tail;

		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (methods, p->methods);
		while (*m) {
			(*m)->params->type = class->type;
			m = &(*m)->next;
		}
	}
}

void
class_begin (class_type_t *class_type)
{
	if (!class_type->is_class) {
		pr_category_t *pr_category;
		category_t *category = class_type->c.category;
		class_t    *class = category->class;

		current_class = &category->class_type;
		category->def = class_def (class_type, 0);
		category->def->initialized = category->def->constant = 1;
		category->def->nosave = 1;
		pr_category = &G_STRUCT (pr_category_t, category->def->ofs);
		EMIT_STRING (pr_category->category_name, category->name);
		EMIT_STRING (pr_category->class_name, class->name);
		EMIT_DEF (pr_category->protocols,
				  emit_protocol_list (category->protocols,
									  va ("%s_%s",
										  class->name,
										  category->name)));
	} else {
		def_t      *meta_def;
		pr_class_t *meta;
		pr_class_t *cls;
		class_t    *class = class_type->c.class;
		
		current_class = &class->class_type;
		class->def = class_def (class_type, 0);
		meta_def = get_def (type_Class.aux_type,
							  va ("_OBJ_METACLASS_%s", class->name),
							  pr.scope, st_static);
		meta_def->initialized = meta_def->constant = 1;
		meta_def->nosave = 1;
		meta = &G_STRUCT (pr_class_t, meta_def->ofs);
		EMIT_STRING (meta->class_pointer, class->name);
		if (class->super_class)
			EMIT_STRING (meta->super_class, class->super_class->name);
		EMIT_STRING (meta->name, class->name);
		meta->info = _PR_CLS_META;
		meta->instance_size = type_size (type_Class.aux_type);
		EMIT_DEF (meta->ivars, emit_struct (type_Class.aux_type, "Class"));
		EMIT_DEF (meta->protocols, emit_protocol_list (class->protocols,
													   class->name));

		class->def->initialized = class->def->constant = 1;
		class->def->nosave = 1;
		cls = &G_STRUCT (pr_class_t, class->def->ofs);
		EMIT_DEF (cls->class_pointer, meta_def);
		if (class->super_class) {
			class_type_t class_type = {1, {0}};
			class_type.c.class = class->super_class;
			EMIT_STRING (cls->super_class, class->super_class->name);
			class_def (&class_type, 1);
		}
		EMIT_STRING (cls->name, class->name);
		cls->info = _PR_CLS_CLASS;
		cls->protocols = meta->protocols;
	}
}

static void
emit_class_ref (const char *class_name)
{
	def_t      *def;
	def_t      *ref;

	def = get_def (&type_pointer, va (".obj_class_ref_%s", class_name),
				   pr.scope, st_static);
	if (def->initialized)
		return;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	ref = get_def (&type_integer, va (".obj_class_name_%s", class_name),
				   pr.scope, st_extern);
	if (!ref->external)
		G_INT (def->ofs) = ref->ofs;
	reloc_def_def (ref, def->ofs);
}

static void
emit_class_name (const char *class_name)
{
	def_t      *def;

	def = get_def (&type_integer, va (".obj_class_name_%s", class_name),
				   pr.scope, st_global);
	if (def->initialized)
		return;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	G_INT (def->ofs) = 0;
}

static void
emit_category_name (const char *class_name, const char *category_name)
{
	def_t      *def;

	def = get_def (&type_integer,
				   va (".obj_category_name_%s_%s", class_name, category_name),
				   pr.scope, st_global);
	if (def->initialized)
		return;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	G_INT (def->ofs) = 0;
}

void
class_finish (class_type_t *class_type)
{
	if (!class_type->is_class) {
		pr_category_t *pr_category;
		category_t *category = class_type->c.category;
		class_t    *class = category->class;

		pr_category = &G_STRUCT (pr_category_t, category->def->ofs);
		EMIT_DEF (pr_category->instance_methods,
				  emit_methods (category->methods, va ("%s_%s",
													   class->name,
													   category->name), 1));
		EMIT_DEF (pr_category->class_methods,
				  emit_methods (category->methods, va ("%s_%s",
													   class->name,
													   category->name), 0));
		emit_class_ref (class->name);
		emit_category_name (class->name, category->name);
	} else {
		pr_class_t *meta;
		pr_class_t *cls;
		class_t    *class = class_type->c.class;
		
		cls = &G_STRUCT (pr_class_t, class->def->ofs);

		meta = &G_STRUCT (pr_class_t, cls->class_pointer);

		EMIT_DEF (meta->methods, emit_methods (class->methods,
											   class->name, 0));

		cls->instance_size = type_size (class->ivars);
		EMIT_DEF (cls->ivars, emit_struct (class->ivars, class->name));
		EMIT_DEF (cls->methods, emit_methods (class->methods,
											  class->name, 1));
		if (class->super_class)
			emit_class_ref (class->super_class->name);
		emit_class_name (class->name);
	}
}

int
class_access (class_type_t *current_class, class_t *class)
{
	if (!current_class)
		return 1;
	if (current_class->is_class)
		return current_class->c.class != class;
	return current_class->c.category->class != class;
}

struct_field_t *
class_find_ivar (class_t *class, int protected, const char *name)
{
	struct_field_t *ivar;
	class_t    *c;

	ivar = struct_find_field (class->ivars, name);
	if (ivar) {
		if (protected && ivar->visibility != vis_public)
			goto access_error;
		return ivar;
	}
	for (c = class->super_class; c; c = c->super_class) {
		ivar = struct_find_field (c->ivars, name);
		if (ivar) {
			if (ivar->visibility == vis_private
				|| (protected && ivar->visibility == vis_protected))
				goto access_error;
			return ivar;
		}
	}
	error (0, "%s.%s does not exist", class->name, name);
	return 0;
  access_error:
	error (0, "%s.%s is not accessable here", class->name, name);
	return 0;
}

expr_t *
class_ivar_expr (class_type_t *class_type, const char *name)
{
	struct_field_t *ivar;
	class_t    *c, *class;

	if (!class_type)
		return 0;

	if (class_type->is_class) {
		class = class_type->c.class;
	} else {
		class = class_type->c.category->class;
	}
	ivar = struct_find_field (class->ivars, name);
	if (!ivar) {
		for (c = class->super_class; c; c = c->super_class) {
			ivar = struct_find_field (c->ivars, name);
			if (ivar)
				break;
		}
	}
	if (!ivar)
		return 0;
	if (ivar->visibility == vis_private) {
		error (0, "%s.%s is not accessable here",
			   class->name, name);
		return 0;
	}
	return binary_expr ('.', new_name_expr ("self"), new_name_expr (name));
}

method_t *
class_find_method (class_type_t *class_type, method_t *method)
{
	methodlist_t *methods;
	method_t   *m;
	dstring_t  *sel;
	const char *class_name;
	const char *category_name = 0;

	if (!class_type->is_class) {
		methods = class_type->c.category->methods;
		category_name = class_type->c.category->name;
		class_name = class_type->c.category->class->name;
	} else {
		methods = class_type->c.class->methods;
		class_name = class_type->c.class->name;
	}
	for (m = methods->head; m; m = m->next)
		if (method_compare (method, m)) {
			if (m->type != method->type)
				error (0, "method type mismatch");
			return m;
		}
	sel = dstring_newstr ();
	selector_name (sel, (keywordarg_t *)method->selector);
	warning (0, "%s method %s not in %s%s",
			 method->instance ? "instance" : "class",
			 sel->str, class_name,
			 category_name ? va (" (%s)", category_name) : "");
	dstring_delete (sel);
	return method;
}

method_t *
class_message_response (class_t *class, expr_t *sel)
{
	pr_sel_t   *selector;
	char       *sel_name;
	method_t   *m;
	class_t    *c = class;
	category_t *cat;

	if (sel->type != ex_pointer && sel->e.pointer.type != type_SEL.aux_type) {
		error (sel, "not a selector");
		return 0;
	}
	selector = &G_STRUCT (pr_sel_t, sel->e.pointer.val);
	sel_name = G_GETSTR (selector->sel_id);
	while (c) {
		if (c->methods) {
			for (cat = c->categories; cat; cat = cat->next) {
				for (m = cat->methods->head; m; m = m->next) {
					if (strcmp (sel_name, m->name) == 0)
						return m;
				}
			}
			for (m = c->methods->head; m; m = m->next) {
				if (strcmp (sel_name, m->name) == 0)
					return m;
			}
		}
		c = c->super_class;
	}
	warning (sel, "%s does not respond to %s", class->name, sel_name);
	return 0;
}

static unsigned long
category_get_hash (void *_c, void *unused)
{
	category_t *c = (category_t *) _c;
	return Hash_String (c->name) ^ Hash_String (c->class->name);
}

static int
category_compare (void *_c1, void *_c2, void *unused)
{
	category_t *c1 = (category_t *) _c1;
	category_t *c2 = (category_t *) _c2;
	return strcmp (c1->name, c2->name) == 0
		   && strcmp (c1->class->name, c2->class->name) == 0;
}

void
class_add_ivars (class_t *class, struct type_s *ivars)
{
	class->ivars = ivars;
}

void
class_check_ivars (class_t *class, struct type_s *ivars)
{
	if (!struct_compare_fields (class->ivars, ivars))
		warning (0, "instance variable missmatch for %s", class->name);
	class->ivars = ivars;
}

category_t *
get_category (const char *class_name, const char *category_name, int create)
{
	category_t *category;
	class_t    *class;

	if (!category_hash) {
		category_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (category_hash,
							 category_get_hash, category_compare);
	}
	class = get_class (class_name, 0);
	if (!class) {
		error (0, "undefined class %s", class_name);
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
	category->class_type.is_class = 0;
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
	if (!category->methods)
		category->methods = new_methodlist ();
	*category->methods->tail = methods->head;
	category->methods->tail = methods->tail;
	free (methods);
}

void
category_add_protocol_methods (category_t *category, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;
	type_t     *type;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0);
	if (!category->methods)
		category->methods = new_methodlist ();
	type = category->class->type;

	for (e = protocols; e; e = e->next) {
		methodlist_t *methods = category->methods;
		method_t   **m = methods->tail;

		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (methods, p->methods);
		while (*m) {
			(*m)->params->type = type;
			m = &(*m)->next;
		}
	}
}

def_t *
class_pointer_def (class_t *class)
{
	def_t      *def;
	class_type_t class_type = {1, {0}};

	class_type.c.class = class;

	def = get_def (class->type,
					 va ("_OBJ_CLASS_POINTER_%s", class->name),
					 pr.scope, st_static);
	if (def->initialized)
		return def;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	if (!class->def)
		class->def = class_def (&class_type, 1);
	if (!class->def->external)
		G_INT (def->ofs) = class->def->ofs;
	reloc_def_def (class->def, def->ofs);
	return def;
}

void
class_finish_module (void)
{
	class_t   **classes = 0, **cl;
	category_t **categories = 0, **ca;
	int         num_classes = 0;
	int         num_categories = 0;
	int         i;
	type_t     *symtab_type;
	def_t      *symtab_def;
	pr_symtab_t *symtab;
	pointer_t   *def_ptr;
	def_t      *module_def;
	pr_module_t *module;
	def_t      *exec_class_def;
	def_t      *init_def;
	function_t *init_func;
	expr_t     *init_expr;

	if (class_hash) {
		classes = (class_t **) Hash_GetList (class_hash);
		for (cl = classes; *cl; cl++)
			if ((*cl)->def && !(*cl)->def->external)
				num_classes++;
	}
	if (category_hash) {
		categories = (category_t **) Hash_GetList (category_hash);
		for (ca = categories; *ca; ca++)
			if ((*ca)->def && !(*ca)->def->external)
				num_categories++;
	}
	if (!num_classes && !num_categories)
		return;
	symtab_type = new_struct (0);
	new_struct_field (symtab_type, &type_integer, "sel_ref_cnt", vis_public);
	new_struct_field (symtab_type, &type_integer, "cls_def_cnt", vis_public);
	new_struct_field (symtab_type, &type_integer, "cat_def_cnt", vis_public);
	for (i = 0; i < num_classes + num_categories; i++)
		new_struct_field (symtab_type, &type_pointer, 0, vis_public);
	symtab_def = get_def (symtab_type, "_OBJ_SYMTAB", pr.scope, st_static);
	symtab_def->initialized = symtab_def->constant = 1;
	symtab_def->nosave = 1;
	symtab = &G_STRUCT (pr_symtab_t, symtab_def->ofs);
	symtab->cls_def_cnt = num_classes;
	symtab->cat_def_cnt = num_categories;
	def_ptr = symtab->defs;
	if (classes) {
		for (cl = classes; *cl; cl++) {
			if ((*cl)->def && !(*cl)->def->external) {
				reloc_def_def ((*cl)->def, POINTER_OFS (def_ptr));
				*def_ptr++ = (*cl)->def->ofs;
			}
		}
	}
	if (categories) {
		for (ca = categories; *ca; ca++) {
			if ((*ca)->def && !(*ca)->def->external) {
				reloc_def_def ((*ca)->def, POINTER_OFS (def_ptr));
				*def_ptr++ = (*ca)->def->ofs;
			}
		}
	}

	module_def = get_def (type_module, "_OBJ_MODULE", pr.scope, st_static);
	module_def->initialized = module_def->constant = 1;
	module_def->nosave = 1;
	module = &G_STRUCT (pr_module_t, module_def->ofs);
	module->size = type_size (type_module);
	EMIT_STRING (module->name, options.output_file);
	EMIT_DEF (module->symtab, symtab_def);

	exec_class_def = get_def (&type_obj_exec_class, "__obj_exec_class",
								pr.scope, st_extern);

	init_def = get_def (&type_function, ".ctor", pr.scope, st_static);
	init_func = new_function (".ctor");
	init_func->def = init_def;
	reloc_def_func (init_func, init_def->ofs);
	init_func->code = pr.code->size;
	build_scope (init_func, init_def, 0);
	build_function (init_func);
	init_expr = new_block_expr ();
	append_expr (init_expr,
				 function_expr (new_def_expr (exec_class_def),
								address_expr (new_def_expr (module_def), 0, 0)));
	emit_function (init_func, init_expr);
	finish_function (init_func);
}

protocol_t *
get_protocol (const char *name, int create)
{
	protocol_t    *p;

	if (name) {
		p = Hash_Find (protocol_hash, name);
		if (p || !create)
			return p;
	}

	p = calloc (sizeof (protocol_t), 1);
	p->name = name;
	if (name)
		Hash_Add (protocol_hash, p);
	return p;
}

void
protocol_add_methods (protocol_t *protocol, methodlist_t *methods)
{
	if (!methods)
		return;
	if (!protocol->methods)
		protocol->methods = new_methodlist ();
	*protocol->methods->tail = methods->head;
	protocol->methods->tail = methods->tail;
	free (methods);
}

void
protocol_add_protocol_methods (protocol_t *protocol, expr_t *protocols)
{
	expr_t     *e;
	protocol_t *p;

	if (!protocol->methods)
		protocol->methods = new_methodlist ();

	for (e = protocols; e; e = e->next) {
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (protocol->methods, p->methods);
	}
}

def_t *
protocol_def (protocol_t *protocol)
{
	return get_def (&type_Protocol, protocol->name, pr.scope, st_static);
}

protocollist_t *
new_protocollist (void)
{
	protocollist_t *protocollist = malloc (sizeof (protocollist_t));

	protocollist->count = 0;
	protocollist->list = 0;
	return protocollist;
}

def_t *
emit_protocol (protocol_t *protocol)
{
	def_t      *proto_def;
	pr_protocol_t *proto;

	proto_def = get_def (type_Protocol.aux_type,
						   va ("_OBJ_PROTOCOL_%s", protocol->name),
						   pr.scope, st_static);
	proto_def->initialized = proto_def->constant = 1;
	proto_def->nosave = 1;
	proto = &G_STRUCT (pr_protocol_t, proto_def->ofs);
	proto->class_pointer = 0;
	EMIT_STRING (proto->protocol_name, protocol->name);
	EMIT_DEF (proto->protocol_list,
			  emit_protocol_list (protocol->protocols,
								  va ("PROTOCOLo_%s", protocol->name)));
	EMIT_DEF (proto->instance_methods,
			  emit_methods (protocol->methods, protocol->name, 1));
	EMIT_DEF (proto->class_methods,
			  emit_methods (protocol->methods, protocol->name, 0));
	return proto_def;
}

def_t *
emit_protocol_list (protocollist_t *protocols, const char *name)
{
	def_t      *proto_list_def;
	type_t     *protocol_list;
	pr_protocol_list_t *proto_list;
	int         i;

	if (!protocols)
		return 0;
	protocol_list = new_struct (0);
	new_struct_field (protocol_list, &type_pointer, "next", vis_public);
	new_struct_field (protocol_list, &type_integer, "count", vis_public);
	for (i = 0; i < protocols->count; i++)
		new_struct_field (protocol_list, &type_pointer, 0, vis_public);
	proto_list_def = get_def (type_Protocol.aux_type,
								va ("_OBJ_PROTOCOLS_%s", name),
								pr.scope, st_static);
	proto_list_def->initialized = proto_list_def->constant = 1;
	proto_list_def->nosave = 1;
	proto_list = &G_STRUCT (pr_protocol_list_t, proto_list_def->ofs);
	proto_list->next = 0;
	proto_list->count = protocols->count;
	for (i = 0; i < protocols->count; i++)
		EMIT_DEF (proto_list->list[i], emit_protocol (protocols->list[i]));
	return proto_list_def;
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
	if (class_hash)
		class_Class.super_class = get_class ("Object", 1);
}
