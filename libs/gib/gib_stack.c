/*
	gib_stack.c

	#DESCRIPTION#

	Copyright (C) 2000       #AUTHOR#

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/gib.h"

#include "gib_stack.h"

gib_instack_t *gib_instack = 0;
gib_substack_t *gib_substack = 0;

int         gib_insp = 0;
int         gib_subsp = 0;

void
GIB_InStack_Push (gib_inst_t * instruction, int argc, char **argv)
{
	int         i;

	gib_instack =
		realloc (gib_instack, sizeof (gib_instack_t) * (gib_insp + 1));
	gib_instack[gib_insp].argv = malloc ((argc + 1) * sizeof(char *));

	for (i = 0; i <= argc; i++) {
		gib_instack[gib_insp].argv[i] = malloc (strlen (argv[i]) + 1);
		strcpy (gib_instack[gib_insp].argv[i], argv[i]);
	}

	gib_instack[gib_insp].argc = argc;
	gib_instack[gib_insp].instruction = instruction;
	gib_insp++;
}

void
GIB_InStack_Pop (void)
{
	int         i;

	gib_insp--;

	for (i = 0; i <= gib_instack[gib_insp].argc; i++)
		free (gib_instack[gib_insp].argv[i]);

	free (gib_instack[gib_insp].argv);

	gib_instack = realloc (gib_instack, sizeof (gib_instack_t) * gib_insp);
}

void
GIB_SubStack_Push (gib_module_t * mod, gib_sub_t * sub, gib_var_t * local)
{
	gib_substack =
		realloc (gib_substack, sizeof (gib_substack_t) * (gib_subsp + 1));
	gib_substack[gib_subsp].mod = mod;
	gib_substack[gib_subsp].sub = sub;
	gib_substack[gib_subsp].local = local;
	gib_subsp++;
}

void
GIB_SubStack_Pop (void)
{
	gib_instack = realloc (gib_instack, sizeof (gib_instack_t) * (--gib_subsp));
}
