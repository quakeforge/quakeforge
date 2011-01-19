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

static qfo_def_t *defs;
static int  num_defs;
static qfo_func_t *funcs;
static int  num_funcs;
static qfo_reloc_t *relocs;
static int  num_relocs;
static strpool_t *types;

static int
count_relocs (reloc_t *reloc)
{
	int         num = 0;
	while (reloc) {
		num++;
		reloc = reloc->next;
	}
	return num;
}

static void
allocate_stuff (pr_info_t *pr)
{
	def_t      *def;
	function_t *func;

	num_defs = 0;
	num_funcs = pr->num_functions - 1;
	num_relocs = 0;
	for (def = pr->scope->head; def; def = def->def_next) {
		if (def->alias)
			continue;
		num_defs++;
		num_relocs += count_relocs (def->refs);
	}
	for (func = pr->func_head; func; func = func->next) {
		num_relocs += count_relocs (func->refs);
		//FIXME if (func->scope) {
		//FIXME 	num_defs += func->scope->num_defs;
		//FIXME 	for (def = func->scope->head; def; def = def->def_next) {
		//FIXME 		num_relocs += count_relocs (def->refs);
		//FIXME 	}
		//FIXME }
	}
	num_relocs += count_relocs (pr->relocs);
	if (num_defs)
		defs = calloc (num_defs, sizeof (qfo_def_t));
	if (num_funcs)
		funcs = calloc (num_funcs, sizeof (qfo_func_t));
	if (num_relocs)
		relocs = calloc (num_relocs, sizeof (qfo_reloc_t));
}

static string_t
type_encoding (type_t *type)
{
	static dstring_t *encoding;

	if (!encoding)
		encoding = dstring_newstr ();
	else
		dstring_clearstr (encoding);
	encode_type (encoding, type);
	return strpool_addstr (types, encoding->str);
}

static int
flags (def_t *d)
{
	int         flags = 0;
	if (d->initialized)
		flags |= QFOD_INITIALIZED;
	if (d->constant)
		flags |= QFOD_CONSTANT;
	if (d->absolute)
		flags |= QFOD_ABSOLUTE;
	if (d->global)
		flags |= QFOD_GLOBAL;
	if (d->external)
		flags |= QFOD_EXTERNAL;
	if (d->local)
		flags |= QFOD_LOCAL;
	if (d->system)
		flags |= QFOD_SYSTEM;
	if (d->nosave)
		flags |= QFOD_NOSAVE;
	return flags;
}

static void
write_one_reloc (reloc_t *r, qfo_reloc_t **reloc, int def)
{
	(*reloc)->ofs  = LittleLong (r->ofs);
	(*reloc)->type = LittleLong (r->type);
	(*reloc)->def  = LittleLong (def);
	(*reloc)++;
}

static void
write_relocs (reloc_t *r, qfo_reloc_t **reloc, int def)
{
	while (r) {
		write_one_reloc (r, reloc, def);
		r = r->next;
	}
}

static void
write_def (def_t *d, qfo_def_t *def, qfo_reloc_t **reloc)
{
	d->obj_def = def - defs;
	def->basic_type = LittleLong (d->type->type);
	def->full_type  = LittleLong (type_encoding (d->type));
	def->name       = LittleLong (ReuseString (d->name));
	def->ofs        = LittleLong (d->ofs);
	def->relocs     = LittleLong (*reloc - relocs);
	def->num_relocs = LittleLong (count_relocs (d->refs));
	def->flags      = LittleLong (flags (d));
	def->file       = LittleLong (d->file);
	def->line       = LittleLong (d->line);
	write_relocs (d->refs, reloc, d->obj_def);
}

static void
setup_data (pr_info_t *pr)
{
	qfo_def_t  *def = defs;
	def_t      *d;
	qfo_func_t *func = funcs;
	function_t *f;
	qfo_reloc_t *reloc = relocs;
	reloc_t    *r;
	dstatement_t *st;
	pr_type_t  *var;
	pr_lineno_t *line;

	for (d = pr->scope->head; d; d = d->def_next)
		if (!d->alias)
			write_def (d, def++, &reloc);
	for (f = pr->func_head; f; f = f->next, func++) {
		func->name           = LittleLong (f->s_name);
		func->file           = LittleLong (f->s_file);
		//FIXME func->line           = LittleLong (f->def->line);
		func->builtin        = LittleLong (f->builtin);
		func->code           = LittleLong (f->code);
		//FIXME if (f->def->obj_def)
		//FIXME	func->def        = LittleLong (f->def->obj_def);
		//FIXMEelse {
		//FIXME	func->def        = LittleLong (def - defs);
		//FIXME	write_def (f->def, def++, &reloc);
		//FIXME}
		//FIXME if (f->scope) {
		//FIXME 	func->locals_size    = LittleLong (f->scope->space->size);
		//FIXME 	func->local_defs     = LittleLong (def - defs);
		//FIXME 	func->num_local_defs = LittleLong (f->scope->num_defs);
		//FIXME }
		if (f->aux)
			func->line_info  = LittleLong (f->aux->line_info);
		func->num_parms      = LittleLong (function_parms (f, func->parm_size));
		func->relocs         = LittleLong (reloc - relocs);
		func->num_relocs     = LittleLong (count_relocs (f->refs));
		write_relocs (f->refs, &reloc, func - funcs);

		//FIXME if (f->scope)
		//FIXME 	for (d = f->scope->head; d; d = d->def_next)
		//FIXME 		write_def (d, def++, &reloc);
	}
	for (r = pr->relocs; r; r = r->next)
		//FIXME if (r->type == rel_def_op)
		//FIXME 	write_one_reloc (r, &reloc, r->label->ofs);
		//FIXME else
			write_one_reloc (r, &reloc, 0);
	for (st = pr->code->code; st - pr->code->code < pr->code->size; st++) {
		st->op = LittleLong (st->op);
		st->a  = LittleLong (st->a);
		st->b  = LittleLong (st->b);
		st->c  = LittleLong (st->c);
	}
	for (var = pr->near_data->data;
		 var - pr->near_data->data < pr->near_data->size; var++)
		var->integer_var = LittleLong (var->integer_var);
	if (pr->far_data)
		for (var = pr->far_data->data;
			 var - pr->far_data->data < pr->far_data->size; var++)
			var->integer_var = LittleLong (var->integer_var);
	for (line = pr->linenos; line - pr->linenos < pr->num_linenos; line++) {
		line->fa.addr = LittleLong (line->fa.addr);
		line->line    = LittleLong (line->line);
	}
}

static void
round_strings (strpool_t *strings)
{
	memset (strings->strings + strings->size, 0,
			strings->max_size - strings->size);
	strings->size = RUP (strings->size, 4);
}

qfo_t *
qfo_from_progs (pr_info_t *pr)
{
	qfo_t      *qfo;

	types = strpool_new ();
	allocate_stuff (pr);
	setup_data (pr);

	round_strings (pr->strings);
	round_strings (types);

	qfo = qfo_new ();
	qfo_add_code (qfo, pr->code->code, pr->code->size);
	qfo_add_data (qfo, pr->near_data->data, pr->near_data->size);
	if (pr->far_data)
		qfo_add_far_data (qfo, pr->far_data->data, pr->far_data->size);
	qfo_add_strings (qfo, pr->strings->strings, pr->strings->size);
	qfo_add_relocs (qfo, relocs, num_relocs);
	qfo_add_defs (qfo, defs, num_defs);
	qfo_add_funcs (qfo, funcs, num_funcs);
	qfo_add_lines (qfo, pr->linenos, pr->num_linenos);
	qfo_add_types (qfo, types->strings, types->size);
	qfo->entity_fields = pr->entity_data->size;

	free (defs);
	free (relocs);
	free (funcs);
	strpool_delete (types);
	return qfo;
}

int
qfo_write (qfo_t *qfo, const char *filename)
{
	qfo_header_t hdr;
	QFile      *file;

	file = Qopen (filename, options.gzip ? "wbz9" : "wb");
	if (!file)
		return -1;

	memset (&hdr, 0, sizeof (hdr));

	memcpy (hdr.qfo, QFO, sizeof (hdr.qfo));
	hdr.version       = LittleLong (QFO_VERSION);
	hdr.code_size     = LittleLong (qfo->code_size);
	hdr.data_size     = LittleLong (qfo->data_size);
	hdr.far_data_size = LittleLong (qfo->far_data_size);
	hdr.strings_size  = LittleLong (qfo->strings_size);
	hdr.num_relocs    = LittleLong (qfo->num_relocs);
	hdr.num_defs      = LittleLong (qfo->num_defs);
	hdr.num_funcs     = LittleLong (qfo->num_funcs);
	hdr.num_lines     = LittleLong (qfo->num_lines);
	hdr.types_size    = LittleLong (qfo->types_size);
	hdr.entity_fields = LittleLong (qfo->entity_fields);

	Qwrite (file, &hdr, sizeof (hdr));
	if (qfo->code_size)
		Qwrite (file, qfo->code, qfo->code_size * sizeof (dstatement_t));
	if (qfo->data_size)
		Qwrite (file, qfo->data, qfo->data_size * sizeof (pr_type_t));
	if (qfo->far_data_size)
		Qwrite (file, qfo->far_data, qfo->far_data_size * sizeof (pr_type_t));
	if (qfo->strings_size)
		Qwrite (file, qfo->strings, qfo->strings_size);
	if (qfo->num_relocs)
		Qwrite (file, qfo->relocs, qfo->num_relocs * sizeof (qfo_reloc_t));
	if (qfo->num_defs)
		Qwrite (file, qfo->defs, qfo->num_defs * sizeof (qfo_def_t));
	if (qfo->num_funcs)
		Qwrite (file, qfo->funcs, qfo->num_funcs * sizeof (qfo_func_t));
	if (qfo->num_lines)
		Qwrite (file, qfo->lines, qfo->num_lines * sizeof (pr_lineno_t));
	if (qfo->types_size)
		Qwrite (file, qfo->types, qfo->types_size);

	Qclose (file);

	return 0;
}

qfo_t *
qfo_read (QFile *file)
{
	qfo_header_t hdr;
	qfo_t      *qfo;
	qfo_def_t  *def;
	qfo_func_t *func;
	qfo_reloc_t *reloc;
	dstatement_t *st;
	pr_type_t  *var;
	pr_lineno_t *line;

	Qread (file, &hdr, sizeof (hdr));

	if (memcmp (hdr.qfo, QFO, 4)) {
		fprintf (stderr, "not a qfo file\n");
		return 0;
	}

	qfo = calloc (1, sizeof (qfo_t));

	hdr.version        = LittleLong (hdr.version);
	qfo->code_size     = LittleLong (hdr.code_size);
	qfo->data_size     = LittleLong (hdr.data_size);
	qfo->far_data_size = LittleLong (hdr.far_data_size);
	qfo->strings_size  = LittleLong (hdr.strings_size);
	qfo->num_relocs    = LittleLong (hdr.num_relocs);
	qfo->num_defs      = LittleLong (hdr.num_defs);
	qfo->num_funcs     = LittleLong (hdr.num_funcs);
	qfo->num_lines     = LittleLong (hdr.num_lines);
	qfo->types_size    = LittleLong (hdr.types_size);
	qfo->entity_fields = LittleLong (hdr.entity_fields);

	if (hdr.version != QFO_VERSION) {
		fprintf (stderr, "can't read version %x.%03x.%03x\n",
				 (hdr.version >> 24) & 0xff, (hdr.version >> 12) & 0xfff,
				 hdr.version & 0xfff);
		free (qfo);
		return 0;
	}

	if (qfo->code_size)
		qfo->code = malloc (qfo->code_size * sizeof (dstatement_t));
	if (qfo->data_size)
		qfo->data = malloc (qfo->data_size * sizeof (pr_type_t));
	if (qfo->far_data_size)
		qfo->far_data = malloc (qfo->far_data_size * sizeof (pr_type_t));
	if (qfo->strings_size)
		qfo->strings = malloc (qfo->strings_size);
	if (qfo->num_relocs)
		qfo->relocs = malloc (qfo->num_relocs * sizeof (qfo_reloc_t));
	if (qfo->num_defs)
		qfo->defs = malloc (qfo->num_defs * sizeof (qfo_def_t));
	if (qfo->num_funcs)
		qfo->funcs = malloc (qfo->num_funcs * sizeof (qfo_func_t));
	if (qfo->num_lines)
		qfo->lines = malloc (qfo->num_lines * sizeof (pr_lineno_t));
	if (qfo->types_size)
		qfo->types = malloc (qfo->types_size);

	if (qfo->code_size)
		Qread (file, qfo->code, qfo->code_size * sizeof (dstatement_t));
	if (qfo->data_size)
		Qread (file, qfo->data, qfo->data_size * sizeof (pr_type_t));
	if (qfo->far_data_size)
		Qread (file, qfo->far_data, qfo->far_data_size * sizeof (pr_type_t));
	if (qfo->strings_size)
		Qread (file, qfo->strings, qfo->strings_size);
	if (qfo->num_relocs)
		Qread (file, qfo->relocs, qfo->num_relocs * sizeof (qfo_reloc_t));
	if (qfo->num_defs)
		Qread (file, qfo->defs, qfo->num_defs * sizeof (qfo_def_t));
	if (qfo->num_funcs)
		Qread (file, qfo->funcs, qfo->num_funcs * sizeof (qfo_func_t));
	if (qfo->num_lines)
		Qread (file, qfo->lines, qfo->num_lines * sizeof (pr_lineno_t));
	if (qfo->types_size)
		Qread (file, qfo->types, qfo->types_size);

	for (st = qfo->code; st - qfo->code < qfo->code_size; st++) {
		st->op = LittleLong (st->op);
		st->a  = LittleLong (st->a);
		st->b  = LittleLong (st->b);
		st->c  = LittleLong (st->c);
	}
	for (var = qfo->data;
		 var - qfo->data < qfo->data_size; var++)
		var->integer_var = LittleLong (var->integer_var);
	if (qfo->far_data)
		for (var = qfo->far_data;
			 var - qfo->far_data < qfo->far_data_size; var++)
			var->integer_var = LittleLong (var->integer_var);
	for (reloc = qfo->relocs; reloc - qfo->relocs < qfo->num_relocs; reloc++) {
		reloc->ofs  = LittleLong (reloc->ofs);
		reloc->type = LittleLong (reloc->type);
		reloc->def  = LittleLong (reloc->def);
	}
	for (def = qfo->defs; def - qfo->defs < qfo->num_defs; def++) {
		def->basic_type = LittleLong (def->basic_type);
		def->full_type  = LittleLong (def->full_type);
		def->name       = LittleLong (def->name);
		def->ofs        = LittleLong (def->ofs);
		def->relocs     = LittleLong (def->relocs);
		def->num_relocs = LittleLong (def->num_relocs);
		def->flags      = LittleLong (def->flags);
		def->file       = LittleLong (def->file);
		def->line       = LittleLong (def->line);
	}
	for (func = qfo->funcs;
		 func - qfo->funcs < qfo->num_funcs; func++) {
		func->name           = LittleLong (func->name);
		func->file           = LittleLong (func->file);
		func->line           = LittleLong (func->line);
		func->builtin        = LittleLong (func->builtin);
		func->code           = LittleLong (func->code);
		func->def            = LittleLong (func->def);
		func->locals_size    = LittleLong (func->locals_size);
		func->local_defs     = LittleLong (func->local_defs);
		func->num_local_defs = LittleLong (func->num_local_defs);
		func->line_info      = LittleLong (func->line_info);
		func->num_parms      = LittleLong (func->num_parms);
		func->relocs         = LittleLong (func->relocs);
		func->num_relocs     = LittleLong (func->num_relocs);
	}
	for (line = qfo->lines; line - qfo->lines < qfo->num_lines; line++) {
		line->fa.addr = LittleLong (line->fa.addr);
		line->line    = LittleLong (line->line);
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

static defspace_t *
init_space (int size, pr_type_t *data)
{
	defspace_t *space = new_defspace ();
	space->size = size;
	space->max_size = RUP (space->size, 32);
	if (!space->max_size)
		space->max_size = 32;
	space->data = malloc (space->max_size * sizeof (pr_type_t));
	if (size && data) {
		memcpy (space->data, data, size * sizeof (pr_type_t));
	}
	return space;
}

int
qfo_to_progs (qfo_t *qfo, pr_info_t *pr)
{
	int         i;
	function_t *pf;
	qfo_func_t *qf;
	def_t      *pd;
	qfo_def_t  *qd;
	reloc_t    *relocs = 0;
	int         first_local;

	if (qfo->num_relocs)
		relocs = calloc (qfo->num_relocs, sizeof (reloc_t));
	for (i = 0; i < qfo->num_relocs; i++) {
		if (i + 1 < qfo->num_relocs)
			relocs[i].next = &relocs[i + 1];
		relocs[i].ofs = qfo->relocs[i].ofs;
		relocs[i].type = qfo->relocs[i].type;
	}

	pr->strings = strpool_build (qfo->strings, qfo->strings_size);

	pr->code = codespace_new ();
	codespace_addcode (pr->code, qfo->code, qfo->code_size);

	pr->near_data = init_space (qfo->data_size, qfo->data);
	pr->far_data = init_space (qfo->far_data_size, qfo->far_data);
	pr->entity_data = new_defspace ();
	pr->scope = new_scope (sc_global, pr->near_data, 0);
	pr->scope->num_defs = qfo->num_defs;
	if (qfo->num_defs)
		pr->scope->head = calloc (pr->scope->num_defs, sizeof (def_t));
	for (i = 0, pd = pr->scope->head, qd = qfo->defs;
		 i < pr->scope->num_defs; i++, pd++, qd++) {
		*pr->scope->tail = pd;
		pr->scope->tail = &pd->def_next;
		pd->type = parse_type (qfo->types + qd->full_type);
		pd->name = qd->name ? qfo->strings + qd->name : 0;
		pd->ofs = qd->ofs;
		if (qd->num_relocs) {
			pd->refs = relocs + qd->relocs;
			pd->refs[qd->num_relocs - 1].next = 0;
		}
		pd->initialized = (qd->flags & QFOD_INITIALIZED) != 0;
		pd->constant    = (qd->flags & QFOD_CONSTANT)    != 0;
		pd->absolute    = (qd->flags & QFOD_ABSOLUTE)    != 0;
		pd->global      = (qd->flags & QFOD_GLOBAL)      != 0;
		pd->external    = (qd->flags & QFOD_EXTERNAL)    != 0;
		pd->local       = (qd->flags & QFOD_LOCAL)       != 0;
		pd->system      = (qd->flags & QFOD_SYSTEM)      != 0;
		pd->nosave      = (qd->flags & QFOD_NOSAVE)      != 0;
		pd->file = qd->file;
		pd->line = qd->line;
	}

	pr->num_functions = qfo->num_funcs + 1;
	if (qfo->num_funcs)
		pr->func_head = calloc (qfo->num_funcs, sizeof (function_t));
	pr->func_tail = &pr->func_head;
	first_local = qfo->num_defs;
	for (i = 0, pf = pr->func_head, qf = qfo->funcs;
		 i < qfo->num_funcs; i++, pf++, qf++) {
		*pr->func_tail = pf;
		pr->func_tail = &pf->next;
		//FIXME pf->def = pr->scope->head + qf->def;
		pf->aux = new_auxfunction ();
		pf->aux->function = i + 1;
		pf->aux->source_line = qf->line;
		pf->aux->line_info = qf->line_info;
		pf->aux->local_defs = 0;
		pf->aux->num_locals = 0;
		//FIXME pf->aux->return_type = pf->def->type->t.func.type->type;
		pf->builtin = qf->builtin;
		pf->code = qf->code;
		pf->function_num = i + 1;
		pf->s_file = qf->file;
		pf->s_name = qf->name;
		//FIXME pf->scope = new_scope (sc_params, init_space (qf->locals_size, 0),
		//FIXME 					   pr->scope);
		if (qf->num_local_defs) {
			if (first_local > qf->local_defs)
				first_local = qf->local_defs;
			//FIXME pf->scope->head = pr->scope->head + qf->local_defs;
			//FIXME pf->scope->tail = &pf->scope->head[qf->num_local_defs - 1].def_next;
			//FIXME *pf->scope->tail = 0;
			pf->aux->local_defs = pr->num_locals;
			//FIXME for (pd = pf->scope->head; pd; pd = pd->def_next) {
			//FIXME 	if (pd->name) {
			//FIXME 		def_to_ddef (pd, new_local (), 0);
			//FIXME 		pf->aux->num_locals++;
			//FIXME 	}
			//FIXME }
		}
		if (qf->num_relocs) {
			pf->refs = relocs + qf->relocs;
			pf->refs[qf->num_relocs - 1].next = 0;
		}
	}
	if (first_local > 0) {
		pd = pr->scope->head + first_local - 1;
		pr->scope->tail = &pd->def_next;
		pd->def_next = 0;
	}
	pr->num_linenos = pr->linenos_size = qfo->num_lines;
	if (pr->num_linenos) {
		pr->linenos = malloc (pr->num_linenos * sizeof (pr_lineno_t));
		memcpy (pr->linenos, qfo->lines,
				pr->num_linenos * sizeof (pr_lineno_t));
	}
	pr->entity_data = init_space (qfo->entity_fields, 0);
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
	if (!code_size)
		return;
	qfo->code = malloc (code_size * sizeof (dstatement_t));
	qfo->code_size = code_size;
	memcpy (qfo->code, code, code_size * sizeof (dstatement_t));
}

void
qfo_add_data (qfo_t *qfo, pr_type_t *data, int data_size)
{
	if (!data_size)
		return;
	qfo->data = malloc (data_size * sizeof (pr_type_t));
	qfo->data_size = data_size;
	memcpy (qfo->data, data, data_size * sizeof (pr_type_t));
}

void
qfo_add_far_data (qfo_t *qfo, pr_type_t *far_data, int far_data_size)
{
	if (!far_data_size)
		return;
	qfo->far_data = malloc (far_data_size * sizeof (pr_type_t));
	qfo->far_data_size = far_data_size;
	memcpy (qfo->far_data, far_data, far_data_size * sizeof (pr_type_t));
}

void
qfo_add_strings (qfo_t *qfo, const char *strings, int strings_size)
{
	if (!strings_size)
		return;
	qfo->strings = malloc (strings_size);
	qfo->strings_size = strings_size;
	memcpy (qfo->strings, strings, strings_size);
}

void
qfo_add_relocs (qfo_t *qfo, qfo_reloc_t *relocs, int num_relocs)
{
	if (!num_relocs)
		return;
	qfo->relocs = malloc (num_relocs * sizeof (qfo_reloc_t));
	qfo->num_relocs = num_relocs;
	memcpy (qfo->relocs, relocs, num_relocs * sizeof (qfo_reloc_t));
}

void
qfo_add_defs (qfo_t *qfo, qfo_def_t *defs, int num_defs)
{
	if (!num_defs)
		return;
	qfo->defs = malloc (num_defs * sizeof (qfo_def_t));
	qfo->num_defs = num_defs;
	memcpy (qfo->defs, defs, num_defs * sizeof (qfo_def_t));
}

void
qfo_add_funcs (qfo_t *qfo, qfo_func_t *funcs, int num_funcs)
{
	if (!num_funcs)
		return;
	qfo->funcs = malloc (num_funcs * sizeof (qfo_func_t));
	qfo->num_funcs = num_funcs;
	memcpy (qfo->funcs, funcs, num_funcs * sizeof (qfo_func_t));
}

void
qfo_add_lines (qfo_t *qfo, pr_lineno_t *lines, int num_lines)
{
	if (!num_lines)
		return;
	qfo->lines = malloc (num_lines * sizeof (pr_lineno_t));
	qfo->num_lines = num_lines;
	memcpy (qfo->lines, lines, num_lines * sizeof (pr_lineno_t));
}

void
qfo_add_types (qfo_t *qfo, const char *types, int types_size)
{
	if (!types_size)
		return;
	qfo->types = malloc (types_size);
	qfo->types_size = types_size;
	memcpy (qfo->types, types, types_size);
}

void
qfo_delete (qfo_t *qfo)
{
	if (!qfo)
		return;
	if (qfo->code)
		free (qfo->code);
	if (qfo->data)
		free (qfo->data);
	if (qfo->far_data)
		free (qfo->far_data);
	if (qfo->strings)
		free (qfo->strings);
	if (qfo->relocs)
		free (qfo->relocs);
	if (qfo->defs)
		free (qfo->defs);
	if (qfo->funcs)
		free (qfo->funcs);
	if (qfo->lines)
		free (qfo->lines);
	if (qfo->types)
		free (qfo->types);
	free (qfo);
}
