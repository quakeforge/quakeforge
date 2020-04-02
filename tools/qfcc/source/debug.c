/*
	debug.c

	debug info support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/14

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

#include "QF/alloc.h"
#include "QF/pr_comp.h"

#include "debug.h"
#include "diagnostic.h"
#include "expr.h"
#include "qfcc.h"
#include "strpool.h"
#include "value.h"

int         lineno_base;

static srcline_t *srclines_freelist;

static void
push_source_file (void)
{
	srcline_t  *srcline;
	ALLOC (16, srcline_t, srclines, srcline);
	srcline->source_file = pr.source_file;
	srcline->source_line = pr.source_line;
	srcline->next = pr.srcline_stack;
	pr.srcline_stack = srcline;
}

static void
pop_source_file (void)
{
	srcline_t  *tmp;

	if (!pr.srcline_stack) {
		notice (0, "unbalanced #includes. bug in preprocessor?");
		return;
	}
	tmp = pr.srcline_stack;
	pr.srcline_stack = tmp->next;
	FREE (srclines, tmp);
}

void
line_info (char *text)
{
	char *p;
	char *s;
	const char *str;
	int line;
	int flags;

	p = text;
	line = strtol (p, &s, 10);
	p = s;
	while (isspace ((unsigned char)*p))
		p++;
	if (!*p)
		error (0, "Unexpected end of file");
	str = make_string (p, &s);		// grab the filename
	p = s;
	while (isspace ((unsigned char) *p))
		p++;
	flags = strtol (p, &s, 10);
	switch (flags) {
		case 1:
			push_source_file ();
			break;
		case 2:
			pop_source_file ();
			break;
	}
	while (*p && *p != '\n')	// ignore rest
		p++;
	pr.source_line = line - 1;
	pr.source_file = ReuseString (str);
}

pr_lineno_t *
new_lineno (void)
{
	if (pr.num_linenos == pr.linenos_size) {
		pr.linenos_size += 1024;
		pr.linenos = realloc (pr.linenos,
							  pr.linenos_size * sizeof (pr_lineno_t));
	}
	memset (&pr.linenos[pr.num_linenos], 0, sizeof (pr_lineno_t));
	return &pr.linenos[pr.num_linenos++];
}
