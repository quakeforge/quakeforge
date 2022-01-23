/*
	test-bi.c

	Builtin functions for the test harness.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/22

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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/progs.h"

#include "test-bi.h"

static void
bi_printf (progs_t *pr)
{
	const char *fmt = P_GSTRING (pr, 0);
	int         count = pr->pr_argc - 1;
	pr_type_t **args = pr->pr_params + 1;
	static dstring_t *dstr;

	if (!dstr)
		dstr = dstring_new ();
	else
		dstring_clear (dstr);

	PR_Sprintf (pr, dstr, "bi_printf", fmt, count, args);
	if (dstr->str)
		fputs (dstr->str, stdout);
}

static void
bi_errno (progs_t *pr)
{
	R_INT (pr) = errno;
}

static void
bi_strerror (progs_t *pr)
{
	int err = P_INT (pr, 0);
	RETURN_STRING (pr, strerror (err));
}

static void
bi_exit (progs_t *pr)
{
	exit (P_INT (pr, 0));
}

static void
bi_spawn (progs_t *pr)
{
	edict_t    *ed;
	ed = ED_Alloc (pr);
	RETURN_EDICT (pr, ed);
}

static void
bi_remove (progs_t *pr)
{
	edict_t    *ed;
	ed = P_EDICT (pr, 0);
	ED_Free (pr, ed);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(printf,   -2, p(string)),
	bi(errno,    0),
	bi(strerror, 1, p(int)),
	bi(exit,     1, p(int)),
	bi(spawn,    0),
	bi(remove,   1, p(entity)),
	{0}
};

void
BI_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins);
}
