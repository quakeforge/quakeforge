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

#include "qfcc.h"

#include "debug.h"
#include "def.h"
#include "expr.h"
#include "function.h"
#include "immediate.h"
#include "opcodes.h"
#include "type.h"

param_t *
new_param (const char *selector, type_t *type, const char *name)
{
	param_t    *param = malloc (sizeof (param_t));

	param->next = 0;
	param->selector = selector;
	param->type = type;
	param->name = name;

	return param;
}

param_t *
_reverse_params (param_t *params, param_t *next)
{
	param_t    *p = params;
	if (params->next)
		p = _reverse_params (params->next, params);
	params->next = next;
	return p;
}

param_t *
reverse_params (param_t *params)
{
	if (!params)
		return 0;
	return _reverse_params (params, 0);
}

type_t *
parse_params (type_t *type, param_t *parms)
{
	param_t    *p;
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_func;
	new.aux_type = type;
	new.num_parms = 0;

	for (p = parms; p; p = p->next) {
		if (new.num_parms > MAX_PARMS) {
			error (0, "too many params");
			return type;
		}
		if (!p->selector && !p->type && !p->name) {
			if (p->next) {
				error (0, "internal error");
				abort ();
			}
			new.num_parms = -(new.num_parms + 1);
		} else if (p->type) {
			new.parm_types[new.num_parms] = p->type;
			new.num_parms++;
		}
	}
	//print_type (&new); puts("");
	return find_type (&new);
}

void
build_scope (function_t *f, def_t *func, param_t *params)
{
	int         i;
	def_t      *def;
	param_t    *p;
	def_t      *argv = 0;

	func->alloc = &func->locals;

	if (func->type->num_parms < 0) {
		def = PR_GetDef (&type_integer, ".argc", func, func->alloc);
		def->used = 1;
		PR_DefInitialized (def);
		argv = PR_GetDef (&type_pointer, ".argv", func, func->alloc);
		argv->used = 1;
		PR_DefInitialized (argv);
	}

	for (p = params, i = 0; p; p = p->next) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		def = PR_GetDef (p->type, p->name, func, func->alloc);
		f->parm_ofs[i] = def->ofs;
		if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i - 1]) {
			error (0, "bad parm order");
			abort ();
		}
		//printf ("%s%s %d\n", p == params ? "" : "    ", p->name, def->ofs);
		def->used = 1;				// don't warn for unused params
		PR_DefInitialized (def);	// params are assumed to be initialized
		i++;
	}

	if (argv) {
		while (i < MAX_PARMS) {
			def = PR_GetDef (&type_vector, 0, func, func->alloc);
			def->used = 1;
			if (argv->type == &type_pointer)
				argv->type = array_type (&type_vector, MAX_PARMS - i);
			i++;
		}
	}
}

function_t *
new_function (void)
{
	function_t	*f;

	f = calloc (1, sizeof (function_t));
	*pr.func_tail = f;
	pr.func_tail = &f->next;

	return f;
}

void
build_builtin_function (def_t *def, expr_t *bi_val)
{
	function_t *f;

	if (def->type->type != ev_func) {
		error (bi_val, "%s is not a function", def->name);
		return;
	}
	
	if (bi_val->type != ex_integer && bi_val->type != ex_float) {
		error (bi_val, "invalid constant for = #");
		return;
	}

	f = new_function ();

	f->builtin = bi_val->type == ex_integer ? bi_val->e.integer_val
											: (int)bi_val->e.float_val;
	f->def = def;
	build_function (f);
	finish_function (f);
}

void
build_function (function_t *f)
{
	f->def->constant = 1;
	f->def->initialized = 1;
	G_FUNCTION (f->def->ofs) = pr.num_functions;
}

void
finish_function (function_t *f)
{
	dfunction_t *df;
	int i, count;

	df = calloc (1, sizeof (dfunction_t));
	pr.num_functions++;
	f->dfunc = df;

	if (f->builtin)
		df->first_statement = -f->builtin;
	else
		df->first_statement = f->code;

	df->s_name = ReuseString (f->def->name);
	df->s_file = s_file;
	df->numparms = f->def->type->num_parms;
	df->locals = f->def->locals;
	df->parm_start = 0;
	if ((count = df->numparms) < 0)
		count = -count - 1;
	for (i = 0; i < count; i++)
		df->parm_size[i] = type_size (f->def->type->parm_types[i]);

	if (f->aux) {
		def_t *def;
		f->aux->function = df - pr.functions;
		for (def = f->def->scope_next; def; def = def->scope_next) {
			if (def->name) {
				ddef_t *d = new_local ();
				d->type = def->type->type;
				d->ofs = def->ofs;
				d->s_name = ReuseString (def->name);

				f->aux->num_locals++;
			}
		}
	}
}

void
emit_function (function_t *f, expr_t *e)
{
	//printf (" %s =\n", f->def->name);

	if (f->aux)
		lineno_base = f->aux->source_line;

	pr_scope = f->def;
	while (e) {
		//printf ("%d ", pr_source_line);
		//print_expr (e);
		//puts("");

		emit_expr (e);
		e = e->next;
	}
	emit_statement (pr_source_line, op_done, 0, 0, 0);
	PR_FlushScope (pr_scope, 0);
	pr_scope = 0;
	PR_ResetTempDefs ();

	//puts ("");
}
