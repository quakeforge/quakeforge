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
#include "QF/va.h"
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

cvar_t         *pr_debug;
cvar_t         *pr_source_path;
static hashtab_t   *file_hash;
static char        *source_path_string;
static char       **source_paths;


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

	if (source_path_string)
		free (source_path_string);
	source_path_string = strdup (var->string);
	if (source_paths)
		free (source_paths);
	for (i = 2, s = source_path_string; *s; s++)
		if (*s == ';')
			i++;
	source_paths = malloc (i * sizeof (char *));
	source_paths[0] = source_path_string;
	for (i = 1, s = source_path_string; *s; s++)
		if (*s == ';') {
			*s++ = 0;
			source_paths[i++] = s;
		}
	source_paths[i] = 0;
}

static void
pr_debug_expression_error (script_t *script, const char *msg)
{
	Sys_Printf ("%s\n", msg);
}

static ddef_t
parse_expression (progs_t *pr, const char *expr, int conditional)
{
	script_t   *es;
	char       *e;
	pr_type_t  *expr_ptr;
	ddef_t      d;

	d.ofs = 0;
	d.type = ev_invalid;
	d.s_name = 0;
	es = Script_New ();
	es->error = pr_debug_expression_error;
	Script_Start (es, "<console>", expr);
	expr_ptr = 0;
	es->single = "{}()':[].";
	if (Script_GetToken (es, 1)) {
		if (strequal (es->token->str, "[")) {
			edict_t    *ent;
			ddef_t     *field;

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
			ddef_t     *global = PR_FindGlobal (pr, es->token->str);
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
	Script_Delete (es);
	return d;
}

void
PR_Debug_Init (void)
{
	file_hash = Hash_NewTable (1024, file_get_key, file_free, 0);
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

static file_t *
PR_Load_Source_File (progs_t *pr, const char *fname)
{
	char       *l, *p, **dir;
	file_t     *f = Hash_Find (file_hash, fname);

	if (f)
		return f;
	f = calloc (1, sizeof (file_t));
	if (!f)
		return 0;
	for (dir = source_paths; *dir && !f->text; dir++) {
		f->text = pr->load_file (pr, va ("%s%s%s", *dir, **dir ? "/" : "",
										 fname),
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
	char       *sym_path;
	const char *path_end, *sym_file;
	off_t       debug_size;
	pr_uint_t   i;
	ddef_t	   *def;
	pr_type_t  *str = 0;

	if (pr->debug)
		pr->free_progs_mem (pr, pr->debug);
	pr->debug = 0;
	pr->auxfunctions = 0;
	if (pr->auxfunction_map)
		pr->free_progs_mem (pr, pr->auxfunction_map);
	pr->auxfunction_map = 0;
	pr->linenos = 0;
	pr->local_defs = 0;

	if (!pr_debug->int_val)
		return 1;

	def = PR_FindGlobal (pr, ".debug_file");
	if (def)
		str = &pr->pr_globals[def->ofs];

	Hash_FlushTable (file_hash);
	if (!str)
		return 1;
	pr->debugfile = PR_GetString (pr, str->string_var);
	sym_file = QFS_SkipPath (pr->debugfile);
	path_end = QFS_SkipPath (pr->progs_name);
	sym_path = malloc (strlen (sym_file) + (path_end - pr->progs_name) + 1);
	strncpy (sym_path, pr->progs_name, path_end - pr->progs_name);
	strcpy (sym_path + (path_end - pr->progs_name), sym_file);
	pr->debug = pr->load_file (pr, sym_path, &debug_size);
	if (!pr->debug) {
		Sys_Printf ("can't load %s for debug info\n", sym_path);
		free (sym_path);
		return 1;
	}
	pr->debug->version = LittleLong (pr->debug->version);
	if (pr->debug->version != PROG_DEBUG_VERSION) {
		Sys_Printf ("ignoring %s with unsupported version %x.%03x.%03x\n",
					sym_path,
					(pr->debug->version >> 24) & 0xff,
					(pr->debug->version >> 12) & 0xfff,
					pr->debug->version & 0xfff);
		pr->debug = 0;
		free (sym_path);
		return 1;
	}
	pr->debug->crc = LittleShort (pr->debug->crc);
	if (pr->debug->crc != pr->crc) {
		Sys_Printf ("ignoring %s that doesn't match %s. (CRCs: "
					"sym:%d dat:%d)\n",
					sym_path,
					pr->progs_name,
					pr->debug->crc,
					pr->crc);
		pr->debug = 0;
		free (sym_path);
		return 1;
	}
	free (sym_path);
	pr->debug->you_tell_me_and_we_will_both_know = LittleShort
		(pr->debug->you_tell_me_and_we_will_both_know);
	pr->debug->auxfunctions = LittleLong (pr->debug->auxfunctions);
	pr->debug->num_auxfunctions = LittleLong (pr->debug->num_auxfunctions);
	pr->debug->linenos = LittleLong (pr->debug->linenos);
	pr->debug->num_linenos = LittleLong (pr->debug->num_linenos);
	pr->debug->locals = LittleLong (pr->debug->locals);
	pr->debug->num_locals = LittleLong (pr->debug->num_locals);

	pr->auxfunctions = (pr_auxfunction_t*)((char*)pr->debug +
										   pr->debug->auxfunctions);
	pr->linenos = (pr_lineno_t*)((char*)pr->debug + pr->debug->linenos);
	pr->local_defs = (ddef_t*)((char*)pr->debug + pr->debug->locals);

	i = pr->progs->numfunctions * sizeof (pr_auxfunction_t *);
	pr->auxfunction_map = pr->allocate_progs_mem (pr, i);
	for (i = 0; (int) i < pr->progs->numfunctions; i++) //FIXME (cast)
		pr->auxfunction_map[i] = 0;

	for (i = 0; i < pr->debug->num_auxfunctions; i++) {
		pr->auxfunctions[i].function = LittleLong
			(pr->auxfunctions[i].function);
		pr->auxfunctions[i].source_line = LittleLong
			(pr->auxfunctions[i].source_line);
		pr->auxfunctions[i].line_info = LittleLong
			(pr->auxfunctions[i].line_info);
		pr->auxfunctions[i].local_defs = LittleLong
			(pr->auxfunctions[i].local_defs);
		pr->auxfunctions[i].num_locals = LittleLong
			(pr->auxfunctions[i].num_locals);

		pr->auxfunction_map[pr->auxfunctions[i].function] =
			&pr->auxfunctions[i];
	}
	for (i = 0; i < pr->debug->num_linenos; i++) {
		pr->linenos[i].fa.func = LittleLong (pr->linenos[i].fa.func);
		pr->linenos[i].line = LittleLong (pr->linenos[i].line);
	}
	for (i = 0; i < pr->debug->num_locals; i++) {
		pr->local_defs[i].type = LittleShort (pr->local_defs[i].type);
		pr->local_defs[i].ofs = LittleShort (pr->local_defs[i].ofs);
		pr->local_defs[i].s_name = LittleLong (pr->local_defs[i].s_name);
	}
	return 1;
}

pr_auxfunction_t *
PR_Get_Lineno_Func (progs_t *pr, pr_lineno_t *lineno)
{
	while (lineno > pr->linenos && lineno->line)
		lineno--;
	if (lineno->line)
		return 0;
	return &pr->auxfunctions[lineno->fa.func];
}

pr_uint_t
PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno)
{
	pr_auxfunction_t *f;

	if (lineno->line)
		return lineno->fa.addr;
	if (lineno->fa.func < pr->debug->num_auxfunctions) {
		f = &pr->auxfunctions[lineno->fa.func];
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
	pr_uint_t   i;
	pr_lineno_t *lineno = 0;

	if (!pr->debug)
		return 0;
	if (!pr->debug->num_linenos)
		return 0;
	for (i = pr->debug->num_linenos; i > 0; i--) {
		if (PR_Get_Lineno_Addr (pr, &pr->linenos[i - 1]) <= addr) {
			lineno = &pr->linenos[i - 1];
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
		return va ("%s:%u", fname, line);

	return va ("%s:%u:%.*s", fname, line, (int)file->lines[line - 1].len,
			   file->lines[line - 1].text);
}

ddef_t *
PR_Get_Param_Def (progs_t *pr, dfunction_t *func, unsigned parm)
{
	pr_uint_t    i;
	pr_auxfunction_t *aux_func;
	ddef_t      *ddef = 0;
	int          num_params;
	int          param_offs = 0;

	if (!pr->debug)
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

	aux_func = pr->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;

	for (i = 0; i < aux_func->num_locals; i++) {
		ddef = &pr->local_defs[aux_func->local_defs + param_offs + i];
		if (!parm--)
			break;
	}
	return ddef;
}

static pr_auxfunction_t *
get_aux_function (progs_t *pr)
{
	dfunction_t *func;
	if (!pr->pr_xfunction || !pr->auxfunction_map)
		return 0;
	func = pr->pr_xfunction->descriptor;
	return pr->auxfunction_map[func - pr->pr_functions];
}

static etype_t
get_etype (progs_t *pr, int typeptr)
{
	//FIXME cache .type_encodings def
	ddef_t     *te_def = PR_FindGlobal (pr, ".type_encodings");
	qfot_type_encodings_t *encodings;
	qfot_type_t *type;

	if (!te_def) {
		// can't decode the type, so make no assumptions about it
		return typeptr;
	}
	encodings = &G_STRUCT (pr, qfot_type_encodings_t, te_def->ofs);
	type = &G_STRUCT (pr, qfot_type_t, encodings->types + typeptr);
	if (type->meta == 0) {
		return type->t.type;
	}
	return ev_void;
}

ddef_t *
PR_Get_Local_Def (progs_t *pr, pr_int_t offs)
{
	pr_uint_t   i;
	dfunction_t *func;
	pr_auxfunction_t *aux_func;

	if (!pr->pr_xfunction)
		return 0;
	func = pr->pr_xfunction->descriptor;
	if (!func)
		return 0;
	aux_func = pr->auxfunction_map[func - pr->pr_functions];
	if (!aux_func)
		return 0;
	offs -= func->parm_start;
	if (offs < 0 || offs >= func->locals)
		return 0;
	for (i = 0; i < aux_func->num_locals; i++)
		if (pr->local_defs[aux_func->local_defs + i].ofs == offs)
			return &pr->local_defs[aux_func->local_defs + i];
	return 0;
}

VISIBLE void
PR_DumpState (progs_t *pr)
{
	if (pr->pr_xfunction) {
		if (pr_debug->int_val && pr->debug) {
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
value_string (progs_t *pr, etype_t type, pr_type_t *val)
{
	static dstring_t *line;
	ddef_t     *def;
	pr_int_t    ofs;
	edict_t    *edict;
	dfunction_t	*f;
	const char *str;

	if (!line)
		line = dstring_new ();

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			if (!PR_StringValid (pr, val->string_var))
				return "*** invalid ***";
			str = PR_GetString (pr, val->string_var);
			dstring_copystr (line, "\"");
			while (*str) {
				const char *s;

				for (s = str; *s && !strchr ("\"\n\t", *s); s++)
					;
				if (s != str)
					dstring_appendsubstr (line, str, s - str);
				if (*s) {
					switch (*s) {
						case '\"':
							dstring_appendstr (line, "\\\"");
							break;
						case '\n':
							dstring_appendstr (line, "\\n");
							break;
						case '\t':
							dstring_appendstr (line, "\\t");
							break;
						default:
							dasprintf (line, "\\x%02x", *s & 0xff);
					}
					s++;
				}
				str = s;
			}
			dstring_appendstr (line, "\"");
			break;
		case ev_entity:
			edict = PROG_TO_EDICT (pr, val->entity_var);
			dsprintf (line, "entity %d", NUM_FOR_BAD_EDICT (pr, edict));
			break;
		case ev_func:
			if (val->func_var < 0 || val->func_var >= pr->progs->numfunctions)
				dsprintf (line, "INVALID:%d", val->func_var);
			else if (!val->func_var)
				return "NULL";
			else {
				f = pr->pr_functions + val->func_var;
				dsprintf (line, "%s()", PR_GetString (pr, f->s_name));
			}
			break;
		case ev_field:
			def = PR_FieldAtOfs (pr, val->integer_var);
			if (def)
				dsprintf (line, ".%s", PR_GetString (pr, def->s_name));
			else
				dsprintf (line, ".<$%04x>", val->integer_var);
			break;
		case ev_void:
			return "void";
		case ev_float:
			if (ISDENORM (val->integer_var) && val->uinteger_var != 0x80000000)
				dsprintf (line, "<%08x>", val->integer_var);
			else
				dsprintf (line, "%g", val->float_var);
			break;
		case ev_vector:
			dsprintf (line, "'%g %g %g'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2]);
			break;
		case ev_pointer:
			def = 0;
			ofs = val->integer_var;
			if (pr_debug->int_val && pr->debug)
				def = PR_Get_Local_Def (pr, ofs);
			if (!def)
				def = PR_GlobalAtOfs (pr, ofs);
			if (def && def->s_name)
				dsprintf (line, "&%s", PR_GetString (pr, def->s_name));
			else
				dsprintf (line, "[$%x]", ofs);
			break;
		case ev_quat:
			dsprintf (line, "'%g %g %g %g'",
					  val->vector_var[0], val->vector_var[1],
					  val->vector_var[2], val->vector_var[3]);
			break;
		case ev_integer:
			dsprintf (line, "%d", val->integer_var);
			break;
		case ev_uinteger:
			dsprintf (line, "$%08x", val->uinteger_var);
			break;
		case ev_double:
			dsprintf (line, "%g", *(double *)val);
			break;
		case ev_short:
		case ev_invalid:
		case ev_type_count:
			//dsprintf (line, "bad type %i", type);
			dsprintf (line, "<%x %x %x %x>",
					  val[0].integer_var, val[1].integer_var,
					  val[2].integer_var, val[3].integer_var);
			break;
	}

	return line->str;
}

static ddef_t *
def_string (progs_t *pr, pr_int_t ofs, dstring_t *dstr)
{
	ddef_t     *def = 0;
	const char *name;

	if (pr_debug->int_val && pr->debug)
		def = PR_Get_Local_Def (pr, ofs);
	if (!def)
		def = PR_GlobalAtOfs (pr, ofs);
	if (!def || !*(name = PR_GetString (pr, def->s_name)))
		dsprintf (dstr, "[$%x]", ofs);
	else
		dsprintf (dstr, "%s", name);
	return def;
}

static const char *
global_string (progs_t *pr, pointer_t ofs, etype_t type, int contents)
{
	static dstring_t *line = NULL;
	ddef_t     *def = NULL;
	const char *s;

	if (!line)
		line = dstring_newstr();

	if (type == ev_short) {
		dsprintf (line, "%04x", (short) ofs);
		return line->str;
	}

	def = def_string (pr, ofs, line);

	if (contents && (def || type != ev_void)) {
		const char *oi = "";
		if (def) {
			if (type == ev_void)
				type = def->type;
			if (type != (etype_t) (def->type & ~DEF_SAVEGLOBAL))
				oi = "?";
		}

		if (ofs > pr->globals_size)
			s = "Out of bounds";
		else
			s = value_string (pr, type, &pr->pr_globals[ofs]);

		if (strequal(line->str, "IMMEDIATE") || strequal(line->str, ".imm")) {
			dsprintf (line, "%s", s);
		} else {
			dasprintf (line, "%s(%s)", oi, s);
		}
	}
	return line->str;
}

VISIBLE void
PR_Debug_Watch (progs_t *pr, const char *expr)
{
	ddef_t      watch;
	if (!expr) {
		Sys_Printf ("watch <watchpoint expr>\n");
		if (pr->watch) {
			Sys_Printf ("    watching [%d]\n",
						(int) (intptr_t) (pr->watch - pr->pr_globals));
			if (pr->wp_conditional)
				Sys_Printf ("        if new val == %d\n",
							pr->wp_val.integer_var);
		} else {
			Sys_Printf ("    none active\n");
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
	ddef_t      print;

	if (!expr) {
		Sys_Printf ("print <print expr>\n");
		return;
	}

	print = parse_expression (pr, expr, 0);
	if (print.type != ev_invalid) {
		const char *s = global_string (pr, print.ofs, print.type, 1);
		Sys_Printf ("[%d] = %s\n", print.ofs, s);
	}
}

VISIBLE void
PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents)
{
	int         addr = s - pr->pr_statements;
	int         dump_code = contents & 2;
	const char *fmt;
	opcode_t   *op;
	static dstring_t *line;
	dfunction_t *call_func = 0;
	ddef_t     *parm_def = 0;
	pr_auxfunction_t *aux_func = 0;

	if (!line)
		line = dstring_new ();

	dstring_clearstr (line);

	if (pr_debug->int_val > 1)
		dump_code = 1;

	if (pr_debug->int_val && pr->debug) {
		const char *source_line = PR_Get_Source_Line (pr, addr);

		if (source_line) {
			dasprintf (line, "%s%s", source_line, dump_code ? "\n" : "");
			if (!dump_code)
				goto do_print;
		}
		if (!dump_code)
			return;
	}

	op = PR_Opcode (s->op);
	if (!op) {
		Sys_Printf ("%sUnknown instruction %d\n", line->str, s->op);
		return;
	}

	if (!(fmt = op->fmt))
		fmt = "%Ga, %Gb, %gc";

	dasprintf (line, "%04x ", addr);
	if (pr_debug->int_val > 2)
		dasprintf (line, "%02x %04x(%8s) %04x(%8s) %04x(%8s)\t",
					s->op,
					s->a, pr_type_name[op->type_a],
					s->b, pr_type_name[op->type_b],
					s->c, pr_type_name[op->type_c]);

	dasprintf (line, "%s ", op->opname);

	while (*fmt) {
		if (*fmt == '%') {
			if (fmt[1] == '%') {
				dstring_appendsubstr (line, fmt + 1, 1);
				fmt += 2;
			} else {
				const char *str;
				char        mode = fmt[1], opchar = fmt[2];
				unsigned    parm_ind = 0;
				pr_int_t    opval;
				etype_t     optype = ev_void;

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
						str = global_string (pr, opval, optype, contents & 1);
						break;
					case 'F':
						str = global_string (pr, opval, optype, contents & 1);
						if (G_FUNCTION (pr, opval) >= 0
							&& G_FUNCTION (pr, opval)
								< pr->progs->numfunctions)
						call_func = pr->pr_functions + G_FUNCTION (pr, opval);
						break;
					case 'P':
						parm_def = PR_Get_Param_Def (pr, call_func, parm_ind);
						optype = ev_void;
						if (parm_def)
							optype = parm_def->type;
						str = global_string (pr, opval, optype, contents & 1);
						break;
					case 'V':
						str = global_string (pr, opval, ev_void, contents & 1);
						break;
					case 'G':
						str = global_string (pr, opval, optype, contents & 1);
						break;
					case 'g':
						str = global_string (pr, opval, optype, 0);
						break;
					case 's':
						str = va ("%d", (short) opval);
						break;
					case 'O':
						str = va ("%04x", addr + (short) opval);
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
							str = global_string (pr, opval, optype,
												 contents & 1);
							str = va ("$%x $%x %s", s->a, s->b, str);
						}
						break;
					default:
						goto err;
				}
				dstring_appendstr (line, str);
				fmt += 3;
				continue;
			err:
				dstring_appendstr (line, fmt);
				break;
			}
		} else {
			dstring_appendsubstr (line, fmt++, 1);
		}
	}
do_print:
	Sys_Printf ("%s\n", line->str);
}

static void
dump_frame (progs_t *pr, prstack_t *frame)
{
	dfunction_t	   *f = frame->f ? frame->f->descriptor : 0;

	if (!f) {
		Sys_Printf ("<NO FUNCTION>\n");
		return;
	}
	if (pr_debug->int_val && pr->debug) {
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
	pr_int_t    max, num, i;
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
	int         type, l;
	pr_uint_t   i;
	const char *name;
	ddef_t     *d;
	pr_type_t  *v;

	if (ed->free) {
		Sys_Printf ("FREE\n");
		return;
	}

	Sys_Printf ("\nEDICT %d:\n", NUM_FOR_BAD_EDICT (pr, ed));
	for (i = 0; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		if (!d->s_name)					// null field def (probably 1st)
			continue;
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_'
			&& strchr ("xyz", name[strlen (name) -1]))
			continue;					// skip _x, _y, _z vars

		v = ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		switch (type) {
			case ev_entity:
			case ev_integer:
			case ev_uinteger:
			case ev_pointer:
			case ev_func:
			case ev_field:
				if (!v->integer_var)
					continue;
				break;
			case ev_string:
				if (PR_StringValid (pr, v->string_var))
					if (!PR_GetString (pr, v->string_var)[0])
						continue;
				break;
			case ev_float:
				if (!v->float_var)
					continue;
				break;
			case ev_vector:
				if (!v[0].float_var && !v[1].float_var && !v[2].float_var)
					continue;
				break;
			case ev_void:
				break;
			default:
				PR_Error (pr, "ED_Print: Unhandled type %d", type);
		}

		l = 15 - strlen (name);
		if (l < 1)
			l = 1;
		Sys_Printf ("%s%*s%s\n", name, l, "", value_string (pr, d->type, v));
	}
}
