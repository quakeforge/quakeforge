/*
	va.c

	varargs printf function

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>

#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/va.h"

struct va_ctx_s {
	dstring_t **strings;
	int         num_strings;
	int         str_index;
};

static va_ctx_t *default_va_ctx;

VISIBLE va_ctx_t *
va_create_context (int buffers)
{
	va_ctx_t   *ctx;

	ctx = malloc (sizeof (va_ctx_t) + buffers * sizeof (dstring_t *));
	ctx->strings = (dstring_t **) (ctx + 1);
	ctx->num_strings = buffers;
	ctx->str_index = 0;

	for (int i = 0; i < buffers; i++) {
		ctx->strings[i] = dstring_new ();
	}
	return ctx;
}

VISIBLE void
va_destroy_context (va_ctx_t *ctx)
{
	if (!ctx) {
		ctx = default_va_ctx;
		default_va_ctx = 0;
		if (!ctx) {
			return;
		}
	}
	for (int i = 0; i < ctx->num_strings; i++) {
		dstring_delete (ctx->strings[i]);
	}
	free (ctx);
}

VISIBLE const char *
va (va_ctx_t *ctx, const char *fmt, ...)
{
	va_list     args;
	dstring_t  *dstr;

	if (!ctx) {
		if (!default_va_ctx) {
			default_va_ctx = va_create_context (4);
		}
		ctx = default_va_ctx;
	}
	dstr = ctx->strings[ctx->str_index++ % ctx->num_strings];

	va_start (args, fmt);
	dvsprintf (dstr, fmt, args);
	va_end (args);

	return dstr->str;
}

VISIBLE char *
nva (const char *fmt, ...)
{
	va_list     args;
	dstring_t  *string;

	string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	return dstring_freeze (string);
}

static void
va_shutdown (void *data)
{
	if (default_va_ctx) {
		va_destroy_context (default_va_ctx);
	}
}

static void __attribute__((constructor))
va_init (void)
{
	Sys_RegisterShutdown (va_shutdown, 0);
}
