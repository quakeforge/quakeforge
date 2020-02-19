/*
	bi_string.c

	QuakeC string api

	Copyright (C) 2004 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/1/6

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

#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/dstring.h"
#include "QF/progs.h"

#include "rua_internal.h"

static void
bi_str_new (progs_t *pr)
{
	R_STRING (pr) = PR_NewMutableString (pr);
}

static void
bi_str_free (progs_t *pr)
{
	PR_FreeString (pr, P_STRING (pr, 0));
}

static void
bi_str_copy (progs_t *pr)
{
	dstring_t  *dst = P_DSTRING (pr, 0);
	const char *src = P_GSTRING (pr, 1);

	dstring_copystr (dst, src);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
bi_str_cat (progs_t *pr)
{
	dstring_t  *dst = P_DSTRING (pr, 0);
	const char *src = P_GSTRING (pr, 1);

	dstring_appendstr (dst, src);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
bi_str_clear (progs_t *pr)
{
	dstring_t  *str = P_DSTRING (pr, 0);

	dstring_clearstr (str);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
bi_str_mid (progs_t *pr)
{
	const char *str = P_GSTRING (pr, 0);
	int         pos = P_INT (pr, 1);
	int         end = P_INT (pr, 2);
	int         size = strlen (str);
	char       *temp;

	if (pr->pr_argc == 2)
		end = size;

	R_STRING (pr) = 0;
	if (pos < 0)
		pos += size;
	if (end < 0)
		end += size;
	if (end > size)
		end = size;
	if (pos < 0 || pos >= size || end <= pos)
		return;
	if (end == size) {
		R_STRING (pr) = str + pos - pr->pr_strings;
		return;
	}
	temp = alloca (end - pos + 1);
	strncpy (temp, str + pos, end - pos);
	temp[end - pos] = 0;
	RETURN_STRING (pr, temp);
}

static void
bi_str_str (progs_t *pr)
{
	const char *haystack = P_GSTRING (pr, 0);
	const char *needle = P_GSTRING (pr, 1);
	char       *res = strstr (haystack, needle);

	R_STRING (pr) = 0;
	if (res)
		R_STRING (pr) = res - pr->pr_strings;
}

static builtin_t builtins[] = {
	{"str_new",		bi_str_new,		-1},
	{"str_free",	bi_str_free,	-1},
	{"str_copy",	bi_str_copy,	-1},
	{"str_cat",		bi_str_cat,		-1},
	{"str_clear",	bi_str_clear,	-1},
	{"str_mid|*i",	bi_str_mid,		-1},
	{"str_mid|*ii",	bi_str_mid,		-1},
	{"str_str",		bi_str_str,		-1},
	{0}
};

void
RUA_String_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
