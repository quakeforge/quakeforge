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

static __attribute__ ((used)) const char rcsid[] =
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

#include "codespace.h"
#include "class.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"

static hashtab_t *class_hash;
static hashtab_t *category_hash;
static hashtab_t *protocol_hash;

class_t     class_id = {1, "id"};
class_t     class_Class = {1, "Class"};
class_t     class_Protocol = {1, "Protocl"};

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

void
class_init (void)
{
	class_id.type = &type_id;
	class_Class.type = &type_Class;
	class_Protocol.type = &type_Protocol;

	class_Class.super_class = get_class (new_symbol ("Object"), 1);
	class_Class.methods = new_methodlist ();
}

def_t *
class_def (class_type_t *class_type, int external)
{
	const char *name = 0;
	type_t     *type = 0;
	storage_class_t storage = external ? st_extern : st_global;

	switch (class_type->type) {
		case ct_category:
			name = va ("_OBJ_CATEGORY_%s_%s",
					   class_type->c.category->class->name,
					   class_type->c.category->name);
			type = &type_category;
			break;
		case ct_class:
			name = va ("_OBJ_CLASS_%s", class_type->c.class->name);
			type = type_Class.t.fldptr.type;
			break;
		case ct_protocol:
			return 0;		// probably in error recovery
	}
	return get_def (type, name, pr.scope, storage);
}

class_t *
get_class (symbol_t *sym, int create)
{
	class_t    *c;
	type_t      new;

	if (!class_hash)
		class_hash = Hash_NewTable (1021, class_get_key, 0, 0);
	if (sym) {
		c = Hash_Find (class_hash, sym->name);
		if (c || !create)
			return c;
	}

	c = calloc (sizeof (class_t), 1);
	if (sym)
		c->name = sym->name;
	new = *type_Class.t.fldptr.type;
	new.t.class = c;
	c->type = find_type (&new);
	c->methods = new_methodlist ();
	c->class_type.type = ct_class;
	c->class_type.c.class = c;
	if (sym)
		Hash_Add (class_hash, c);
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

	*class->methods->tail = methods->head;
	class->methods->tail = methods->tail;
	free (methods);

	methods_set_self_type (class, class->methods);
}

void
class_add_protocols (class_t *class, protocollist_t *protocols)
{
	int         i;
	protocol_t *p;
	methodlist_t *methods;

	if (!protocols)
		return;

	methods = class->methods;

	for (i = 0; i < protocols->count; i++) {
		p = protocols->list[i];
		copy_methods (methods, p->methods);
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

	current_class = &category->class_type;
	category->def = class_def (current_class, 0);
	category->def->initialized = category->def->constant = 1;
	category->def->nosave = 1;
	pr_category = &G_STRUCT (pr_category_t, category->def->ofs);
	EMIT_STRING (pr_category->category_name, category->name);
	EMIT_STRING (pr_category->class_name, class->name);
	EMIT_DEF (pr_category->protocols,
			  emit_protocol_list (category->protocols,
								  va ("%s_%s", class->name, category->name)));
}

static void
begin_class (class_t *class)
{
	def_t      *meta_def;
	pr_class_t *meta;
	pr_class_t *cls;

	current_class = &class->class_type;
	class->def = class_def (current_class, 0);
	meta_def = get_def (type_Class.t.fldptr.type,
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
	meta->instance_size = type_size (type_Class.t.fldptr.type);
#if 0  //FIXME
	EMIT_DEF (meta->ivars,
			  emit_struct (type_Class.t.fldptr.type->t.class->ivars,
						   "Class"));
#endif
	class->def->initialized = class->def->constant = 1;
	class->def->nosave = 1;
	cls = &G_STRUCT (pr_class_t, class->def->ofs);
	EMIT_DEF (cls->class_pointer, meta_def);
	if (class->super_class) {
		class_type_t class_type = {ct_class, {0}};
		class_type.c.class = class->super_class;
		EMIT_STRING (cls->super_class, class->super_class->name);
		class_def (&class_type, 1);
	}
	EMIT_STRING (cls->name, class->name);
	cls->info = _PR_CLS_CLASS;
	EMIT_DEF (cls->protocols,
			  emit_protocol_list (class->protocols, class->name));
}

void
class_begin (class_type_t *class_type)
{
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

void
emit_category_ref (const char *class_name, const char *category_name)
{
	def_t      *def;
	def_t      *ref;

	def = get_def (&type_pointer,
			va (".obj_category_ref_%s_%s", class_name, category_name),
			pr.scope, st_static);
	if (def->initialized)
		return;
	def->initialized = def->constant = 1;
	def->nosave = 1;
	ref = get_def (&type_integer,
			va (".obj_category_name_%s_%s", class_name, category_name),
			pr.scope, st_extern);
	if (!ref->external)
		G_INT (def->ofs) = ref->ofs;
	reloc_def_def (ref, def->ofs);
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

static void
finish_category (category_t *category)
{
	pr_category_t *pr_category;
	class_t    *class = category->class;
	char       *name;

	if (!category->def)	// probably in error recovery
		return;
	name = nva ("%s_%s", class->name, category->name);
	pr_category = &G_STRUCT (pr_category_t, category->def->ofs);
	EMIT_DEF (pr_category->instance_methods,
			emit_methods (category->methods, name, 1));
	EMIT_DEF (pr_category->class_methods,
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

	if (!class->def)	// probably in error recovery
		return;
	cls = &G_STRUCT (pr_class_t, class->def->ofs);

	meta = &G_STRUCT (pr_class_t, cls->class_pointer);

	EMIT_DEF (meta->methods, emit_methods (class->methods,
										   class->name, 0));

	cls->instance_size = class->ivars ? class->ivars->size : 0;
#if 0 // FIXME
	EMIT_DEF (cls->ivars, emit_struct (class->ivars, class->name));
#endif
	EMIT_DEF (cls->methods, emit_methods (class->methods,
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

	ivar = symtab_lookup (class->ivars, name);
	if (ivar) {
		if (ivar->visibility > (vis_e) vis)
			goto access_error;
		return ivar;
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
	symbol_t   *ivar;
	class_t    *class;

	if (!class_type || !(class = extract_class (class_type)))
		return 0;

	ivar = symtab_lookup (class->ivars, name);
	if (!ivar)
		return 0;
	return binary_expr ('.', new_name_expr ("self"), new_name_expr (name));
}

method_t *
class_find_method (class_type_t *class_type, method_t *method)
{
	methodlist_t *methods = 0, *start_methods;
	method_t   *m;
	dstring_t  *sel;
	class_t    *class = 0, *start_class;
	const char *class_name;
	const char *category_name = 0;

	switch (class_type->type) {
		case ct_category:
			methods = class_type->c.category->methods;
			category_name = class_type->c.category->name;
			class = class_type->c.category->class;
			break;
		case ct_class:
			class = class_type->c.class;
			methods = class->methods;
			break;
		case ct_protocol:
			return 0;		// probably in error recovery
	}
	class_name = class->name;
	start_methods = methods;
	start_class = class;
	while (class) {
		for (m = methods->head; m; m = m->next)
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

method_t *
class_message_response (class_t *class, int class_msg, expr_t *sel)
{
	selector_t *selector;
	method_t   *m;
	class_t    *c = class;
	category_t *cat;

	if (sel->type != ex_pointer
		&& sel->e.pointer.type != type_SEL.t.fldptr.type) {
		error (sel, "not a selector");
		return 0;
	}
	selector = get_selector (sel);
	if (class->type == &type_id) {
		m = find_method (selector->name);
		if (m)
			return m;
		//FIXME right option?
		if (options.warnings.interface_check)
			warning (sel, "could not find method for %c%s",
					 class_msg ? '+' : '-', selector->name);
		return 0;
	} else {
		while (c) {
			for (cat = c->categories; cat; cat = cat->next) {
				for (m = cat->methods->head; m; m = m->next) {
					if (((!c->super_class && class_msg)
						 || class_msg != m->instance)
						&& strcmp (selector->name, m->name) == 0)
						return m;
				}
			}
			for (m = c->methods->head; m; m = m->next) {
				if (((!c->super_class && class_msg)
					 || class_msg != m->instance)
					&& strcmp (selector->name, m->name) == 0)
					return m;
			}
			c = c->super_class;
		}
		//FIXME right option?
		if (options.warnings.interface_check)
			warning (sel, "%s may not respond to %c%s", class->name,
					 class_msg ? '+' : '-', selector->name);
	}
	return 0;
}

static uintptr_t
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

symtab_t *
class_new_ivars (class_t *class)
{
	symtab_t   *ivars;
	symtab_t   *super_ivars = 0;
	
	if (class->super_class)
		super_ivars = class->super_class->ivars;
	ivars = new_symtab (super_ivars, stab_local);
	return ivars;
}

void
class_add_ivars (class_t *class, symtab_t *ivars)
{
	class->ivars = ivars;
}

void
class_check_ivars (class_t *class, symtab_t *ivars)
{
#if 0 //FIXME
	if (!struct_compare_fields (class->ivars, ivars)) {
		//FIXME right option?
		if (options.warnings.interface_check)
			warning (0, "instance variable missmatch for %s", class->name);
	}
#endif
	class->ivars = ivars;
}

category_t *
get_category (symbol_t *class_name, const char *category_name, int create)
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
	*category->methods->tail = methods->head;
	category->methods->tail = methods->tail;
	free (methods);

	methods_set_self_type (category->class, category->methods);
}

void
category_add_protocols (category_t *category, protocollist_t *protocols)
{
	int         i;
	protocol_t *p;
	methodlist_t *methods;

	if (!protocols)
		return;

	methods = category->methods;

	for (i = 0; i < protocols->count; i++) {
		p = protocols->list[i];
		copy_methods (methods, p->methods);
		if (p->protocols)
			category_add_protocols (category, p->protocols);
	}
	category->protocols = protocols;
}

def_t *
class_pointer_def (class_t *class)
{
	def_t      *def;
	class_type_t class_type = {ct_class, {0}};

	class_type.c.class = class;

	def = get_def (pointer_type (class->type),
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
	static struct_def_t symtab_struct[] = {
		{"sel_ref_cnt",	&type_integer},
		{"refs",		&type_SEL},
		{"cls_def_cnt",	&type_integer},
		{"cat_def_cnt",	&type_integer},
		{"defs",		0},				// type will be filled in at run time
		{0, 0}
	};
	class_t   **classes = 0, **cl;
	category_t **categories = 0, **ca;
	int         num_classes = 0;
	int         num_categories = 0;
	def_t      *selector_table_def;
	type_t     *symtab_type;
	//symbol_t   *symtab_sym;
	def_t      *symtab_def;
	pr_symtab_t *symtab;
	pointer_t   *def_ptr;
	def_t      *module_def;
	pr_module_t *module;
	def_t      *exec_class_def;
	def_t      *init_def;
	function_t *init_func;
	expr_t     *init_expr;

	selector_table_def = emit_selectors ();
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
	if (!selector_table_def && !num_classes && !num_categories)
		return;
	symtab_struct[4].type = array_type (&type_pointer,
										num_classes + num_categories);
	symtab_type = make_structure (0, 's', symtab_struct, 0)->type;
	symtab_def = get_def (symtab_type, "_OBJ_SYMTAB", pr.scope, st_static);
	symtab_def->initialized = symtab_def->constant = 1;
	symtab_def->nosave = 1;
	symtab = &G_STRUCT (pr_symtab_t, symtab_def->ofs);
	if (selector_table_def) {
		symtab->sel_ref_cnt = selector_table_def->type->t.array.size;
		EMIT_DEF (symtab->refs, selector_table_def);
	}
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

	module_def = get_def (&type_module, "_OBJ_MODULE", pr.scope, st_static);
	module_def->initialized = module_def->constant = 1;
	module_def->nosave = 1;
	module = &G_STRUCT (pr_module_t, module_def->ofs);
	module->size = type_size (&type_module);
	EMIT_STRING (module->name, G_GETSTR (pr.source_file));
	EMIT_DEF (module->symtab, symtab_def);

	exec_class_def = get_def (&type_obj_exec_class, "__obj_exec_class",
			pr.scope, st_extern);

	init_def = get_def (&type_function, ".ctor", pr.scope, st_static);
	current_func = init_func = new_function (init_def->name, 0);
	add_function (init_func);
	reloc_def_func (init_func, init_def->ofs);
	init_func->code = pr.code->size;
	//FIXME build_scope (init_func, init_def, 0);
	build_function (init_func);
	init_expr = new_block_expr ();
	append_expr (init_expr,
			build_function_call (new_def_expr (exec_class_def),
								 exec_class_def->type,
								 address_expr (new_def_expr (module_def),
									 		   0, 0)));
	emit_function (init_func, init_expr);
	finish_function (init_func);
	current_func = 0;
}

protocol_t *
get_protocol (const char *name, int create)
{
	protocol_t    *p;

	if (!protocol_hash)
		protocol_hash = Hash_NewTable (1021, protocol_get_key, 0, 0);

	if (name) {
		p = Hash_Find (protocol_hash, name);
		if (p || !create)
			return p;
	}

	p = calloc (sizeof (protocol_t), 1);
	p->name = name;
	p->methods = new_methodlist ();
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
	*protocol->methods->tail = methods->head;
	protocol->methods->tail = methods->tail;
	free (methods);
}

void
protocol_add_protocols (protocol_t *protocol, protocollist_t *protocols)
{
	protocol->protocols = protocols;
}

def_t *
protocol_def (protocol_t *protocol)
{
	return get_def (&type_Protocol, protocol->name, pr.scope, st_static);
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
								  sizeof (protocol_t) * protocollist->count);
	protocollist->list[protocollist->count - 1] = protocol;
	return protocollist;
}

def_t *
emit_protocol (protocol_t *protocol)
{
	def_t      *proto_def;
	pr_protocol_t *proto;

	proto_def = get_def (&type_Protocol,
						 va ("_OBJ_PROTOCOL_%s", protocol->name),
						 pr.scope, st_none);
	if (proto_def)
		return proto_def;
	proto_def = get_def (&type_Protocol,
						 va ("_OBJ_PROTOCOL_%s", protocol->name),
						 pr.scope, st_static);
	proto_def->initialized = proto_def->constant = 1;
	proto_def->nosave = 1;
	proto = &G_STRUCT (pr_protocol_t, proto_def->ofs);
	proto->class_pointer = 0;
	EMIT_STRING (proto->protocol_name, protocol->name);
	EMIT_DEF (proto->protocol_list,
			  emit_protocol_list (protocol->protocols,
								  va ("PROTOCOL_%s", protocol->name)));
	EMIT_DEF (proto->instance_methods,
			  emit_method_descriptions (protocol->methods, protocol->name, 1));
	EMIT_DEF (proto->class_methods,
			  emit_method_descriptions (protocol->methods, protocol->name, 0));
	emit_class_ref ("Protocol");
	return proto_def;
}

def_t *
emit_protocol_list (protocollist_t *protocols, const char *name)
{
	static struct_def_t proto_list_struct[] = {
		{"next",	&type_pointer},
		{"count",	&type_integer},
		{"list",	0},				// type will be filled in at run time
		{0, 0},
	};
	type_t     *proto_list_type;
	def_t      *proto_list_def;
	//symbol_t   *proto_list_sym;
	pr_protocol_list_t *proto_list;
	int         i;

	if (!protocols)
		return 0;
	proto_list_struct[2].type = array_type (&type_pointer, protocols->count);
	proto_list_type = make_structure (0, 's', proto_list_struct, 0)->type;
	proto_list_def = get_def (proto_list_type,
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
		class_Class.super_class = get_class (new_symbol ("Object"), 1);
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
