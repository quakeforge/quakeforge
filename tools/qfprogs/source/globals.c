/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "QF/progs.h"
#include "QF/va.h"

#include "qfprogs.h"
#include "globals.h"

void
dump_globals (progs_t *pr)
{
	int i;
	const char *name;
	const char *type;
	int         saveglobal;
	int         offset;
	char       *comment;

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		ddef_t *def = &pr->pr_globaldefs[i];

		name = PR_GetString (pr, def->s_name);
		type = pr_type_name[def->type & ~DEF_SAVEGLOBAL];
		saveglobal = (def->type & DEF_SAVEGLOBAL) != 0;
		offset = def->ofs;

		if (!offset)
			continue;

		comment = " ";

		if (def->type == ev_func) {
			func_t      func = G_FUNCTION (pr, offset);
			int         start;
			if (func >= 0 && func < pr->progs->numfunctions) {
				start = pr->pr_functions[func].first_statement;
				if (start > 0)
					comment = va (" %d @ %d", func, start);
				else
					comment = va (" %d = #%d", func, -start);
			} else {
				comment = va (" %d = illegal function", func);
			}
		}

		printf ("%s %d %d %s%s\n", type, saveglobal, offset, name, comment);
	}
}

void
dump_fields (progs_t *pr)
{
	int i;
	const char *name;
	const char *type;
	int         offset;
	char       *comment;

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		ddef_t *def = &pr->pr_fielddefs[i];

		name = PR_GetString (pr, def->s_name);
		type = pr_type_name[def->type & ~DEF_SAVEGLOBAL];
		offset = def->ofs;

		comment = " ";

		printf ("%s %d %s%s\n", type, offset, name, comment);
	}
}

void
dump_functions (progs_t *pr)
{
	int i;
	const char *name;
	int         start;
	char       *comment;

	for (i = 0; i < pr->progs->numfunctions; i++) {
		dfunction_t *func = &pr->pr_functions[i];

		name = PR_GetString (pr, func->s_name);

		comment = " ";

		start = func->first_statement;
		if (start > 0)
			comment = va (" @ %d", start);
		else
			comment = va (" = #%d", -start);

		printf ("%s%s\n", name, comment);
	}
}
