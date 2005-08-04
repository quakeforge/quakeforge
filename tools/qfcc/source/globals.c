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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/progs.h"
#include "QF/va.h"

#include "qfprogs.h"

static int
cmp (const void *_a, const void *_b)
{
	const ddef_t *a = (const ddef_t *)_a;
	const ddef_t *b = (const ddef_t *)_b;

	return a->ofs - b->ofs;
}

void
dump_globals (progs_t *pr)
{
	unsigned int i;
	const char *name;
	const char *type;
	int         saveglobal;
	int         offset;
	const char *comment;
	ddef_t     *global_defs = pr->pr_globaldefs;

	if (sorted) {
		global_defs = malloc (pr->progs->numglobaldefs * sizeof (ddef_t));
		memcpy (global_defs, pr->pr_globaldefs,
				pr->progs->numglobaldefs * sizeof (ddef_t));
		qsort (global_defs, pr->progs->numglobaldefs, sizeof (ddef_t), cmp);
	}
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		ddef_t *def = &global_defs[i];

		if (!def->type && !def->ofs && !def->s_name)
			continue;

		name = PR_GetString (pr, def->s_name);
		type = pr_type_name[def->type & ~DEF_SAVEGLOBAL];
		saveglobal = (def->type & DEF_SAVEGLOBAL) != 0;
		offset = def->ofs;

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
	unsigned int i;
	const char *name;
	const char *type;
	int         offset;
	const char *comment;

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
	int         i, j;
	const char *name;
	int         start, count;
	const char *comment;

	for (i = 0; i < pr->progs->numfunctions; i++) {
		dfunction_t *func = &pr->pr_functions[i];

		name = PR_GetString (pr, func->s_name);

		comment = " ";

		start = func->first_statement;
		if (start > 0)
			comment = va (" @ %d", start);
		else
			comment = va (" = #%d", -start);

		printf ("%-5d %s%s: %d", i, name, comment, func->numparms);
		if (func->numparms < 0)
			count = -func->numparms - 1;
		else
			count = func->numparms;
		for (j = 0; j < count; j++)
			printf (" %d", func->parm_size[j]);
		puts ("");
	}
}
