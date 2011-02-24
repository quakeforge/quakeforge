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
#include "symtab.h"
#include "type.h"

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
	return flags;
}

static int
qfo_encode_relocs (qfo_t *qfo, reloc_t *relocs, qfo_reloc_t **qfo_relocs,
				   qfo_def_t *def)
{
	int         count;
	reloc_t    *r;
	qfo_reloc_t *q;

	for (count = 0, r = relocs; r; r = r->next) {
		count++;
		q = (*qfo_relocs)++;
		if (r->space)	// op_* relocs do not have a space (code is implied)
			q->space = r->space->qfo_space;
		q->offset = r->offset;
		q->type = r->type;
		q->def = def - qfo->defs;
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
		q->num_relocs = qfo_encode_relocs (qfo, d->relocs, qfo_relocs, q);
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
		if (func->symtab && func->symtab->space) {
			qfo->num_spaces++;
			qfo_count_space_stuff (qfo, func->symtab->space);
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
	qfo_count_function_stuff (qfo, pr->func_head);
	qfo->num_relocs += count_relocs (pr->relocs);
}

static void
qfo_init_string_space (qfo_t *qfo, qfo_mspace_t *space, strpool_t *strings)
{
	strings->qfo_space = space - qfo->spaces;
	space->type = qfos_string;
	space->num_defs = 0;
	space->defs = 0;
	space->d.strings = strings->strings;
	space->data_size = strings->size;
}

static void
qfo_init_code_space (qfo_t *qfo, qfo_mspace_t *space, codespace_t *code)
{
	code->qfo_space = space - qfo->spaces;
	space->type = qfos_code;
	space->num_defs = 0;
	space->defs = 0;
	space->d.code = code->code;
	space->data_size = code->size;
}

static void
qfo_init_data_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					 qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_data;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->d.data = data->data;
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
	space->d.data = 0;
	space->data_size = data->size;
}

static void
qfo_init_type_space (qfo_t *qfo, qfo_def_t **defs, qfo_reloc_t **relocs,
					 qfo_mspace_t *space, defspace_t *data)
{
	data->qfo_space = space - qfo->spaces;
	space->type = qfos_type;
	space->defs = *defs;
	space->num_defs = qfo_encode_defs (qfo, data->defs, defs, relocs);
	space->d.data = data->data;
	space->data_size = data->size;
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
		if (f->symtab && f->symtab->space) {
			qfo_init_data_space (qfo, defs, relocs, space++, f->symtab->space);
			q->locals_space = f->symtab->space->qfo_space;
		}
		if (f->aux)
			q->line_info = f->aux->line_info;
		q->relocs = *relocs - qfo->relocs;
		q->num_relocs = qfo_encode_relocs (qfo, f->refs, relocs, 0);
	}
}

qfo_t *
qfo_from_progs (pr_info_t *pr)
{
	qfo_t      *qfo;
	qfo_def_t  *def;
	qfo_reloc_t *reloc;

	qfo = calloc (1, sizeof (qfo_t));
	qfo->num_spaces = qfo_num_spaces; // certain spaces are always present
	qfo_count_stuff (qfo, pr);
	qfo->spaces = calloc (qfo->num_spaces, sizeof (qfo_mspace_t));
	qfo->relocs = calloc (qfo->num_relocs, sizeof (qfo_reloc_t));
	qfo->defs = calloc (qfo->num_defs, sizeof (qfo_def_t));
	qfo->funcs = calloc (qfo->num_funcs, sizeof (qfo_func_t));

	def = qfo->defs;
	reloc = qfo->relocs;

	qfo_init_code_space (qfo, &qfo->spaces[qfo_code_space], pr->code);
	qfo_init_data_space (qfo, &def, &reloc, &qfo->spaces[qfo_near_data_space],
						 pr->near_data);
	qfo_init_data_space (qfo, &def, &reloc, &qfo->spaces[qfo_far_data_space],
						 pr->far_data);
	qfo_init_entity_space (qfo, &def, &reloc, &qfo->spaces[qfo_entity_space],
						   pr->entity_data);
	qfo_init_type_space (qfo, &def, &reloc, &qfo->spaces[qfo_type_space],
						 pr->type_data);

	qfo_encode_functions (qfo, &def, &reloc, qfo->spaces + qfo_num_spaces,
						  pr->func_head);
	qfo->lines = pr->linenos;
	qfo->num_lines = pr->num_linenos;
	// strings must be done last because encoding defs and functions adds the
	// names
	qfo_init_string_space (qfo, &qfo->spaces[qfo_strings_space], pr->strings);

	qfo->num_loose_relocs = qfo->num_relocs - (reloc - qfo->relocs);
	qfo_encode_relocs (qfo, pr->relocs, &reloc, 0);

	return qfo;
}

static int
qfo_space_size (qfo_mspace_t *space)
{
	switch (space->type) {
		case qfos_null:
			return 0;
		case qfos_code:
			return space->data_size * sizeof (*space->d.code);
		case qfos_data:
			// data size > 0 but d.data == null -> all data is zero
			if (!space->d.data)
				return 0;
			return space->data_size * sizeof (*space->d.data);
		case qfos_string:
			return space->data_size * sizeof (*space->d.strings);
		case qfos_entity:
			return 0;
		case qfos_type:
			return space->data_size * sizeof (*space->d.data);
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
			for (val = (pr_type_t *) space, c = 0; c < size; c++, val++)
				val->integer_var = LittleLong (val->integer_var);
			break;
	}
}

int
qfo_write (qfo_t *qfo, const char *filename)
{
	int         size;
	int         space_offset;
	int         i;
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
	if (!file)
		return -1;

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
		if (qfo->spaces[i].d.data) {
			int         space_size = qfo_space_size (qfo->spaces + i);
			spaces[i].data = LittleLong (space_data - data);
			memcpy (space_data, qfo->spaces[i].d.data, space_size);
			qfo_byteswap_space (space_data, qfo->spaces[i].data_size,
								qfo->spaces[i].type);
			space_data += RUP (space_size, 16);
		}
		spaces[i].data_size = LittleLong (qfo->spaces[i].data_size);
	}
	for (i = 0; i < qfo->num_relocs; i++) {
		relocs[i].space = LittleLong (qfo->relocs[i].space);
		relocs[i].offset = LittleLong (qfo->relocs[i].offset);
		relocs[i].type = LittleLong (qfo->relocs[i].type);
		relocs[i].def = LittleLong (qfo->relocs[i].def);
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
	int         i;

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
	qfo = calloc (1, sizeof (qfo_t));

	qfo->num_spaces = LittleLong (header->num_spaces);
	qfo->num_relocs = LittleLong (header->num_relocs);
	qfo->num_defs = LittleLong (header->num_defs);
	qfo->num_funcs = LittleLong (header->num_funcs);
	qfo->num_lines = LittleLong (header->num_lines);
	qfo->num_loose_relocs = LittleLong (header->num_loose_relocs);

	spaces = (qfo_space_t *) &header[1];
	qfo->data = data;
	qfo->spaces = calloc (sizeof (qfo_mspace_t), qfo->num_spaces);
	qfo->relocs = (qfo_reloc_t *) &spaces[qfo->num_spaces];
	qfo->defs = (qfo_def_t *) &qfo->relocs[qfo->num_relocs];
	qfo->funcs = (qfo_func_t *) &qfo->defs[qfo->num_defs];
	qfo->lines = (pr_lineno_t *) &qfo->funcs[qfo->num_funcs];

	for (i = 0; i < qfo->num_spaces; i++) {
		qfo->spaces[i].type = LittleLong (spaces[i].type);
		qfo->spaces[i].defs = qfo->defs + LittleLong (spaces[i].defs);
		qfo->spaces[i].num_defs = LittleLong (spaces[i].num_defs);
		qfo->spaces[i].data_size = LittleLong (spaces[i].data_size);
		if (spaces[i].data) {
			qfo->spaces[i].d.strings = data + LittleLong (spaces[i].data);
			qfo_byteswap_space (qfo->spaces[i].d.data,
								qfo->spaces[i].data_size, qfo->spaces[i].type);
		}
	}
	for (i = 0; i < qfo->num_relocs; i++) {
		qfo->relocs[i].space = LittleLong (qfo->relocs[i].space);
		qfo->relocs[i].offset = LittleLong (qfo->relocs[i].offset);
		qfo->relocs[i].type = LittleLong (qfo->relocs[i].type);
		qfo->relocs[i].def = LittleLong (qfo->relocs[i].def);
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

int
qfo_to_progs (qfo_t *qfo, pr_info_t *pr)
{
	return 0;
}

qfo_t *
qfo_new (void)
{
	return calloc (1, sizeof (qfo_t));
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
	if (qfo->data) {
		free (qfo->data);
	} else {
		int         i;
		for (i = 0; i < qfo->num_spaces; i++)
			free (qfo->spaces->d.data);
		free (qfo->relocs);
		free (qfo->defs);
		free (qfo->funcs);
		free (qfo->lines);
	}
	free (qfo->spaces);
	free (qfo);
}
