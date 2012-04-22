/*
	dump_lines.c

	Dump line number information.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/09/06

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

#include "QF/progs.h"

#include "qfprogs.h"

void
dump_lines (progs_t *pr)
{
	unsigned int i, line, addr;
	pr_lineno_t *lineno;
	pr_auxfunction_t *aux_func = 0;
	dfunction_t *func = 0;

	if (!pr->debug)
		return;
	for (i = 0; i < pr->debug->num_linenos; i++) {
		lineno = &pr->linenos[i];

		if (!lineno->line) {
			aux_func = 0;
			func = 0;
			if (lineno->fa.func >= 0
				&& lineno->fa.func < pr->debug->num_auxfunctions)
				aux_func = pr->auxfunctions + lineno->fa.func;
			if (aux_func && aux_func->function >= 0
				&& aux_func->function < (unsigned int) pr->progs->numfunctions)
				func = pr->pr_functions + aux_func->function;
		}

		printf ("%5u %5u", lineno->fa.addr, lineno->line);
		line = addr = -1;
		if (aux_func)
			line = aux_func->source_line + lineno->line;
		if (func)
			addr = lineno->line ? lineno->fa.addr
								: (unsigned int) func->first_statement;
		if (aux_func && func)
			printf (" %05x %s:%u %s+%u %d", addr, pr->pr_strings + func->s_file,
					line, pr->pr_strings + func->s_name,
					addr - func->first_statement, aux_func->return_type);
		else if (aux_func)
			printf ("%u %u %u %u %u %d", aux_func->function, line,
					aux_func->line_info, aux_func->local_defs,
					aux_func->num_locals, aux_func->return_type);
		else if (lineno->line)
			printf ("%5x", lineno->fa.addr);
		printf ("\n");
	}
}
