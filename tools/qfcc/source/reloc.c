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

#include "def.h"
#include "expr.h"
#include "qfcc.h"
#include "reloc.h"

void
relocate_refs (reloc_t *refs, int ofs)
{
	int         o;

	while (refs) {
		switch (refs->type) {
			case rel_none:
				break;
			case rel_op_a_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.statements[refs->ofs].a = ofs;
				break;
			case rel_op_b_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.statements[refs->ofs].b = ofs;
				break;
			case rel_op_c_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.statements[refs->ofs].c = ofs;
				break;
			case rel_op_a_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.statements[refs->ofs].a = o;
				break;
			case rel_op_b_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.statements[refs->ofs].b = o;
				break;
			case rel_op_c_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.statements[refs->ofs].c = o;
				break;
			case rel_def_op:
				if (ofs >= pr.num_statements)
					error (0, "invalid statement offset");
				else
					G_INT (refs->ofs) = ofs;
				break;
			case rel_def_def:
				G_INT (refs->ofs) = ofs;
				break;
		}
		refs = refs->next;
	}
}

reloc_t *
new_reloc (int ofs, reloc_type type)
{
	reloc_t    *ref = calloc (1, sizeof (reloc_t));

	ref->ofs = ofs;
	ref->type = type;
	return ref;
}
