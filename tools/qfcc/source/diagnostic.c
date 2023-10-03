/*
	diagnostic.c

	Diagnostic messages.

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/24

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

#include <QF/dstring.h>

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/strpool.h"

diagnostic_hook bug_hook;
diagnostic_hook error_hook;
diagnostic_hook warning_hook;
diagnostic_hook notice_hook;

static void
report_function (const expr_t *e)
{
	static function_t *last_func = (function_t *)-1L;
	static pr_string_t last_file;
	pr_string_t file = pr.source_file;
	srcline_t  *srcline;

	if (e)
		file = e->file;

	if (file != last_file) {
		for (srcline = pr.srcline_stack; srcline; srcline = srcline->next)
			fprintf (stderr, "In file included from %s:%d:\n",
					 GETSTR (srcline->source_file), srcline->source_line);
	}
	last_file = file;
	if (current_func != last_func) {
		if (current_func) {
			fprintf (stderr, "%s: In function `%s':\n", GETSTR (file),
					 current_func->name);
		} else if (current_class) {
			fprintf (stderr, "%s: In class `%s':\n", GETSTR (file),
					 get_class_name (current_class, 1));
		} else {
			fprintf (stderr, "%s: At top level:\n", GETSTR (file));
		}
	}
	last_func = current_func;
}

void
print_srcline (int rep, const expr_t *e)
{
	pr_string_t file = pr.source_file;
	int         line = pr.source_line;
	if (e) {
		file = e->file;
		line = e->line;
	}
	if (rep) {
		report_function (e);
	}
	printf ("%s:%d\n", GETSTR (file), line);
}

static __attribute__((format(PRINTF, 4, 0))) void
format_message (dstring_t *message, const char *msg_type, const expr_t *e,
				const char *fmt, va_list args)
{
	pr_string_t file = pr.source_file;
	int         line = pr.source_line;
	const char *colon = fmt ? ": " : "";

	if (e) {
		file = e->file;
		line = e->line;
	}
	dsprintf (message, "%s:%d: %s%s", GETSTR (file), line, msg_type, colon);
	if (fmt) {
		davsprintf (message, fmt, args);
	}
}

static __attribute__((format(PRINTF, 5, 0))) void
__warning (const expr_t *e, const char *file, int line, const char *func,
		   const char *fmt, va_list args)
{
	static int  promoted = 0;
	dstring_t  *message = dstring_new ();

	report_function (e);
	if (options.warnings.promote) {
		if (!promoted) {
			promoted = 1;	// want to do this only once
			fprintf (stderr, "%s: warnings treated as errors\n", "qfcc");
		}
		pr.error_count++;
		format_message (message, "error", e, fmt, args);
	} else {
		format_message (message, "warning", e, fmt, args);
	}

	if (options.verbosity > 0) {
		dasprintf (message, " (%s:%d in %s)", file, line, func);
	}
	if (warning_hook) {
		warning_hook (message->str);
	} else {
		fprintf (stderr, "%s\n", message->str);
	}
	dstring_delete (message);
}

void
_debug (const expr_t *e, const char *file, int line, const char *func,
		const char *fmt, ...)
{
	va_list     args;

	if (options.verbosity < 2)
		return;

	report_function (e);
	va_start (args, fmt);
	{
		dstring_t  *message = dstring_new ();

		format_message (message, "debug", e, fmt, args);
		dasprintf (message, " (%s:%d in %s)", file, line, func);
		fprintf (stderr, "%s\n", message->str);
		dstring_delete (message);
	}
	va_end (args);
}

static __attribute__((noreturn, format(PRINTF, 5, 0))) void
__internal_error (const expr_t *e, const char *file, int line,
				  const char *func, const char *fmt, va_list args)
{
	dstring_t  *message = dstring_new ();

	report_function (e);

	format_message (message, "internal error", e, fmt, args);
	dasprintf (message, " (%s:%d in %s)", file, line, func);
	fprintf (stderr, "%s\n", message->str);
	dstring_delete (message);
	abort ();
}

void
_bug (const expr_t *e, const char *file, int line, const char *func,
	  const char *fmt, ...)
{
	va_list     args;

	if (options.bug.silent)
		return;

	va_start (args, fmt);
	if (options.bug.promote) {
		__internal_error (e, file, line, func, fmt, args);
	}

	{
		dstring_t  *message = dstring_new ();

		report_function (e);

		format_message (message, "BUG", e, fmt, args);
		dasprintf (message, " (%s:%d in %s)", file, line, func);
		if (bug_hook) {
			bug_hook (message->str);
		} else {
			fprintf (stderr, "%s\n", message->str);
		}
		dstring_delete (message);
	}
	va_end (args);
}

const expr_t *
_notice (const expr_t *e, const char *file, int line, const char *func,
		 const char *fmt, ...)
{
	va_list     args;

	if (options.notices.silent)
		return e;

	va_start (args, fmt);
	if (options.notices.promote) {
		__warning (e, file, line, func, fmt, args);
	} else {
		dstring_t  *message = dstring_new ();

		report_function (e);

		format_message (message, "notice", e, fmt, args);
		if (options.verbosity > 0) {
			dasprintf (message, " (%s:%d in %s)", file, line, func);
		}
		if (notice_hook) {
			notice_hook (message->str);
		} else {
			fprintf (stderr, "%s\n", message->str);
		}
		dstring_delete (message);
	}
	va_end (args);
	return e;
}

const expr_t *
_warning (const expr_t *e, const char *file, int line, const char *func,
		  const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	__warning (e, file, line, func, fmt, args);
	va_end (args);
	return e;
}

void
_internal_error (const expr_t *e, const char *file, int line,
				 const char *func, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	__internal_error (e, file, line, func, fmt, args);
	va_end (args);
}

const expr_t *
_error (const expr_t *e, const char *file, int line, const char *func,
		const char *fmt, ...)
{
	va_list     args;

	pr.error_count++;

	report_function (e);

	va_start (args, fmt);
	{
		dstring_t  *message = dstring_new ();

		format_message (message, "error", e, fmt, args);
		if (options.verbosity > 0) {
			dasprintf (message, " (%s:%d in %s)", file, line, func);
		}
		if (error_hook) {
			error_hook (message->str);
		} else {
			fprintf (stderr, "%s\n", message->str);
		}
		dstring_delete (message);
	}
	va_end (args);

	expr_t *err = new_expr ();
	err->type = ex_error;
	return err;
}
