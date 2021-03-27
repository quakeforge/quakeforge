/*
	dump_strings.c

	Dump progs strings.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/16

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

#include <stdlib.h>

#include "QF/progs.h"
#include "QF/sys.h"

#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/qfprogs.h"

static void
dump_string_block (const char *strblock, size_t size)
{
	const char *s = strblock;

	printf ("%x \"", 0);
	while ((size_t) (s - strblock) < size) {
		char        c = *s++;
		switch (c) {
			case 0:
				fputs ("\"\n", stdout);
				if ((size_t) (s - strblock) < size)
					printf ("%zx \"", s - strblock);
				break;
			case 9:
				fputs ("\\t", stdout);
				break;
			case 10:
				fputs ("\\n", stdout);
				break;
			case 13:
				fputs ("\\r", stdout);
				break;
			case '\"':
				fputs ("\\\"", stdout);
				break;
			case '\\':
				fputs ("\\\\", stdout);
				break;
			default:
				fputc (sys_char_map[(unsigned char)c], stdout);
				break;
		}
	}
}

void
dump_strings (progs_t *pr)
{
	dump_string_block (pr->pr_strings, pr->progs->numstrings);
}

void
qfo_strings (qfo_t *qfo)
{
	qfo_mspace_t *space = &qfo->spaces[qfo_strings_space];

	if (qfo_strings_space >= qfo->num_spaces) {
		printf ("no strings space\n");
		return;
	}
	if (!space->data_size) {
		printf ("no strings\n");
		return;
	}
	dump_string_block (space->d.strings, space->data_size);
}
