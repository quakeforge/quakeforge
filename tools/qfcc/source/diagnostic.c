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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include <QF/dstring.h>

#include "qfcc.h"
#include "class.h"
#include "diagnostic.h"
#include "expr.h"
#include "function.h"
#include "options.h"
#include "strpool.h"

static void
report_function (expr_t *e)
{
	static function_t *last_func = (function_t *)-1L;
	static string_t last_file;
	string_t    file = pr.source_file;
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

static void
_warning (expr_t *e, const char *fmt, va_list args)
{
	string_t    file = pr.source_file;
	int         line = pr.source_line;

	report_function (e);
	if (options.warnings.promote) {
		options.warnings.promote = 0;	// want to do this only once
		fprintf (stderr, "%s: warnings treated as errors\n", "qfcc");
		pr.error_count++;
	}

	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: warning: ", GETSTR (file), line);
	vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
}

void
debug (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	if (options.verbosity < 1)
		return;

	va_start (args, fmt);
	if (options.notices.promote) {
		_warning (e, fmt, args);
	} else {
		string_t    file = pr.source_file;
		int         line = pr.source_line;

		report_function (e);
		if (e) {
			file = e->file;
			line = e->line;
		}
		fprintf (stderr, "%s:%d: debug: ", GETSTR (file), line);
		vfprintf (stderr, fmt, args);
		fputs ("\n", stderr);
	}
	va_end (args);
}

expr_t *
notice (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	if (options.notices.silent)
		return e;

	va_start (args, fmt);
	if (options.notices.promote) {
		_warning (e, fmt, args);
	} else {
		string_t    file = pr.source_file;
		int         line = pr.source_line;

		report_function (e);
		if (e) {
			file = e->file;
			line = e->line;
		}
		fprintf (stderr, "%s:%d: notice: ", GETSTR (file), line);
		vfprintf (stderr, fmt, args);
		fputs ("\n", stderr);
	}
	va_end (args);
	return e;
}

expr_t *
warning (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	_warning (e, fmt, args);
	va_end (args);
	return e;
}

static void
_error (expr_t *e, const char *err, const char *fmt, va_list args)
{
	string_t    file = pr.source_file;
	int         line = pr.source_line;

	report_function (e);

	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: %s%s", GETSTR (file), line, err,
			 fmt ? ": " : "");
	if (fmt)
		vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
	pr.error_count++;
}

void
internal_error (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	_error (e, "internal error", fmt, args);
	va_end (args);
	abort ();
}

expr_t *
error (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	_error (e, "error", fmt, args);
	va_end (args);

	if (!e)
		e = new_expr ();
	e->type = ex_error;
	return e;
}
