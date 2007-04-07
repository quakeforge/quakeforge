/*
	obj_file.h

	object file support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/6/16

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

	$Id$
*/

#ifndef __obj_file_h
#define __obj_file_h

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"
#include "QF/quakeio.h"

#define QFO			"QFO"
#define QFO_VERSION 0x00001005		// MMmmmRRR 0.001.005 (hex)

typedef struct qfo_header_s {
	int8_t      qfo[4];
	pr_int_t    version;
	pr_int_t    code_size;
	pr_int_t    data_size;
	pr_int_t    far_data_size;
	pr_int_t    strings_size;
	pr_int_t    num_relocs;
	pr_int_t    num_defs;
	pr_int_t    num_funcs;
	pr_int_t    num_lines;
	pr_int_t    types_size;
	pr_int_t    entity_fields;
} qfo_header_t;

typedef struct qfo_def_s {
	etype_t     basic_type;
	string_t    full_type;
	string_t    name;
	pr_int_t    ofs;

	pr_int_t    relocs;
	pr_int_t    num_relocs;

	pr_uint_t   flags;

	string_t    file;
	pr_int_t    line;
} qfo_def_t;

#define QFOD_INITIALIZED	(1u<<0)
#define QFOD_CONSTANT		(1u<<1)
#define QFOD_ABSOLUTE		(1u<<2)
#define QFOD_GLOBAL			(1u<<3)
#define QFOD_EXTERNAL		(1u<<4)
#define QFOD_LOCAL			(1u<<5)
#define QFOD_SYSTEM			(1u<<6)
#define QFOD_NOSAVE			(1u<<7)

typedef struct qfo_func_s {
	string_t    name;
	string_t    file;
	pr_int_t    line;

	pr_int_t    builtin;
	pr_int_t    code;

	pr_int_t    def;

	pr_int_t    locals_size;
	pr_int_t    local_defs;
	pr_int_t    num_local_defs;

	pr_int_t    line_info;

	pr_int_t    num_parms;
	byte        parm_size[MAX_PARMS];

	pr_int_t    relocs;
	pr_int_t    num_relocs;
} qfo_func_t;

typedef struct qfo_reloc_s {
	pr_int_t    ofs;
	pr_int_t    type;
	pr_int_t    def;
} qfo_reloc_t;

typedef struct qfo_s {
	dstatement_t *code;
	int         code_size;
	pr_type_t  *data;
	int         data_size;
	pr_type_t  *far_data;
	int         far_data_size;
	char       *strings;
	int         strings_size;
	qfo_reloc_t *relocs;
	int         num_relocs;
	qfo_def_t  *defs;
	int         num_defs;
	qfo_func_t *funcs;
	int         num_funcs;
	pr_lineno_t *lines;
	int         num_lines;
	char       *types;
	int         types_size;
	int         entity_fields;
} qfo_t;

#define QFO_var(q, t, o)	((q)->data[o].t##_var)
#define	QFO_FLOAT(q, o)		QFO_var (q, float, o)
#define	QFO_INT(q, o)		QFO_var (q, integer, o)
#define	QFO_VECTOR(q, o)	QFO_var (q, vector, o)
#define	QFO_STRING(q, o)	G_GETSTR (QFO_var (q, string, o))
#define	QFO_FUNCTION(q, o)	QFO_var (q, func, o)
#define QFO_POINTER(q, t,o)	((t *)((q)->data + o))
#define QFO_STRUCT(q, t,o)	(*QFO_POINTER (q, t, o))

struct pr_info_s;

qfo_t *qfo_from_progs (struct pr_info_s *pr);
int qfo_write (qfo_t *qfo, const char *filename);
qfo_t *qfo_read (QFile *file);
qfo_t *qfo_open (const char *filename);
int qfo_to_progs (qfo_t *qfo, struct pr_info_s *pr);

qfo_t *qfo_new (void);
void qfo_add_code (qfo_t *qfo, dstatement_t *code, int code_size);
void qfo_add_data (qfo_t *qfo, pr_type_t *data, int data_size);
void qfo_add_far_data (qfo_t *qfo, pr_type_t *far_data, int far_data_size);
void qfo_add_strings (qfo_t *qfo, const char *strings, int strings_size);
void qfo_add_relocs (qfo_t *qfo, qfo_reloc_t *relocs, int num_relocs);
void qfo_add_defs (qfo_t *qfo, qfo_def_t *defs, int num_defs);
void qfo_add_funcs (qfo_t *qfo, qfo_func_t *funcs, int num_funcs);
void qfo_add_lines (qfo_t *qfo, pr_lineno_t *lines, int num_lines);
void qfo_add_types (qfo_t *qfo, const char *types, int types_size);
void qfo_delete (qfo_t *qfo);

#endif//__obj_file_h
