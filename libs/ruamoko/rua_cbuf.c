/*
	bi_cbuf.c

	CSQC cbuf builtins

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <stdlib.h>
#include <string.h>

#include "QF/cbuf.h"
#include "QF/idparse.h" // For now, use the id console parser
#include "QF/progs.h"
#include "QF/ruamoko.h"

#include "rua_internal.h"

typedef struct bi_cbuf_interpreter_s {
	pr_func_t   construct;
	pr_func_t   destruct;
	pr_func_t   reset;
	pr_func_t   add;
	pr_func_t   insert;
	pr_func_t   execute;
	pr_func_t   complete;
} bi_cbuf_interpreter_t;

typedef struct biinterp_s {
	struct cbuf_resources_s *res;
	pr_ptr_t    interp;	// bi_cbuf_interpreter_t in progs memory
	pr_ptr_t    obj;
} biinterp_t;

typedef struct bicbuf_s {
	struct bicbuf_s *next;
	struct bicbuf_s **prev;
	pr_ptr_t    interp;	// bi_cbuf_interpreter_t in progs memory
	pr_ptr_t    obj;
	cbuf_t     *cbuf;
	struct cbuf_resources_s *res;
} bicbuf_t;

typedef struct cbuf_resources_s {
	progs_t    *pr;
	cbuf_t     *default_cbuf;
	PR_RESMAP (bicbuf_t) cbuf_map;
	PR_RESMAP (biinterp_t) interp_map;
	bicbuf_t   *cbufs;
} cbuf_resources_t;

static bicbuf_t *
cbuf_new (cbuf_resources_t *res)
{
	return PR_RESNEW (res->cbuf_map);
}

static void
cbuf_free (cbuf_resources_t *res, bicbuf_t *cbuf)
{
	PR_RESFREE (res->cbuf_map, cbuf);
}

static void
cbuf_reset (cbuf_resources_t *res)
{
	PR_RESRESET (res->cbuf_map);
}

static bicbuf_t *
cbuf_get (cbuf_resources_t *res, int index)
{
	return PR_RESGET (res->cbuf_map, index);
}

static int __attribute__ ((pure))
cbuf_index (cbuf_resources_t *res, bicbuf_t *cbuf)
{
	return PR_RESINDEX (res->cbuf_map, cbuf);
}

static cbuf_t * __attribute__ ((pure))
_get_cbuf (progs_t *pr, cbuf_resources_t *res, int arg, const char *func)
{
	qfZoneScoped (true);
	cbuf_t     *cbuf = 0;

	if (arg == 0) {
		// a nil cbuf is valid only if the default cbuf has been set
		cbuf = res->default_cbuf;
	} else {
		bicbuf_t   *bicbuf = cbuf_get (res, arg);
		if (bicbuf) {
			cbuf = bicbuf->cbuf;
		}
	}
	if (!cbuf) {
		PR_RunError (pr, "%s: Invalid cbuf_t: %d", func, arg);
	}

	return cbuf;
}
#define get_cbuf(pr, res, arg) _get_cbuf(pr, res, arg, __FUNCTION__)

#define bi(n) static void bi_##n (progs_t *pr, void *data)

static void
bi_cbi_construct (cbuf_t *cbuf)
{
	cbuf_resources_t *res = cbuf->data;
	bicbuf_t   *bicbuf = cbuf_new (res);
	*bicbuf = (bicbuf_t) {
		.next = res->cbufs,
		.prev = &res->cbufs,
		.cbuf = cbuf,
		.res = res,
	};
	if (res->cbufs) {
		res->cbufs->prev = &bicbuf->next;
	}
	res->cbufs = bicbuf;
	cbuf->data = bicbuf;
}

static void
bi__call_cbuf (progs_t *pr, pr_func_t func, pr_int_t cbuf)
{
	PR_PushFrame (pr);
	auto params = PR_SaveParams (pr);
	PR_SetupParams (pr, 1, 1);
	P_INT (pr, 0) = cbuf;
	PR_ExecuteProgram (pr, func);
	PR_RestoreParams (pr, params);
	PR_PopFrame (pr);
}

static void
bi__call_cbuf_string (progs_t *pr, pr_func_t func, pr_int_t cbuf,
					  const char *str)
{
	PR_PushFrame (pr);
	auto params = PR_SaveParams (pr);
	PR_SetupParams (pr, 2, 1);
	P_INT (pr, 0) = cbuf;
	P_STRING (pr, 1) = PR_SetTempString (pr, str);
	PR_ExecuteProgram (pr, func);
	PR_RestoreParams (pr, params);
	PR_PopFrame (pr);
}

static void
bi_cbi_destruct (cbuf_t *cbuf)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->destruct) {
		bi__call_cbuf (pr, interp->destruct, handle);
	}
}

static void
bi_cbi_reset (cbuf_t *cbuf)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->reset) {
		bi__call_cbuf (pr, interp->reset, handle);
	}
}

static void
bi_cbi_add (cbuf_t *cbuf, const char *str)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->add) {
		bi__call_cbuf_string (pr, interp->add, handle, str);
	}
}

static void
bi_cbi_insert (cbuf_t *cbuf, const char *str)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->insert) {
		bi__call_cbuf_string (pr, interp->insert, handle, str);
	}
}

static void
bi_cbi_execute (cbuf_t *cbuf)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->execute) {
		bi__call_cbuf (pr, interp->execute, handle);
	}
}

static void
bi_cbi_execute_sets (cbuf_t *cbuf)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->execute) {
		bi__call_cbuf (pr, interp->execute, handle);
	}
}

static const char**
bi_cbi_complete (cbuf_t *cbuf, const char *str)
{
	bicbuf_t   *bicbuf = cbuf->data;
	auto res = bicbuf->res;
	auto pr = res->pr;
	int  handle = cbuf_index (res, bicbuf);

	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->complete) {
		bi__call_cbuf_string (pr, interp->complete, handle, str);
	}
	return 0;
}


static cbuf_interpreter_t bi_cbuf_interp = {
	.construct = bi_cbi_construct,
	.destruct = bi_cbi_destruct,
	.reset = bi_cbi_reset,
	.add = bi_cbi_add,
	.insert = bi_cbi_insert,
	.execute = bi_cbi_execute,
	.execute_sets = bi_cbi_execute_sets,
	.complete = bi_cbi_complete,
};

bi(Cbuf_New)
{
	cbuf_resources_t *res = data;
	cbuf_t     *cbuf = _Cbuf_New (&bi_cbuf_interp, res);
	// bi_cbi_construct sets cbuf->data to the bicbuf_t object
	bicbuf_t   *bicbuf = cbuf->data;
	int         handle = cbuf_index (res, bicbuf);

	bicbuf->interp = P_POINTER (pr, 0);
	bicbuf->obj = P_POINTER (pr, 1);
	auto interp = &G_PACKED (pr, bi_cbuf_interpreter_t, bicbuf->interp);
	if (interp->construct) {
		bi__call_cbuf (pr, interp->construct, handle);
	}
	R_INT (pr) = handle;
}

bi(Cbuf_Delete)
{
	cbuf_resources_t *res = data;
	bicbuf_t   *cbuf = PR_RESGET (res->cbuf_map, P_INT (pr, 0));
	if (!cbuf) {
		PR_RunError (pr, "%s: Invalid cbuf_t", __FUNCTION__);
	}
	Cbuf_Delete (cbuf->cbuf);
	if (cbuf->next) {
		cbuf->next->prev = cbuf->prev;
	}
	*cbuf->prev = cbuf->next;
	cbuf_free (res, cbuf);
}

bi(Cbuf_Reset)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Reset (cbuf);
}

bi(Cbuf_AddText)
{
	qfZoneScoped (true);
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	const char *text = P_GSTRING (pr, 1);
	Cbuf_AddText (cbuf, text);
}

bi(Cbuf_InsertText)
{
	qfZoneScoped (true);
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	const char *text = P_GSTRING (pr, 1);
	Cbuf_InsertText (cbuf, text);
}

bi(Cbuf_Execute)
{
	qfZoneScoped (true);
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Execute (cbuf);
}

bi(Cbuf_Execute_Stack)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Execute_Stack (cbuf);
}

bi(Cbuf_Execute_Sets)
{
	qfZoneScoped (true);
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Execute_Sets (cbuf);
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Cbuf_New,                2, p(ptr), p(ptr)),
	bi(Cbuf_Delete,             1, p(ptr)),
	bi(Cbuf_Reset,              1, p(ptr)),
	bi(Cbuf_AddText,            2, p(ptr), p(string)),
	bi(Cbuf_InsertText,         2, p(ptr), p(string)),
	bi(Cbuf_Execute,            1, p(ptr)),
	bi(Cbuf_Execute_Stack,      1, p(ptr)),
	bi(Cbuf_Execute_Sets,       1, p(ptr)),
	{0}
};

static void
bi_cbuf_clear (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	cbuf_resources_t *res = data;
	for (bicbuf_t *cbuf = res->cbufs; cbuf; cbuf = cbuf->next) {
		Cbuf_Delete (cbuf->cbuf);
	}
	res->cbufs = 0;
	cbuf_reset (res);
}

static void
bi_cbuf_destroy (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	free (data);
}

void
RUA_Cbuf_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	cbuf_resources_t *res = calloc (sizeof (cbuf_resources_t), 1);
	res->pr = pr;
	PR_Resources_Register (pr, "Cbuf", res, bi_cbuf_clear, bi_cbuf_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}

VISIBLE void
RUA_Cbuf_SetCbuf (progs_t *pr, cbuf_t *cbuf)
{
	qfZoneScoped (true);
	cbuf_resources_t *res = PR_Resources_Find (pr, "Cbuf");
	res->default_cbuf = cbuf;
}
