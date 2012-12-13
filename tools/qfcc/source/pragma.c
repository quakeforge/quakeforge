/*
	praga.c

	pragma handling

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/22

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

#include "diagnostic.h"
#include "opcodes.h"
#include "options.h"
#include "pragma.h"
#include "type.h"

static void
set_traditional (int traditional)
{
	switch (traditional) {
		case 0:
			options.traditional = 0;
			options.advanced = true;
			options.code.progsversion = PROG_VERSION;
			type_default = &type_integer;
			break;
		case 1:
			options.traditional = 1;
			options.advanced = false;
			options.code.progsversion = PROG_ID_VERSION;
			type_default = &type_float;
			break;
		case 2:
			options.traditional = 2;
			options.advanced = false;
			options.code.progsversion = PROG_ID_VERSION;
			type_default = &type_float;
			break;
	}
	opcode_init ();		// reset the opcode table
}

void
pragma (const char *id)
{
	if (!strcmp (id, "traditional")) {
		set_traditional (2);
		return;
	}
	if (!strcmp (id, "extended")) {
		set_traditional (1);
		return;
	}
	if (!strcmp (id, "advanced")) {
		set_traditional (0);
		return;
	}
	warning (0, "unknown pragma: %s", id);
}
