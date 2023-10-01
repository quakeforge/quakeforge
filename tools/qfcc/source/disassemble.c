/*
	disassemble.c

	Dump progs disassembly.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/13

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

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <sys/types.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/fcntl.h>
#endif

#include "QF/progs.h"
#include "QF/sys.h"

#include "tools/qfcc/include/qfprogs.h"

void
disassemble_progs (progs_t *pr)
{
	unsigned int i;

	for (i = 0; i < pr->progs->statements.count; i++) {
		dfunction_t *desc = func_find (i);

		if (desc) {
			bfunction_t func;

			func.first_statement = desc->first_statement;
			func.params_start = desc->params_start;
			func.locals = desc->locals;
			func.numparams = desc->numparams;
			memcpy (func.param_size, desc->param_size,
					sizeof (func.param_size));
			func.descriptor = desc;

			Sys_Printf ("%s:\n", PR_GetString (pr, desc->name));
			pr->pr_xfunction = &func;

			if (pr->pr_depth) {
				//FIXME assumes 1 is frame
				//FIXME update when making locals have -ve addresses
				pr->pr_bases[1] = pr->pr_stack[0].stack_ptr - func.params_start;
			}
		}
		PR_PrintStatement (pr, &pr->pr_statements[i], 2 | (verbosity > 0));
	}
}
