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
cexpr_array_getelement (const exprval_t *a, const exprval_t *b, exprval_t *c,
						exprctx_t *ctx)
{
	__auto_type array = (exprarray_t *) a->type->data;
	unsigned    index = *(const unsigned *) b->value;
	exprval_t *val = 0;
	if (index < array->size) {
		val = cmemalloc (ctx->memsuper, sizeof (exprval_t));
		val->type = array->type;
		val->value = a->value + array->type->size * index;
	} else {
		cexpr_error (ctx, "index %d out of bounds for %s", index,
					 a->type->name);
	}
	*(exprval_t **) c->value = val;
}

VISIBLE binop_t cexpr_array_binops[] = {
	{ '[', &cexpr_int, &cexpr_exprval, cexpr_array_getelement },
	{}
};

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
	} else {
		cexpr_error (ctx, "%s has no field %s", a->type->name, name);
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
	.name = "cvar",
	.size = sizeof (void *),	// ref to struct (will always be 0)
	.binops = cvar_binops,
	.unops = 0,
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
	if (var->value.type) {
		exprval_t  *val = cexpr_value (type, ctx);
		cexpr_assign_value (val, &var->value, ctx);
		return val;
	}
	cexpr_error (ctx, "cvar %s has unknown type", var->name);
	return 0;
}

VISIBLE exprval_t *
cexpr_cvar_struct (exprctx_t *ctx)
{
	exprval_t  *cvars = cexpr_value (&cvar_type, ctx);
	*(void **) cvars->value = 0;
	return cvars;
}

static const char *
expr_getkey (const void *s, void *unused)
{
	__auto_type sym = (exprsym_t *) s;
	return sym->name;
}

void
cexpr_init_symtab (exprtab_t *symtab, exprctx_t *ctx)
{
	exprsym_t  *sym;

	if (!symtab->tab) {
		symtab->tab = Hash_NewTable (61, expr_getkey, 0, 0, ctx->hashctx);
		for (sym = symtab->symbols; sym->name; sym++) {
			Hash_Add (symtab->tab, sym);
		}
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
