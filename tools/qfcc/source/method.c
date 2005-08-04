/*
	method.c

	QC method support code

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

#include "expr.h"
#include "class.h"
#include "def.h"
#include "emit.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "type.h"

static hashtab_t *known_methods;

static const char *
method_get_key (void *meth, void *unused)
{
	return ((method_t *) meth)->name;
}

static void
method_free (void *_meth, void *unused)
{
	method_t   *meth = (method_t *) meth;

	free (meth->name);
	free (meth->types);
	free (meth);
}

method_t *
new_method (type_t *ret_type, param_t *selector, param_t *opt_parms)
{
	method_t   *meth = malloc (sizeof (method_t));
	param_t    *cmd = new_param (0, &type_SEL, "_cmd");
	param_t    *self = new_param (0, &type_id, "self");
	dstring_t  *name = dstring_newstr ();
	dstring_t  *types = dstring_newstr ();

	opt_parms = reverse_params (opt_parms);
	selector = _reverse_params (selector, opt_parms);
	cmd->next = selector;
	self->next = cmd;

	meth->next = 0;
	meth->instance = 0;
	meth->selector = selector;
	meth->params = self;
	meth->type = parse_params (ret_type, meth->params);

	selector_name (name, (keywordarg_t *)selector);
	method_types (types, meth);
	meth->name = name->str;
	meth->types = types->str;
	free (name);
	free (types);

	//print_type (meth->type); puts ("");
	meth->def = 0;

	if (!known_methods)
		known_methods = Hash_NewTable (1021, method_get_key, method_free, 0);
	Hash_Add (known_methods, meth);

	return meth;
}

method_t *
copy_method (method_t *method)
{
	method_t   *meth = calloc (sizeof (method_t), 1);
	param_t    *self = copy_params (method->params);

	meth->next = 0;
	meth->instance = method->instance;
	meth->selector = self->next->next;
	meth->params = self;
	meth->type = method->type;
	meth->name = method->name;
	meth->types = method->types;
	return meth;
}

void
add_method (methodlist_t *methodlist, method_t *method)
{
	if (method->next) {
		error (0, "add_method: method loop detected");
		abort ();
	}

	*methodlist->tail = method;
	methodlist->tail = &method->next;
}

def_t *
method_def (class_type_t *class_type, method_t *method)
{
	dstring_t  *str = dstring_newstr ();
	def_t      *def;
	char       *s;
	const char *class_name;
	const char *category_name = "";

	if (class_type->is_class) {
		class_name = class_type->c.class->name;
	} else {
		class_name = class_type->c.category->class->name;
		category_name = class_type->c.category->name;
	}

	dsprintf (str, "_%c_%s_%s_%s",
			  method->instance ? 'i' : 'c',
			  class_name,
			  category_name,
			  method->name);
	for (s = str->str; *s; s++)
		if (*s == ':')
			*s = '_';
	//printf ("%s %s %s %ld\n", method->name, method->types, str->str, str->size);
	def = get_def (method->type, str->str, pr.scope, st_static);
	dstring_delete (str);
	return def;
}

void
method_set_param_names (method_t *dst, method_t *src)
{
	param_t    *dp, *sp;

	for (dp = dst->params, sp = src->params; dp && sp;
		 dp = dp->next, sp = sp->next) {
		dp->name = sp->name;
	}
	if (dp || sp) {
		error (0, "internal compiler error: missmatched method params");
		abort ();
	}
}

methodlist_t *
new_methodlist (void)
{
	methodlist_t *l = malloc (sizeof (methodlist_t));
	l->head = 0;
	l->tail = &l->head;
	return l;
}

void
copy_methods (methodlist_t *dst, methodlist_t *src)
{
	method_t *s, *d;

	for (s = src->head; s; s = s->next) {
		d = malloc (sizeof (method_t));
		*d = *s;
		d->next = 0;
		add_method (dst, d);
	}
}

int
method_compare (method_t *m1, method_t *m2)
{
	if (m1->instance != m2->instance)
		return 0;
	return strcmp (m1->name, m2->name) == 0
			&& strcmp (m1->types, m2->types) == 0;
}

keywordarg_t *
new_keywordarg (const char *selector, struct expr_s *expr)
{
	keywordarg_t *k = malloc (sizeof (keywordarg_t));

	k->next = 0;
	k->selector = selector;
	k->expr = expr;
	return k;
}

keywordarg_t *
copy_keywordargs (const keywordarg_t *kwargs)
{
	keywordarg_t *n_kwargs = 0, **kw = &n_kwargs;

	while (kwargs) {
		*kw = new_keywordarg (kwargs->selector, kwargs->expr);
		kwargs = kwargs->next;
		kw = &(*kw)->next;
	}
	return n_kwargs;
}

expr_t *
send_message (int super)
{
	if (super)
		return new_def_expr (get_def (&type_supermsg, "obj_msgSend_super",
									  pr.scope, st_extern));
	else
		return new_def_expr (get_def (&type_IMP, "obj_msgSend", pr.scope,
									  st_extern));
}

method_t *
find_method (const char *sel_name)
{
	if (!known_methods)
		return 0;
	return Hash_Find (known_methods, sel_name);
}

void
selector_name (dstring_t *sel_id, keywordarg_t *selector)
{
	dstring_clearstr (sel_id);
	while (selector && selector->selector) {
		dstring_appendstr (sel_id, selector->selector);
		if (selector->expr)
			dstring_appendstr (sel_id, ":");
		selector = selector->next;
	}
}

void
method_types (dstring_t *sel_types, method_t *method)
{
	dstring_clearstr (sel_types);
	encode_type (sel_types, method->type);
}

static hashtab_t *sel_hash;
static hashtab_t *sel_index_hash;
static int sel_index;

static unsigned long
sel_get_hash (void *_sel, void *unused)
{
	selector_t *sel = (selector_t *) _sel;
	unsigned long hash;

	hash = Hash_String (sel->name);
	return hash;
}

static int
sel_compare (void *_s1, void *_s2, void *unused)
{
	selector_t *s1 = (selector_t *) _s1;
	selector_t *s2 = (selector_t *) _s2;
	int         cmp;

	cmp = strcmp (s1->name, s2->name) == 0;
	return cmp;
}

static unsigned long
sel_index_get_hash (void *_sel, void *unused)
{
	selector_t *sel = (selector_t *) _sel;
	return sel->index;
}

static int
sel_index_compare (void *_s1, void *_s2, void *unused)
{
	selector_t *s1 = (selector_t *) _s1;
	selector_t *s2 = (selector_t *) _s2;
	return s1->index == s2->index;
}

int
selector_index (const char *sel_id)
{
	selector_t  _sel = {save_string (sel_id), 0, 0};
	selector_t *sel = &_sel;

	if (!sel_hash) {
		sel_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (sel_hash, sel_get_hash, sel_compare);
		sel_index_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (sel_index_hash, sel_index_get_hash,
							 sel_index_compare);
	}
	sel = Hash_FindElement (sel_hash, sel);
	if (sel)
		return sel->index;
	sel = malloc (sizeof (selector_t));
	sel->name = _sel.name;
	sel->types = _sel.types;
	sel->index = sel_index++;
	Hash_AddElement (sel_hash, sel);
	Hash_AddElement (sel_index_hash, sel);
	return sel->index;
}

selector_t *
get_selector (expr_t *sel)
{
	selector_t  _sel = {0, 0, sel->e.pointer.val};
	_sel.index /= type_size (type_SEL.aux_type);
	return (selector_t *) Hash_FindElement (sel_index_hash, &_sel);
}

def_t *
emit_selectors (void)
{
	def_t      *sel_def;
	type_t     *sel_type;
	pr_sel_t   *sel;
	selector_t **selectors, **s;
	
	if (!sel_index)
		return 0;

	sel_type = array_type (type_SEL.aux_type, sel_index);
	sel_def = get_def (type_SEL.aux_type, "_OBJ_SELECTOR_TABLE", pr.scope,
					   st_extern);
	sel_def->type = sel_type;
	sel_def->ofs = new_location (sel_type, pr.near_data);
	set_storage_bits (sel_def, st_static);
	sel = G_POINTER (pr_sel_t, sel_def->ofs);
	selectors = (selector_t **) Hash_GetList (sel_hash);
	
	for (s = selectors; *s; s++) {
		EMIT_STRING (sel[(*s)->index].sel_id, (*s)->name);
		EMIT_STRING (sel[(*s)->index].sel_types, (*s)->types);
	}
	free (selectors);
	return sel_def;
}

def_t *
emit_methods (methodlist_t *_methods, const char *name, int instance)
{
	const char *type = instance ? "INSTANCE" : "CLASS";
	method_t   *method;
	int         i, count;
	def_t      *methods_def;
	pr_method_list_t *methods;
	struct_t     *method_list;

	if (!_methods)
		return 0;

	for (count = 0, method = _methods->head; method; method = method->next)
		if (!method->instance == !instance) {
			if (!method->def && options.warnings.unimplemented) {
				warning (0, "Method `%c%s' not implemented",
						method->instance ? '-' : '+', method->name);
			}
			count++;
		}
	if (!count)
		return 0;
	method_list = get_struct (0, 1);
	init_struct (method_list, new_type (), str_struct, 0);
	new_struct_field (method_list, &type_pointer, "method_next", vis_public);
	new_struct_field (method_list, &type_integer, "method_count", vis_public);
	for (i = 0; i < count; i++)
		new_struct_field (method_list, type_Method.aux_type, 0, vis_public);
	methods_def = get_def (method_list->type,
						   va ("_OBJ_%s_METHODS_%s", type, name),
						   pr.scope, st_static);
	methods_def->initialized = methods_def->constant = 1;
	methods_def->nosave = 1;
	methods = &G_STRUCT (pr_method_list_t, methods_def->ofs);
	methods->method_next = 0;
	methods->method_count = count;
	for (i = 0, method = _methods->head; method; method = method->next) {
		if (!method->instance != !instance || !method->def)
			continue;
		EMIT_STRING (methods->method_list[i].method_name, method->name);
		EMIT_STRING (methods->method_list[i].method_types, method->types);
		methods->method_list[i].method_imp = G_FUNCTION (method->def->ofs);
		if (method->func) {
			reloc_def_func (method->func,
							POINTER_OFS (&methods->method_list[i].method_imp));
		}
		i++;
	}
	return methods_def;
}

def_t *
emit_method_descriptions (methodlist_t *_methods, const char *name,
						  int instance)
{
	const char *type = instance ? "PROTOCOL_INSTANCE" : "PROTOCOL_CLASS";
	method_t   *method;
	int         i, count;
	def_t      *methods_def;
	pr_method_description_list_t *methods;
	struct_t     *method_list;

	if (!_methods)
		return 0;

	for (count = 0, method = _methods->head; method; method = method->next)
		if (!method->instance == !instance)
			count++;
	if (!count)
		return 0;
	method_list = get_struct (0, 1);
	init_struct (method_list, new_type (), str_struct, 0);
	new_struct_field (method_list, &type_integer, "count", vis_public);
	for (i = 0; i < count; i++)
		new_struct_field (method_list, &type_method_description, 0,
						  vis_public);
	methods_def = get_def (method_list->type,
						   va ("_OBJ_%s_METHODS_%s", type, name),
						   pr.scope, st_static);
	methods_def->initialized = methods_def->constant = 1;
	methods_def->nosave = 1;
	methods = &G_STRUCT (pr_method_description_list_t, methods_def->ofs);
	methods->count = count;
	for (i = 0, method = _methods->head; method; method = method->next) {
		if (!method->instance != !instance || !method->def)
			continue;
		EMIT_STRING (methods->list[i].name, method->name);
		EMIT_STRING (methods->list[i].types, method->types);
		i++;
	}
	return methods_def;
}

void
clear_selectors (void)
{
	if (sel_hash) {
		Hash_FlushTable (sel_hash);
		Hash_FlushTable (sel_index_hash);
	}
	sel_index = 0;
	if (known_methods)
		Hash_FlushTable (known_methods);
}

expr_t *
method_check_params (method_t *method, expr_t *args)
{
	int         i, count, parm_count;
	expr_t     *a, **arg_list, *err = 0;
	type_t     *mtype = method->type;

	if (mtype->num_parms == -1)
		return 0;

	for (count = 0, a = args; a; a = a->next)
		count++;

	if (count > MAX_PARMS)
		return error (args, "more than %d parameters", MAX_PARMS);

	if (mtype->num_parms >= 0)
		parm_count = mtype->num_parms;
	else
		parm_count = -mtype->num_parms - 1;

	if (count < parm_count)
		return error (args, "too few arguments");
	if (mtype->num_parms >= 0 && count > mtype->num_parms)
		return error (args, "too many arguments");

	arg_list = malloc (count * sizeof (expr_t *));
	for (i = count - 1, a = args; a; a = a->next)
		arg_list[i--] = a;
	for (i = 2; i < count; i++) {
		expr_t     *e = arg_list[i];
		type_t     *t = get_type (e);

		if (!t)
			return e;
 
		if (i < parm_count) {
			if (e->type != ex_nil)
				if (!type_assignable (mtype->parm_types[i], t)) {
					err = error (e, "type mismatch for parameter %d of %s",
								 i - 1, method->name);
				}
		} else {
			if (e->type == ex_integer && options.warnings.vararg_integer)
				warning (e, "passing integer consant into ... function");
		}
	}
	free (arg_list);
	return err;
}
