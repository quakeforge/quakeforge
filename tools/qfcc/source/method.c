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

#include "qfcc.h"

#include "class.h"
#include "method.h"
#include "type.h"

static def_t   *send_message_def;
static expr_t  *send_message_expr;

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
method_def (class_t *klass, method_t *method)
{
	dstring_t  *str = dstring_newstr ();
	dstring_t  *sel = dstring_newstr ();
	param_t    *p;

	dsprintf (str, "_%c_%s", method->instance ? 'i' : 'c', klass->name);
	for (p = method->selector; p && p->selector; p = p->next) {
		dstring_clearstr (sel);
		dsprintf (sel, "_%s", p->selector);
		dstring_appendstr (str, sel->str);
	}
	//printf ("%s\n", str->str);
	// FIXME need a file scope
	return PR_GetDef (method->type, str->str, 0, &numpr_globals);
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
	if (!send_message_def) {
		send_message_expr = new_expr ();
		send_message_expr->type = ex_def;
		send_message_expr->e.def = send_message_def;
	}
	return send_message_expr;
}
