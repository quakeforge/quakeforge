/*
	codespace.c

	management of code segement

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/07

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

#include "QF/pr_comp.h"

#include "tools/qfcc/include/codespace.h"

codespace_t *
codespace_new (void)
{
	return calloc (1, sizeof (codespace_t));
}

void
codespace_delete (codespace_t *codespace)
{
	free (codespace->code);
	free (codespace);
}

void
codespace_addcode (codespace_t *codespace, dstatement_t *code, int size)
{
	if (codespace->size + size > codespace->max_size) {
		codespace->max_size = (codespace->size + size + 16383) & ~16383;
		codespace->code = realloc (codespace->code,
								   codespace->max_size * sizeof (dstatement_t));
	}
	memcpy (codespace->code + codespace->size, code,
			size * sizeof (dstatement_t));
	codespace->size += size;
}

dstatement_t *
codespace_newstatement (codespace_t *codespace)
{
	if (codespace->size >= codespace->max_size) {
		codespace->max_size += 16384;
		codespace->code = realloc (codespace->code,
								   codespace->max_size * sizeof (dstatement_t));
	}
	return codespace->code + codespace->size++;
}
