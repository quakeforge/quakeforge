/*
	function.c

	QC function support code

	Copyright (C) 2001 Bill Currie

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
#include "QF/pr_obj.h"
#include "QF/va.h"

#include "qfcc.h"

#include "class.h"
#include "def.h"
#include "expr.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "struct.h"
#include "type.h"

static hashtab_t *class_hash;
static hashtab_t *category_hash;
static hashtab_t *protocol_hash;

class_t         class_id = {1, "id", 0, 0, 0, 0, 0, 0, &type_id};
class_t         class_Class = {1, "Class", 0, 0, 0, 0, 0, 0, &type_Class};
class_t         class_Protocol = {1, "Protocl", 0, 0, 0, 0, 0, 0, &type_Protocol};

static const char *
class_get_key (void *class, void *unused)
{
	return ((class_t *) class)->class_name;
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
	c->class_name = name;
	new = *type_Class.aux_type;
	new.class = c;
	c->type = pointer_type (find_type (&new));
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
		if (!(p = get_protocol (e->e.string_val, 0))) {
			error (e, "undefined protocol `%s'", e->e.string_val);
			continue;
		}
		copy_methods (class->methods, p->methods);
	}
}

void
class_add_protocol (class_t *class, protocol_t *protocol)
{
	if (!class->protocols)
		class->protocols = new_protocollist ();
	add_protocol (class->protocols, protocol);
}

void
class_begin (class_t *class)
{
	current_class = class;
	if (class->def)
		return;
	if (class->class_name && class->category_name) {
		pr_category_t *category;

		class->def = get_def (type_category,
								va ("_OBJ_CATEGORY_%s_%s",
									class->class_name,
									class->category_name),
								pr.scope, 1);
		class->def->initialized = class->def->constant = 1;
		category = &G_STRUCT (pr_category_t, class->def->ofs);
		category->category_name = ReuseString (class->category_name);
		category->class_name = ReuseString (class->class_name);
		category->protocols = emit_protocol_list (class->protocols,
												  va ("%s_%s",
													  class->class_name,
													  class->category_name));
	} else if (class->class_name) {
		def_t      *meta_def;
		pr_class_t *meta;
		pr_class_t *cls;
		
		meta_def = get_def (type_Class.aux_type,
							  va ("_OBJ_METACLASS_%s", class->class_name),
							  pr.scope, 1);
		meta_def->initialized = meta_def->constant = 1;
		meta = &G_STRUCT (pr_class_t, meta_def->ofs);
		meta->class_pointer  = ReuseString (class->class_name);
		if (class->super_class)
			meta->super_class = ReuseString (class->super_class->class_name);
		meta->name = meta->class_pointer;
		meta->info = _PR_CLS_META;
		meta->instance_size = type_size (type_Class.aux_type);
		meta->ivars = emit_struct (type_Class.aux_type, "Class");
		meta->protocols = emit_protocol_list (class->protocols,
											  class->class_name);

		class->def = get_def (type_Class.aux_type,
								va ("_OBJ_CLASS_%s", class->class_name),
								pr.scope, 1);
		class->def->initialized = class->def->constant = 1;
		cls = &G_STRUCT (pr_class_t, class->def->ofs);
		cls->class_pointer = meta_def->ofs;
		cls->super_class = meta->super_class;
		cls->name = meta->name;
		meta->info = _PR_CLS_CLASS;
		cls->protocols = meta->protocols;
	}
}

void
class_finish (class_t *class)
{
	if (class->class_name && class->category_name) {
		pr_category_t *category;

		category = &G_STRUCT (pr_category_t, class->def->ofs);
		category->instance_methods = emit_methods (class->methods,
												   va ("%s_%s",
													   class->class_name,
													   class->category_name),
												   1);
		category->class_methods = emit_methods (class->methods,
												va ("%s_%s",
													class->class_name,
													class->category_name),
												0);
	} else if (class->class_name) {
		pr_class_t *meta;
		pr_class_t *cls;
		
		cls = &G_STRUCT (pr_class_t, class->def->ofs);

		meta = &G_STRUCT (pr_class_t, cls->class_pointer);

		meta->methods = emit_methods (class->methods, class->class_name, 0);

		cls->instance_size = type_size (class->ivars);
		cls->ivars = emit_struct (class->ivars, class->class_name);
		cls->methods = emit_methods (class->methods, class->class_name, 1);
	}
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
	error (0, "%s.%s does not exist", class->class_name, name);
	return 0;
  access_error:
	error (0, "%s.%s is not accessable here", class->class_name, name);
	return 0;
}

expr_t *
class_ivar_expr (class_t *class, const char *name)
{
	struct_field_t *ivar;
	class_t    *c;

	if (!class)
		return 0;

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
			   class->class_name, name);
		return 0;
	}
	return binary_expr ('.', new_name_expr ("self"), new_name_expr (name));
}

method_t *
class_find_method (class_t *class, method_t *method)
{
	method_t   *m;
	dstring_t  *sel;

	for (m = class->methods->head; m; m = m->next)
		if (method_compare (method, m))
			return m;
	sel = dstring_newstr ();
	selector_name (sel, (keywordarg_t *)method->selector);
	warning (0, "%s method %s not in %s%s",
			 method->instance ? "instance" : "class",
			 sel->str, class->class_name,
			 class->category_name ? va (" (%s)", class->category_name) : "");
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

	if (sel->type != ex_pointer && sel->e.pointer.type != type_SEL.aux_type) {
		error (sel, "not a selector");
		return 0;
	}
	selector = &G_STRUCT (pr_sel_t, sel->e.pointer.val);
	sel_name = pr.strings + selector->sel_id;
	while (c) {
		if (c->methods) {
			for (m = c->methods->head; m; m = m->next) {
				if (strcmp (sel_name, m->name) == 0)
					return m;
			}
		}
		c = c->super_class;
	}
	warning (sel, "%s does not respond to %s", class->class_name, sel_name);
	return 0;
}

static unsigned long
category_get_hash (void *_c, void *unused)
{
	class_t    *c = (class_t *) _c;
	return Hash_String (c->class_name) ^ Hash_String (c->category_name);
}

static int
category_compare (void *_c1, void *_c2, void *unused)
{
	class_t    *c1 = (class_t *) _c1;
	class_t    *c2 = (class_t *) _c2;
	return strcmp (c1->class_name, c2->class_name) == 0
		   && strcmp (c1->category_name, c2->category_name) == 0;
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
		warning (0, "instance variable missmatch for %s", class->class_name);
	class->ivars = ivars;
}

class_t *
get_category (const char *class_name, const char *category_name, int create)
{
	class_t    *c;

	if (!category_hash) {
		category_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (category_hash,
							 category_get_hash, category_compare);
	}
	if (class_name && category_name) {
		class_t     _c = {0, class_name, category_name};

		c = Hash_FindElement (category_hash, &_c);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	c->class_name = class_name;
	c->category_name = category_name;
	if (class_name && category_name)
		Hash_AddElement (category_hash, c);
	return c;
}

def_t *
class_def (class_t *class)
{
	def_t      *def;

	def = get_def (class->type,
					 va ("_OBJ_CLASS_POINTER_%s", class->class_name),
					 pr.scope, 1);
	if (def->initialized)
		return def;
	if (class->def) {	//FIXME need externals?
		G_INT (def->ofs) = class->def->ofs;
	} else {
		warning (0, "%s not implemented", class->class_name);
	}
	def->initialized = def->constant = 1;
	return def;
}

void
class_finish_module (void)
{
	class_t   **classes = 0;
	class_t   **categories = 0;
	class_t   **t;
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
	function_t *exec_class_func;
	def_t      *init_def;
	function_t *init_func;
	expr_t     *init_expr;

	if (class_hash) {
		classes = (class_t **) Hash_GetList (class_hash);
		for (t = classes; *t; t++)
			if ((*t)->def)
				num_classes++;
	}
	if (category_hash) {
		categories = (class_t **) Hash_GetList (category_hash);
		for (t = categories; *t; t++)
			if ((*t)->def)
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
	symtab_def = get_def (symtab_type, "_OBJ_SYMTAB", pr.scope, 1);
	symtab_def->initialized = symtab_def->constant = 1;
	symtab = &G_STRUCT (pr_symtab_t, symtab_def->ofs);
	symtab->cls_def_cnt = num_classes;
	symtab->cat_def_cnt = num_categories;
	def_ptr = symtab->defs;
	for (i = 0, t = classes; i < num_classes; i++, t++)
		if ((*t)->def)
			*def_ptr++ = (*t)->def->ofs;
	for (i = 0, t = categories; i < num_categories; i++, t++)
		if ((*t)->def)
			*def_ptr++ = (*t)->def->ofs;

	module_def = get_def (type_module, "_OBJ_MODULE", pr.scope, 1);
	module_def->initialized = module_def->constant = 1;
	module = &G_STRUCT (pr_module_t, module_def->ofs);
	module->size = type_size (type_module);
	module->name = ReuseString (options.output_file);
	module->symtab = symtab_def->ofs;

	exec_class_def = get_def (&type_obj_exec_class, "__obj_exec_class",
								pr.scope, 1);
	exec_class_func = new_function ("__obj_exec_class");
	exec_class_func->builtin = 0;
	exec_class_func->def = exec_class_def;
	exec_class_func->refs = new_reloc (exec_class_def->ofs, rel_def_func);
	build_function (exec_class_func);
	finish_function (exec_class_func);

	init_def = get_def (&type_function, ".ctor", pr.scope, 1);
	init_func = new_function (".ctor");
	init_func->def = init_def;
	init_func->refs = new_reloc (init_def->ofs, rel_def_func);
	init_func->code = pr.num_statements;
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
	return get_def (&type_Protocol, protocol->name, pr.scope, 1);
}

protocollist_t *
new_protocollist (void)
{
	protocollist_t *protocollist = malloc (sizeof (protocollist_t));

	protocollist->count = 0;
	protocollist->list = 0;
	return protocollist;
}

void
add_protocol (protocollist_t *protocollist, protocol_t *protocol)
{
	protocollist->list = realloc (protocollist->list,
								  (protocollist->count + 1)
								  * sizeof (protocollist_t));
	protocollist->list[protocollist->count++] = protocol;
}

int
emit_protocol (protocol_t *protocol)
{
	def_t      *proto_def;
	pr_protocol_t *proto;

	proto_def = get_def (type_Protocol.aux_type,
						   va ("_OBJ_PROTOCOL_%s", protocol->name),
						   pr.scope, 1);
	proto_def->initialized = proto_def->constant = 1;
	proto = &G_STRUCT (pr_protocol_t, proto_def->ofs);
	proto->class_pointer = 0;
	proto->protocol_name = ReuseString (protocol->name);
	proto->protocol_list =
		emit_protocol_list (protocol->protocols,
							va ("PROTOCOL_%s", protocol->name));
	proto->instance_methods = emit_methods (protocol->methods,
											protocol->name, 1);
	proto->class_methods = emit_methods (protocol->methods, protocol->name, 0);
	return proto_def->ofs;
}

int
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
								pr.scope, 1);
	proto_list_def->initialized = proto_list_def->constant = 1;
	proto_list = &G_STRUCT (pr_protocol_list_t, proto_list_def->ofs);
	proto_list->next = 0;
	proto_list->count = protocols->count;
	for (i = 0; i < protocols->count; i++)
		proto_list->list[i] = emit_protocol (protocols->list[i]);
	return proto_list_def->ofs;
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
}
