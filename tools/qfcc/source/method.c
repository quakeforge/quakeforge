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
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "shared.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

static hashtab_t *known_methods;

static const char *
method_get_key (const void *meth, void *unused)
{
	return ((method_t *) meth)->name;
}

static void
method_free (void *_meth, void *unused)
{
	method_t   *meth = (method_t *) _meth;

	free (meth->name);
	free (meth->types);
	free (meth);
}

method_t *
new_method (type_t *ret_type, param_t *selector, param_t *opt_params)
{
	method_t   *meth = malloc (sizeof (method_t));
	param_t    *cmd = new_param (0, &type_SEL, "_cmd");
	param_t    *self = new_param (0, &type_id, "self");
	dstring_t  *name = dstring_newstr ();
	dstring_t  *types = dstring_newstr ();

	selector = reverse_params (selector);
	selector = append_params (selector, opt_params);
	cmd->next = selector;
	self->next = cmd;

	meth->next = 0;
	meth->func = 0;
	meth->instance = 0;
	meth->selector = selector;
	meth->params = self;
	meth->type = parse_params (ret_type, meth->params);
	meth->type = find_type (meth->type);

	selector_name (name, (keywordarg_t *)selector);
	method_types (types, meth);
	meth->name = name->str;
	meth->types = types->str;
	free (name);
	free (types);

	//print_type (meth->type);
	meth->def = 0;

	if (!known_methods)
		known_methods = Hash_NewTable (1021, method_get_key, method_free, 0);
	Hash_Add (known_methods, meth);

	return meth;
}

const char *
method_name (method_t *method)
{
	return nva ("%c%s", method->instance ? '-' : '+', method->name);
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
	if (method->next)
		internal_error (0, "add_method: method loop detected");

	for (method_t *m = methodlist->head; m; m = m->next) {
		if (method_compare (m, method)) {
			debug (0, "dropping duplicate method: %s", method->name);
			return;
		}
	}
	*methodlist->tail = method;
	methodlist->tail = &method->next;
}

symbol_t *
method_symbol (class_type_t *class_type, method_t *method)
{
	dstring_t  *str = dstring_newstr ();
	symbol_t   *sym;
	char       *s;
	const char *class_name;

	class_name = get_class_name (class_type, 0);

	dsprintf (str, "_%c_%s_%s",
			  method->instance ? 'i' : 'c',
			  class_name,
			  method->name);
	for (s = str->str; *s; s++)
		if (*s == ':')
			*s = '_';
	//printf ("%s %s %s %ld\n", method->name, method->types, str->str,
	//		str->size);
	sym = new_symbol_type (str->str, method->type);
	sym = function_symbol (sym, 0, 1);
	sym->params = method->params;
	dstring_delete (str);
	return sym;
}

void
method_set_param_names (method_t *dst, method_t *src)
{
	param_t    *dp, *sp;

	for (dp = dst->params, sp = src->params; dp && sp;
		 dp = dp->next, sp = sp->next) {
		dp->name = sp->name;
	}
	if (dp || sp)
		internal_error (0, "missmatched method params");
}

methodlist_t *
new_methodlist (void)
{
	methodlist_t *l = malloc (sizeof (methodlist_t));
	l->head = 0;
	l->tail = &l->head;
	return l;
}

static uintptr_t
methodset_get_hash (const void *_method, void *unused)
{
	method_t   *method = (method_t *) _method;
	uintptr_t   hash;

	hash = Hash_String (method->name);
	return hash ^ (method->instance << 3);
}

static int
methodset_compare (const void *_m1, const void *_m2, void *unused)
{
	method_t   *m1 = (method_t *) _m1;
	method_t   *m2 = (method_t *) _m2;
	int         cmp;

	cmp = strcmp (m1->name, m2->name) == 0;
	return cmp && m1->instance == m2->instance;
}

methodset_t *
new_methodset (void)
{
	methodset_t *s = malloc (sizeof (*s));
	s->tab = Hash_NewTable (31, 0, 0, 0);
	Hash_SetHashCompare (s->tab, methodset_get_hash, methodset_compare);
	return s;
}

void
methodset_add_methods (methodset_t *methodset, methodlist_t *methods)
{
	method_t   *m;

	for (m = methods->head; m; m = m->next) {
		Hash_AddElement (methodset->tab, m);
	}
}

int
methodset_contains_method (methodset_t *methodset, method_t *method)
{
	return Hash_FindElement (methodset->tab, method) != 0;
}

static int __attribute__((pure))
method_in_list (methodlist_t *method_list, method_t *method)
{
	method_t    *m;

	for (m = method_list->head; m; m = m->next) {
		if (method_compare (m, method)) {
			return 1;
		}
	}
	return 0;
}

void
merge_method_lists (methodlist_t *dst, methodlist_t *src)
{
	while (src->head) {
		method_t   *s = src->head;
		src->head = s->next;
		s->next = 0;
		if (method_in_list (dst, s)) {
			debug (0, "dropping duplicate method: %s", s->name);
			free (s);
		} else {
			// add_method does the duplicate check
			*dst->tail = s;
			dst->tail = &s->next;
		}
	}
	free (src);
}

void
copy_methods (methodlist_t *dst, methodlist_t *src, methodset_t *except)
{
	method_t   *s, *d;
	param_t    *self;

	for (s = src->head; s; s = s->next) {
		if (methodset_contains_method (except, s) || method_in_list (dst, s)) {
			debug (0, "skipping duplicate method: %s", s->name);
			continue;
		}
		d = malloc (sizeof (method_t));
		*d = *s;
		// The above is only a shallow copy and thus even though the methods
		// are not shared between the source and destination lists, the
		// parameters are. Thus, duplicate the self (first) parameter so
		// changing its type to match the class into which it is inserted does
		// not affect the source list. The rest of the parameters do not need
		// to be copied as they will not be altered.
		self = malloc (sizeof (param_t));
		*self = *d->params;
		d->params = self;
		d->next = 0;
		// add_method does the duplicate check
		*dst->tail = d;
		dst->tail = &d->next;
	}
}

__attribute__((pure)) int
method_compare (method_t *m1, method_t *m2)
{
	if (m1->instance != m2->instance)
		return 0;
	return strcmp (m1->name, m2->name) == 0 && m1->type == m2->type;
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
	symbol_t   *sym;
	const char *sm_name = "obj_msgSend";
	type_t     *sm_type = &type_IMP;

	if (super) {
		sm_name = "obj_msgSend_super";
		sm_type = &type_supermsg;
	}
	sym = symtab_lookup (pr.symtab, sm_name);
	if (!sym) {
		symtab_t   *save = current_symtab;
		current_symtab = pr.symtab;
		sym = new_symbol_type (sm_name, sm_type);
		sym = function_symbol (sym, 0, 1);
		make_function (sym, 0, sym->table->space, sc_extern);
		current_symtab = save;
	}
	return new_symbol_expr (sym);
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

static uintptr_t
sel_get_hash (const void *_sel, void *unused)
{
	selector_t *sel = (selector_t *) _sel;
	uintptr_t   hash;

	hash = Hash_String (sel->name);
	return hash;
}

static int
sel_compare (const void *_s1, const void *_s2, void *unused)
{
	selector_t *s1 = (selector_t *) _s1;
	selector_t *s2 = (selector_t *) _s2;
	int         cmp;

	cmp = strcmp (s1->name, s2->name) == 0;
	return cmp;
}

static uintptr_t
sel_index_get_hash (const void *_sel, void *unused)
{
	selector_t *sel = (selector_t *) _sel;
	return sel->index;
}

static int
sel_index_compare (const void *_s1, const void *_s2, void *unused)
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
	selector_t  _sel = {0, 0, 0};

	if (sel->type != ex_expr && sel->e.expr.op != '&'
		&& sel->e.expr.type != &type_SEL) {
		error (sel, "not a selector");
		return 0;
	}
	_sel.index = expr_short (sel->e.expr.e2);
	_sel.index /= type_size (type_SEL.t.fldptr.type);
	return (selector_t *) Hash_FindElement (sel_index_hash, &_sel);
}

def_t *
emit_selectors (void)
{
	symbol_t   *sel_sym;
	def_t      *sel_def;
	type_t     *sel_type;
	pr_sel_t   *sel;
	selector_t **selectors, **s;

	if (!sel_index)
		return 0;

	sel_type = array_type (type_SEL.t.fldptr.type, sel_index);
	sel_sym = make_symbol ("_OBJ_SELECTOR_TABLE", sel_type,
						   pr.far_data, sc_static);
	if (!sel_sym->table)
		symtab_addsymbol (pr.symtab, sel_sym);
	sel_def = sel_sym->s.def;
	sel_def->initialized = sel_def->constant = 1;
	sel_def->nosave = 1;

	sel = D_POINTER (pr_sel_t, sel_def);

	selectors = (selector_t **) Hash_GetList (sel_hash);

	for (s = selectors; *s; s++) {
		EMIT_STRING (sel_def->space,  sel[(*s)->index].sel_id, (*s)->name);
		EMIT_STRING (sel_def->space,  sel[(*s)->index].sel_types, (*s)->types);
	}
	free (selectors);
	return sel_def;
}

static void
emit_methods_next (def_t *def, void *data, int index)
{
	if (def->type != &type_pointer)
		internal_error (0, "%s: expected pointer def", __FUNCTION__);
	D_INT (def) = 0;
}

static void
emit_methods_count (def_t *def, void *data, int index)
{
	methodlist_t *methods = (methodlist_t *) data;

	if (def->type != &type_integer)
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = methods->count;
}

static void
emit_methods_list_item (def_t *def, void *data, int index)
{
	methodlist_t *methods = (methodlist_t *) data;
	method_t   *m;
	pr_method_t *meth;

	if (!is_array (def->type) || def->type->t.array.type != &type_obj_method)
		internal_error (0, "%s: expected array of method def",
						__FUNCTION__);
	if (index < 0 || index >= methods->count)
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index, methods->count);

	meth = D_POINTER (pr_method_t, def);

	for (m = methods->head; m; m = m->next) {
		if (!m->instance != !methods->instance || !m->def)
			continue;
		if (!index--)
			break;
	}
	EMIT_STRING (def->space, meth->method_name, m->name);
	EMIT_STRING (def->space, meth->method_types, m->types);
	meth->method_imp = D_FUNCTION (m->def);
	if (m->func) {
		def_t       loc;
		loc.space = def->space;
		loc.offset = POINTER_OFS (def->space, &meth->method_imp);
		reloc_def_func (m->func, &loc);
	}
}

def_t *
emit_methods (methodlist_t *methods, const char *name, int instance)
{
	static struct_def_t methods_struct[] = {
		{"method_next",		&type_pointer, emit_methods_next},
		{"method_count",	&type_integer, emit_methods_count},
		{"method_list",		0,             emit_methods_list_item},
		{0, 0}
	};
	const char *type = instance ? "INSTANCE" : "CLASS";
	method_t   *m;
	int         count;

	if (!methods)
		return 0;

	for (count = 0, m = methods->head; m; m = m->next) {
		if (!m->instance == !instance) {
			if (!m->def && options.warnings.unimplemented) {
				warning (0, "Method `%c%s' not implemented",
						m->instance ? '-' : '+', m->name);
			}
			if (m->def)
				count++;
		}
	}
	if (!count)
		return 0;
	methods->count = count;
	methods->instance = instance;

	methods_struct[2].type = array_type (&type_obj_method, count);
	return emit_structure (va ("_OBJ_%s_METHODS_%s", type, name), 's',
						   methods_struct, 0, methods, sc_static);
}

static void
emit_method_list_count (def_t *def, void *data, int index)
{
	methodlist_t *methods = (methodlist_t *) data;

	if (def->type != &type_integer)
		internal_error (0, "%s: expected integer def", __FUNCTION__);
	D_INT (def) = methods->count;
}

static void
emit_method_list_item (def_t *def, void *data, int index)
{
	methodlist_t *methods = (methodlist_t *) data;
	method_t   *m;
	pr_method_description_t *desc;

	if (!is_array (def->type)
		|| def->type->t.array.type != &type_obj_method_description) {
		internal_error (0, "%s: expected array of method_description def",
						__FUNCTION__);
	}
	if (index < 0 || index >= methods->count)
		internal_error (0, "%s: out of bounds index: %d %d",
						__FUNCTION__, index, methods->count);

	desc = D_POINTER (pr_method_description_t, def);

	for (m = methods->head; m; m = m->next) {
		if (!m->instance != !methods->instance)
			continue;
		if (!index--)
			break;
	}
	EMIT_STRING (def->space, desc->name, m->name);
	EMIT_STRING (def->space, desc->types, m->types);
}

def_t *
emit_method_descriptions (methodlist_t *methods, const char *name,
						  int instance)
{
	static struct_def_t method_list_struct[] = {
		{"count",       &type_integer, emit_method_list_count},
		{"method_list", 0,             emit_method_list_item},
		{0, 0}
	};
	const char *type = instance ? "PROTOCOL_INSTANCE" : "PROTOCOL_CLASS";
	method_t   *m;
	int         count;

	if (!methods)
		return 0;

	for (count = 0, m = methods->head; m; m = m->next)
		if (!m->instance == !instance)
			count++;
	if (!count)
		return 0;

	methods->count = count;
	methods->instance = instance;

	method_list_struct[1].type = array_type (&type_obj_method_description,
											 count);
	return emit_structure (va ("_OBJ_%s_METHODS_%s", type, name), 's',
						   method_list_struct, 0, methods, sc_static);
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
	int         i, count, param_count;
	expr_t     *a, **arg_list, *err = 0;
	type_t     *mtype = method->type;

	if (mtype->t.func.num_params == -1)
		return 0;

	for (count = 0, a = args; a; a = a->next)
		count++;

	if (count > MAX_PARMS)
		return error (args, "more than %d parameters", MAX_PARMS);

	if (mtype->t.func.num_params >= 0)
		param_count = mtype->t.func.num_params;
	else
		param_count = -mtype->t.func.num_params - 1;

	if (count < param_count)
		return error (args, "too few arguments");
	if (mtype->t.func.num_params >= 0 && count > mtype->t.func.num_params)
		return error (args, "too many arguments");

	arg_list = malloc (count * sizeof (expr_t *));
	for (i = count - 1, a = args; a; a = a->next)
		arg_list[i--] = a;
	for (i = 2; i < count; i++) {
		expr_t     *e = arg_list[i];
		type_t     *t = get_type (e);

		if (!t)
			return e;

		if (i < param_count) {
			if (e->type != ex_nil)
				if (!type_assignable (mtype->t.func.param_types[i], t)) {
					err = param_mismatch (e, i - 1, method->name,
										  mtype->t.func.param_types[i], t);
				}
		} else {
			if (is_integer_val (e) && options.warnings.vararg_integer)
				warning (e, "passing integer consant into ... function");
		}
	}
	free (arg_list);
	return err;
}
