/*
	method.c

	QC method support code

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

#include "QF/dstring.h"
#include "QF/hash.h"

#include "qfcc.h"

#include "class.h"
#include "method.h"
#include "type.h"

static def_t   *send_message_def;
static function_t *send_message_func;

method_t *
new_method (type_t *ret_type, param_t *selector, param_t *opt_parms)
{
	method_t   *meth = malloc (sizeof (method_t));
	param_t    *cmd = new_param (0, &type_pointer, "_cmd");
	param_t    *self = new_param (0, &type_id, "self");

	opt_parms = reverse_params (opt_parms);
	selector = _reverse_params (selector, opt_parms);
	cmd->next = selector;
	self->next = cmd;

	meth->next = 0;
	meth->instance = 0;
	meth->selector = selector;
	meth->params = self;
	meth->type = parse_params (ret_type, meth->params);
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
method_def (class_t *class, method_t *method)
{
	dstring_t  *str = dstring_newstr ();
	dstring_t  *sel = dstring_newstr ();
	def_t      *def;
	char       *s;

	selector_name (sel, (keywordarg_t *)method->selector);
	dsprintf (str, "_%c_%s_%s_%s",
			  method->instance ? 'i' : 'c',
			  class->class_name,
			  class->category_name ? class->category_name : "",
			  sel->str);
	for (s = str->str; *s; s++)
		if (*s == ':')
			*s = '_';
	//printf ("%s\n", str->str);
	// FIXME need a file scope
	def = PR_GetDef (method->type, str->str, 0, &numpr_globals);
	dstring_delete (str);
	dstring_delete (sel);
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

	for (s = src->head; s; s++) {
		d = malloc (sizeof (method_t));
		*d = *s;
		d->next = 0;
		add_method (dst, d);
	}
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

expr_t *
send_message (void)
{
	expr_t     *e;

	if (!send_message_def) {
		send_message_def = PR_GetDef (&type_IMP, "obj_msgSend",
									  0, &numpr_globals);
		send_message_func = new_function ();
		send_message_func->builtin = 0;
		send_message_func->def = send_message_def;
		build_function (send_message_func);
		finish_function (send_message_func);
	}
	e = new_expr ();
	e->type = ex_def;
	e->e.def = send_message_def;
	return e;
}

void
selector_name (dstring_t *sel_id, keywordarg_t *selector)
{
	dstring_clearstr (sel_id);
	while (selector && selector->selector) {
		dstring_appendstr (sel_id, selector->selector);
		dstring_appendstr (sel_id, ":");
		selector = selector->next;
	}
}

void
selector_types (dstring_t *sel_types, keywordarg_t *selector)
{
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

	hash = Hash_String (G_STRING (sel_def->sel_id))
		   ^ Hash_String (G_STRING (sel_def->sel_types));
	return hash;
}

static int
sel_def_compare (void *_sd1, void *_sd2, void *unused)
{
	sel_def_t  *sd1 = (sel_def_t*)_sd1;
	sel_def_t  *sd2 = (sel_def_t*)_sd2;
	int         cmp;

	cmp = strcmp (G_STRING (sd1->sel_id), G_STRING (sd2->sel_id)) == 0;
	if (cmp)
		cmp = strcmp (G_STRING (sd1->sel_types),
					  G_STRING (sd2->sel_types)) == 0;
	return cmp;
}

def_t *
selector_def (const char *_sel_id, const char *_sel_types)
{
	string_t    sel_id = ReuseString (_sel_id);
	string_t    sel_types = ReuseString (_sel_types);
	sel_def_t   _sel_def = {sel_id, sel_types, 0};
	sel_def_t  *sel_def = &_sel_def;

	if (!sel_def_hash) {
		sel_def_hash = Hash_NewTable (1021, 0, 0, 0);
		Hash_SetHashCompare (sel_def_hash, sel_def_get_hash, sel_def_compare);
	}
	sel_def = Hash_FindElement (sel_def_hash, sel_def);
	if (sel_def)
		return sel_def->def;
	sel_def = malloc (sizeof (sel_def_t));
	sel_def->sel_id = sel_id;
	sel_def->sel_types = sel_types;
	sel_def->def = PR_NewDef (type_SEL.aux_type, ".imm", 0);
	sel_def->def->ofs = PR_NewLocation (type_SEL.aux_type);
	G_INT (sel_def->def->ofs) = sel_id;
	G_INT (sel_def->def->ofs + 1) = sel_types;
	Hash_AddElement (sel_def_hash, sel_def);
	return sel_def->def;
}
