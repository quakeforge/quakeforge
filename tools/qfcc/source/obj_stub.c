/*
	obj_file.c

	qfcc object file support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/6/21

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/quakeio.h"

#include "codespace.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "immediate.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
#include "type.h"

qfo_t *
qfo_from_progs (pr_info_t *pr)
{
	return 0;
}

int
qfo_write (qfo_t *qfo, const char *filename)
{
	return 0;
}

qfo_t *
qfo_read (QFile *file)
{
	return 0;
}

qfo_t *
qfo_open (const char *filename)
{
	return 0;
}

int
qfo_to_progs (qfo_t *qfo, pr_info_t *pr)
{
	return 0;
}

qfo_t *
qfo_new (void)
{
	return 0;
}

void
qfo_add_code (qfo_t *qfo, dstatement_t *code, int code_size)
{
}

void
qfo_add_data (qfo_t *qfo, pr_type_t *data, int data_size)
{
}

void
qfo_add_far_data (qfo_t *qfo, pr_type_t *far_data, int far_data_size)
{
}

void
qfo_add_strings (qfo_t *qfo, const char *strings, int strings_size)
{
}

void
qfo_add_relocs (qfo_t *qfo, qfo_reloc_t *relocs, int num_relocs)
{
}

void
qfo_add_defs (qfo_t *qfo, qfo_def_t *defs, int num_defs)
{
}

void
qfo_add_funcs (qfo_t *qfo, qfo_func_t *funcs, int num_funcs)
{
}

void
qfo_add_lines (qfo_t *qfo, pr_lineno_t *lines, int num_lines)
{
}

void
qfo_add_types (qfo_t *qfo, const char *types, int types_size)
{
}

void
qfo_delete (qfo_t *qfo)
{
}
