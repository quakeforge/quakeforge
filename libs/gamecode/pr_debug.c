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
#include <inttypes.h>

#include "QF/fbsearch.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "QF/progs/pr_debug.h"
#include "QF/progs/pr_type.h"
#include "QF/simd/types.h"

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

typedef struct compunit_s {
	const char *file;
	pr_compunit_t *unit;
} compunit_t;

typedef struct {
	const char *file;
	pr_uint_t   line;
} func_key_t;

typedef struct prdeb_resources_s {
	progs_t    *pr;
	dstring_t  *string;
	dstring_t  *dva;
	dstring_t  *line;
	dstring_t  *dstr;
	va_ctx_t   *va;
	const char *debugfile;
	pr_debug_header_t *debug;
	pr_auxfunction_t *auxfunctions;
	pr_auxfunction_t **auxfunction_map;
	pr_func_t  *sorted_functions;
	pr_lineno_t *linenos;
	pr_def_t   *local_defs;
	pr_def_t   *type_encodings_def;
	qfot_type_t void_type;
	qfot_type_t *type_encodings[ev_type_count];
	pr_def_t   *debug_defs;
	pr_type_t  *debug_data;
	hashtab_t  *debug_syms;
	hashtab_t  *compunits;	// by source file
	PR_RESMAP (compunit_t) compmap;	// handy allocation/freeing
	hashtab_t  *file_hash;
} prdeb_resources_t;

typedef struct {
	progs_t    *pr;
	dstring_t  *dstr;
} pr_debug_data_t;

int pr_debug;
static cvar_t pr_debug_cvar = {
	.name = "pr_debug",
	.description =
		"enable progs debugging",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pr_debug },
};
char *pr_source_path;
static cvar_t pr_source_path_cvar = {
	.name = "pr_source_path",
	.description =
		"where to look (within gamedir) for source files",
	.default_value = ".",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &pr_source_path },
};
static char    *source_path_string;
static char   **source_paths;

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
#define EV_TYPE(t) \
	static void pr_debug_##t##_view (qfot_type_t *type, pr_type_t *value, \
									 void *_data);
#include "QF/progs/pr_type_names.h"

static type_view_t raw_type_view = {
	pr_debug_struct_view,
	pr_debug_union_view,
	pr_debug_enum_view,
	pr_debug_array_view,
	pr_debug_class_view,
#define EV_TYPE(t) \
	pr_debug_##t##_view,
#include "QF/progs/pr_type_names.h"
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

static const char *
def_get_key (const void *d, void *p)
{
	__auto_type def = (pr_def_t *) d;
	__auto_type pr = (progs_t *) p;
	return PR_GetString (pr, def->name);
}

static const char *
compunit_get_key (const void *cu, void *p)
{
	__auto_type compunit = (compunit_t *) cu;
	return compunit->file;
}

static void
source_path_f (void *data, const cvar_t *cvar)
{
	int         i;
	char       *s;

	if (source_path_string) {
		free (source_path_string);
	}
	source_path_string = strdup (pr_source_path);
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
		case ty_handle:
			return pr_type_size[type->type];
		case ty_struct:
		case ty_union:
			size = 0;
			for (pr_int_t i = 0; i < type->strct.num_fields; i++) {
				const qfot_var_t *field = &type->strct.fields[i];
				aux_type = &G_STRUCT (pr, qfot_type_t, field->type);
				size = max (size,
							field->offset + pr_debug_type_size (pr, aux_type));
			}
			return size;
		case ty_enum:
			return pr_type_size[ev_int];
		case ty_array:
			aux_type = &G_STRUCT (pr, qfot_type_t, type->array.type);
			size = pr_debug_type_size (pr, aux_type);
			return type->array.size * size;
		case ty_class:
			return 1;	//FIXME or should it return sizeof class struct?
		case ty_alias:
			aux_type = &G_STRUCT (pr, qfot_type_t, type->alias.aux_type);
			return pr_debug_type_size (pr, aux_type);
		case ty_algebra:
			return 1;	//FIXME wip
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
		type->type = def->type;
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
			expr_ptr = &E_fld (ent, field->ofs);
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
				PR_PTR (int, &pr->wp_val) = strtol (es->token->str, &e, 0);
				if (e == es->token->str)
					goto error;
				if (*e == '.' || *e == 'e' || *e == 'E')
					PR_PTR (float, &pr->wp_val) = strtod (es->token->str, &e);
				pr->wp_conditional = 1;
			}
		}
		if (Script_TokenAvailable (es, 1))
			Sys_Printf ("ignoring tail\n");
	}
error:
	if (es->error) {
		Sys_Printf ("%s\n", es->error);
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
	Hash_FlushTable (res->file_hash);
	Hash_FlushTable (res->debug_syms);
	Hash_FlushTable (res->compunits);
	PR_RESRESET (res->compmap);
	res->debug = 0;
	res->auxfunctions = 0;
	if (res->auxfunction_map)
		pr->free_progs_mem (pr, res->auxfunction_map);
	res->auxfunction_map = 0;
	if (res->sorted_functions)
		pr->free_progs_mem (pr, res->sorted_functions);
	res->sorted_functions = 0;
	res->linenos = 0;
	res->local_defs = 0;

	pr->watch = 0;
	pr->wp_conditional = 0;
	PR_PTR (int, &pr->wp_val) = 0;

	for (int i = 0; i < ev_type_count; i++ ) {
		res->type_encodings[i] = &res->void_type;
	}
}

static void
pr_debug_destroy (progs_t *pr, void *_res)
{
	__auto_type res = (prdeb_resources_t *) _res;

	dstring_delete (res->string);
	dstring_delete (res->dva);
	dstring_delete (res->line);
	dstring_delete (res->dstr);
	va_destroy_context (res->va);

	Hash_DelTable (res->file_hash);
	Hash_DelTable (res->debug_syms);
	Hash_DelTable (res->compunits);

	PR_RESDELMAP (res->compmap);

	pr->pr_debug_resources = 0;

	free (res);
}

static file_t *
PR_Load_Source_File (progs_t *pr, const char *fname)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	char       *l, *p, **dir;
	file_t     *f = Hash_Find (res->file_hash, fname);

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
	Hash_Add (res->file_hash, f);
	return f;
}

static void
byteswap_def (pr_def_t *def)
{
	def->type = LittleShort (def->type);
	def->size = LittleShort (def->size);
	def->ofs = LittleLong (def->ofs);
	def->name = LittleLong (def->name);
	def->type_encoding = LittleLong (def->type_encoding);
}

static compunit_t *
new_compunit (prdeb_resources_t *res)
{
	return PR_RESNEW (res->compmap);
}

static void
process_compunit (prdeb_resources_t *res, pr_def_t *def)
{
	progs_t    *pr = res->pr;
	__auto_type compunit = (pr_compunit_t *) (res->debug_data + def->ofs);

	for (unsigned i = 0; i < compunit->num_files; i++) {
		compunit_t *cu = new_compunit (res);
		cu->unit = compunit;
		cu->file = PR_GetString (pr, compunit->files[i]);
		Hash_Add (res->compunits, cu);
	}
}

static int
def_compare_sort (const void *_da, const void *_db, void *_res)
{
	pr_def_t    da = *(const pr_def_t *)_da;
	pr_def_t    db = *(const pr_def_t *)_db;
	return da.ofs - db.ofs;
}

static int
func_compare_sort (const void *_fa, const void *_fb, void *_res)
{
	prdeb_resources_t *res = _res;
	progs_t    *pr = res->pr;
	pr_func_t   fa = *(const pr_func_t *)_fa;
	pr_func_t   fb = *(const pr_func_t *)_fb;
	const char *fa_file = PR_GetString (pr, pr->pr_functions[fa].file);
	const char *fb_file = PR_GetString (pr, pr->pr_functions[fb].file);
	int         cmp = strcmp (fa_file, fb_file);
	if (cmp) {
		return cmp;
	}
	pr_auxfunction_t *fa_aux = res->auxfunction_map[fa];
	pr_auxfunction_t *fb_aux = res->auxfunction_map[fb];
	pr_uint_t   fa_line = fa_aux ? fa_aux->source_line : 0;
	pr_uint_t   fb_line = fb_aux ? fb_aux->source_line : 0;
	return fa_line - fb_line;
}

static int
func_compare_search (const void *_key, const void *_f, void *_res)
{
	prdeb_resources_t *res = _res;
	progs_t    *pr = res->pr;
	const func_key_t *key = _key;
	pr_func_t   f = *(const pr_func_t *)_f;
	const char *f_file = PR_GetString (pr, pr->pr_functions[f].file);
	int         cmp = strcmp (key->file, f_file);
	if (cmp) {
		return cmp;
	}
	pr_auxfunction_t *f_aux = res->auxfunction_map[f];
	pr_uint_t   f_line = f_aux ? f_aux->source_line : 0;
	return key->line - f_line;
}

VISIBLE void
PR_DebugSetSym (progs_t *pr, pr_debug_header_t *debug)
{
	prdeb_resources_t *res = pr->pr_debug_resources;

	res->auxfunctions = (pr_auxfunction_t*)((char*)debug + debug->auxfunctions);
	res->linenos = (pr_lineno_t*)((char*)debug + debug->linenos);
	res->local_defs = (pr_def_t*)((char*)debug + debug->locals);
	res->debug_defs = (pr_def_t*)((char*)debug + debug->debug_defs);
	res->debug_data = (pr_type_t*)((char*)debug + debug->debug_data);

	size_t      size;
	size = pr->progs->functions.count * sizeof (pr_auxfunction_t *);
	res->auxfunction_map = pr->allocate_progs_mem (pr, size);
	size = pr->progs->functions.count * sizeof (pr_func_t);
	res->sorted_functions = pr->allocate_progs_mem (pr, size);

	for (pr_uint_t i = 0; i < pr->progs->functions.count; i++) {
		res->auxfunction_map[i] = 0;
		res->sorted_functions[i] = i;
	}

	qfot_type_encodings_t *encodings = 0;
	pr_ptr_t    type_encodings = 0;
	res->type_encodings_def = PR_FindGlobal (pr, ".type_encodings");
	if (res->type_encodings_def) {
		encodings = &G_STRUCT (pr, qfot_type_encodings_t,
							   res->type_encodings_def->ofs);
		type_encodings = encodings->types;
	}

	for (pr_uint_t i = 0; i < debug->num_auxfunctions; i++) {
		if (type_encodings) {
			res->auxfunctions[i].return_type += type_encodings;
		}
		res->auxfunction_map[res->auxfunctions[i].function] =
			&res->auxfunctions[i];
		heapsort_r (res->local_defs + res->auxfunctions[i].local_defs,
					res->auxfunctions[i].num_locals, sizeof (pr_def_t),
					def_compare_sort, res);
	}
	heapsort_r (res->sorted_functions, pr->progs->functions.count,
				sizeof (pr_func_t), func_compare_sort, res);

	for (pr_uint_t i = 0; i < debug->num_locals; i++) {
		if (type_encodings) {
			res->local_defs[i].type_encoding += type_encodings;
		}
	}

	pr_string_t compunit_str = PR_FindString (pr, ".compile_unit");
	for (pr_uint_t i = 0; i < debug->num_debug_defs; i++) {
		pr_def_t   *def = &res->debug_defs[i];
		if (type_encodings) {
			def->type_encoding += type_encodings;
		}
		Hash_Add (res->debug_syms, def);
		if (def->name == compunit_str) {
			process_compunit (res, def);
		}
	}

	if (encodings) {
		qfot_type_t *type;
		for (pr_ptr_t type_ptr = 4; type_ptr < encodings->size;
			 type_ptr += type->size) {
			type = &G_STRUCT (pr, qfot_type_t, type_encodings + type_ptr);
			if (type->meta == ty_basic
				&& (unsigned) type->type < ev_type_count) {
				res->type_encodings[type->type] = type;
			}
		}
	}

	res->debug = debug;
}

VISIBLE int
PR_LoadDebug (progs_t *pr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	char       *sym_path;
	const char *path_end, *sym_file;
	off_t       debug_size;
	pr_uint_t   i;
	pr_def_t   *def;
	pr_type_t  *str = 0;
	pr_debug_header_t *debug;

	res->debug = 0;

	if (!pr_debug)
		return 1;

	def = PR_FindGlobal (pr, ".debug_file");
	if (def)
		str = &pr->pr_globals[def->ofs];

	if (!str)
		return 1;
	res->debugfile = PR_GetString (pr, PR_PTR (string, str));
	sym_file = QFS_SkipPath (res->debugfile);
	path_end = QFS_SkipPath (pr->progs_name);
	sym_path = malloc (strlen (sym_file) + (path_end - pr->progs_name) + 1);
	strncpy (sym_path, pr->progs_name, path_end - pr->progs_name);
	strcpy (sym_path + (path_end - pr->progs_name), sym_file);
	debug = pr->load_file (pr, sym_path, &debug_size);
	if (!debug) {
		Sys_Printf ("can't load %s for debug info\n", sym_path);
		free (sym_path);
		return 1;
	}
	debug->version = LittleLong (debug->version);
	if (debug->version != PROG_DEBUG_VERSION) {
		Sys_Printf ("ignoring %s with unsupported version %x.%03x.%03x\n",
					sym_path,
					(debug->version >> 24) & 0xff,
					(debug->version >> 12) & 0xfff,
					debug->version & 0xfff);
		free (sym_path);
		return 1;
	}
	debug->crc = LittleShort (debug->crc);
	if (debug->crc != pr->crc) {
		Sys_Printf ("ignoring %s that doesn't match %s. (CRCs: "
					"sym:%d dat:%d)\n",
					sym_path, pr->progs_name, debug->crc, pr->crc);
		free (sym_path);
		return 1;
	}
	free (sym_path);
	debug->you_tell_me_and_we_will_both_know = LittleShort
		(debug->you_tell_me_and_we_will_both_know);
	debug->auxfunctions = LittleLong (debug->auxfunctions);
	debug->num_auxfunctions = LittleLong (debug->num_auxfunctions);
	debug->linenos = LittleLong (debug->linenos);
	debug->num_linenos = LittleLong (debug->num_linenos);
	debug->locals = LittleLong (debug->locals);
	debug->num_locals = LittleLong (debug->num_locals);
	debug->debug_defs = LittleLong (debug->debug_defs);
	debug->num_debug_defs = LittleLong (debug->num_debug_defs);
	debug->debug_data = LittleLong (debug->debug_data);
	debug->debug_data_size = LittleLong (debug->debug_data_size);

	__auto_type auxfuncs = (pr_auxfunction_t*)((char*)debug
											   + debug->auxfunctions);
	for (i = 0; i < debug->num_auxfunctions; i++) {
		auxfuncs[i].function = LittleLong (auxfuncs[i].function);
		auxfuncs[i].source_line = LittleLong (auxfuncs[i].source_line);
		auxfuncs[i].line_info = LittleLong (auxfuncs[i].line_info);
		auxfuncs[i].local_defs = LittleLong (auxfuncs[i].local_defs);
		auxfuncs[i].num_locals = LittleLong (auxfuncs[i].num_locals);
	}

	__auto_type linenos = (pr_lineno_t*)((char*)debug + debug->linenos);
	for (i = 0; i < debug->num_linenos; i++) {
		linenos[i].fa.func = LittleLong (linenos[i].fa.func);
		linenos[i].line = LittleLong (linenos[i].line);
	}

	__auto_type local_defs = (pr_def_t*)((char*)debug + debug->locals);
	for (i = 0; i < debug->num_locals; i++) {
		byteswap_def (&local_defs[i]);
	}
	__auto_type debug_defs = (pr_def_t*)((char*)debug + debug->locals);
	for (i = 0; i < debug->num_debug_defs; i++) {
		byteswap_def (&debug_defs[i]);
	}

	PR_DebugSetSym (pr, debug);
	return 1;
}

VISIBLE const char *
PR_Debug_GetBaseDirectory (progs_t *pr, const char *file)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	__auto_type cu = (compunit_t *) Hash_Find (res->compunits, file);

	if (cu) {
		return PR_GetString (pr, cu->unit->basedir);
	}
	return 0;
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
	if (!res->debug || func >= pr->progs->functions.count) {
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

pr_uint_t
PR_FindSourceLineAddr (progs_t *pr, const char *file, pr_uint_t line)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	func_key_t  key = { file, line };
	pr_func_t  *f = fbsearch_r (&key, res->sorted_functions,
								pr->progs->functions.count, sizeof (pr_func_t),
								func_compare_search, res);
	if (!f) {
		return 0;
	}
	dfunction_t *func = &pr->pr_functions[*f];
	if (func->first_statement <= 0
		|| strcmp (file, PR_GetString (pr, func->file)) != 0) {
		return 0;
	}
	pr_auxfunction_t *aux = res->auxfunction_map[*f];
	if (!aux) {
		return 0;
	}
	pr_uint_t   addr = func->first_statement;
	line -= aux->source_line;

	//FIXME put lineno count in sym file
	for (pr_uint_t i = aux->line_info + 1; i < res->debug->num_linenos; i++) {
		if (!res->linenos[i].line) {
			break;
		}
		if (res->linenos[i].line <= line) {
			addr = res->linenos[i].fa.addr;
		} else {
			break;
		}
	}
	return addr;
}

const char *
PR_Get_Source_File (progs_t *pr, pr_lineno_t *lineno)
{
	pr_auxfunction_t *f;

	f = PR_Get_Lineno_Func (pr, lineno);
	if (f->function >= (unsigned) pr->progs->functions.count)
		return 0;
	return PR_GetString(pr, pr->pr_functions[f->function].file);
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
PR_Get_Param_Def (progs_t *pr, dfunction_t *func, unsigned param)
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

	num_params = func->numparams;
	if (num_params < 0) {
		num_params = ~num_params;	// one's compliment
		param_offs = 1;	// skip over @args def
	}
	if (param >= (unsigned) num_params)
		return 0;

	aux_func = res->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;

	for (i = 0; i < aux_func->num_locals; i++) {
		ddef = &res->local_defs[aux_func->local_defs + param_offs + i];
		if (!param--)
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

static qfot_type_t *
get_type (prdeb_resources_t *res, int typeptr)
{
	progs_t    *pr = res->pr;

	if (!typeptr) {
		return &res->void_type;
	}
	return &G_STRUCT (pr, qfot_type_t, typeptr);
}

pr_def_t *
PR_Get_Local_Def (progs_t *pr, pr_ptr_t *offset)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	dfunction_t *func;
	pr_auxfunction_t *aux_func;
	pr_ptr_t    offs = *offset;
	pr_def_t   *def;

	if (!pr->pr_xfunction)
		return 0;
	func = pr->pr_xfunction->descriptor;
	if (!func)
		return 0;
	aux_func = res->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;

	pr_ptr_t    locals_start;
	if (pr->progs->version == PROG_VERSION) {
		if (pr->pr_depth) {
			prstack_t  *frame = pr->pr_stack + pr->pr_depth - 1;
			locals_start = frame->stack_ptr - func->params_start;
		} else {
			locals_start = 0;	//FIXME ? when disassembling in qfprogs
		}
	} else {
		locals_start = func->params_start;
	}
	offs -= locals_start;
	if (offs >= func->locals)
		return 0;
	if ((def =  PR_SearchDefs (res->local_defs + aux_func->local_defs,
							   aux_func->num_locals, offs))) {
		*offset = offs - def->ofs;
	}
	return def;
}

VISIBLE void
PR_DumpState (progs_t *pr)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	if (pr->pr_xfunction) {
		if (pr_debug && res->debug) {
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

static void
value_string (pr_debug_data_t *data, qfot_type_t *type, pr_type_t *value)
{
	switch (type->meta) {
		case ty_algebra:
			if (type->type == ev_invalid) {
				dstring_appendstr (data->dstr, "<?""?>");
				break;
			}
			// fall through
		case ty_handle:
		case ty_basic:
			switch (type->type) {
#define EV_TYPE(t) \
				case ev_##t: \
					raw_type_view.t##_view (type, value, data); \
					break;
#include "QF/progs/pr_type_names.h"

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
		case ty_alias://XXX
			type = &G_STRUCT (data->pr, qfot_type_t, type->alias.aux_type);
			value_string (data, type, value);
			break;
	}
}

static pr_def_t *
pr_debug_find_def (progs_t *pr, pr_ptr_t *ofs)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	pr_def_t   *def = 0;

	if (pr_debug && res->debug) {
		def = PR_Get_Local_Def (pr, ofs);
	}
	if (*ofs >= pr->progs->globals.count) {
		return 0;
	}
	if (!def) {
		def = PR_GlobalAtOfs (pr, *ofs);
		if (def) {
			*ofs -= def->ofs;
		}
	}
	return def;
}

static const char *
global_string (pr_debug_data_t *data, pr_ptr_t offset, qfot_type_t *type,
			   int contents)
{
	progs_t    *pr = data->pr;
	prdeb_resources_t *res = pr->pr_debug_resources;
	dstring_t  *dstr = data->dstr;
	pr_def_t   *def = NULL;
	qfot_type_t dummy_type = { };
	const char *name = 0;
	pr_ptr_t    offs = offset;

	dstring_clearstr (dstr);

	if (type && type->meta == ty_basic && type->type == ev_short) {
		dsprintf (dstr, "%04x", (short) offset);
		return dstr->str;
	}

	if (offset > pr->globals_size) {
		dsprintf (dstr, "%08x out of bounds", offset);
		return dstr->str;
	}

	def = pr_debug_find_def (pr, &offs);
	if (!def || !PR_StringValid (pr, def->name)
		|| !*(name = PR_GetString (pr, def->name))) {
		dsprintf (dstr, "[$%x]", offset);
	}
	if (name) {
		if (strequal (name, "IMMEDIATE") || strequal (name, ".imm")) {
			contents = 1;
		} else {
			if (offs) {
				dsprintf (dstr, "{%s + %u}", name, offs);
			} else {
				dsprintf (dstr, "%s", name);
			}
		}
	}
	if (contents) {
		if (name) {
			dstring_appendstr (dstr, "(");
		}
		if (!type) {
			if (def) {
				if (!def->type_encoding) {
					dummy_type.type = def->type;
					type = &dummy_type;
				} else {
					type = &G_STRUCT (pr, qfot_type_t, def->type_encoding);
				}
			} else {
				type = &res->void_type;
			}
		}
		value_string (data, type, pr->pr_globals + offset);
		if (name) {
			dstring_appendstr (dstr, ")");
		}
	}
	return dstr->str;
}

static const char *
extend_string (pr_debug_data_t *data, pr_uint_t ext)
{
	progs_t    *pr = data->pr;
	prdeb_resources_t *res = pr->pr_debug_resources;
	static const char *extend_range[] = {
		"1>2", "1>3", "1>4",
		"2>3", "2>4", "3>4",
		"res1", "res2",
	};
	static const char *extend_string[] = {
		"0", "1", "c", "-1",
	};
	return va (res->va, "[%s%s:%s]%d",
			   extend_range[ext & 7],
			   ext & 0100 ? ":r" : "",
			   extend_string[(ext >> 3) & 2],
			   32 << ((ext >> 5) & 1));
}

static const char *
hop_string (pr_debug_data_t *data, pr_uint_t hop)
{
	static const char *hop_string[] = {
		"&", "|", "^", "+",
		"!&", "!|", "!^", "+",
	};
	return hop_string[hop & 7];
}

static const char *
swizzle_string (pr_debug_data_t *data, pr_uint_t swiz)
{
	progs_t    *pr = data->pr;
	prdeb_resources_t *res = pr->pr_debug_resources;
	static char swizzle_components[] = "xyzw";
	const char *swizzle = "";

	for (int i = 0; i < 4; i++) {
		if (swiz & (0x1000 << i)) {
			swizzle = va (res->va, "%s0", swizzle);
		} else {
			swizzle = va (res->va, "%s%s%c", swizzle,
						  swiz & (0x100 << i) ? "-" : "",
						  swizzle_components[(swiz >> 2 * i) & 3]);
		}
	}
	return swizzle;
}

const char *
PR_Debug_ValueString (progs_t *pr, pr_ptr_t offset, qfot_type_t *type,
					  dstring_t *dstr)
{
	pr_debug_data_t data = {pr, dstr};
	value_string (&data, type, pr->pr_globals + offset);
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
	pr_string_t string = PR_PTR (string, value);
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

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value++) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		if (data->pr->progs->version == PROG_ID_VERSION
			&& ISDENORM (PR_PTR (int, value))
			&& PR_PTR (uint, value) != 0x80000000) {
			dasprintf (dstr, "<%08x>", PR_PTR (int, value));
		} else {
			dasprintf (dstr, "%.9g", PR_PTR (float, value));
		}
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_vector_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "'%.9g %.9g %.9g'", VectorExpand (&PR_PTR (float, value)));
}

static void
pr_debug_entity_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;

	if (pr->pr_edicts
		&& PR_PTR (entity, value) < pr->max_edicts
		&& !(PR_PTR (entity, value) % pr->pr_edict_size)) {
		edict_t    *edict = PROG_TO_EDICT (pr, PR_PTR (entity, value));
		if (edict) {
			dasprintf (dstr, "entity %d", NUM_FOR_BAD_EDICT (pr, edict));
			return;
		}
	}
	dasprintf (dstr, "entity [%x]", PR_PTR (entity, value));
}

static void
pr_debug_field_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	pr_def_t   *def = PR_FieldAtOfs (pr, PR_PTR (int, value));

	if (def) {
		dasprintf (dstr, ".%s", PR_GetString (pr, def->name));
	} else {
		dasprintf (dstr, ".<$%04x>", PR_PTR (int, value));
	}
}

static void
pr_debug_func_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;

	if (PR_PTR (func, value) >= pr->progs->functions.count) {
		dasprintf (dstr, "INVALID:%d", PR_PTR (func, value));
	} else if (!PR_PTR (func, value)) {
		dstring_appendstr (dstr, "NULL");
	} else {
		dfunction_t *f = pr->pr_functions + PR_PTR (func, value);
		dasprintf (dstr, "%s()", PR_GetString (pr, f->name));
	}
}

static void
pr_debug_ptr_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	pr_ptr_t    offset = PR_PTR (int, value);
	pr_ptr_t    offs = offset;
	pr_def_t   *def = 0;

	def = pr_debug_find_def (pr, &offs);
	if (def && def->name) {
		if (offs) {
			dasprintf (dstr, "&%s + %u", PR_GetString (pr, def->name), offs);
		} else {
			dasprintf (dstr, "&%s", PR_GetString (pr, def->name));
		}
	} else {
		dasprintf (dstr, "[$%x]", offset);
	}
}

static void
pr_debug_quaternion_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "'%.9g %.9g %.9g %.9g'", QuatExpand (&PR_PTR (float, value)));
}

static void
pr_debug_int_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value++) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		dasprintf (dstr, "%d", PR_PTR (int, value));
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_uint_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value++) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		dasprintf (dstr, "$%08x", PR_PTR (uint, value));
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_short_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "%04x", (short)PR_PTR (int, value));
}

static void
pr_debug_double_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value += 2) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		dasprintf (dstr, "%.17g", *(double *)value);
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_long_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value += 2) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		dasprintf (dstr, "%" PRIi64, *(int64_t *)value);
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_ulong_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "[");
	}
	for (int i = 0; i < type->basic.width; i++, value += 2) {
		if (i) {
			dstring_appendstr (dstr, ", ");
		}
		dasprintf (dstr, "%" PRIu64, *(uint64_t *)value);
	}
	if (type->basic.width > 1) {
		dstring_appendstr (dstr, "]");
	}
}

static void
pr_debug_ushort_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	__auto_type data = (pr_debug_data_t *) _data;
	dstring_t  *dstr = data->dstr;

	dasprintf (dstr, "%04x", (pr_ushort_t)PR_PTR (int, value));
}

static void
pr_dump_struct (qfot_type_t *type, pr_type_t *value, void *_data,
				const char *struct_type)
{
	__auto_type data = (pr_debug_data_t *) _data;
	progs_t    *pr = data->pr;
	dstring_t  *dstr = data->dstr;
	qfot_struct_t *strct = &type->strct;

	dstring_appendstr (dstr, "{");
	for (int i = 0; i < strct->num_fields; i++) {
		qfot_var_t *field = strct->fields + i;
		qfot_type_t *val_type = &G_STRUCT (pr, qfot_type_t, field->type);
		pr_type_t  *val = value + field->offset;
		dasprintf (dstr, "%s=", PR_GetString (pr, field->name));
		value_string (data, val_type, val);
		if (i < strct->num_fields - 1) {
			dstring_appendstr (dstr, ", ");
		}
	}
	dstring_appendstr (dstr, "}");
	//dasprintf (dstr, "<%s>", struct_type);
}
static void
pr_debug_struct_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	pr_dump_struct (type, value, _data, "struct");
}

static void
pr_debug_union_view (qfot_type_t *type, pr_type_t *value, void *_data)
{
	pr_dump_struct (type, value, _data, "union");
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
							PR_PTR (int, &pr->wp_val));
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
			Sys_Printf ("    if new val == %d\n", PR_PTR (int, &pr->wp_val));
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
	if (print.type_encoding) {
		qfot_type_t *type = get_type (res, print.type_encoding);
		const char *s = global_string (&data, print.ofs, type, 1);
		Sys_Printf ("[%d] = %s\n", print.ofs, s);
	}
}

static const char *
print_raw_op (progs_t *pr, pr_short_t op, pr_ushort_t base_ind,
			  etype_t op_type, int op_width)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	const char *width = va (res->va, "%d", op_width);
	return va (res->va, "%d:%04hx<%08x>%s:%-8s",
				base_ind, op, op + pr->pr_bases[base_ind],
				op_width > 0 ? width : op_width < 0 ? "X" : "?",
				pr_type_name[op_type]);
}

static pr_uint_t
get_opval (progs_t *pr, pr_short_t op)
{
	if (pr->progs->version < PROG_VERSION) {
		return (pr_ushort_t) op;
	}
	return op;
}

VISIBLE void
PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	int         addr = s - pr->pr_statements;
	int         dump_code = contents & 2;
	const char *fmt;
	const char *mnemonic;
	const char *width = "";
	dfunction_t *call_func = 0;
	pr_def_t   *param_def = 0;
	pr_auxfunction_t *aux_func = 0;
	pr_debug_data_t data;
	etype_t     op_type[3];
	int         op_width[3];

	dstring_clearstr (res->line);

	data.pr = pr;
	data.dstr = res->dstr;

	if (pr_debug > 1)
		dump_code = 1;

	if (pr_debug && res->debug) {
		const char *source_line = PR_Get_Source_Line (pr, addr);

		if (source_line) {
			dasprintf (res->line, "%s%s", source_line, dump_code ? "\n" : "");
			if (!dump_code)
				goto do_print;
		}
		if (!dump_code)
			return;
	}

	if (pr->progs->version < PROG_VERSION) {
		const v6p_opcode_t *op = PR_v6p_Opcode (s->op);
		if (!op) {
			Sys_Printf ("%sUnknown instruction %d\n", res->line->str, s->op);
			return;
		}
		VectorSet (op->type_a, op->type_b, op->type_c, op_type);
		VectorSet (1, 1, 1, op_width);
		fmt = op->fmt;
		mnemonic = op->opname;
	} else {
		const opcode_t *op = PR_Opcode (s->op);
		if (!op) {
			Sys_Printf ("%sUnknown instruction %d\n", res->line->str, s->op);
			return;
		}
		VectorCopy (op->widths, op_width);
		VectorCopy (op->types, op_type);
		fmt = op->fmt;
		mnemonic = op->mnemonic;
	}

	if (!fmt) {
		fmt = "%Ga, %Gb, %gc";
	}

	dasprintf (res->line, "%04x ", addr);
	if (pr_debug > 2) {
		if (pr->progs->version < PROG_VERSION) {
			dasprintf (res->line,
						"%03x %04hx(%8s) %04hx(%8s) %04hx(%8s)\t",
						s->op,
						s->a, pr_type_name[op_type[0]],
						s->b, pr_type_name[op_type[1]],
						s->c, pr_type_name[op_type[2]]);
		} else {
			dasprintf (res->line, "%04x %s %s %s\t",
						s->op,
						print_raw_op (pr, s->a, PR_BASE_IND (s->op, A),
									  op_type[0], op_width[0]),
						print_raw_op (pr, s->b, PR_BASE_IND (s->op, B),
									  op_type[1], op_width[1]),
						print_raw_op (pr, s->c, PR_BASE_IND (s->op, C),
									  op_type[2], op_width[2]));
		}
	} else if (op_width[0] > 1 || op_width[1] > 1 || op_width[2] > 1) {
		width = va (res->va, "{%d,%d,%d}", VectorExpand (op_width));
	}

	dasprintf (res->line, "%s%s ", mnemonic, width);

	while (*fmt) {
		if (*fmt == '%') {
			if (fmt[1] == '%') {
				dstring_appendsubstr (res->line, fmt + 1, 1);
				fmt += 2;
			} else {
				const char *str;
				char        mode = fmt[1], opchar = fmt[2];
				unsigned    param_ind = 0;
				pr_uint_t   shift = 0;
				pr_uint_t   opreg;
				pr_uint_t   opval;
				qfot_type_t basic_type = {};
				qfot_type_t *optype = &basic_type;
				pr_func_t   func;

				if (mode == 'P') {
					opchar = fmt[3];
					param_ind = fmt[2] - '0';
					fmt++;				// P has one extra item
					if (param_ind >= PR_MAX_PARAMS)
						goto err;
				}
				if (mode == 'M' || mode == 'm') {
					if (!isxdigit ((byte) fmt[3])) {
						goto err;
					}
					shift = fmt[3];
					shift = (shift & 0xf)
							+ (((shift & 0x40) >> 3) | ((shift & 0x40) >> 5));
					fmt++; // M/m have one extra item
				}

				switch (opchar) {
					case 'a':
						opreg = PR_BASE_IND (s->op, A);
						opval = get_opval (pr, s->a);
						basic_type = *res->type_encodings[op_type[0]];
						basic_type.basic.width = op_width[0];
						break;
					case 'b':
						opreg = PR_BASE_IND (s->op, B);
						opval = get_opval (pr, s->b);
						basic_type = *res->type_encodings[op_type[1]];
						basic_type.basic.width = op_width[1];
						break;
					case 'c':
						opreg = PR_BASE_IND (s->op, C);
						opval = get_opval (pr, s->c);
						basic_type = *res->type_encodings[op_type[2]];
						basic_type.basic.width = op_width[2];
						break;
					case 'o':
						opreg = 0;
						opval = s->op;
						break;
					case 'x':
						if (mode == 'P') {
							opval = pr->pr_real_params[param_ind]
									- pr->pr_globals;
							break;
						}
						goto err;
					default:
						goto err;
				}
				switch (mode) {
					case 'R':
						optype = &res->void_type;
						aux_func = get_aux_function (pr);
						if (aux_func) {
							optype = get_type (res, aux_func->return_type);
						}
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'F':
						str = global_string (&data, opval, optype,
											 contents & 1);
						func = G_FUNCTION (pr, opval);
						if (func < pr->progs->functions.count) {
							call_func = pr->pr_functions + func;
						}
						break;
					case 'P':
						param_def = PR_Get_Param_Def (pr, call_func, param_ind);
						optype = &res->void_type;
						if (param_def) {
							optype = get_type (res, param_def->type_encoding);
						}
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'V':
						opval += pr->pr_bases[opreg];
						optype = &res->void_type;
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'G':
						opval += pr->pr_bases[opreg];
						str = global_string (&data, opval, optype,
											 contents & 1);
						break;
					case 'g':
						opval += pr->pr_bases[opreg];
						str = global_string (&data, opval, optype, 0);
						break;
					case 's':
						str = dsprintf (res->dva, "%d", (short) opval);
						break;
					case 'H':
						str = hop_string (&data, opval);
						break;
					case 'S':
						str = swizzle_string (&data, opval);
						break;
					case 'X':
						str = extend_string (&data, opval);
						break;
					case 'O':
						str = dsprintf (res->dva, "%04x",
										addr + (short) opval);
						break;
					case 'C':
						str = dsprintf (res->dva, "%03o", opval);
						break;
					case 'E':
						{
							edict_t    *ed = 0;
							opval = G_ENTITY (pr, (pr_ushort_t) s->a);
							param_ind = G_FIELD (pr, (pr_ushort_t) s->b);
							if (param_ind < pr->progs->entityfields
								&& opval > 0
								&& opval < pr->pr_edict_area_size) {
								ed = PROG_TO_EDICT (pr, opval);
								opval = &E_fld(ed, param_ind) - pr->pr_globals;
							}
							if (!ed) {
								str = "bad entity.field";
								break;
							}
							str = global_string (&data, opval, optype,
												 contents & 1);
							str = dsprintf (res->dva, "$%x $%x %s",
											(pr_ushort_t) s->a,
											(pr_ushort_t) s->b, str);
						}
						break;
					case 'M':
					case 'm':
						{
							pr_ptr_t    ptr = 0;
							pr_int_t    offs = 0;
							switch ((opval >> shift) & 3) {
								case 0:
									ptr = s->a + PR_BASE (pr, s, A);
									break;
								case 1:
									break;
								case 2:
									ptr = s->a + PR_BASE (pr, s, A);
									ptr = G_POINTER (pr, ptr);
									offs = s->b;
									break;
								case 3:
									ptr = s->a + PR_BASE (pr, s, A);
									ptr = G_POINTER (pr, ptr);
									offs = s->b + PR_BASE (pr, s, B);
									offs = G_INT (pr, offs);
									break;
							}
							ptr += offs;
							str = global_string (&data, ptr, optype,
												 mode == 'M');
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
	dfunction_t	   *f = frame->func ? frame->func->descriptor : 0;

	if (!f) {
		Sys_Printf ("<NO FUNCTION>\n");
		return;
	}
	if (pr_debug && res->debug) {
		pr_lineno_t *lineno = PR_Find_Lineno (pr, frame->staddr);
		pr_auxfunction_t *func = PR_Get_Lineno_Func (pr, lineno);
		pr_uint_t   line = PR_Get_Lineno_Line (pr, lineno);
		pr_uint_t   addr = PR_Get_Lineno_Addr (pr, lineno);

		line += func->source_line;
		if (addr == frame->staddr) {
			Sys_Printf ("%12s:%u : %s: %x\n",
						PR_GetString (pr, f->file),
						line,
						PR_GetString (pr, f->name),
						frame->staddr);
		} else {
			Sys_Printf ("%12s:%u+%d : %s: %x\n",
						PR_GetString (pr, f->file),
						line, frame->staddr - addr,
						PR_GetString (pr, f->name),
						frame->staddr);
		}
	} else {
		Sys_Printf ("%12s : %s: %x\n", PR_GetString (pr, f->file),
					PR_GetString (pr, f->name), frame->staddr);
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

	top.staddr = pr->pr_xstatement;
	top.func = pr->pr_xfunction;
	dump_frame (pr, &top);
	for (i = pr->pr_depth - 1; i >= 0; i--)
		dump_frame (pr, pr->pr_stack + i);
}

VISIBLE void
PR_Profile (progs_t * pr)
{
	pr_ulong_t  max, num, i;
	pr_ulong_t  total;
	dfunction_t *f;
	bfunction_t *best, *bf;

	num = 0;
	total = 0;
	for (i = 0; i < pr->progs->functions.count; i++) {
		bf = &pr->function_table[i];
		total += bf->profile;
	}
	do {
		max = 0;
		best = NULL;
		for (i = 0; i < pr->progs->functions.count; i++) {
			bf = &pr->function_table[i];
			if (bf->profile > max) {
				max = bf->profile;
				best = bf;
			}
		}
		if (best) {
			if (num < 10) {
				f = pr->pr_functions + (best - pr->function_table);
				Sys_Printf ("%7"PRIu64" %s%s\n", best->profile,
							PR_GetString (pr, f->name),
							f->first_statement < 0 ? " (builtin)" : "");
			}
			num++;
			best->profile = 0;
		}
	} while (best);
	Sys_Printf ("total: %7"PRIu64"\n", total);
}

static void
print_field (progs_t *pr, edict_t *ed, pr_def_t *d)
{
	prdeb_resources_t *res = pr->pr_debug_resources;
	int         l;
	pr_type_t  *v;
	qfot_type_t dummy_type = { };
	qfot_type_t *type;
	pr_debug_data_t data = {pr, res->dstr};
	const char *name;

	name = PR_GetString (pr, d->name);
	type = get_def_type (pr, d, &dummy_type);
	v = &E_fld (ed, d->ofs);

	l = 15 - strlen (name);
	if (l < 1)
		l = 1;

	dstring_clearstr (res->dstr);
	value_string (&data, type, v);
	Sys_Printf ("%s%*s%s\n", name, l, "", res->dstr->str);
}

/*
	ED_Print

	For debugging
*/
VISIBLE void
ED_Print (progs_t *pr, edict_t *ed, const char *fieldname)
{
	pr_uint_t   i, j;
	const char *name;
	pr_def_t   *d;
	int         l;

	if (ed->free) {
		Sys_Printf ("FREE\n");
		return;
	}

	Sys_Printf ("\nEDICT %d:\n", NUM_FOR_BAD_EDICT (pr, ed));

	if (fieldname) {
		d = PR_FindField(pr, fieldname);
		if (!d) {
			Sys_Printf ("unknown field '%s'\n", fieldname);
		} else {
			print_field (pr, ed, d);
		}
		return;
	}
	for (i = 0; i < pr->progs->fielddefs.count; i++) {
		d = &pr->pr_fielddefs[i];
		if (!d->name)					// null field def (probably 1st)
			continue;
		name = PR_GetString (pr, d->name);
		l = strlen (name);
		if (l >= 2 && name[l - 2] == '_' && strchr ("xyz", name[l - 1]))
			continue;					// skip _x, _y, _z vars

		qfot_type_t dummy_type = { };
		get_def_type (pr, d, &dummy_type);
		for (j = 0; j < d->size; j++) {
			if (E_INT (ed, d->ofs + j)) {
				break;
			}
		}
		if (j == d->size) {
			continue;
		}

		print_field (pr, ed, d);
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
	res->va = va_create_context (8);

	res->void_type.meta = ty_basic;
	res->void_type.size = 4;
	res->void_type.encoding = 0;
	res->void_type.type = ev_void;
	for (int i = 0; i < ev_type_count; i++ ) {
		res->type_encodings[i] = &res->void_type;
	}
	res->file_hash = Hash_NewTable (509, file_get_key, file_free, 0,
									pr->hashctx);
	res->debug_syms = Hash_NewTable (509, def_get_key, 0, pr, pr->hashctx);
	res->compunits = Hash_NewTable (509, compunit_get_key, 0, pr, pr->hashctx);

	PR_Resources_Register (pr, "PR_Debug", res, pr_debug_clear,
						   pr_debug_destroy);
	pr->pr_debug_resources = res;
}

static void
pr_debug_shutdown (void *data)
{
	free (source_paths);
	free (source_path_string);
}

void
PR_Debug_Init_Cvars (void)
{
	Sys_RegisterShutdown (pr_debug_shutdown, 0);
	Cvar_Register (&pr_debug_cvar, 0, 0);
	Cvar_Register (&pr_source_path_cvar, source_path_f, 0);
}
