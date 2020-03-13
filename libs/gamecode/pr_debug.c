/*
	pr_debug.c

	progs debugging

	Copyright (C) 2001       Bill Currie <bill@tanwiha.org>

	Author: Bill Currie <bill@tanwiha.org>
	Date: 2001/7/13

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
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/pr_debug.h"
#include "QF/pr_type.h"
#include "QF/progs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

typedef struct {
	char *text;
	size_t len;
} line_t;

typedef struct {
	char *name;
	char *text;
	off_t size;
	line_t *lines;
	pr_uint_t   num_lines;
	progs_t *pr;
} file_t;

typedef struct prdeb_resources_s {
	progs_t    *pr;
	dstring_t  *string;
	dstring_t  *dva;
	dstring_t  *line;
	dstring_t  *dstr;
	const char *debugfile;
	struct pr_debug_header_s *debug;
	struct pr_auxfunction_s *auxfunctions;
	struct pr_auxfunction_s **auxfunction_map;
	struct pr_lineno_s *linenos;
	pr_def_t   *local_defs;
	pr_def_t   *type_encodings_def;
} prdeb_resources_t;

typedef struct {
	progs_t    *pr;
	dstring_t  *dstr;
} pr_debug_data_t;

cvar_t         *pr_debug;
cvar_t         *pr_source_path;
static hashtab_t   *file_hash;
static char        *source_path_string;
static char       **source_paths;

static void pr_debug_void_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_string_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_float_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_vector_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_entity_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_field_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_func_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_pointer_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_quat_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_integer_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_uinteger_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_short_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_double_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_struct_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_union_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_enum_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_array_view (qfot_type_t *type, pr_type_t *value,
								void *_data);
static void pr_debug_class_view (qfot_type_t *type, pr_type_t *value,
								void *_data);

static type_view_t raw_type_view = {
	pr_debug_void_view,
	pr_debug_string_view,
	pr_debug_float_view,
	pr_debug_vector_view,
	pr_debug_entity_view,
	pr_debug_field_view,
	pr_debug_func_view,
	pr_debug_pointer_view,
	pr_debug_quat_view,
	pr_debug_integer_view,
	pr_debug_uinteger_view,
	pr_debug_short_view,
	pr_debug_double_view,
	pr_debug_struct_view,
	pr_debug_union_view,
	pr_debug_enum_view,
	pr_debug_array_view,
	pr_debug_class_view,
};

static const char *
file_get_key (const void *_f, void *unused)
{
	return ((file_t*)_f)->name;
}

static void
file_free (void *_f, void *unused)
{
	file_t     *f = (file_t*)_f;
	progs_t    *pr = f->pr;

	free (f->lines);
	((progs_t *) pr)->free_progs_mem (pr, f->text);
	free (f->name);
	free (f);
}

static void
source_path_f (cvar_t *var)
{
	int         i;
	char       *s;

	if (source_path_string) {
		free (source_path_string);
	}
	source_path_string = strdup (var->string);
	if (source_paths) {
		free (source_paths);
	}
	// i starts at 2 because an empty path is equivalent to "." and the
	// list is null terminated
	for (i = 2, s = source_path_string; *s; s++) {
		if (*s == ';') {
			i++;
		}
	}
	source_paths = malloc (i * sizeof (char *));
	source_paths[0] = source_path_string;
	// i starts at one because the first path is in 0 and any additional
	// paths come after, then the null terminator
	for (i = 1, s = source_path_string; *s; s++) {
		if (*s == ';') {
			*s = 0;
			source_paths[i++] = s + 1;
		}
	}
	source_paths[i] = 0;
}

#define RUP(x,a) (((x) + ((a) - 1)) & ~((a) - 1))
static pr_short_t __attribute__((pure))
pr_debug_type_size (const progs_t *pr, const qfot_type_t *type)
{
	pr_short_t  size;
	qfot_type_t *aux_type;
	switch (type->meta) {
		case ty_basic:
			return pr_type_size[type->t.type];
		case ty_struct:
		case ty_union:
			size = 0;
			for (pr_int_t i = 0; i < type->t.strct.num_fields; i++) {
				const qfot_var_t *field = &type->t.strct.fields[i];
				aux_type = &G_STRUCT (pr, qfot_type_t, field->type);
				size = max (size,
							field->offset + pr_debug_type_size (pr, aux_type));
			}
			return size;
		case ty_enum:
			return pr_type_size[ev_integer];
		case ty_array:
			aux_type = &G_STRUCT (pr, qfot_type_t, type->t.array.type);
			size = pr_debug_type_size (pr, aux_type);
			return type->t.array.size * size;
		case ty_class:
			return 1;	//FIXME or should it return sizeof class struct?
	}
	return 0;
}

static qfot_type_t *
get_def_type (progs_t *pr, pr_def_t *def, qfot_type_t *type)
{
	if (!def->type_encoding) {
		// no type encoding, so use basic type data to fill in and return
		// the dummy encoding
		memset (type, 0, sizeof (*type));
		type->t.type = def->type;
	} else {
		type = &G_STRUCT (pr, qfot_type_t, def->type_encoding);
		if (!def->size) {
			def->size = pr_debug_type_size (pr, type);
		}
	}
	return type;
}

static pr_def_t
parse_expression (progs_t *pr, const char *expr, int conditional)
{
	script_t   *es;
	char       *e;
	pr_type_t  *expr_ptr;
	pr_def_t    d;

	d.ofs = 0;
	d.type = ev_invalid;
	d.name = 0;
	es = Script_New ();
	Script_Start (es, "<console>", expr);
	expr_ptr = 0;
	es->single = "{}()':[].";
	if (Script_GetToken (es, 1)) {
		if (strequal (es->token->str, "[")) {
			edict_t    *ent;
			pr_def_t   *field;

			if (!Script_GetToken (es, 1))
				goto error;
			ent = EDICT_NUM (pr, strtol (es->token->str, &e, 0));
			if (e == es->token->str)
				goto error;
			if (!Script_GetToken (es, 1) && !strequal (es->token->str, "]" ))
				goto error;
			if (!Script_GetToken (es, 1) && !strequal (es->token->str, "." ))
				goto error;
			if (!Script_GetToken (es, 1))
				goto error;
			field = PR_FindField (pr, es->token->str);
			if (!field)
				goto error;
			d = *field;
			expr_ptr = &ent->v[field->ofs];
			d.ofs = PR_SetPointer (pr, expr_ptr);
		} else if (isdigit ((byte) es->token->str[0])) {
			expr_ptr = PR_GetPointer (pr, strtol (es->token->str, 0, 0));
			d.type = ev_void;
			d.ofs = PR_SetPointer (pr, expr_ptr);
		} else {
			pr_def_t   *global = PR_FindGlobal (pr, es->token->str);
			if (!global)
				goto error;
			d = *global;
		}
		if (conditional) {
			es->single = "{}()':[]";
			pr->wp_conditional = 0;
			if (Script_TokenAvailable (es, 1)) {
				if (!Script_GetToken (es, 1)
					&& !strequal (es->token->str, "==" ))
					goto error;
				if (!Script_GetToken (es, 1))
					goto error;
				pr->wp_val.integer_var = strtol (es->token->str, &e, 0);
				if (e == es->token->str)
					goto error;
				if (*e == '.' || *e == 'e' || *e == 'E')
					pr->wp_val.float_var = strtod (es->token->str, &e);
				pr->wp_conditional = 1;
			}
		}
		if (Script_TokenAvailable (es, 1))
			Sys_Printf ("ignoring tail\n");
	}
error:
	if (es->error) {
		Sys_Printf (es->error);
	}
	Script_Delete (es);
	return d;
}

static void
pr_debug_clear (progs_t *pr, void *data)
{
	__auto_type res = (prdeb_resources_t *) data;

	dstring_clearstr (res->string);
	dstring_clearstr (res->dva);
	dstring_clearstr (res->line);
	dstring_clearstr (res->dstr);

	if (res->debug)
		pr->free_progs_mem (pr, res->debug);
	res->debug = 0;
	res->auxfunctions = 0;
	if (res->auxfunction_map)
		pr->free_progs_mem (pr, res->auxfunction_map);
	res->auxfunction_map = 0;
	res->linenos = 0;
	res->local_defs = 0;

	pr->pr_debug_resources = res;
	pr->watch = 0;
	pr->wp_conditional = 0;
	pr->wp_val.integer_var = 0;
}

static file_t *
PR_Load_Source_File (progs_t *pr, const char *fname)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	char       *l, *p, **dir;
	file_t     *f = Hash_Find (file_hash, fname);

	if (f)
		return f;
	f = calloc (1, sizeof (file_t));
	if (!f)
		return 0;
	for (dir = source_paths; *dir && !f->text; dir++) {
		f->text = pr->load_file (pr, dsprintf (res->dva, "%s%s%s", *dir,
											   **dir ? "/" : "", fname),
								 &f->size);
	}
	if (!f->text) {
		pr->file_error (pr, fname);
	} else {
		for (f->num_lines = 1, l = f->text; *l; l++)
			if (*l == '\n')
				f->num_lines++;
	}
	f->name = strdup (fname);
	if (!f->name) {
		pr->free_progs_mem (pr, f->text);
		free (f);
		return 0;
	}
	if (f->num_lines) {
		f->lines = malloc (f->num_lines * sizeof (line_t));
		if (!f->lines) {
			free (f->name);
			pr->free_progs_mem (pr, f->text);
			free (f);
			return 0;
		}
		f->lines[0].text = f->text;
		for (f->num_lines = 0, l = f->text; *l; l++) {
			if (*l == '\n') {
				for (p = l; p > f->lines[f->num_lines].text
							&& isspace((byte) p[-1]); p--)
					;
				f->lines[f->num_lines].len = p - f->lines[f->num_lines].text;
				f->lines[++f->num_lines].text = l + 1;
			}
		}
		f->lines[f->num_lines].len = l - f->lines[f->num_lines].text;
		f->num_lines++;
	}
	f->pr = pr;
	Hash_Add (file_hash, f);
	return f;
}

VISIBLE int
PR_LoadDebug (progs_t *pr)
{
	prdeb_resources_t *res = PR_Resources_Find (pr, "PR_Debug");
	char       *sym_path;
	const char *path_end, *sym_file;
	off_t       debug_size;
	pr_uint_t   i;
	pr_def_t   *def;
	pr_type_t  *str = 0;
	pointer_t   type_encodings = 0;

	if (!pr_debug->int_val)
		return 1;

	def = PR_FindGlobal (pr, ".debug_file");
	if (def)
		str = &pr->pr_globals[def->ofs];

	Hash_FlushTable (file_hash);
	if (!str)
		return 1;
	res->debugfile = PR_GetString (pr, str->string_var);
	sym_file = QFS_SkipPath (res->debugfile);
	path_end = QFS_SkipPath (pr->progs_name);
	sym_path = malloc (strlen (sym_file) + (path_end - pr->progs_name) + 1);
	strncpy (sym_path, pr->progs_name, path_end - pr->progs_name);
	strcpy (sym_path + (path_end - pr->progs_name), sym_file);
	res->debug = pr->load_file (pr, sym_path, &debug_size);
	if (!res->debug) {
		Sys_Printf ("can't load %s for debug info\n", sym_path);
		free (sym_path);
		return 1;
	}
	res->debug->version = LittleLong (res->debug->version);
	if (res->debug->version != PROG_DEBUG_VERSION) {
		Sys_Printf ("ignoring %s with unsupported version %x.%03x.%03x\n",
					sym_path,
					(res->debug->version >> 24) & 0xff,
					(res->debug->version >> 12) & 0xfff,
					res->debug->version & 0xfff);
		res->debug = 0;
		free (sym_path);
		return 1;
	}
	res->debug->crc = LittleShort (res->debug->crc);
	if (res->debug->crc != pr->crc) {
		Sys_Printf ("ignoring %s that doesn't match %s. (CRCs: "
					"sym:%d dat:%d)\n",
					sym_path,
					pr->progs_name,
					res->debug->crc,
					pr->crc);
		res->debug = 0;
		free (sym_path);
		return 1;
	}
	free (sym_path);
	res->debug->you_tell_me_and_we_will_both_know = LittleShort
		(res->debug->you_tell_me_and_we_will_both_know);
	res->debug->auxfunctions = LittleLong (res->debug->auxfunctions);
	res->debug->num_auxfunctions = LittleLong (res->debug->num_auxfunctions);
	res->debug->linenos = LittleLong (res->debug->linenos);
	res->debug->num_linenos = LittleLong (res->debug->num_linenos);
	res->debug->locals = LittleLong (res->debug->locals);
	res->debug->num_locals = LittleLong (res->debug->num_locals);

	res->auxfunctions = (pr_auxfunction_t*)((char*)res->debug +
										   res->debug->auxfunctions);
	res->linenos = (pr_lineno_t*)((char*)res->debug + res->debug->linenos);
	res->local_defs = (pr_def_t*)((char*)res->debug + res->debug->locals);

	i = pr->progs->numfunctions * sizeof (pr_auxfunction_t *);
	res->auxfunction_map = pr->allocate_progs_mem (pr, i);
	for (i = 0; i < pr->progs->numfunctions; i++)
		res->auxfunction_map[i] = 0;

	for (i = 0; i < res->debug->num_auxfunctions; i++) {
		res->auxfunctions[i].function = LittleLong
			(res->auxfunctions[i].function);
		res->auxfunctions[i].source_line = LittleLong
			(res->auxfunctions[i].source_line);
		res->auxfunctions[i].line_info = LittleLong
			(res->auxfunctions[i].line_info);
		res->auxfunctions[i].local_defs = LittleLong
			(res->auxfunctions[i].local_defs);
		res->auxfunctions[i].num_locals = LittleLong
			(res->auxfunctions[i].num_locals);

		res->auxfunction_map[res->auxfunctions[i].function] =
			&res->auxfunctions[i];
	}
	for (i = 0; i < res->debug->num_linenos; i++) {
		res->linenos[i].fa.func = LittleLong (res->linenos[i].fa.func);
		res->linenos[i].line = LittleLong (res->linenos[i].line);
	}
	res->type_encodings_def = PR_FindGlobal (pr, ".type_encodings");
	if (res->type_encodings_def) {
		__auto_type encodings = &G_STRUCT (pr, qfot_type_encodings_t,
										   res->type_encodings_def->ofs);
		type_encodings = encodings->types;
	}
	for (i = 0; i < res->debug->num_locals; i++) {
		res->local_defs[i].type = LittleShort (res->local_defs[i].type);
		res->local_defs[i].size = LittleShort (res->local_defs[i].size);
		res->local_defs[i].ofs = LittleLong (res->local_defs[i].ofs);
		res->local_defs[i].name = LittleLong (res->local_defs[i].name);
		res->local_defs[i].type_encoding
			= LittleLong (res->local_defs[i].type_encoding);
		if (type_encodings) {
			res->local_defs[i].type_encoding += type_encodings;
		}
	}
	return 1;
}

VISIBLE pr_auxfunction_t *
PR_Debug_AuxFunction (progs_t *pr, pr_uint_t func)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (!res->debug || func >= res->debug->num_auxfunctions) {
		return 0;
	}
	return &res->auxfunctions[func];
}

VISIBLE pr_auxfunction_t *
PR_Debug_MappedAuxFunction (progs_t *pr, pr_uint_t func)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (!res->debug || func >= pr->progs->numfunctions) {
		return 0;
	}
	return res->auxfunction_map[func];
}

VISIBLE pr_def_t *
PR_Debug_LocalDefs (progs_t *pr, pr_auxfunction_t *aux_function)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (!res->debug || !aux_function) {
		return 0;
	}
	if (aux_function->local_defs > res->debug->num_locals) {
		return 0;
	}
	return res->local_defs + aux_function->local_defs;
}

VISIBLE pr_lineno_t *
PR_Debug_Linenos (progs_t *pr, pr_auxfunction_t *aux_function,
				  pr_uint_t *num_linenos)
{
	pr_uint_t   i, count;
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (!res->debug) {
		return 0;
	}
	if (!aux_function) {
		*num_linenos = res->debug->num_linenos;
		return res->linenos;
	}
	if (aux_function->line_info > res->debug->num_linenos) {
		return 0;
	}
	//FIXME put lineno count in sym file
	for (count = 1, i = aux_function->line_info + 1;
		 i < res->debug->num_linenos; i++, count++) {
		if (!res->linenos[i].line) {
			break;
		}
	}
	*num_linenos = count;
	return res->linenos + aux_function->line_info;
}

pr_auxfunction_t *
PR_Get_Lineno_Func (progs_t *pr, pr_lineno_t *lineno)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	while (lineno > res->linenos && lineno->line)
		lineno--;
	if (lineno->line)
		return 0;
	return &res->auxfunctions[lineno->fa.func];
}

pr_uint_t
PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_auxfunction_t *f;

	if (lineno->line)
		return lineno->fa.addr;
	if (lineno->fa.func < res->debug->num_auxfunctions) {
		f = &res->auxfunctions[lineno->fa.func];
		return pr->pr_functions[f->function].first_statement;
	}
	// take a wild guess that only the line number is bogus and return
	// the address anyway
	return lineno->fa.addr;
}

pr_uint_t
PR_Get_Lineno_Line (progs_t *pr, pr_lineno_t *lineno)
{
	if (lineno->line)
		return lineno->line;
	return 0;
}

pr_lineno_t *
PR_Find_Lineno (progs_t *pr, pr_uint_t addr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_uint_t   i;
	pr_lineno_t *lineno = 0;

	if (!res->debug)
		return 0;
	if (!res->debug->num_linenos)
		return 0;
	for (i = res->debug->num_linenos; i > 0; i--) {
		if (PR_Get_Lineno_Addr (pr, &res->linenos[i - 1]) <= addr) {
			lineno = &res->linenos[i - 1];
			break;
		}
	}
	return lineno;
}

const char *
PR_Get_Source_File (progs_t *pr, pr_lineno_t *lineno)
{
	pr_auxfunction_t *f;

	f = PR_Get_Lineno_Func (pr, lineno);
	if (f->function >= (unsigned) pr->progs->numfunctions)
		return 0;
	return PR_GetString(pr, pr->pr_functions[f->function].s_file);
}

const char *
PR_Get_Source_Line (progs_t *pr, pr_uint_t addr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	const char *fname;
	pr_uint_t   line;
	file_t     *file;
	pr_auxfunction_t *func;
	pr_lineno_t *lineno;

	lineno = PR_Find_Lineno (pr, addr);
	if (!lineno || PR_Get_Lineno_Addr (pr, lineno) != addr)
		return 0;
	func = PR_Get_Lineno_Func (pr, lineno);
	fname = PR_Get_Source_File (pr, lineno);
	if (!func || !fname)
		return 0;
	line = PR_Get_Lineno_Line (pr, lineno);
	line += func->source_line;

	file = PR_Load_Source_File (pr, fname);

	if (!file || !file->lines || !line || line > file->num_lines)
		return dsprintf (res->dva, "%s:%u", fname, line);

	return dsprintf (res->dva, "%s:%u:%.*s", fname, line,
					 (int)file->lines[line - 1].len,
					 file->lines[line - 1].text);
}

pr_def_t *
PR_Get_Param_Def (progs_t *pr, dfunction_t *func, unsigned parm)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_uint_t    i;
	pr_auxfunction_t *aux_func;
	pr_def_t    *ddef = 0;
	int          num_params;
	int          param_offs = 0;

	if (!res->debug)
		return 0;
	if (!func)
		return 0;

	num_params = func->numparms;
	if (num_params < 0) {
		num_params = ~num_params;	// one's compliment
		param_offs = 1;	// skip over @args def
	}
	if (parm >= (unsigned) num_params)
		return 0;

	aux_func = res->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;

	for (i = 0; i < aux_func->num_locals; i++) {
		ddef = &res->local_defs[aux_func->local_defs + param_offs + i];
		if (!parm--)
			break;
	}
	return ddef;
}

static pr_auxfunction_t *
get_aux_function (progs_t *pr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	dfunction_t *func;
	if (!pr->pr_xfunction || !res->auxfunction_map)
		return 0;
	func = pr->pr_xfunction->descriptor;
	return res->auxfunction_map[func - pr->pr_functions];
}

static etype_t
get_etype (progs_t *pr, int typeptr)
{
	qfot_type_t *type;

	if (!typeptr) {
		return ev_void;
	}
	type = &G_STRUCT (pr, qfot_type_t, typeptr);
	if (type->meta == ty_basic) {
		return type->t.type;
	}
	return ev_void;
}

pr_def_t *
PR_Get_Local_Def (progs_t *pr, pointer_t offs)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_uint_t   i;
	dfunction_t *func;
	pr_auxfunction_t *aux_func;

	if (!pr->pr_xfunction)
		return 0;
	func = pr->pr_xfunction->descriptor;
	if (!func)
		return 0;
	aux_func = res->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;
	offs -= func->parm_start;
	if (offs >= func->locals)
		return 0;
	for (i = 0; i < aux_func->num_locals; i++)
		if (res->local_defs[aux_func->local_defs + i].ofs == offs)
			return &res->local_defs[aux_func->local_defs + i];
	return 0;
}

VISIBLE void
PR_DumpState (progs_t *pr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (pr->pr_xfunction) {
		if (pr_debug->int_val && res->debug) {
			pr_lineno_t *lineno;
			pr_auxfunction_t *func = 0;
			dfunction_t *descriptor = pr->pr_xfunction->descriptor;
			pr_int_t    addr = pr->pr_xstatement;

			lineno = PR_Find_Lineno (pr, addr);
			if (lineno)
				func = PR_Get_Lineno_Func (pr, lineno);
			if (func && descriptor == pr->pr_functions + func->function)
				addr = PR_Get_Lineno_Addr (pr, lineno);
			else
				addr = max (descriptor->first_statement, addr - 5);

			while (addr != pr->pr_xstatement)
				PR_PrintStatement (pr, pr->pr_statements + addr++, 3);
		}
		PR_PrintStatement (pr, pr->pr_statements + pr->pr_xstatement, 3);
	}
	PR_StackTrace (pr);
}

#define ISDENORM(x) ((x) && !((x) & 0x7f800000))

static const char *
value_string (pr_debug_data_t *data, qfot_type_t *type, pr_type_t *value)
{
	switch (type->meta) {
		case ty_basic:
			switch (type->t.type) {
				case ev_void:
					raw_type_view.void_view (type, value, data);
					break;
				case ev_string:
					raw_type_view.string_view (type, value, data);
					break;
				case ev_float:
					raw_type_view.float_view (type, value, data);
					break;
				case ev_vector:
					raw_type_view.vector_view (type, value, data);
					break;
				case ev_entity:
					raw_type_view.entity_view (type, value, data);
					break;
				case ev_field:
					raw_type_view.field_view (type, value, data);
					break;
				case ev_func:
					raw_type_view.func_view (type, value, data);
					break;
				case ev_pointer:
					raw_type_view.pointer_view (type, value, data);
					break;
				case ev_quat:
					raw_type_view.quat_view (type, value, data);
					break;
				case ev_integer:
					raw_type_view.integer_view (type, value, data);
					break;
				case ev_uinteger:
					raw_type_view.uinteger_view (type, value, data);
					break;
				case ev_short:
					raw_type_view.short_view (type, value, data);
					break;
				case ev_double:
					raw_type_view.double_view (type, value, data);
					break;
				case ev_invalid:
				case ev_type_count:
					dstring_appendstr (data->dstr, "<?""?>");
			}
			break;
		case ty_struct:
			raw_type_view.struct_view (type, value, data);
			break;
		case ty_union:
			raw_type_view.union_view (type, value, data);
			break;
		case ty_enum:
			raw_type_view.enum_view (type, value, data);
			break;
		case ty_array:
			raw_type_view.array_view (type, value, data);
			break;
		case ty_class:
			raw_type_view.class_view (type, value, data);
			break;
	}
	return data->dstr->str;
}

static pr_def_t *
pr_debug_find_def (progs_t *pr, pr_int_t ofs)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_def_t   *def = 0;

	if (pr_debug->int_val && res->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = PR_GlobalAtOfs (pr, ofs);
	return def;
}

static const char *
global_string (pr_debug_data_t *data, pointer_t ofs, etype_t etype,
			   int contents)
{
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	pr_def_t   *def = NULL;
	qfot_type_t dummy_type = { };
	qfot_type_t *type;
	const char *name = 0;

	dstring_clearstr (dstr);

	if (etype == ev_short) {
		dsprintf (dstr, "%04x", (short) ofs);
		return dstr->str;
	}

	if (ofs > pr->globals_size) {
		dsprintf (dstr, "%08x out of bounds", ofs);
		return dstr->str;
	}

	def = pr_debug_find_def (pr, ofs);
	if (!def || !PR_StringValid (pr, def->name)
		|| !*(name = PR_GetString (pr, def->name))) {
		dsprintf (dstr, "[$%x]", ofs);
	}
	if (name) {
		if (strequal (name, "IMMEDIATE") || strequal (name, ".imm")) {
			contents = 1;
		} else {
			dsprintf (dstr, "%s", name);
		}
	}
	if (contents) {
		if (name) {
			dstring_appendstr (dstr, "(");
		}
		if (def) {
			if (!def->type_encoding) {
				dummy_type.t.type = def->type;
				type = &dummy_type;
			} else {
				type = &G_STRUCT (pr, qfot_type_t, def->type_encoding);
			}
		} else {
			dummy_type.t.type = etype;
			type = &dummy_type;
		}
		value_string (data, type, pr->pr_globals + ofs);
		if (name) {
			dstring_appendstr (dstr, ")");
		}
	}
	return dstr->str;
}

static void
pr_debug_void_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dasprintf (data->dstr, "<void>");
}

static void
pr_debug_string_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;
	string_t    string = value->string_var;
	if (PR_StringValid (data->pr, string)) {
		const char *str = PR_GetString (data->pr, string);

		dstring_appendstr (dstr, "\"");
		while (*str) {
			const char *s;

			for (s = str; *s && !strchr ("\"\n\t", *s); s++) {
			}
			if (s != str) {
				dstring_appendsubstr (dstr, str, s - str);
			}
			if (*s) {
				switch (*s) {
					case '\"':
						dstring_appendstr (dstr, "\\\"");
						break;
					case '\n':
						dstring_appendstr (dstr, "\\n");
						break;
					case '\t':
						dstring_appendstr (dstr, "\\t");
						break;
					default:
						dasprintf (dstr, "\\x%02x", *s & 0xff);
				}
				s++;
			}
			str = s;
		}
		dstring_appendstr (dstr, "\"");
	} else {
		dstring_appendstr (dstr, "*** invalid string offset ***");
	}
}

static void
pr_debug_float_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (data->pr->progs->version == PROG_ID_VERSION
		&& ISDENORM (value->integer_var)
		&& value->uinteger_var != 0x80000000) {
		dasprintf (dstr, "<%08x>", value->integer_var);
	} else {
		dasprintf (dstr, "%g", value->float_var);
	}
}

static void
pr_debug_vector_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "'%g %g %g'",
			  value->vector_var[0], value->vector_var[1],
			  value->vector_var[2]);
}

static void
pr_debug_entity_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	edict_t    *edict = PROG_TO_EDICT (pr, value->entity_var);

	dasprintf (dstr, "entity %d", NUM_FOR_BAD_EDICT (pr, edict));
}

static void
pr_debug_field_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	pr_def_t   *def = PR_FieldAtOfs (pr, value->integer_var);

	if (def) {
		dasprintf (dstr, ".%s", PR_GetString (pr, def->name));
	} else {
		dasprintf (dstr, ".<$%04x>", value->integer_var);
	}
}

static void
pr_debug_func_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;

	if (value->func_var >= pr->progs->numfunctions) {
		dasprintf (dstr, "INVALID:%d", value->func_var);
	} else if (!value->func_var) {
		dstring_appendstr (dstr, "NULL");
	} else {
		dfunction_t *f = pr->pr_functions + value->func_var;
		dasprintf (dstr, "%s()", PR_GetString (pr, f->s_name));
	}
}

static void
pr_debug_pointer_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	prdeb_resources_t *res = pr->pr_debug_resources; //FIXME
	dstring_t  *dstr = data->dstr;
	pointer_t   ofs = value->integer_var;
	pr_def_t   *def = 0;

	if (pr_debug->int_val && res->debug) {
		def = PR_Get_Local_Def (pr, ofs);
	}
	if (!def) {
		def = PR_GlobalAtOfs (pr, ofs);
	}
	if (def && def->name) {
		dasprintf (dstr, "&%s", PR_GetString (pr, def->name));
	} else {
		dasprintf (dstr, "[$%x]", ofs);
	}
}

static void
pr_debug_quat_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "'%g %g %g %g'",
			   value->vector_var[0], value->vector_var[1],
			   value->vector_var[2], value->vector_var[3]);
}

static void
pr_debug_integer_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "%d", value->integer_var);
}

static void
pr_debug_uinteger_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "$%08x", value->uinteger_var);
}

static void
pr_debug_short_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "%d", (short)value->integer_var);
}

static void
pr_debug_double_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "%g", *(double *)value);
}

static void
pr_debug_struct_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dstring_appendstr (dstr, "<struct>");
}

static void
pr_debug_union_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	qfot_struct_t *strct = &type->t.strct;

	dstring_appendstr (dstr, "{");
	for (int i = 0; i < strct->num_fields; i++) {
		qfot_var_t *field = strct->fields + i;
		qfot_type_t *val_type = &G_STRUCT (pr, qfot_type_t, field->type);
		pr_type_t  *val = value + field->offset;
		dasprintf (dstr, "%s=", PR_GetString (pr, field->name));
		value_string (data, val_type, val);
		if (i < strct->num_fields) {
			dstring_appendstr (dstr, ", ");
		}
	}
	dstring_appendstr (dstr, "}");
	dstring_appendstr (dstr, "<union>");
}

static void
pr_debug_enum_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dstring_appendstr (dstr, "<enum>");
}

static void
pr_debug_array_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dstring_appendstr (dstr, "<array>");
}

static void
pr_debug_class_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dstring_appendstr (dstr, "<class>");
}

VISIBLE void
PR_Debug_Watch (progs_t *pr, const char *expr)
{
	pr_def_t    watch;
	if (!expr) {
		Sys_Printf ("watch <watchpoint expr>\n");
		if (pr->watch) {
			Sys_Printf ("    watching [%d]\n",
						(int) (intptr_t) (pr->watch - pr->pr_globals));
			if (pr->wp_conditional)
				Sys_Printf ("        if new val == %d\n",
							pr->wp_val.integer_var);
		} else { Sys_Printf ("    none active\n");
		}
		return;
	}

	pr->watch = 0;
	watch = parse_expression (pr, expr, 1);
	if (watch.type != ev_invalid)
		pr->watch = &pr->pr_globals[watch.ofs];
	if (pr->watch) {
		Sys_Printf ("watchpoint set to [%d]\n", PR_SetPointer (pr, pr->watch));
		if (pr->wp_conditional)
			Sys_Printf ("    if new val == %d\n", pr->wp_val.integer_var);
	} else {
		Sys_Printf ("watchpoint cleared\n");
	}
}

VISIBLE void
PR_Debug_Print (progs_t *pr, const char *expr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_def_t    print;
	pr_debug_data_t data = {pr, res->dstr};

	if (!expr) {
		Sys_Printf ("print <print expr>\n");
		return;
	}

	print = parse_expression (pr, expr, 0);
	if (print.type != ev_invalid) {
		const char *s = global_string (&data, print.ofs, print.type, 1);
		Sys_Printf ("[%d] = %s\n", print.ofs, s);
	}
}

VISIBLE void
PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	int         addr = s - pr->pr_statements;
	int         dump_code = contents & 2;
	const char *fmt;
	opcode_t   *op;
	dfunction_t *call_func = 0;
	pr_def_t   *parm_def = 0;
	pr_auxfunction_t *aux_func = 0;
	pr_debug_data_t data;

	dstring_clearstr (res->line);

	data.pr = pr;
	data.dstr = res->dstr;

	if (pr_debug->int_val > 1)
		dump_code = 1;

	if (pr_debug->int_val && res->debug) {
		const char *source_line = PR_Get_Source_Line (pr, addr);

		if (source_line) {
			dasprintf (res->line, "%s%s", source_line, dump_code ? "\n" : "");
			if (!dump_code)
				goto do_print;
		}
		if (!dump_code)
			return;
	}

	op = PR_Opcode (s->op);
	if (!op) {
		Sys_Printf ("%sUnknown instruction %d\n", res->line->str, s->op);
		return;
	}

	if (!(fmt = op->fmt))
		fmt = "%Ga, %Gb, %gc";

	dasprintf (res->line, "%04x ", addr);
	if (pr_debug->int_val > 2)
		dasprintf (res->line, "%02x %04x(%8s) %04x(%8s) %04x(%8s)\t",
					s->op,
					s->a, pr_type_name[op->type_a],
					s->b, pr_type_name[op->type_b],
					s->c, pr_type_name[op->type_c]);

	dasprintf (res->line, "%s ", op->opname);

	while (*fmt) {
		if (*fmt == '%') {
			if (fmt[1] == '%') {
				dstring_appendsubstr (res->line, fmt + 1, 1);
				fmt += 2;
			} else {
				const char *str;
				char        mode = fmt[1], opchar = fmt[2];
				unsigned    parm_ind = 0;
				pr_int_t    opval;
				etype_t     optype = ev_void;
				func_t      func;

				if (mode == 'P') {
					opchar = fmt[3];
					parm_ind = fmt[2] - '0';
					fmt++;				// P has one extra item
					if (parm_ind >= MAX_PARMS)
						goto err;
				}

				switch (opchar) {
					case 'a':
						opval = s->a;
						optype = op->type_a;
						break;
					case 'b':
						opval = s->b;
						optype = op->type_b;
						break;
					case 'c':
						opval = s->c;
						optype = op->type_c;
						break;
					case 'x':
						if (mode == 'P') {
							opval = pr->pr_real_params[parm_ind]
									- pr->pr_globals;
							break;
						}
					default:
						goto err;
				}
				switch (mode) {
					case 'R':
						optype = ev_void;
						aux_func = get_aux_function (pr);
						if (aux_func)
							optype = get_etype (pr, aux_func->return_type);
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'F':
						str = global_string (&data, opval, optype,
											 contents & 1);
						func = G_FUNCTION (pr, opval);
						if (func < pr->progs->numfunctions) {
							call_func = pr->pr_functions + func;
						}
						break;
					case 'P':
						parm_def = PR_Get_Param_Def (pr, call_func, parm_ind);
						optype = ev_void;
						if (parm_def)
							optype = parm_def->type;
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'V':
						str = global_string (&data, opval, ev_void,
											 contents & 1);
						break;
					case 'G':
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'g':
						str = global_string (&data, opval, optype, 0);
						break;
					case 's':
						str = dsprintf (res->dva, "%d", (short) opval);
						break;
					case 'O':
						str = dsprintf (res->dva, "%04x",
										addr + (short) opval);
						break;
					case 'E':
						{
							edict_t    *ed = 0;
							opval = pr->pr_globals[s->a].entity_var;
							parm_ind = pr->pr_globals[s->b].uinteger_var;
							if (parm_ind < pr->progs->entityfields
								&& opval >= 0
								&& opval < pr->pr_edictareasize) {
								ed = PROG_TO_EDICT (pr, opval);
								opval = &ed->v[parm_ind] - pr->pr_globals;
							}
							if (!ed) {
								str = "bad entity.field";
								break;
							}
							str = global_string (&data, opval, optype,
												 contents & 1);
							str = dsprintf (res->dva, "$%x $%x %s",
											s->a, s->b, str);
						}
						break;
					default:
						goto err;
				}
				dstring_appendstr (res->line, str);
				fmt += 3;
				continue;
			err:
				dstring_appendstr (res->line, fmt);
				break;
			}
		} else {
			dstring_appendsubstr (res->line, fmt++, 1);
		}
	}
do_print:
	Sys_Printf ("%s\n", res->line->str);
}

static void
dump_frame (progs_t *pr, prstack_t *frame)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	dfunction_t	   *f = frame->f ? frame->f->descriptor : 0;

	if (!f) {
		Sys_Printf ("<NO FUNCTION>\n");
		return;
	}
	if (pr_debug->int_val && res->debug) {
		pr_lineno_t *lineno = PR_Find_Lineno (pr, frame->s);
		pr_auxfunction_t *func = PR_Get_Lineno_Func (pr, lineno);
		pr_uint_t   line = PR_Get_Lineno_Line (pr, lineno);
		pr_int_t    addr = PR_Get_Lineno_Addr (pr, lineno);

		line += func->source_line;
		if (addr == frame->s) {
			Sys_Printf ("%12s:%u : %s: %x\n",
						PR_GetString (pr, f->s_file),
						line,
						PR_GetString (pr, f->s_name),
						frame->s);
		} else {
			Sys_Printf ("%12s:%u+%d : %s: %x\n",
						PR_GetString (pr, f->s_file),
						line, frame->s - addr,
						PR_GetString (pr, f->s_name),
						frame->s);
		}
	} else {
		Sys_Printf ("%12s : %s: %x\n", PR_GetString (pr, f->s_file),
					PR_GetString (pr, f->s_name), frame->s);
	}
}

VISIBLE void
PR_StackTrace (progs_t *pr)
{
	int         i;
	prstack_t   top;

	if (pr->pr_depth == 0) {
		Sys_Printf ("<NO STACK>\n");
		return;
	}

	top.s = pr->pr_xstatement;
	top.f = pr->pr_xfunction;
	dump_frame (pr, &top);
	for (i = pr->pr_depth - 1; i >= 0; i--)
		dump_frame (pr, pr->pr_stack + i);
}

VISIBLE void
PR_Profile (progs_t * pr)
{
	pr_uint_t   max, num, i;
	dfunction_t *best, *f;

	num = 0;
	do {
		max = 0;
		best = NULL;
		for (i = 0; i < pr->progs->numfunctions; i++) {
			f = &pr->pr_functions[i];
			if (f->profile > max) {
				max = f->profile;
				best = f;
			}
		}
		if (best) {
			if (num < 10)
				Sys_Printf ("%7i %s\n", best->profile,
							PR_GetString (pr, best->s_name));
			num++;
			best->profile = 0;
		}
	} while (best);
}

/*
	ED_Print

	For debugging
*/
VISIBLE void
ED_Print (progs_t *pr, edict_t *ed)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	int         l;
	pr_uint_t   i, j;
	const char *name;
	pr_def_t   *d;
	pr_type_t  *v;
	qfot_type_t dummy_type = { };
	qfot_type_t *type;
	pr_debug_data_t data = {pr, res->dstr};

	if (ed->free) {
		Sys_Printf ("FREE\n");
		return;
	}

	Sys_Printf ("\nEDICT %d:\n", NUM_FOR_BAD_EDICT (pr, ed));
	for (i = 0; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		if (!d->name)					// null field def (probably 1st)
			continue;
		type = get_def_type (pr, d, &dummy_type);
		name = PR_GetString (pr, d->name);
		if (name[strlen (name) - 2] == '_'
			&& strchr ("xyz", name[strlen (name) -1]))
			continue;					// skip _x, _y, _z vars

		for (j = 0; j < d->size; j++) {
			if (E_INT (ed, d->ofs + j)) {
				break;
			}
		}
		if (j == d->size) {
			continue;
		}

		v = ed->v + d->ofs;

		l = 15 - strlen (name);
		if (l < 1)
			l = 1;

		dstring_clearstr (res->dstr);
		value_string (&data, type, v);
		Sys_Printf ("%s%*s%s\n", name, l, "", res->dstr->str);
	}
}

void
PR_Debug_Init (progs_t *pr)
{
	prdeb_resources_t *res = calloc (1, sizeof (*res));
	res->pr = pr;
	res->string = dstring_newstr ();
	res->dva = dstring_newstr ();
	res->line = dstring_newstr ();
	res->dstr = dstring_newstr ();

	PR_Resources_Register (pr, "PR_Debug", res, pr_debug_clear);
	if (!file_hash) {
		file_hash = Hash_NewTable (1024, file_get_key, file_free, 0);
	}
	PR_AddLoadFunc (pr, PR_LoadDebug);
}

void
PR_Debug_Init_Cvars (void)
{
	pr_debug = Cvar_Get ("pr_debug", "0", CVAR_NONE, NULL,
						 "enable progs debugging");
	pr_source_path = Cvar_Get ("pr_source_path", ".", CVAR_NONE, source_path_f,
							   "where to look (within gamedir) for source "
							   "files");
}
