/*
	dstring.c

	dynamic string buffer functions

	Copyright (C) 2002  Brian Koropoff.

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

static const char rcsid[] =
	"$Id$";

#include <stdlib.h>
#include <string.h>
#include "QF/sys.h"
#include "QF/dstring.h"

#include "compat.h"

dstring_t *
dstring_new (void)
{
	dstring_t *new;
	
	new = calloc (1, sizeof(dstring_t));
	if (!new)
		Sys_Error ("dstring_new: Failed to allocate memory.\n");
	return new;
}

void
dstring_delete (dstring_t *dstr)
{
	if (dstr->str)
		free (dstr->str);
	free (dstr);
}
	
void
dstring_adjust (dstring_t *dstr)
{
	if (dstr->size > dstr->truesize) {
		dstr->str = realloc(dstr->str, dstr->size);
		if (!dstr->str)
			Sys_Error ("dstring_adjust:  Failed to reallocate memory.\n");
		dstr->truesize = dstr->size;
	}
}

void
dstring_append (dstring_t *dstr, const char *data, unsigned int len)
{
	unsigned int ins = dstr->size; // Save insertion point
	
	dstr->size += len;
	dstring_adjust (dstr);
	memcpy (dstr->str + ins, data, len);
}

void
dstring_insert (dstring_t *dstr, const char *data, unsigned int len,
				unsigned int pos)
{
	unsigned int oldsize = dstr->size;
	
	dstr->size += len;
	dstring_adjust (dstr);
	memmove (dstr->str+pos+len, dstr->str+pos, oldsize - pos);
	memcpy (dstr->str+pos, data, len);
}

void
dstring_snip (dstring_t *dstr, unsigned int pos, unsigned int len)
{
	memmove (dstr->str+pos, dstr->str+pos+len, dstr->size-pos-len);
	dstr->size -= len;
	dstring_adjust (dstr);
}

void
dstring_clear (dstring_t *dstr)
{
	dstr->size = 0;
	dstring_adjust (dstr);
}

dstring_t *
dstring_newstr (void)
{
	dstring_t *new;
	
	new = calloc(1, sizeof(dstring_t));
	if (!new)
		Sys_Error ("dstring_newstr:  Failed to allocate memory.\n");
	new->size = 1;
	dstring_adjust(new);
	new->str[0] = 0;
	return new;
}

void
dstring_appendstr (dstring_t *dstr, const char *str) {
		dstr->size += strlen(str);
		dstring_adjust(dstr);
		strcat(dstr->str, str);
}

void
dstring_insertstr (dstring_t *dstr, const char *str, unsigned int pos)
{
	// Don't instert strlen + 1 to achieve concatenation
	dstring_insert (dstr, str, strlen(str), pos);
}

void
dstring_clearstr (dstring_t *dstr)
{
	dstr->size = 1;
	dstring_adjust (dstr);
	dstr->str[0] = 0;
}

int
dvsprintf (dstring_t *dstr, const char *fmt, va_list args)
{
	int         size;

	size = vsnprintf (dstr->str, dstr->truesize, fmt, args) + 1;  // +1 for nul
	while (size <= 0 || size > dstr->truesize) {
		if (size > 0)
			dstr->size = (size + 1023) & ~1023; // 1k multiples
		else
			dstr->size = dstr->truesize + 1024;
		dstring_adjust (dstr);
		size = vsnprintf (dstr->str, dstr->truesize, fmt, args) + 1;
	}
	dstr->size = size;
	return size - 1;
}

int
dsprintf (dstring_t *dstr, const char *fmt, ...)
{
	va_list     args;
	int         ret;

	va_start (args, fmt);
	ret = dvsprintf (dstr, fmt, args);
	va_end (args);

	return ret;
}
