/*
	cexpr-vars.c

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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
#include <stddef.h>
#include <math.h>

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/hash.h"

#include "libs/util/cexpr-parse.h"

VISIBLE void
cexpr_struct_getfield (const exprval_t *a, const exprval_t *b, exprval_t *c,
					   exprctx_t *ctx)
{
	__auto_type symtab = (exprtab_t *) a->type->data;
	__auto_type name = (const char *) b->value;
	__auto_type field = (exprsym_t *) Hash_Find (symtab->tab, name);
	exprval_t *val = 0;
	if (field) {
		val = cmemalloc (ctx->memsuper, sizeof (exprval_t));
		val->type = field->type;
		val->value = a->value + (ptrdiff_t) field->value;
	}
	*(exprval_t **) c->value = val;
}

VISIBLE binop_t cexpr_struct_binops[] = {
	{ '.', &cexpr_field, &cexpr_exprval, cexpr_struct_getfield },
	{}
};

VISIBLE void
cexpr_struct_pointer_getfield (const exprval_t *a, const exprval_t *b,
							   exprval_t *c, exprctx_t *ctx)
{
	// "dereference" the pointer
	exprval_t   struct_val = { a->type, *(void **) a->value };
	cexpr_struct_getfield (&struct_val, b, c, ctx);
}

VISIBLE binop_t cexpr_struct_pointer_binops[] = {
	{ '.', &cexpr_field, &cexpr_exprval, cexpr_struct_pointer_getfield },
	{}
};

static void
cvar_get (const exprval_t *a, const exprval_t *b, exprval_t *c, exprctx_t *ctx)
{
	__auto_type name = (const char *) b->value;
	exprval_t  *var = cexpr_cvar (name, ctx);
	if (!var) {
		cexpr_error (ctx, "unknown cvar %s", name);
	}
	*(exprval_t **) c->value = var;
}

static binop_t cvar_binops[] = {
	{ '.', &cexpr_field, &cexpr_exprval, cvar_get },
	{}
};

static exprtype_t cvar_type = {
	"cvar",
	sizeof (void *),	// ref to struct (will always be 0)
	cvar_binops,
	0,
};

VISIBLE exprval_t *
cexpr_cvar (const char *name, exprctx_t *ctx)
{
	cvar_t     *var = Cvar_FindVar (name);
	if (!var) {
		var = Cvar_FindAlias (name);
	}
	if (!var) {
		return 0;
	}

	exprtype_t *type = ctx->result->type;
	binop_t *cast = cexpr_find_cast (type, &cexpr_int);

	exprval_t  *val = 0;
	if (cast || val->type == &cexpr_int || val->type == &cexpr_uint) {
		val = cexpr_value (type, ctx);
		*(int *) val->value = var->int_val;
	} else if (val->type == &cexpr_float) {
		val = cexpr_value (type, ctx);
		*(float *) val->value = var->value;
	} else if (val->type == &cexpr_double) {
		val = cexpr_value (type, ctx);
		//FIXME cvars need to support double values
		*(double *) val->value = var->value;
	}
	return val;
}

VISIBLE exprval_t *
cexpr_cvar_struct (exprctx_t *ctx)
{
	exprval_t  *cvars = cexpr_value (&cvar_type, ctx);
	*(void **) cvars->value = 0;
	return cvars;
}

static const char *expr_getkey (const void *s, void *unused)
{
	__auto_type sym = (exprsym_t *) s;
	return sym->name;
}

void cexpr_init_symtab (exprtab_t *symtab, exprctx_t *ctx)
{
	exprsym_t  *sym;

	symtab->tab = Hash_NewTable (61, expr_getkey, 0, 0, ctx->hashlinks);
	for (sym = symtab->symbols; sym->name; sym++) {
		Hash_Add (symtab->tab, sym);
	}
}

VISIBLE exprval_t *
cexpr_value_reference (exprtype_t *type, void *data, exprctx_t *ctx)
{
	__auto_type ref = (exprval_t *) cmemalloc (ctx->memsuper,
											   sizeof (exprval_t));
	ref->type = type;
	ref->value = data;
	return ref;
}

VISIBLE exprval_t *
cexpr_value (exprtype_t *type, exprctx_t *ctx)
{
	exprval_t *val = cexpr_value_reference (type, 0, ctx);
	val->value = cmemalloc (ctx->memsuper, type->size);
	return val;
}
