/*
	dot.c

	General dot output support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/07

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

#include "QF/va.h"

#include "dot.h"
#include "function.h"
#include "options.h"
#include "strpool.h"

static function_t *last_func;
static int dot_index;

void
dump_dot (const char *stage, void *data,
		  void (*dump_func) (void *data, const char *fname))
{
	char       *fname;

	if (last_func != current_func) {
		last_func = current_func;
		dot_index = 0;
	} else {
		dot_index++;
	}
	if (current_func) {
		fname = nva ("%s.%s.%03d.%s.dot", options.output_file,
					 current_func->name, dot_index, stage);
	} else {
		fname = nva ("%s.%03d.%s.dot", options.output_file, dot_index, stage);
	}
	dump_func (data, fname);
	free (fname);
}
