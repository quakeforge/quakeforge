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
#include <ctype.h>

#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/dstring.h"
#include "QF/progs.h"

#include "rua_internal.h"

static void
bi_strlen (progs_t *pr)
{
	const char	*s;

	s = P_GSTRING (pr, 0);
	R_INT (pr) = strlen(s);
}

static void
bi_sprintf (progs_t *pr)
{
	const char *fmt = P_GSTRING (pr, 0);
	int         count = pr->pr_argc - 1;
	pr_type_t **args = pr->pr_params + 1;
	dstring_t  *dstr;

	dstr = dstring_newstr ();
	PR_Sprintf (pr, dstr, "bi_sprintf", fmt, count, args);
	RETURN_STRING (pr, dstr->str);
	dstring_delete (dstr);
}

static void
bi_vsprintf (progs_t *pr)
{
	const char *fmt = P_GSTRING (pr, 0);
	__auto_type args = &P_PACKED (pr, pr_va_list_t, 1);
	pr_type_t  *list_start = PR_GetPointer (pr, args->list);
	pr_type_t **list = alloca (args->count * sizeof (*list));
	dstring_t  *dstr;

	for (int i = 0; i < args->count; i++) {
		list[i] = list_start + i * pr->pr_param_size;
	}

	dstr = dstring_newstr ();
	PR_Sprintf (pr, dstr, "bi_vsprintf", fmt, args->count, list);
	RETURN_STRING (pr, dstr->str);
	dstring_delete (dstr);
}

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
bi_str_hold (progs_t *pr)
{
	string_t    str = P_STRING (pr, 0);
	PR_HoldString (pr, str);
	R_STRING (pr) = str;
}

static void
bi_str_valid (progs_t *pr)
{
	R_INT (pr) = PR_StringValid (pr, P_STRING (pr, 0));
}

static void
bi_str_mutable (progs_t *pr)
{
	R_INT (pr) = PR_StringMutable (pr, P_STRING (pr, 0));
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

static void
bi_str_char (progs_t *pr)
{
	const char *str = P_GSTRING (pr, 0);
	int         ind = P_INT (pr, 1);

	if (ind < 0) {
		PR_RunError (pr, "negative index to str_char");
	}
	R_INT (pr) = str[ind];
}

static void
bi_str_quote (progs_t *pr)
{
	const char *str = P_GSTRING (pr, 0);
	// can have up to 4 chars per char (a -> \x61)
	char       *quote = alloca (strlen (str) * 4 + 1);
	char       *q = quote;
	char        c;
	int         h;

	while ((c = *str++)) {
		if (c >= ' ' && c < 127 && c != '\"') {
			*q++ = c;
		} else {
			*q++ = '\\';
			switch (c) {
				case '\a': c = 'a'; break;
				case '\b': c = 'b'; break;
				case '\f': c = 'f'; break;
				case '\n': c = 'n'; break;
				case '\r': c = 'r'; break;
				case '\t': c = 't'; break;
				case '\"': c = '\"'; break;
				default:
					*q++ = 'x';
					h = (c & 0xf0) >> 4;
					*q++ = h > 9 ? h + 'a' - 10 : h + '0';
					h = (c & 0x0f);
					c = h > 9 ? h + 'a' - 10 : h + '0';
					break;
			}
			*q++ = c;
		}
	}
	*q++ = 0;

	RETURN_STRING (pr, quote);
}

static void
bi_str_lower (progs_t *pr)
{
	const char *str = P_GSTRING (pr, 0);
	char       *lower = alloca (strlen (str) + 1);
	char       *l = lower;
	byte        c;

	while ((c = *str++)) {
		*l++ = tolower (c);
	}
	*l++ = 0;

	RETURN_STRING (pr, lower);
}

static builtin_t builtins[] = {
	{"strlen",		bi_strlen,		-1},
	{"sprintf",		bi_sprintf,		-1},
	{"vsprintf",	bi_vsprintf,	-1},
	{"str_new",		bi_str_new,		-1},
	{"str_free",	bi_str_free,	-1},
	{"str_hold",	bi_str_hold,	-1},
	{"str_valid",	bi_str_valid,	-1},
	{"str_mutable",	bi_str_mutable,	-1},
	{"str_copy",	bi_str_copy,	-1},
	{"str_cat",		bi_str_cat,		-1},
	{"str_clear",	bi_str_clear,	-1},
	{"str_mid|*i",	bi_str_mid,		-1},
	{"str_mid|*ii",	bi_str_mid,		-1},
	{"str_str",		bi_str_str,		-1},
	{"str_char",	bi_str_char,	-1},
	{"str_quote",	bi_str_quote,	-1},
	{"str_lower",	bi_str_lower,	-1},
	{0}
};

void
RUA_String_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
