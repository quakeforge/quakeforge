/*
	obj_file.c

	qfcc object file support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

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
static const char rcsid[] =
	"$Id$";

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

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/vfile.h"

#include "debug.h"
#include "def.h"
#include "function.h"
#include "immediate.h"
#include "obj_file.h"
#include "reloc.h"
#include "qfcc.h"
#include "type.h"

static qfo_def_t *defs;
static int  num_defs;
static qfo_function_t *functions;
static int  num_functions;
static qfo_reloc_t *relocs;
static int  num_relocs;

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
allocate_stuff (void)
{
	def_t      *def;
	function_t *func;

	num_defs = pr.scope->num_defs;
	num_functions = pr.num_functions;
	num_relocs = 0;
	for (def = pr.scope->head; def; def = def->def_next) {
		num_relocs += count_relocs (def->refs);
	}
	for (func = pr.func_head; func; func = func->next) {
		num_relocs += count_relocs (func->refs);
		if (func->scope) {
			num_defs += func->scope->num_defs;
			for (def = func->scope->head; def; def = def->def_next) {
				num_relocs += count_relocs (def->refs);
			}
		}
	}
	defs = calloc (num_defs, sizeof (qfo_def_t));
	functions = calloc (num_functions, sizeof (qfo_function_t));
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
	return ReuseString (encoding->str);
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
	return flags;
}

static void
write_relocs (reloc_t *r, qfo_reloc_t **reloc)
{
	while (r) {
		(*reloc)->ofs  = LittleLong (r->ofs);
		(*reloc)->type = LittleLong (r->type);
		(*reloc)++;
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
	write_relocs (d->refs, reloc);
}

static void
setup_data (void)
{
	qfo_def_t  *def = defs;
	def_t      *d;
	qfo_function_t *func = functions;
	function_t *f;
	qfo_reloc_t *reloc = relocs;
	dstatement_t *st;
	pr_type_t  *var;
	pr_lineno_t *line;

	for (d = pr.scope->head; d; d = d->def_next)
		write_def (d, def++, &reloc);
	for (f = pr.func_head; f; f = f->next) {
		func->name           = LittleLong (f->s_name);
		func->file           = LittleLong (f->s_file);
		func->line           = LittleLong (f->def->line);
		func->builtin        = LittleLong (f->builtin);
		func->code           = LittleLong (f->code);
		if (f->def->obj_def)
			func->def        = LittleLong (f->def->obj_def);
		else {
			func->def        = LittleLong (def - defs);
			write_def (f->def, def++, &reloc);
		}
		if (f->scope) {
			func->locals_size    = LittleLong (f->scope->space->size);
			func->local_defs     = LittleLong (def - defs);
			func->num_local_defs = LittleLong (f->scope->num_defs);
		}
		if (f->aux)
			func->line_info  = LittleLong (f->aux->line_info);
		func->num_parms      = LittleLong (function_parms (f, func->parm_size));
		func->relocs         = LittleLong (reloc - relocs);
		func->num_relocs     = LittleLong (count_relocs (f->refs));
		write_relocs (f->refs, &reloc);

		if (f->scope)
			for (d = f->scope->head; d; d = d->def_next)
				write_def (d, def++, &reloc);
	}
	for (st = pr.statements; st - pr.statements < pr.num_statements; st++) {
		st->op = LittleLong (st->op);
		st->a  = LittleLong (st->a);
		st->b  = LittleLong (st->b);
		st->c  = LittleLong (st->c);
	}
	for (var = pr.near_data->data;
		 var - pr.near_data->data < pr.near_data->size; var++)
		var->integer_var = LittleLong (var->integer_var);
	if (pr.far_data)
		for (var = pr.far_data->data;
			 var - pr.far_data->data < pr.far_data->size; var++)
			var->integer_var = LittleLong (var->integer_var);
	for (line = linenos; line - linenos < num_linenos; line++) {
		line->fa.addr = LittleLong (line->fa.addr);
		line->line    = LittleLong (line->line);
	}
}

int
write_obj_file (const char *filename)
{
	qfo_header_t hdr;
	VFile      *file;

	allocate_stuff ();
	setup_data ();

	file = Qopen (filename, "wbz9");

	pr.strofs = (pr.strofs + 3) & ~3;

	memset (&hdr, 0, sizeof (hdr));

	memcpy (hdr.qfo, QFO, sizeof (hdr.qfo));
	hdr.version       = LittleLong (QFO_VERSION);
	hdr.code_size     = LittleLong (pr.num_statements);
	hdr.data_size     = LittleLong (pr.near_data->size);
	if (pr.far_data) {
		hdr.far_data_size = LittleLong (pr.far_data->size);
	}
	hdr.strings_size  = LittleLong (pr.strofs);
	hdr.num_relocs    = LittleLong (num_relocs);
	hdr.num_defs      = LittleLong (num_defs);
	hdr.num_functions = LittleLong (num_functions);
	hdr.num_lines     = LittleLong (num_linenos);

	Qwrite (file, &hdr, sizeof (hdr));
	if (pr.num_statements)
		Qwrite (file, pr.statements, pr.num_statements * sizeof (dstatement_t));
	if (pr.near_data->size)
		Qwrite (file, pr.near_data->data,
				pr.near_data->size * sizeof (pr_type_t));
	if (pr.far_data && pr.far_data->size) {
		Qwrite (file, pr.far_data->data,
				pr.far_data->size * sizeof (pr_type_t));
	}
	if (pr.strofs)
		Qwrite (file, pr.strings, pr.strofs);
	if (num_relocs)
		Qwrite (file, relocs, num_relocs * sizeof (qfo_reloc_t));
	if (num_defs)
		Qwrite (file, defs, num_defs * sizeof (qfo_def_t));
	if (num_functions)
		Qwrite (file, functions, num_functions * sizeof (qfo_function_t));
	if (num_linenos)
		Qwrite (file, linenos, num_linenos * sizeof (pr_lineno_t));

	Qclose (file);

	free (defs);
	free (relocs);
	free (functions);
	return 0;
}

qfo_t *
read_obj_file (const char *filename)
{
	VFile      *file;
	qfo_header_t hdr;
	qfo_t      *qfo;
	qfo_def_t  *def;
	qfo_function_t *func;
	qfo_reloc_t *reloc;
	dstatement_t *st;
	pr_type_t  *var;
	pr_lineno_t *line;

	file = Qopen (filename, "rbz");
	if (!file) {
		perror (filename);
		return 0;
	}

	Qread (file, &hdr, sizeof (hdr));

	if (strcmp (hdr.qfo, QFO)) {
		fprintf (stderr, "not a qfo file\n");
		Qclose (file);
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
	qfo->num_functions = LittleLong (hdr.num_functions);
	qfo->num_lines     = LittleLong (hdr.num_lines);

	if (hdr.version != QFO_VERSION) {
		fprintf (stderr, "can't read version %x.%03x.%03x\n",
				 (hdr.version >> 24) & 0xff, (hdr.version >> 12) & 0xfff,
				 hdr.version & 0xfff);
		Qclose (file);
		free (qfo);
		return 0;
	}

	qfo->code = malloc (qfo->code_size * sizeof (dstatement_t));
	qfo->data = malloc (qfo->data_size * sizeof (pr_type_t));
	if (qfo->far_data_size)
		qfo->far_data = malloc (qfo->far_data_size * sizeof (pr_type_t));
	qfo->strings = malloc (qfo->strings_size);
	qfo->relocs = malloc (qfo->num_relocs * sizeof (qfo_reloc_t));
	qfo->defs = malloc (qfo->num_defs * sizeof (qfo_def_t));
	qfo->functions = malloc (qfo->num_functions * sizeof (qfo_function_t));
	qfo->lines = malloc (qfo->num_lines * sizeof (pr_lineno_t));

	Qread (file, qfo->code, qfo->code_size * sizeof (dstatement_t));
	Qread (file, qfo->data, qfo->data_size * sizeof (pr_type_t));
	if (qfo->far_data_size)
		Qread (file, qfo->far_data, qfo->far_data_size * sizeof (pr_type_t));
	Qread (file, qfo->strings, qfo->strings_size);
	Qread (file, qfo->relocs, qfo->num_relocs * sizeof (qfo_reloc_t));
	Qread (file, qfo->defs, qfo->num_defs * sizeof (qfo_def_t));
	Qread (file, qfo->functions, qfo->num_functions * sizeof (qfo_function_t));
	if (qfo->num_lines)
		Qread (file, qfo->lines, qfo->num_lines * sizeof (pr_lineno_t));

	Qclose (file);

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
	for (func = qfo->functions;
		 func - qfo->functions < qfo->num_functions; func++) {
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
