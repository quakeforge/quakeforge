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
#include <stdlib.h>

#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/dstring.h"
#include "QF/progs.h"

#include "rua_internal.h"

typedef struct str_resources_s {
	dstring_t  *printbuf;
} str_resources_t;

static void
bi_strlen (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char	*s;

	s = P_GSTRING (pr, 0);
	R_INT (pr) = strlen(s);
}

void
RUA_Sprintf (progs_t *pr, dstring_t *dstr, const char *func, int fmt_arg)
{
	qfZoneScoped (true);
	const char *fmt = P_GSTRING (pr, fmt_arg);
	int         count = pr->pr_argc - (fmt_arg + 1);
	pr_type_t **args = pr->pr_params + (fmt_arg + 1);

	if (pr->progs->version == PROG_VERSION) {
		__auto_type va_list = &P_PACKED (pr, pr_va_list_t, (fmt_arg + 1));
		count = va_list->count;
		if (count) {
			args = alloca (count * sizeof (pr_type_t *));
			for (int i = 0; i < count; i++) {
				args[i] = &pr->pr_globals[va_list->list + i * 4];
			}
		} else {
			args = 0;
		}
	}

	PR_Sprintf (pr, dstr, func, fmt, count, args);
}

static void
bi_sprintf (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	str_resources_t *res = _res;
	dstring_clearstr (res->printbuf);
	RUA_Sprintf (pr, res->printbuf, "sprintf", 0);
	RETURN_STRING (pr, res->printbuf->str);
}

static void
bi_vsprintf (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	str_resources_t *res = _res;
	const char *fmt = P_GSTRING (pr, 0);
	__auto_type args = &P_PACKED (pr, pr_va_list_t, 1);
	pr_type_t  *list_start = PR_GetPointer (pr, args->list);
	pr_type_t **list = alloca (args->count * sizeof (*list));

	for (int i = 0; i < args->count; i++) {
		list[i] = list_start + i * pr->pr_param_size;
	}

	dstring_clearstr (res->printbuf);
	PR_Sprintf (pr, res->printbuf, "bi_vsprintf", fmt, args->count, list);
	RETURN_STRING (pr, res->printbuf->str);
}

static void
bi_str_new (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_STRING (pr) = PR_NewMutableString (pr);
}

static void
bi_str_unmutable (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	RETURN_STRING (pr, P_GSTRING (pr, 0));
}

static void
bi_str_free (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	PR_FreeString (pr, P_STRING (pr, 0));
}

static void
bi_str_hold (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr_string_t str = P_STRING (pr, 0);
	PR_HoldString (pr, str);
	R_STRING (pr) = str;
}

static void
bi_str_valid (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_INT (pr) = PR_StringValid (pr, P_STRING (pr, 0));
}

static void
bi_str_mutable (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_INT (pr) = PR_StringMutable (pr, P_STRING (pr, 0));
}

static void
bi_str_copy (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	dstring_t  *dst = P_DSTRING (pr, 0);
	const char *src = P_GSTRING (pr, 1);

	dstring_copystr (dst, src);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
bi_str_cat (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	dstring_t  *dst = P_DSTRING (pr, 0);
	const char *src = P_GSTRING (pr, 1);

	dstring_appendstr (dst, src);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
bi_str_clear (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	dstring_t  *str = P_DSTRING (pr, 0);

	dstring_clearstr (str);
	R_STRING (pr) = P_STRING (pr, 0);
}

static void
str_mid (progs_t *pr, const char *str, int pos, int end, int size)
{
	qfZoneScoped (true);
	char       *temp;

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
bi_str_mid_2 (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	int         pos = P_INT (pr, 1);
	int         size = strlen (str);

	str_mid (pr, str, pos, size, size);
}

static void
bi_str_mid_3 (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	int         pos = P_INT (pr, 1);
	int         end = P_INT (pr, 2);
	int         size = strlen (str);

	str_mid (pr, str, pos, end, size);
}

static void
bi_str_str (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *haystack = P_GSTRING (pr, 0);
	const char *needle = P_GSTRING (pr, 1);
	char       *res = strstr (haystack, needle);

	R_INT (pr) = -1;
	if (res) {
		R_INT (pr) = res - haystack;
	}
}

static void
bi_strchr (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *s = P_GSTRING (pr, 0);
	int         c = P_INT (pr, 1);
	char       *res = strchr (s, c);

	R_INT (pr) = -1;
	if (res) {
		R_INT (pr) = res - s;
	}
}

static void
bi_strrchr (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *s = P_GSTRING (pr, 0);
	int         c = P_INT (pr, 1);
	char       *res = strrchr (s, c);

	R_INT (pr) = -1;
	if (res) {
		R_INT (pr) = res - s;
	}
}

static void
bi_str_char (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	int         ind = P_INT (pr, 1);

	if (ind < 0) {
		PR_RunError (pr, "negative index to str_char");
	}
	R_INT (pr) = str[ind];
}

static void
bi_str_quote (progs_t *pr, void *data)
{
	qfZoneScoped (true);
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
bi_str_lower (progs_t *pr, void *data)
{
	qfZoneScoped (true);
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

static void
bi_str_upper (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	char       *upper = alloca (strlen (str) + 1);
	char       *l = upper;
	byte        c;

	while ((c = *str++)) {
		*l++ = toupper (c);
	}
	*l++ = 0;

	RETURN_STRING (pr, upper);
}

static void
bi_strtod (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	pr_type_t  *end = P_GPOINTER (pr, 1);
	char       *end_ptr;
	R_DOUBLE (pr) = strtod (str, &end_ptr);
	if (end) {
		end->value = end_ptr - str;
	}
}

static void
bi_strtof (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	pr_type_t  *end = P_GPOINTER (pr, 1);
	char       *end_ptr;
	R_FLOAT (pr) = strtof (str, &end_ptr);
	if (end) {
		end->value = end_ptr - str;
	}
}

static void
bi_strtol (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	pr_type_t  *end = P_GPOINTER (pr, 1);
	char       *end_ptr;
	R_LONG (pr) = strtol (str, &end_ptr, P_INT (pr, 2));
	if (end) {
		end->value = end_ptr - str;
	}
}

static void
bi_strtoul (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *str = P_GSTRING (pr, 0);
	pr_type_t  *end = P_GPOINTER (pr, 1);
	char       *end_ptr;
	R_ULONG (pr) = strtoul (str, &end_ptr, P_INT (pr, 2));
	if (end) {
		end->value = end_ptr - str;
	}
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(strlen,      1, p(string)),
	bi(sprintf,     -2, p(string)),
	bi(vsprintf,    2, p(string), P(1, 2)),
	bi(str_new,     0),
	bi(str_unmutable,   1, p(string)),
	bi(str_free,    1, p(string)),
	bi(str_hold,    1, p(string)),
	bi(str_valid,   1, p(string)),
	bi(str_mutable, 1, p(string)),
	bi(str_copy,    2, p(string), p(string)),
	bi(str_cat,     2, p(string), p(string)),
	bi(str_clear,   1, p(string)),
	{"str_mid|*i",	bi_str_mid_2,		-1, 2, {p(string), p(int)}},
	{"str_mid|*ii",	bi_str_mid_3,		-1, 3, {p(string), p(int), p(int)}},
	bi(str_str,     2, p(string), p(string)),
	bi(strchr,      2, p(string), p(int)),
	bi(strrchr,     2, p(string), p(int)),
	bi(str_char,    2, p(string), p(int)),
	bi(str_quote,   1, p(string)),
	bi(str_lower,   1, p(string)),
	bi(str_upper,   1, p(string)),
	bi(strtod,      1, p(string)),
	bi(strtof,      1, p(string)),
	bi(strtol,      1, p(string)),
	bi(strtoul,     1, p(string)),
	{0}
};

static void
rua_string_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
}

static void
rua_string_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	str_resources_t *res = _res;
	dstring_delete (res->printbuf);
	free (res);
}

void
RUA_String_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	str_resources_t *res = malloc (sizeof (str_resources_t));
	res->printbuf = dstring_newstr ();

	PR_Resources_Register (pr, "string", res, rua_string_clear,
						   rua_string_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
