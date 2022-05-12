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

#include "QF/cbuf.h"
#include "QF/idparse.h" // For now, use the id console parser
#include "QF/progs.h"
#include "QF/ruamoko.h"

#include "rua_internal.h"

typedef struct {
	cbuf_t     *cbuf;
} cbuf_resources_t;

static cbuf_t * __attribute__((pure))
_get_cbuf (progs_t *pr, cbuf_resources_t *res, int arg, const char *func)
{
	cbuf_t     *cbuf = 0;

	if (arg == 0) {
		cbuf = res->cbuf;
	} else {
		PR_RunError (pr, "%s: Invalid cbuf_t", func);
	}
	if (!cbuf)
		PR_RunError (pr, "Invalid cbuf_t");

	return cbuf;
}
#define get_cbuf(pr, res, arg) _get_cbuf(pr, res, arg, __FUNCTION__)

static void
bi_Cbuf_AddText (progs_t *pr, void *data)
{
	const char *text = P_GSTRING (pr, 0);
	cbuf_t     *cbuf = get_cbuf (pr, data, 0);
	Cbuf_AddText (cbuf, text);
}

static void
bi_Cbuf_InsertText (progs_t *pr, void *data)
{
	const char *text = P_GSTRING (pr, 0);
	cbuf_t     *cbuf = get_cbuf (pr, data, 0);
	Cbuf_InsertText (cbuf, text);
}

static void
bi_Cbuf_Execute (progs_t *pr, void *data)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, 0);
	Cbuf_Execute (cbuf);
}

static void
bi_Cbuf_Execute_Sets (progs_t *pr, void *data)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, 0);
	Cbuf_Execute_Sets (cbuf);
}

static void
bi_cbuf_clear (progs_t *pr, void *data)
{
}

static void
bi_cbuf_destroy (progs_t *pr, void *data)
{
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Cbuf_AddText,      1, p(string)),
	bi(Cbuf_InsertText,   1, p(string)),
	bi(Cbuf_Execute,      0),
	bi(Cbuf_Execute_Sets, 0),
	{0}
};

void
RUA_Cbuf_Init (progs_t *pr, int secure)
{
	cbuf_resources_t *res = calloc (sizeof (cbuf_resources_t), 1);
	PR_Resources_Register (pr, "Cbuf", res, bi_cbuf_clear, bi_cbuf_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}

VISIBLE void
RUA_Cbuf_SetCbuf (progs_t *pr, cbuf_t *cbuf)
{
	cbuf_resources_t *res = PR_Resources_Find (pr, "Cbuf");
	res->cbuf = cbuf;
}
