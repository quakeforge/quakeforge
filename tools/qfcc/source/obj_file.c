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

#define _GNU_SOURCE	// for qsort_r

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
#include "QF/va.h"

#include "compat.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static int
count_relocs (reloc_t *r)
{
	int         count;

	for (count = 0; r; r = r->next)
		count++;
	return count;
}

static unsigned
qfo_def_flags (def_t *def)
{
	unsigned    flags = 0;
	if (def->initialized)
		flags |= QFOD_INITIALIZED;
	if (def->constant)
		flags |= QFOD_CONSTANT;
	if (def->global)
		flags |= QFOD_GLOBAL;
	if (def->external)
		flags |= QFOD_EXTERNAL;
	if (def->local)
		flags |= QFOD_LOCAL;
	if (def->system)
		flags |= QFOD_SYSTEM;
	if (def->nosave)
		flags |= QFOD_NOSAVE;
	if (def->param)
		flags |= QFOD_PARAM;
	return flags;
}

static void
qfo_encode_one_reloc (reloc_t *reloc, qfo_reloc_t **qfo_reloc, pr_int_t target)
{
	qfo_reloc_t *q;

	q = (*qfo_reloc)++;
	if (reloc->space)	// op_* relocs do not have a space (code is implied)
		q->space = reloc->space->qfo_space;
	q->offset = reloc->offset;
	q->type = reloc->type;
	q->target = target;
}

static int
qfo_encode_relocs (reloc_t *relocs, qfo_reloc_t **qfo_relocs, pr_int_t target)
{
	int         count;
	reloc_t    *r;

	for (count = 0, r = relocs; r; r = r->next) {
		count++;
		qfo_encode_one_reloc (r, qfo_relocs, target);
	}
	return count;
}

static int
qfo_encode_defs (qfo_t *qfo, def_t *defs, qfo_def_t **qfo_defs,
				 qfo_reloc_t **qfo_relocs)
{
	int         count;
	def_t      *d;
	qfo_def_t  *q;

	for (count = 0, d = defs; d; d = d->next) {
		count++;
		q = (*qfo_defs)++;
		d->qfo_def = q - qfo->defs;
		// defs in the type data space do not have types
		if (d->type)
			q->type = d->type->type_def->offset;
		q->name = ReuseString (d->name);
		q->offset = d->offset;
		q->relocs = *qfo_relocs - qfo->relocs;
		q->num_relocs = qfo_encode_relocs (d->relocs, qfo_relocs,
										   q - qfo->defs);
		q->flags = qfo_def_flags (d);
		q->file = d->file;
		q->line = d->line;
	}
	return count;
}

static void
qfo_count_space_stuff (qfo_t *qfo, defspace_t *space)
{
	def_t      *d;

	for (d = space->defs; d; d = d->next) {
		qfo->num_defs++;
		qfo->num_relocs += count_relocs (d->relocs);
	}
}

static void
qfo_count_function_stuff (qfo_t *qfo, function_t *functions)
{
	function_t *func;

	for (func = functions; func; func = func->next) {
		qfo->num_funcs++;
		qfo->num_relocs += count_relocs (func->refs);
		if (func->locals && func->locals->space) {
			qfo->num_spaces++;
			qfo_count_space_stuff (qfo, func->locals->space);
		}
	}
}

static void
qfo_count_stuff (qfo_t *qfo, pr_info_t *pr)
{
	qfo_count_space_stuff (qfo, pr->near_data);
	qfo_count_space_stuff (qfo, pr->far_data);
	qfo_count_space_stuff (qfo, pr->entity_data);
	qfo_count_space_stuff (qfo, pr->type_data);
	qfo_count_space_stuff (qfo, pr->debug_data);
	qfo_count_function_stuff (qfo, pr->func_head);
	qfo->num_relocs += count_relocs (pr->relocs);
}

static void
qfo_init_string_space (qfo_t *qfo, qfo_mspace_t *space, strpool_t *strings)
{
	size_t      size = strings->size * sizeof (*strings->strings);
	strings->qfo_space = space - qfo->spaces;
	space->type = qfos_string;
	space->num_defs = 0;
	space->defs = 0;
	space->strings = 0;
	if (strings->strings) {
		space->strings = malloc (size);
		memcpy (space->strings, strings->strings, size);
	}
	space->data_size = strings->size;
	space->id = qfo_strings_space;
}

static void
qfo_init_code_space (qfo_t *qfo, qfo_mspace_t *space, codespace_t *code)
{
	size_t      size = code->size * sizeof (*code->code);
	code->qfo_space = space - qfo->spaces;
	space->type = qfos_code;
	space->num_defs = 0;
	space->defs = 0;
	space->code = 0;
	if (code->code) {
		space->code = malloc (size);
		memcpy (space->code, code->code, size);
	}
	space->data_size = code->size;
	space->id = qfo_code_space;
}

static void
qfo_init_data_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					 qfo_mspace_t *space, defspace_t *data)
{
	size_t      size = data->size * sizeof (*data->data);
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_data;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->data = 0;
	if (data->data) {
		space->data = malloc (size);
		memcpy (space->data, data->data, size);
	}
	space->data_size = data->size;
}

static void
qfo_init_entity_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					   qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_entity;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->data = 0;
	space->data_size = data->size;
	space->id = qfo_entity_space;
}

static void
qfo_init_type_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					 qfo_mspace_t *space, defspace_t *data)
{
	size_t      size = data->size * sizeof (*data->data);
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_type;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->data = 0;
	if (data->data) {
		space->data = malloc (size);
		memcpy (space->data, data->data, size);
	}
	space->data_size = data->size;
	space->id = qfo_type_space;
}

static void
qfo_init_debug_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					  qfo_mspace_t *space, defspace_t *data)
{
	size_t      size = data->size * sizeof (*data->data);
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_debug;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->data = 0;
	if (data->data) {
		space->data = malloc (size);
		memcpy (space->data, data->data, size);
	}
	space->data_size = data->size;
	space->id = qfo_debug_space;
}

static void
qfo_encode_functions (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					  qfo_mspace_t *space, function_t *functions)
{
	function_t *f;
	qfo_func_t *q;

	for (f = functions, q = qfo->funcs; f; f = f->next, q++) {
		q->name = f->s_name;
		q->type = f->def->type->type_def->offset;
		q->file = f->s_file;
		q->line = f->def->line;
		q->code = f->code;
		if (f->builtin)		// FIXME redundant
			q->code = -f->builtin;
		q->def = f->def->qfo_def;
		if (f->locals && f->locals->space) {
			space->id = f->locals->space->qfo_space;
			qfo_init_data_space (qfo, defs, relocs, space++, f->locals->space);
			q->locals_space = f->locals->space->qfo_space;
		}
		q->line_info = f->line_info;
		q->relocs = *relocs - qfo->relocs;
		q->num_relocs = qfo_encode_relocs (f->refs, relocs, q - qfo->funcs);
		q->params_start = f->params_start;
	}
}

qfo_t *
qfo_from_progs (pr_info_t *pr)
{
	qfo_t      *qfo;
	qfo_def_t  *def;
	qfo_reloc_t *reloc;
	reloc_t    *r;

	qfo = calloc (1, sizeof (qfo_t));
	qfo->num_spaces = qfo_num_spaces; // certain spaces are always present
	qfo_count_stuff (qfo, pr);
	qfo->spaces = calloc (qfo->num_spaces, sizeof (qfo_mspace_t));
	qfo->relocs = calloc (qfo->num_relocs, sizeof (qfo_reloc_t));
	qfo->defs = calloc (qfo->num_defs, sizeof (qfo_def_t));
	qfo->funcs = calloc (qfo->num_funcs, sizeof (qfo_func_t));

	def = qfo->defs;
	reloc = qfo->relocs;

	pr->code->qfo_space = qfo_code_space;
	pr->near_data->qfo_space = qfo_near_data_space;
	pr->far_data->qfo_space = qfo_far_data_space;
	pr->entity_data->qfo_space = qfo_entity_space;
	pr->type_data->qfo_space = qfo_type_space;
	pr->strings->qfo_space = qfo_strings_space;

	qfo_init_code_space (qfo, &qfo->spaces[qfo_code_space], pr->code);
	qfo_init_data_space (qfo, &def, &reloc, &qfo->spaces[qfo_near_data_space],
						 pr->near_data);
	qfo->spaces[qfo_near_data_space].id = qfo_near_data_space;
	qfo_init_data_space (qfo, &def, &reloc, &qfo->spaces[qfo_far_data_space],
						 pr->far_data);
	qfo->spaces[qfo_far_data_space].id = qfo_far_data_space;
	qfo_init_entity_space (qfo, &def, &reloc, &qfo->spaces[qfo_entity_space],
						   pr->entity_data);
	qfo_init_type_space (qfo, &def, &reloc, &qfo->spaces[qfo_type_space],
						 pr->type_data);
	qfo_init_debug_space (qfo, &def, &reloc, &qfo->spaces[qfo_debug_space],
						  pr->debug_data);

	qfo_encode_functions (qfo, &def, &reloc, qfo->spaces + qfo_num_spaces,
						  pr->func_head);
	if (pr->num_linenos) {
		qfo->lines = malloc (pr->num_linenos * sizeof (pr_lineno_t));
		memcpy (qfo->lines, pr->linenos,
				pr->num_linenos * sizeof (pr_lineno_t));
		qfo->num_lines = pr->num_linenos;
	}
	// strings must be done last because encoding defs and functions adds the
	// names
	qfo_init_string_space (qfo, &qfo->spaces[qfo_strings_space], pr->strings);

	qfo->num_loose_relocs = qfo->num_relocs - (reloc - qfo->relocs);
	for (r = pr->relocs; r; r = r->next) {
		if (r->type == rel_def_op) {
			qfo_encode_one_reloc (r, &reloc, r->label->dest->offset);
		} else if (r->type == rel_def_string) {
			def_t       d;
			d.space = r->space;
			d.offset = r->offset;
			qfo_encode_one_reloc (r, &reloc, D_STRING (&d));
		} else {
			qfo_encode_one_reloc (r, &reloc, 0);
		}
	}

	return qfo;
}

static int
qfo_space_size (qfo_mspace_t *space)
{
	switch (space->type) {
		case qfos_null:
			return 0;
		case qfos_code:
			return space->data_size * sizeof (*space->code);
		case qfos_data:
			// data size > 0 but data == null -> all data is zero
			if (!space->data)
				return 0;
			return space->data_size * sizeof (*space->data);
		case qfos_string:
			return space->data_size * sizeof (*space->strings);
		case qfos_entity:
			return 0;
		case qfos_type:
			return space->data_size * sizeof (*space->data);
		case qfos_debug:
			return space->data_size * sizeof (*space->data);
	}
	return 0;
}

static void
qfo_byteswap_space (void *space, int size, qfos_type_t type)
{
	int         c;
	pr_type_t  *val;
	dstatement_t *s;
	switch (type) {
		case qfos_null:
		case qfos_string:
			break;
		case qfos_code:
			for (s = (dstatement_t *) space, c = 0; c < size; c++, s++) {
				s->op = LittleShort (s->op);
				s->a = LittleShort (s->a);
				s->b = LittleShort (s->b);
				s->c = LittleShort (s->c);
			}
			break;
		case qfos_data:
		case qfos_entity:
		case qfos_type:
		case qfos_debug:
			for (val = (pr_type_t *) space, c = 0; c < size; c++, val++)
				val->int_var = LittleLong (val->int_var);
			break;
	}
}

int
qfo_write (qfo_t *qfo, const char *filename)
{
	unsigned    size;
	int         space_offset;
	unsigned    i;
	byte       *data;
	qfo_header_t *header;
	qfo_space_t *spaces;
	qfo_reloc_t *relocs;
	qfo_def_t  *defs;
	qfo_func_t *funcs;
	pr_lineno_t *lines;
	byte       *space_data;
	QFile      *file;

	file = Qopen (filename, options.gzip ? "wbz9" : "wb");
	if (!file) {
		perror (va (0, "failed to open %s for writing", filename));
		return -1;
	}

	size = sizeof (qfo_header_t);
	size += sizeof (qfo_space_t) * qfo->num_spaces;
	size += sizeof (qfo_reloc_t) * qfo->num_relocs;
	size += sizeof (qfo_def_t) * qfo->num_defs;
	size += sizeof (qfo_func_t) * qfo->num_funcs;
	size += sizeof (pr_lineno_t) * qfo->num_lines;
	space_offset = size;
	for (i = 0; i < qfo->num_spaces; i++)
		size += RUP (qfo_space_size (qfo->spaces + i), 16);
	data = calloc (1, size);
	header = (qfo_header_t *) data;
	memcpy (header->qfo, QFO, 4);
	header->version = LittleLong (QFO_VERSION);
	header->num_spaces = LittleLong (qfo->num_spaces);
	header->num_relocs = LittleLong (qfo->num_relocs);
	header->num_defs = LittleLong (qfo->num_defs);
	header->num_funcs = LittleLong (qfo->num_funcs);
	header->num_lines = LittleLong (qfo->num_lines);
	header->num_loose_relocs = LittleLong (qfo->num_loose_relocs);
	header->progs_version = LittleLong (options.code.progsversion);
	spaces = (qfo_space_t *) &header[1];
	relocs = (qfo_reloc_t *) &spaces[qfo->num_spaces];
	defs = (qfo_def_t *) &relocs[qfo->num_relocs];
	funcs = (qfo_func_t *) &defs[qfo->num_defs];
	lines = (pr_lineno_t *) &funcs[qfo->num_funcs];
	space_data = data + space_offset;
	for (i = 0; i < qfo->num_spaces; i++) {
		spaces[i].type = LittleLong (qfo->spaces[i].type);
		if (qfo->spaces[i].defs)
			spaces[i].defs = LittleLong (qfo->spaces[i].defs - qfo->defs);
		spaces[i].num_defs = LittleLong (qfo->spaces[i].num_defs);
		if (qfo->spaces[i].data) {
			int         space_size = qfo_space_size (qfo->spaces + i);
			spaces[i].data = LittleLong (space_data - data);
			memcpy (space_data, qfo->spaces[i].data, space_size);
			qfo_byteswap_space (space_data, qfo->spaces[i].data_size,
								qfo->spaces[i].type);
			space_data += RUP (space_size, 16);
		}
		spaces[i].data_size = LittleLong (qfo->spaces[i].data_size);
		spaces[i].id = LittleLong (qfo->spaces[i].id);
	}
	for (i = 0; i < qfo->num_relocs; i++) {
		relocs[i].space = LittleLong (qfo->relocs[i].space);
		relocs[i].offset = LittleLong (qfo->relocs[i].offset);
		relocs[i].type = LittleLong (qfo->relocs[i].type);
		relocs[i].target = LittleLong (qfo->relocs[i].target);
	}
	for (i = 0; i < qfo->num_defs; i++) {
		defs[i].type = LittleLong (qfo->defs[i].type);
		defs[i].name = LittleLong (qfo->defs[i].name);
		defs[i].offset = LittleLong (qfo->defs[i].offset);
		defs[i].relocs = LittleLong (qfo->defs[i].relocs);
		defs[i].num_relocs = LittleLong (qfo->defs[i].num_relocs);
		defs[i].flags = LittleLong (qfo->defs[i].flags);
		defs[i].file = LittleLong (qfo->defs[i].file);
		defs[i].line = LittleLong (qfo->defs[i].line);
	}
	for (i = 0; i < qfo->num_funcs; i++) {
		funcs[i].name = LittleLong (qfo->funcs[i].name);
		funcs[i].type = LittleLong (qfo->funcs[i].type);
		funcs[i].file = LittleLong (qfo->funcs[i].file);
		funcs[i].line = LittleLong (qfo->funcs[i].line);
		funcs[i].code = LittleLong (qfo->funcs[i].code);
		funcs[i].def = LittleLong (qfo->funcs[i].def);
		funcs[i].locals_space = LittleLong (qfo->funcs[i].locals_space);
		funcs[i].line_info = LittleLong (qfo->funcs[i].line_info);
		funcs[i].relocs = LittleLong (qfo->funcs[i].relocs);
		funcs[i].num_relocs = LittleLong (qfo->funcs[i].num_relocs);
		funcs[i].params_start = LittleLong (qfo->funcs[i].params_start);
	}
	for (i = 0; i < qfo->num_lines; i++) {
		lines[i].fa.addr = LittleLong (qfo->lines[i].fa.addr);
		lines[i].line = LittleLong (qfo->lines[i].line);
	}

	Qwrite (file, data, size);
	free (data);
	Qclose (file);
	return 0;
}

qfo_t *
qfo_read (QFile *file)
{
	char       *data;
	int         size;
	qfo_header_t *header;
	qfo_space_t *spaces;
	qfo_t      *qfo;
	unsigned    i;

	size = Qfilesize (file);
	data = malloc (size);
	Qread (file, data, size);

	header = (qfo_header_t *) data;
	header->version = LittleLong (header->version);
	if (memcmp (header->qfo, QFO, 4) || header->version != QFO_VERSION) {
		fprintf (stderr, "not a valid qfo file\n");
		free (data);
		return 0;
	}
	header->progs_version = LittleLong (header->progs_version);
	if (header->progs_version != PROG_ID_VERSION
		&& header->progs_version != PROG_V6P_VERSION
		&& header->progs_version != PROG_VERSION) {
		fprintf (stderr, "not a compatible qfo file\n");
		free (data);
		return 0;
	}
	// qfprogs leaves progsversion as 0
	if (options.code.progsversion
		&& header->progs_version != options.code.progsversion) {
		fprintf (stderr, "qfo file target VM does not match current target\n");
		free (data);
		return 0;
	}
	qfo = calloc (1, sizeof (qfo_t));

	qfo->num_spaces = LittleLong (header->num_spaces);
	qfo->num_relocs = LittleLong (header->num_relocs);
	qfo->num_defs = LittleLong (header->num_defs);
	qfo->num_funcs = LittleLong (header->num_funcs);
	qfo->num_lines = LittleLong (header->num_lines);
	qfo->num_loose_relocs = LittleLong (header->num_loose_relocs);
	qfo->progs_version = header->progs_version;	//already swapped

	spaces = (qfo_space_t *) &header[1];
	qfo->data = data;
	qfo->spaces = calloc (sizeof (qfo_mspace_t), qfo->num_spaces);
	qfo->relocs = (qfo_reloc_t *) &spaces[qfo->num_spaces];
	qfo->defs = (qfo_def_t *) &qfo->relocs[qfo->num_relocs];
	qfo->funcs = (qfo_func_t *) &qfo->defs[qfo->num_defs];
	qfo->lines = (pr_lineno_t *) &qfo->funcs[qfo->num_funcs];

	for (i = 0; i < qfo->num_spaces; i++) {
		qfo->spaces[i].type = LittleLong (spaces[i].type);
		qfo->spaces[i].num_defs = LittleLong (spaces[i].num_defs);
		if (qfo->spaces[i].num_defs)
			qfo->spaces[i].defs = qfo->defs + LittleLong (spaces[i].defs);
		qfo->spaces[i].data_size = LittleLong (spaces[i].data_size);
		if (spaces[i].data) {
			qfo->spaces[i].strings = data + LittleLong (spaces[i].data);
			qfo_byteswap_space (qfo->spaces[i].data,
								qfo->spaces[i].data_size, qfo->spaces[i].type);
		}
		qfo->spaces[i].id = LittleLong (spaces[i].id);
	}
	for (i = 0; i < qfo->num_relocs; i++) {
		qfo->relocs[i].space = LittleLong (qfo->relocs[i].space);
		qfo->relocs[i].offset = LittleLong (qfo->relocs[i].offset);
		qfo->relocs[i].type = LittleLong (qfo->relocs[i].type);
		qfo->relocs[i].target = LittleLong (qfo->relocs[i].target);
	}
	for (i = 0; i < qfo->num_defs; i++) {
		qfo->defs[i].type = LittleLong (qfo->defs[i].type);
		qfo->defs[i].name = LittleLong (qfo->defs[i].name);
		qfo->defs[i].offset = LittleLong (qfo->defs[i].offset);
		qfo->defs[i].relocs = LittleLong (qfo->defs[i].relocs);
		qfo->defs[i].num_relocs = LittleLong (qfo->defs[i].num_relocs);
		qfo->defs[i].flags = LittleLong (qfo->defs[i].flags);
		qfo->defs[i].file = LittleLong (qfo->defs[i].file);
		qfo->defs[i].line = LittleLong (qfo->defs[i].line);
	}
	for (i = 0; i < qfo->num_funcs; i++) {
		qfo->funcs[i].name = LittleLong (qfo->funcs[i].name);
		qfo->funcs[i].type = LittleLong (qfo->funcs[i].type);
		qfo->funcs[i].file = LittleLong (qfo->funcs[i].file);
		qfo->funcs[i].line = LittleLong (qfo->funcs[i].line);
		qfo->funcs[i].code = LittleLong (qfo->funcs[i].code);
		qfo->funcs[i].def = LittleLong (qfo->funcs[i].def);
		qfo->funcs[i].locals_space = LittleLong (qfo->funcs[i].locals_space);
		qfo->funcs[i].line_info = LittleLong (qfo->funcs[i].line_info);
		qfo->funcs[i].relocs = LittleLong (qfo->funcs[i].relocs);
		qfo->funcs[i].num_relocs = LittleLong (qfo->funcs[i].num_relocs);
	}
	for (i = 0; i < qfo->num_lines; i++) {
		qfo->lines[i].fa.addr = LittleLong (qfo->lines[i].fa.addr);
		qfo->lines[i].line = LittleLong (qfo->lines[i].line);
	}
	return qfo;
}

qfo_t *
qfo_open (const char *filename)
{
	qfo_t      *qfo;
	QFile      *file;

	file = Qopen (filename, "rbz");
	if (!file) {
		perror (filename);
		return 0;
	}
	qfo = qfo_read (file);
	Qclose (file);
	return qfo;
}

qfo_t *
qfo_new (void)
{
	return calloc (1, sizeof (qfo_t));
}

void
qfo_delete (qfo_t *qfo)
{
	if (qfo->data) {
		free (qfo->data);
	} else {
		unsigned    i;
		for (i = 0; i < qfo->num_spaces; i++)
			free (qfo->spaces[i].data);
		free (qfo->relocs);
		free (qfo->defs);
		free (qfo->funcs);
		free (qfo->lines);
	}
	free (qfo->spaces);
	free (qfo);
}

static etype_t
get_def_type (qfo_t *qfo, pr_ptr_t type)
{
	qfot_type_t *type_def;
	if (type >= qfo->spaces[qfo_type_space].data_size)
		return ev_void;
	type_def = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, type);
	switch ((ty_meta_e)type_def->meta) {
		case ty_alias:	//XXX
		case ty_basic:
			// field, pointer and function types store their basic type in
			// the same location.
			return type_def->type;
		case ty_struct:
		case ty_union:
			return ev_invalid;
		case ty_enum:
			if (options.code.progsversion == PROG_ID_VERSION)
				return ev_float;
			return ev_int;
		case ty_array:
		case ty_class:
			return ev_invalid;
	}
	return ev_invalid;
}

static __attribute__((pure)) int
get_type_size (qfo_t *qfo, pr_ptr_t type)
{
	qfot_type_t *type_def;
	int          i, size;
	if (type >= qfo->spaces[qfo_type_space].data_size)
		return 1;
	type_def = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, type);
	switch ((ty_meta_e)type_def->meta) {
		case ty_alias:
			return get_type_size (qfo, type_def->alias.aux_type);
		case ty_basic:
			// field, pointer and function types store their basic type in
			// the same location.
			return pr_type_size[type_def->type];
		case ty_struct:
			for (i = size = 0; i < type_def->strct.num_fields; i++)
				size += get_type_size (qfo, type_def->strct.fields[i].type);
			return size;
		case ty_union:
			for (i = size = 0; i < type_def->strct.num_fields; i++) {
				int         s;
				s = get_type_size (qfo, type_def->strct.fields[i].type);
				if (s > size)
					size = s;
			}
			return size;
		case ty_enum:
			return pr_type_size[ev_int];
		case ty_array:
			return type_def->array.size
					* get_type_size (qfo, type_def->array.type);
		case ty_class:
			return 0;	// FIXME
	}
	return 0;
}

int
qfo_log2 (unsigned x)
{
	int         log2 = 0;

	while (x > 1) {
		x >>= 1;
		log2++;
	}
	return log2;
}

static __attribute__((pure)) int
get_type_alignment_log (qfo_t *qfo, pr_ptr_t type)
{
	qfot_type_t *type_def;
	int          i, alignment;
	if (type >= qfo->spaces[qfo_type_space].data_size)
		return 0;
	type_def = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, type);
	switch ((ty_meta_e)type_def->meta) {
		case ty_alias:
			return get_type_alignment_log (qfo, type_def->alias.aux_type);
		case ty_basic:
			// field, pointer and function types store their basic type in
			// the same location.
			return qfo_log2 (ev_types[type_def->type]->alignment);
		case ty_struct:
		case ty_union:
			for (i = alignment = 0; i < type_def->strct.num_fields; i++) {
				qfot_var_t *field = type_def->strct.fields + i;
				int         a;
				a = get_type_alignment_log (qfo, field->type);
				if (a > alignment) {
					alignment = a;
				}
			}
			return alignment;
		case ty_enum:
			return qfo_log2 (ev_types[ev_int]->alignment);
		case ty_array:
			return get_type_alignment_log (qfo, type_def->array.type);
		case ty_class:
			return 0;	// FIXME
	}
	return 0;
}

static __attribute__((pure)) dparmsize_t
get_paramsize (qfo_t *qfo, pr_ptr_t type)
{
	dparmsize_t paramsize = {
		get_type_size (qfo, type),
		get_type_alignment_log (qfo, type),
	};
	return paramsize;
}

static void
function_params (qfo_t *qfo, qfo_func_t *func, dfunction_t *df)
{
	qfot_type_t *type;
	int         num_params;
	int         i;

	if (func->type >= qfo->spaces[qfo_type_space].data_size)
		return;
	type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, func->type);
	if (type->meta == ty_alias) {
		type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t,
							type->alias.aux_type);
	}
	if (type->meta != ty_basic || type->type != ev_func)
		return;
	df->numparams = num_params = type->func.num_params;
	if (num_params < 0)
		num_params = ~num_params;
	for (i = 0; i < num_params; i++) {
		df->param_size[i] = get_paramsize (qfo, type->func.param_types[i]);
	}
}

static void
qfo_def_to_ddef (qfo_t *qfo, const qfo_def_t *def, ddef_t *ddef)
{
	ddef->type = get_def_type (qfo, def->type);
	ddef->ofs = def->offset;
	ddef->name = def->name;
	if (!(def->flags & QFOD_NOSAVE)
		&& !(def->flags & QFOD_CONSTANT)
		&& (def->flags & QFOD_GLOBAL)
		&& ddef->type != ev_func
		&& ddef->type != ev_field)
		ddef->type |= DEF_SAVEGLOBAL;
}

static void
qfo_def_to_prdef (qfo_t *qfo, const qfo_def_t *def, pr_def_t *prdef)
{
	prdef->type = get_def_type (qfo, def->type);
	prdef->size = get_type_size (qfo, def->type);
	prdef->ofs = def->offset;
	prdef->name = def->name;
	prdef->type_encoding = def->type;
}

static void
qfo_relocate_refs (qfo_t *qfo)
{
	unsigned    i;
	qfo_reloc_t *reloc;

	for (i = 0, reloc = qfo->relocs; i < qfo->num_relocs; i++, reloc++) {
		// this will be valid only for *_def[_ofs] and *_field[_ofs] relocs
		qfo_def_t  *def = qfo->defs + reloc->target;

		switch (reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
				QFO_STATEMENT (qfo, reloc->offset)->a = def->offset;
				break;
			case rel_op_b_def:
				QFO_STATEMENT (qfo, reloc->offset)->b = def->offset;
				break;
			case rel_op_c_def:
				QFO_STATEMENT (qfo, reloc->offset)->c = def->offset;
				break;
			case rel_op_a_op:
			case rel_op_b_op:
			case rel_op_c_op:
				// these should never appear in a qfo file
				break;
			case rel_def_op:
				QFO_INT (qfo, reloc->space, reloc->offset) = reloc->target;
				break;
			case rel_def_def:
				QFO_INT (qfo, reloc->space, reloc->offset) = def->offset;
				break;
			case rel_def_func:
				QFO_INT (qfo, reloc->space, reloc->offset) = reloc->target + 1;
				break;
			case rel_def_string:
				QFO_INT (qfo, reloc->space, reloc->offset) = reloc->target;
				break;
			case rel_def_field:
				QFO_INT (qfo, reloc->space, reloc->offset) = def->offset;
				break;
			case rel_op_a_def_ofs:
				QFO_STATEMENT (qfo, reloc->offset)->a += def->offset;
				break;
			case rel_op_b_def_ofs:
				QFO_STATEMENT (qfo, reloc->offset)->b += def->offset;
				break;
			case rel_op_c_def_ofs:
				QFO_STATEMENT (qfo, reloc->offset)->c += def->offset;
				break;
			case rel_def_def_ofs:
				QFO_INT (qfo, reloc->space, reloc->offset) += def->offset;
				break;
			case rel_def_field_ofs:
				QFO_INT (qfo, reloc->space, reloc->offset) += def->offset;
				break;
		}
	}
}

static unsigned
align_globals_size (unsigned size)
{
	if (options.code.progsversion == PROG_ID_VERSION)
		return size;
	return RUP (size, 16 / sizeof (pr_type_t));
}

static int
qfo_def_offset_compare (const void *i1, const void *i2, void *d)
{
	__auto_type defs = (const qfo_def_t *) d;
	unsigned    ind1 = *(unsigned *) i1;
	unsigned    ind2 = *(unsigned *) i2;
	const qfo_def_t *def1 = defs + ind1;
	const qfo_def_t *def2 = defs + ind2;
	return def1->offset - def2->offset;
}

static pr_uint_t
qfo_count_locals (const qfo_t *qfo, pr_uint_t *big_locals)
{
	if (options.code.progsversion == PROG_VERSION) {
		// Ruamoko progs keep their locals on the stack rather than a
		// "protected" area in the globals
		*big_locals = 0;
		return 0;
	}

	pr_uint_t   locals_size = 0;
	for (pr_uint_t i = qfo_num_spaces; i < qfo->num_spaces; i++) {
		if (options.code.local_merging) {
			if (locals_size < qfo->spaces[i].data_size) {
				locals_size = qfo->spaces[i].data_size;
				*big_locals = i;
			}
		} else {
			locals_size += align_globals_size (qfo->spaces[i].data_size);
		}
	}
	return locals_size;
}

typedef struct globals_info_s {
	pr_uint_t   locals_start;
	pr_uint_t   locals_size;
	pr_uint_t   big_locals;
	pr_uint_t   near_data_size;
	pr_uint_t   type_encodings_start;
	pr_uint_t   xdefs_start;
	pr_uint_t   xdefs_size;
	pr_uint_t   globals_size;
} globals_info_t;


static globals_info_t
qfo_count_globals (qfo_t *qfo, dprograms_t *progs, int word_align)
{
	globals_info_t info = {};
	info.globals_size = qfo->spaces[qfo_near_data_space].data_size;
	info.globals_size = RUP (info.globals_size, word_align);

	info.locals_start = info.globals_size;
	info.locals_size = qfo_count_locals (qfo, &info.big_locals);
	info.globals_size += info.locals_size;
	info.near_data_size = info.globals_size;

	info.globals_size = RUP (info.globals_size, word_align);
	info.globals_size += qfo->spaces[qfo_far_data_space].data_size;

	info.type_encodings_start = info.globals_size;
	info.globals_size += qfo->spaces[qfo_type_space].data_size;
	info.globals_size = RUP (info.globals_size, type_xdef.alignment);

	info.xdefs_start = info.globals_size;
	info.xdefs_size = progs->globaldefs.count + progs->fielddefs.count;
	info.xdefs_size *= type_size (&type_xdef);
	info.globals_size += info.xdefs_size;

	return info;
}

static pr_uint_t
qfo_next_offset (pr_chunk_t chunk, pr_uint_t size, pr_uint_t align)
{
	size *= chunk.count;
	return RUP (chunk.offset + size, align);
}

dprograms_t *
qfo_to_progs (qfo_t *in_qfo, int *size)
{
	char       *strings;
	dstatement_t *statements;
	dfunction_t *functions;
	ddef_t     *globaldefs;
	ddef_t     *fielddefs;
	pr_type_t  *globals;
	pr_type_t  *locals;
	pr_type_t  *far_data;
	pr_type_t  *type_data;
	pr_type_t  *xdef_data;
	dprograms_t *progs;
	qfo_def_t  *types_def = 0;
	qfo_def_t  *xdefs_def = 0;
	unsigned    i, j;
	int         big_func = 0;
	pr_xdefs_t *xdefs = 0;
	xdef_t     *xdef;
	unsigned   *def_indices;
	unsigned   *far_def_indices;
	unsigned   *field_def_indices;

	// id progs were aligned to only 4 bytes, but 8 is much more reasonable
	pr_uint_t   byte_align = 8;
	if (options.code.progsversion == PROG_VERSION) {
		byte_align = __alignof__ (pr_lvec4_t);
	} else if (options.code.progsversion == PROG_V6P_VERSION) {
		byte_align = 16;
	}
	pr_uint_t   word_align = byte_align / sizeof (pr_int_t);


	qfo_t      *qfo = alloca (sizeof (qfo_t)
							  + in_qfo->num_spaces * sizeof (qfo_mspace_t));
	*qfo = *in_qfo;
	qfo->spaces = (qfo_mspace_t *) (qfo + 1);
	for (i = 0; i < qfo->num_spaces; i++) {
		qfo->spaces[i] = in_qfo->spaces[i];
	}

	*size = RUP (sizeof (dprograms_t), 16);
	progs = calloc (1, *size);
	progs->version = options.code.progsversion;
	// crc is set in qfcc.c if enabled

	// these are in order in which they usually appear in the file rather
	// than the order they appear in the struct, though with the offsets
	// it doesn't matter too much. However, as people expect a certain
	// layout, ti does matter enough to preserve the traditional file order.
	progs->strings.offset = *size;
	progs->strings.count = qfo->spaces[qfo_strings_space].data_size;

	progs->statements.offset = qfo_next_offset (progs->strings,
												sizeof (char),
												byte_align);
	progs->statements.count = qfo->spaces[qfo_code_space].data_size;

	progs->functions.offset = qfo_next_offset (progs->statements,
											   sizeof (dstatement_t),
											   byte_align);
	progs->functions.count = qfo->num_funcs + 1;

	progs->globaldefs.offset = qfo_next_offset (progs->functions,
												sizeof (dfunction_t),
												byte_align);
	progs->globaldefs.count = qfo->spaces[qfo_near_data_space].num_defs;
	//ddef offsets are 16 bits so the ddef ofs will likely be invalid
	//thus it will be forced invalid and the true offset written to the
	//.xdefs array in the progs file
	progs->globaldefs.count += qfo->spaces[qfo_far_data_space].num_defs;

	progs->fielddefs.offset = qfo_next_offset (progs->globaldefs,
											   sizeof (ddef_t),
											   byte_align);
	progs->fielddefs.count = qfo->spaces[qfo_entity_space].num_defs;

	progs->globals.offset = qfo_next_offset (progs->fielddefs,
											 sizeof (ddef_t),
											 byte_align);
	globals_info_t globals_info = qfo_count_globals (qfo, progs, word_align);
	progs->globals.count = globals_info.globals_size;

	progs->entityfields = qfo->spaces[qfo_entity_space].data_size;
	// qfo_debug_space does not go in the progs file

	*size = qfo_next_offset (progs->globals, sizeof (pr_type_t), 1);

	progs = realloc (progs, *size);
	memset (progs + 1, 0, *size - sizeof (dprograms_t));

	def_indices = alloca ((progs->globaldefs.count + progs->fielddefs.count)
						  * sizeof (*def_indices));
	far_def_indices = def_indices + qfo->spaces[qfo_near_data_space].num_defs;
	field_def_indices = def_indices + progs->globaldefs.count;
	for (unsigned i = 0; i < qfo->spaces[qfo_near_data_space].num_defs; i++) {
		def_indices[i] = i;
	}
	for (unsigned i = 0; i < qfo->spaces[qfo_far_data_space].num_defs; i++) {
		far_def_indices[i] = i;
	}
	for (unsigned i = 0; i < qfo->spaces[qfo_entity_space].num_defs; i++) {
		field_def_indices[i] = i;
	}
	qsort_r (def_indices, qfo->spaces[qfo_near_data_space].num_defs,
			 sizeof (unsigned), qfo_def_offset_compare,
			 qfo->spaces[qfo_near_data_space].defs);
	qsort_r (far_def_indices, qfo->spaces[qfo_far_data_space].num_defs,
			 sizeof (unsigned), qfo_def_offset_compare,
			 qfo->spaces[qfo_far_data_space].defs);
	qsort_r (field_def_indices, qfo->spaces[qfo_entity_space].num_defs,
			 sizeof (unsigned), qfo_def_offset_compare,
			 qfo->spaces[qfo_entity_space].defs);

#define qfo_block(t,b) (t *) ((byte *) progs + progs->b.offset)
	strings = qfo_block (char, strings);
	statements = qfo_block (dstatement_t, statements);
	functions = qfo_block (dfunction_t, functions);
	functions++;	// skip over null function
	globaldefs = qfo_block (ddef_t, globaldefs);
	fielddefs = qfo_block (ddef_t, fielddefs);
	globals = qfo_block (pr_type_t, globals);
	locals = globals + globals_info.locals_start;
	far_data = globals + globals_info.near_data_size;
	type_data = globals + globals_info.type_encodings_start;
	xdef_data = globals + globals_info.xdefs_start;

	memcpy (strings, qfo->spaces[qfo_strings_space].strings,
			qfo->spaces[qfo_strings_space].data_size * sizeof (char));
	qfo->spaces[qfo_strings_space].strings = strings;

	memcpy (statements, qfo->spaces[qfo_code_space].code,
			qfo->spaces[qfo_code_space].data_size * sizeof (dstatement_t));
	qfo->spaces[qfo_code_space].code = statements;

	for (i = 0; i < qfo->num_funcs; i++) {
		dfunction_t *df = functions + i;
		qfo_func_t *qf = qfo->funcs + i;
		qfo_mspace_t *space = qfo->spaces + qf->locals_space;

		df->first_statement = qf->code;
		df->locals = space->data_size;
		if (options.code.progsversion < PROG_VERSION) {
			df->params_start = globals_info.locals_start;
			// finalize the offsets of the local defs
			for (j = 0; j < space->num_defs; j++)
				space->defs[j].offset += globals_info.locals_start;
			if (!options.code.local_merging)
				globals_info.locals_start += align_globals_size (df->locals);
		} else {
			// relative to start of locals for Ruamoko progs
			df->params_start = qf->params_start;
		}
		df->profile = 0;
		df->name = qf->name;
		df->file = qf->file;
		function_params (qfo, qf, df);
	}

	for (i = 0; i < qfo->spaces[qfo_near_data_space].num_defs; i++) {
		unsigned    ind = def_indices[i];
		qfo_def_t  *def = qfo->spaces[qfo_near_data_space].defs + ind;
		const char *defname = QFO_GETSTR (qfo, def->name);
		if (!strcmp (defname, ".type_encodings"))
			types_def = def;
		if (!strcmp (defname, ".xdefs"))
			xdefs_def = def;
		qfo_def_to_ddef (qfo, def, globaldefs++);
	}

	for (i = 0; i < qfo->spaces[qfo_far_data_space].num_defs; i++) {
		unsigned    ind = far_def_indices[i];
		qfo_def_t  *def = qfo->spaces[qfo_far_data_space].defs + ind;
		def->offset += far_data - globals;
		qfo_def_to_ddef (qfo, def, globaldefs);
		// the correct offset will be written to the far data space
		globaldefs->ofs = -1;
		globaldefs++;
	}

	for (i = 0; i < qfo->spaces[qfo_type_space].num_defs; i++) {
		qfo->spaces[qfo_type_space].defs[i].offset += globals_info.type_encodings_start;
	}

	for (i = 0; i < qfo->spaces[qfo_entity_space].num_defs; i++) {
		unsigned    ind = field_def_indices[i];
		qfo_def_to_ddef (qfo, qfo->spaces[qfo_entity_space].defs + ind,
					 fielddefs + i);
	}

	// copy near data
	memcpy (globals, qfo->spaces[qfo_near_data_space].data,
			qfo->spaces[qfo_near_data_space].data_size * sizeof (pr_type_t));
	qfo->spaces[qfo_near_data_space].data = globals;
	// clear locals data
	memset (locals, 0, globals_info.locals_size * sizeof (pr_type_t));
	// copy far data
	memcpy (far_data, qfo->spaces[qfo_far_data_space].data,
			qfo->spaces[qfo_far_data_space].data_size * sizeof (pr_type_t));
	qfo->spaces[qfo_far_data_space].data = far_data;
	// copy type data
	memcpy (type_data, qfo->spaces[qfo_type_space].data,
			qfo->spaces[qfo_type_space].data_size * sizeof (pr_type_t));
	qfo->spaces[qfo_type_space].data = type_data;

	qfo_relocate_refs (qfo);
	if (types_def) {
		qfot_type_encodings_t *encodings;
		encodings = (qfot_type_encodings_t *) &globals[types_def->offset];
		encodings->types = globals_info.type_encodings_start;
		encodings->size = qfo->spaces[qfo_type_space].data_size;
	}
	if (xdefs_def) {
		xdefs = (pr_xdefs_t *) &globals[xdefs_def->offset];
		xdef = (xdef_t *) xdef_data;
		xdefs->xdefs = globals_info.xdefs_start;
		xdefs->num_xdefs = progs->globaldefs.count + progs->fielddefs.count;
		for (i = 0; i < qfo->spaces[qfo_near_data_space].num_defs;
			 i++, xdef++) {
			qfo_def_t  *def = qfo->spaces[qfo_near_data_space].defs + i;
			xdef->type = def->type + globals_info.type_encodings_start;
			xdef->ofs = def->offset;
		}
		for (i = 0; i < qfo->spaces[qfo_far_data_space].num_defs;
			 i++, xdef++) {
			qfo_def_t  *def = qfo->spaces[qfo_far_data_space].defs + i;
			xdef->type = def->type + globals_info.type_encodings_start;
			xdef->ofs = def->offset;
		}
		for (i = 0; i < qfo->spaces[qfo_entity_space].num_defs;
			 i++, xdef++) {
			qfo_def_t  *def =  qfo->spaces[qfo_entity_space].defs + i;
			xdef->type = def->type + globals_info.type_encodings_start;
			xdef->ofs = def->offset;
		}
	}

	// undo the relocation of the offsets of local defs so the local defs have
	// the correct offset in the debug info
	for (i = 0; i < qfo->num_funcs; i++) {
		dfunction_t *df = functions + i;
		qfo_func_t *qf = qfo->funcs + i;
		qfo_mspace_t *space = qfo->spaces + qf->locals_space;

		if (qf->locals_space == globals_info.big_locals)
			big_func = i;
		for (j = 0; j < space->num_defs; j++)
			space->defs[j].offset -= df->params_start;
	}

	if (options.verbosity >= 0) {
		const char *big_function = "";
		if (big_func)
			big_function = va (0, " (%s)", strings + qfo->funcs[big_func].name);
		printf ("%6i strofs\n", progs->strings.count);
		printf ("%6i statements\n", progs->statements.count);
		printf ("%6i functions\n", progs->functions.count);
		printf ("%6i global defs\n", progs->globaldefs.count);
		printf ("%6i field defs\n", progs->fielddefs.count);
		printf ("%6i globals\n", progs->globals.count);
		printf ("    %6i near globals\n", globals_info.near_data_size);
		printf ("        %6i locals size%s\n",
				globals_info.locals_size, big_function);
		printf ("    %6i far globals\n",
				qfo->spaces[qfo_far_data_space].data_size);
		printf ("    %6i type globals\n",
				qfo->spaces[qfo_type_space].data_size);
		if (xdefs) {
			printf ("    %6i extended defs\n",
					xdefs->num_xdefs * type_size (&type_xdef));
		}
		printf ("%6i entity fields\n", progs->entityfields);
	}

	if (options.verbosity >= -1)
		printf ("%6i TOTAL SIZE\n", *size);

	return progs;
}

pr_debug_header_t *
qfo_to_sym (qfo_t *qfo, int *size)
{
	pr_debug_header_t *sym;
	unsigned    i, j;
	pr_auxfunction_t *auxfuncs;
	pr_auxfunction_t *aux;
	pr_lineno_t *linenos;
	pr_def_t   *locals, *ld, *debug_defs;
	pr_type_t  *debug_data;

	*size = sizeof (pr_debug_header_t);
	sym = calloc (1, *size);

	sym->version = PROG_DEBUG_VERSION;
	for (i = 0; i < qfo->num_funcs; i++) {
		qfo_func_t *func = qfo->funcs + i;
		unsigned    num_locals = 0;

		if (func->locals_space)
			num_locals = qfo->spaces[func->locals_space].num_defs;
		if (!func->line_info && !num_locals)
			continue;
		sym->num_auxfunctions++;
		sym->num_locals += num_locals;
	}
	sym->num_linenos = qfo->num_lines;
	sym->num_debug_defs = qfo->spaces[qfo_debug_space].num_defs;
	sym->debug_data_size = qfo->spaces[qfo_debug_space].data_size;

	*size += sym->num_auxfunctions * sizeof (pr_auxfunction_t);
	*size += sym->num_linenos * sizeof (pr_lineno_t);
	*size += sym->num_locals * sizeof (pr_def_t);
	*size += sym->num_debug_defs * sizeof (pr_def_t);
	*size += sym->debug_data_size * sizeof (pr_type_t);
	sym = realloc (sym, *size);

	auxfuncs = (pr_auxfunction_t *)(sym + 1);
	linenos = (pr_lineno_t *)(auxfuncs + sym->num_auxfunctions);
	locals = (pr_def_t *)(linenos + sym->num_linenos);
	debug_defs = locals + sym->num_locals;
	debug_data = (pr_type_t *)(debug_defs + sym->num_debug_defs);

	sym->auxfunctions = (char *) auxfuncs - (char *) sym;
	sym->linenos = (char *) linenos - (char *) sym;
	sym->locals = (char *) locals - (char *) sym;
	sym->debug_defs = (char *) debug_defs - (char *) sym;
	sym->debug_data = (char *) debug_data - (char *) sym;

	ld = locals;

	for (i = 0, aux = auxfuncs; i < qfo->num_funcs; i++) {
		qfo_func_t *func = qfo->funcs + i;
		qfo_def_t  *def = 0;
		unsigned    num_locals = 0;
		qfot_type_t *type;

		if (func->locals_space) {
			num_locals = qfo->spaces[func->locals_space].num_defs;
			def = qfo->spaces[func->locals_space].defs;
		}
		if (!func->line_info && !num_locals)
			continue;
		memset (aux, 0, sizeof (*aux));
		aux->function = i + 1;
		aux->source_line = func->line;
		aux->line_info = func->line_info;
		if (func->line_info)
			qfo->lines[func->line_info].fa.func = aux - auxfuncs;
		if (num_locals) {
			aux->local_defs = ld - locals;
			for (j = 0; j < num_locals; j++, def++, ld++) {
				qfo_def_to_prdef (qfo, def, ld);
			}
		}
		aux->num_locals = num_locals;
		//FIXME check type
		type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t, func->type);
		if (type->meta == ty_alias) {
			type = QFO_POINTER (qfo, qfo_type_space, qfot_type_t,
								type->alias.aux_type);
		}
		aux->return_type = type->func.return_type;
		aux++;
	}
	memcpy (linenos, qfo->lines, qfo->num_lines * sizeof (pr_lineno_t));
	for (i = 0; i < sym->num_debug_defs; i++) {
		qfo_def_t  *def = &qfo->spaces[qfo_debug_space].defs[i];
		pr_def_t   *prdef = &debug_defs[i];
		qfo_def_to_prdef (qfo, def, prdef);
	}
	memcpy (debug_data, qfo->spaces[qfo_debug_space].data,
			sym->debug_data_size * sizeof (*debug_data));
	return sym;
}
