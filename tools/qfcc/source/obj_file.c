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
static qfo_ref_t *refs;
static int  num_refs;

static int
count_refs (reloc_t *ref)
{
	int         num = 0;
	while (ref) {
		num++;
		ref = ref->next;
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
	num_refs = 0;
	for (def = pr.scope->head; def; def = def->def_next) {
		num_refs += count_refs (def->refs);
	}
	for (func = pr.func_head; func; func = func->next) {
		num_refs += count_refs (func->refs);
		num_defs += func->scope->num_defs;
		for (def = func->scope->head; def; def = def->def_next) {
			num_refs += count_refs (def->refs);
		}
	}
	defs = calloc (num_defs, sizeof (qfo_def_t));
	functions = calloc (num_functions, sizeof (qfo_function_t));
	refs = calloc (num_refs, sizeof (qfo_ref_t));
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
	if (d->global)
		flags |= QFOD_GLOBAL;
	if (d->absolute)
		flags |= QFOD_ABSOLUTE;
	return flags;
}

static void
write_refs (reloc_t *r, qfo_ref_t **ref)
{
	while (r) {
		(*ref)->ofs  = LittleLong (r->ofs);
		(*ref)->type = LittleLong (r->type);
		(*ref)++;
		r = r->next;
	}
}

static void
write_def (def_t *d, qfo_def_t *def, qfo_ref_t **ref)
{
	d->obj_def = def - defs;
	def->basic_type = LittleLong (d->type->type);
	def->full_type  = LittleLong (type_encoding (d->type));
	def->name       = LittleLong (ReuseString (d->name));
	def->ofs        = LittleLong (d->ofs);
	def->refs       = LittleLong (*ref - refs);
	def->num_refs   = LittleLong (count_refs (d->refs));
	def->flags      = LittleLong (flags (d));
	def->file       = LittleLong (d->file);
	def->line       = LittleLong (d->line);
	write_refs (d->refs, ref);
}

static void
setup_data (void)
{
	qfo_def_t  *def = defs;
	def_t      *d;
	qfo_function_t *func = functions;
	function_t   *f;
	qfo_ref_t    *ref = refs;

	for (d = pr.scope->head; d; d = d->def_next)
		write_def (d, def++, &ref);
	for (f = pr.func_head; f; f = f->next) {
		func->name           = LittleLong (f->dfunc->s_name);
		func->file           = LittleLong (f->dfunc->s_file);
		func->line           = LittleLong (f->def->line);
		func->builtin        = LittleLong (f->builtin);
		func->code           = LittleLong (f->code);
		if (f->def->obj_def)
			func->def        = LittleLong (f->def->obj_def);
		else {
			func->def        = LittleLong (def - defs);
			write_def (f->def, def++, &ref);
		}
		func->locals_size    = LittleLong (f->scope->space->size);
		func->local_defs     = LittleLong (def - defs);
		func->num_local_defs = LittleLong (f->scope->num_defs);
		//func->line_info
		//func->num_lines
		func->num_parms       = LittleLong (f->dfunc->numparms);
		memcpy (func->parm_size, f->dfunc->parm_size, MAX_PARMS);
		func->refs           = LittleLong (ref - refs);
		write_refs (f->refs, &ref);

		for (d = f->scope->head; d; d = d->def_next)
			write_def (d, def++, &ref);
	}
}

int
write_obj_file (const char *filename)
{
	qfo_header_t hdr;
	VFile      *file;
	int         ofs;

	allocate_stuff ();
	setup_data ();

	file = Qopen (filename, "wbz9");

	pr.strofs = (pr.strofs + 3) & ~3;

	memset (&hdr, 0, sizeof (hdr));

	ofs = Qwrite (file, &hdr, sizeof (hdr));

	memcpy (hdr.qfo, QFO, sizeof (hdr.qfo));
	hdr.version       = LittleLong (QFO_VERSION);
	hdr.code_ofs      = LittleLong (ofs);
	hdr.code_size     = LittleLong (pr.num_statements);
	ofs += Qwrite (file, pr.statements,
				   pr.num_statements * sizeof (dstatement_t));
	hdr.data_ofs      = LittleLong (ofs);
	hdr.data_size     = LittleLong (pr.near_data->size);
	ofs += Qwrite (file, pr.near_data->data,
				   pr.near_data->size * sizeof (pr_type_t));
	if (pr.far_data) {
		hdr.far_data_ofs  = LittleLong (ofs);
		hdr.far_data_size = LittleLong (pr.far_data->size);
		ofs += Qwrite (file, pr.far_data->data,
					   pr.far_data->size * sizeof (pr_type_t));
	}
	hdr.strings_ofs   = LittleLong (ofs);
	hdr.strings_size  = LittleLong (pr.strofs);
	ofs += Qwrite (file, pr.strings, pr.strofs);
	hdr.relocs_ofs    = LittleLong (ofs);
	hdr.num_relocs    = LittleLong (num_refs);
	ofs += Qwrite (file, refs, num_refs * sizeof (qfo_ref_t));
	hdr.defs_ofs      = LittleLong (ofs);
	hdr.num_defs      = LittleLong (num_defs);
	ofs += Qwrite (file, defs, num_defs * sizeof (qfo_def_t));
	hdr.functions_ofs = LittleLong (ofs);
	hdr.num_functions = LittleLong (num_functions);
	Qwrite (file, defs, num_functions * sizeof (qfo_function_t));

	Qseek (file, 0, SEEK_SET);
	Qwrite (file, &hdr, sizeof (hdr));
	Qseek (file, 0, SEEK_END);
	Qclose (file);
	return 0;
}
