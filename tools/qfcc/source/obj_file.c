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

enum {
	qfo_null_space,
	qfo_strings_space,
	qfo_code_space,
	qfo_near_data_space,
	qfo_far_data_space,
	qfo_entity_space,
	qfo_type_space,

	qfo_num_spaces
};

static qfo_mspace_t *spaces;

static int
count_defs (def_t *d)
{
	int         count;

	for (count = 0; d; d = d->next)
		count++;
	return count;
}

static int
count_relocs (reloc_t *r)
{
	int         count;

	for (count = 0; r; r = r->next)
		count++;
	return count;
}

static void
qfo_init_string_space (qfo_mspace_t *space, strpool_t *strings)
{
	strings->qfo_space = space - spaces;
	space->type = qfos_string;
	space->num_defs = 0;
	space->defs = 0;
	space->d.strings = strings->strings;
	space->data_size = strings->size;
}

static void
qfo_init_code_space (qfo_mspace_t *space, codespace_t *code)
{
	code->qfo_space = space - spaces;
	space->type = qfos_code;
	space->num_defs = 0;
	space->defs = 0;
	space->d.code = code->code;
	space->data_size = code->size;
}

static void
qfo_init_data_space (qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - spaces;
	space->type = qfos_data;
	space->num_defs = count_defs (data->defs);
	space->defs = 0;
	space->d.data = data->data;
	space->data_size = data->size;
}

static void
qfo_init_entity_space (qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - spaces;
	space->type = qfos_entity;
	space->num_defs = count_defs (data->defs);
	space->defs = 0;
	space->d.data = 0;
	space->data_size = data->size;
}

static void
qfo_init_type_space (qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - spaces;
	space->type = qfos_type;
	space->num_defs = count_defs (data->defs);
	space->defs = 0;
	space->d.data = data->data;
	space->data_size = data->size;
}


qfo_t *
qfo_from_progs (pr_info_t *pr)
{
	spaces = calloc (qfo_num_spaces, sizeof (qfo_mspace_t));
	qfo_init_string_space (&spaces[qfo_strings_space], pr->strings);
	qfo_init_code_space (&spaces[qfo_code_space], pr->code);
	qfo_init_data_space (&spaces[qfo_near_data_space], pr->near_data);
	qfo_init_data_space (&spaces[qfo_far_data_space], pr->far_data);
	qfo_init_entity_space (&spaces[qfo_entity_space], pr->entity_data);
	qfo_init_type_space (&spaces[qfo_type_space], pr->type_data);

	count_relocs (pr->relocs);
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
