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

#include <stdlib.h>


#include "qfcc.h"

type_t *
pointer_type (type_t *aux)
{
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_pointer;
	new.aux_type = aux;
	return PR_FindType (&new);
}

void
print_type (type_t *type)
{
	if (!type)
		printf (" (null)");
	switch (type->type) {
		case ev_func:
			print_type (type->aux_type);
			if (type->num_parms < 0) {
				printf ("(...)");
			} else {
				int         i;
				printf ("(");
				for (i = 0; i < type->num_parms; i++) {
					if (i)
						printf (", ");
					print_type (type->parm_types[i]);
				}
				printf (")");
			}
			break;
		case ev_pointer:
			print_type (type->aux_type);
			if (type->num_parms)
				printf ("[%d]", type->num_parms);
			else
				printf ("[]");
			break;
		default:
			printf(" %s", pr_type_name[type->type]);
			break;
	}
}
