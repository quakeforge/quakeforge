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

#include "expr.h"
#include "class.h"
#include "def.h"
#include "emit.h"
#include "immediate.h"
#include "method.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "type.h"

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
	selector_types (types, (keywordarg_t *)selector);
	meth->name = name->str;
	meth->types = types->str;
	free (name);
	free (types);

	//print_type (meth->type); puts ("");
	meth->def = 0;
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
selector_types (dstring_t *sel_types, keywordarg_t *selector)
{
	dstring_clearstr (sel_types);
}

typedef struct {
	string_t    sel_id;
	string_t    sel_types;
	def_t      *def;
} sel_def_t;

static hashtab_t *sel_def_hash;

static unsigned long
sel_def_get_hash (void *_sel_def, void *unused)
{
	sel_def_t  *sel_def = (sel_def_t*)_sel_def;
	unsigned long hash;

	hash = Hash_String (G_GETSTR (sel_def->sel_id))
		   ^ Hash_String (G_GETSTR (sel_def->sel_types));
	return hash;
}

static int
sel_def_compare (void *_sd1, void *_sd2, void *unused)
{
	sel_def_t  *sd1 = (sel_def_t*)_sd1;
	sel_def_t  *sd2 = (sel_def_t*)_sd2;
	int         cmp;

	cmp = strcmp (G_GETSTR (sd1->sel_id), G_GETSTR (sd2->sel_id)) == 0;
	if (cmp)
		cmp = strcmp (G_GETSTR (sd1->sel_types),
					  G_GETSTR (sd2->sel_types)) == 0;
	return cmp;
}

def_t *
selector_def (const char *sel_id, const char *sel_types)
{
	sel_def_t   _sel_def = {ReuseString (sel_id), ReuseString (sel_types), 0};
	sel_def_t  *sel_def = &_sel_def;

	if (!sel_def_hash) {
		sel_def_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (sel_def_hash, sel_def_get_hash, sel_def_compare);
	}
	sel_def = Hash_FindElement (sel_def_hash, sel_def);
	if (sel_def)
		return sel_def->def;
	sel_def = malloc (sizeof (sel_def_t));
	sel_def->def = new_def (type_SEL.aux_type, ".imm", pr.scope);
	sel_def->def->initialized = sel_def->def->constant = 1;
	sel_def->def->nosave = 1;
	sel_def->def->ofs = new_location (type_SEL.aux_type, pr.near_data);
	EMIT_STRING (G_INT (sel_def->def->ofs), sel_id);
	EMIT_STRING (G_INT (sel_def->def->ofs + 1), sel_types);
	sel_def->sel_id = G_INT (sel_def->def->ofs);
	sel_def->sel_types = G_INT (sel_def->def->ofs + 1);
	Hash_AddElement (sel_def_hash, sel_def);
	return sel_def->def;
}

def_t *
emit_methods (methodlist_t *_methods, const char *name, int instance)
{
	const char *type = instance ? "INSTANCE" : "CLASS";
	method_t   *method;
	int         i, count;
	def_t      *methods_def;
	pr_method_list_t *methods;
	type_t     *method_list;

	if (!_methods)
		return 0;
	for (count = 0, method = _methods->head; method; method = method->next)
		if (!method->instance == !instance) {
			if (!method->def) {
				warning (0, "method %s not implemented", method->name);
			}
			count++;
		}
	if (!count)
		return 0;
	method_list = new_struct (0);
	new_struct_field (method_list, &type_pointer, "method_next", vis_public);
	new_struct_field (method_list, &type_integer, "method_count", vis_public);
	for (i = 0; i < count; i++)
		new_struct_field (method_list, type_Method.aux_type, 0, vis_public);
	methods_def = get_def (method_list, va ("_OBJ_%s_METHODS_%s", type, name),
						   pr.scope, st_static);
	methods_def->initialized = methods_def->constant = 1;
	methods_def->nosave = 1;
	methods = &G_STRUCT (pr_method_list_t, methods_def->ofs);
	methods->method_next = 0;
	methods->method_count = count;
	for (i = 0, method = _methods->head; method; method = method->next) {
		if (!method->instance != !instance || !method->def)
			continue;
		EMIT_STRING (methods->method_list[i].method_name.sel_id, method->name);
		EMIT_STRING (methods->method_list[i].method_name.sel_types, method->types);
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

void
clear_selectors (void)
{
	if (sel_def_hash)
		Hash_FlushTable (sel_def_hash);
}
