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
	progs_t    *pr;
	cbuf_t     *default_cbuf;
} cbuf_resources_t;

static cbuf_t * __attribute__((pure))
_get_cbuf (progs_t *pr, cbuf_resources_t *res, int arg, const char *func)
{
	cbuf_t     *cbuf = 0;

	if (arg == 0) {
		// a nil cbuf is valid only if the default cbuf has been set
		cbuf = res->default_cbuf;
	} else {
	}
	if (!cbuf) {
		PR_RunError (pr, "%s: Invalid cbuf_t: %d", func, arg);
	}

	return cbuf;
}
#define get_cbuf(pr, res, arg) _get_cbuf(pr, res, arg, __FUNCTION__)

#define bi(n) static void bi_##n (progs_t *pr, void *data)

bi(Cbuf_AddText)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	const char *text = P_GSTRING (pr, 1);
	Cbuf_AddText (cbuf, text);
}

bi(Cbuf_InsertText)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	const char *text = P_GSTRING (pr, 1);
	Cbuf_InsertText (cbuf, text);
}

bi(Cbuf_Execute)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Execute (cbuf);
}

bi(Cbuf_Execute_Sets)
{
	cbuf_t     *cbuf = get_cbuf (pr, data, P_INT (pr, 0));
	Cbuf_Execute_Sets (cbuf);
}

static void
bi_cbuf_clear (progs_t *pr, void *data)
{
}

static void
bi_cbuf_destroy (progs_t *pr, void *data)
{
	free (data);
}

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Cbuf_AddText,            2, p(ptr), p(string)),
	bi(Cbuf_InsertText,         2, p(ptr), p(string)),
	bi(Cbuf_Execute,            1, p(ptr)),
	bi(Cbuf_Execute_Sets,       1, p(ptr)),
	{0}
};

void
RUA_Cbuf_Init (progs_t *pr, int secure)
{
	cbuf_resources_t *res = calloc (sizeof (cbuf_resources_t), 1);
	res->pr = pr;
	PR_Resources_Register (pr, "Cbuf", res, bi_cbuf_clear, bi_cbuf_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}

VISIBLE void
RUA_Cbuf_SetCbuf (progs_t *pr, cbuf_t *cbuf)
{
	cbuf_resources_t *res = PR_Resources_Find (pr, "Cbuf");
	res->default_cbuf = cbuf;
}
