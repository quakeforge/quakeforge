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

	$Id$
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
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/pr_debug.h"
#include "QF/progs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vfs.h"
#include "QF/zone.h"

typedef struct {
	char *text;
	size_t len;
} line_t;

typedef struct {
	char *name;
	char *text;
	line_t *lines;
	int num_lines;
} file_t;

cvar_t      *pr_debug;
cvar_t      *pr_source_path;
static hashtab_t  *file_hash;

file_t *
PR_Load_Source_File (progs_t *pr, const char *fname)
{
	file_t *f = Hash_Find (file_hash, fname);
	char *l;
	char *path;

	if (f)
		return f;
	f = malloc (sizeof (file_t));
	if (!f)
		return 0;
	path = Hunk_TempAlloc (strlen (pr_source_path->string) + strlen (fname) + 2);
	sprintf (path, "%s/%s", pr_source_path->string, fname);
	f->text = COM_LoadFile (path, 0);
	if (!f->text) {
		free (f);
		return 0;
	}
	for (f->num_lines = 1, l = f->text; *l; l++)
		if (*l == '\n')
			f->num_lines++;
	f->name = strdup (fname);
	if (!f->name) {
		free (f->text);
		free (f);
		return 0;
	}
	f->lines = malloc (f->num_lines * sizeof (line_t));
	if (!f->lines) {
		free (f->name);
		free (f->text);
		free (f);
		return 0;
	}
	f->lines[0].text = f->text;
	for (f->num_lines = 0, l = f->text; *l; l++) {
		if (*l == '\n') {
			f->lines[f->num_lines].len = l - f->lines[f->num_lines].text;
			f->lines[++f->num_lines].text = l + 1;
		}
	}
	f->lines[f->num_lines++].len = l - f->lines[f->num_lines].text;
	Hash_Add (file_hash, f);
	return f;
}

void
PR_LoadDebug (progs_t *pr)
{
	pr_type_t  *str = 0;
	int         start = Hunk_LowMark ();
	int         i;
	char       *path_end;
	char       *sym_file;
	char       *sym_path;
	ddef_t     *def;

	def = PR_FindGlobal (pr, ".debug_file");
	if (def)
		str = &pr->pr_globals[def->ofs];

	Hash_FlushTable (file_hash);
	if (!str)
		return;
	pr->debugfile = PR_GetString (pr, str->string_var);
	sym_file = COM_SkipPath (pr->debugfile);
	path_end = COM_SkipPath (pr->progs_name);
	sym_path = Hunk_TempAlloc (strlen (sym_file) + (path_end - pr->progs_name) + 1);
	strncpy (sym_path, pr->progs_name, path_end - pr->progs_name);
	strcpy (sym_path + (path_end - pr->progs_name), sym_file);
	pr->debug = (pr_debug_header_t*)COM_LoadHunkFile (sym_path);
	if (!pr->debug) {
		Sys_Printf ("can't load %s for debug info\n", sym_path);
		return;
	}
	pr->debug->version = LittleLong (pr->debug->version);
	if (pr->debug->version != PROG_DEBUG_VERSION) {
		Sys_Printf ("ignoring %s with unsupported version %x.%03x.%03x\n",
					sym_path,
					(pr->debug->version >> 24) & 0xff,
					(pr->debug->version >> 12) & 0xfff,
					pr->debug->version & 0xfff);
		Hunk_FreeToLowMark (start);
		pr->debug = 0;
		pr->auxfunctions = 0;
		pr->linenos = 0;
		pr->local_defs = 0;
		return;
	}
	pr->debug->crc = LittleShort (pr->debug->crc);
	if (pr->debug->crc != pr->crc) {
		Sys_Printf ("ignoring %s that doesn't match %s. (crcs: sys:%d dat:%d)",
					sym_path,
					pr->progs_name,
					pr->debug->crc,
					pr->crc);
		Hunk_FreeToLowMark (start);
		pr->debug = 0;
		pr->auxfunctions = 0;
		pr->linenos = 0;
		pr->local_defs = 0;
		return;
	}
	pr->debug->you_tell_me_and_we_will_both_know = LittleShort (pr->debug->you_tell_me_and_we_will_both_know);
	pr->debug->auxfunctions = LittleLong (pr->debug->auxfunctions);
	pr->debug->num_auxfunctions = LittleLong (pr->debug->num_auxfunctions);
	pr->debug->linenos = LittleLong (pr->debug->linenos);
	pr->debug->num_linenos = LittleLong (pr->debug->num_linenos);
	pr->debug->locals = LittleLong (pr->debug->locals);
	pr->debug->num_locals = LittleLong (pr->debug->num_locals);

	pr->auxfunctions = (pr_auxfunction_t*)((char*)pr->debug + pr->debug->auxfunctions);
	pr->linenos = (pr_lineno_t*)((char*)pr->debug + pr->debug->linenos);
	pr->local_defs = (ddef_t*)((char*)pr->debug + pr->debug->locals);

	for (i = 0; i < pr->debug->num_auxfunctions; i++) {
		pr->auxfunctions[i].function = LittleLong (pr->auxfunctions[i].function);
		pr->auxfunctions[i].source_line = LittleLong (pr->auxfunctions[i].source_line);
		pr->auxfunctions[i].line_info = LittleLong (pr->auxfunctions[i].line_info);
		pr->auxfunctions[i].local_defs = LittleLong (pr->auxfunctions[i].local_defs);
		pr->auxfunctions[i].num_locals = LittleLong (pr->auxfunctions[i].num_locals);
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

unsigned long
PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno)
{
	pr_auxfunction_t *f;

	if (lineno->line)
		return lineno->fa.addr;
	f = &pr->auxfunctions[lineno->fa.func];
	return pr->pr_functions[f->function].first_statement;
}

unsigned long
PR_Get_Lineno_Line (progs_t *pr, pr_lineno_t *lineno)
{
	pr_auxfunction_t *f;

	if (lineno->line)
		return lineno->line;
	f = &pr->auxfunctions[lineno->fa.func];
	return f->source_line;
}

pr_lineno_t *
PR_Find_Lineno (progs_t *pr, unsigned long addr)
{
	pr_lineno_t *lineno = 0;
	int i;

	if (!pr->debug)
		return 0;
	if (!pr->debug->num_linenos)
		return 0;
	for (i = pr->debug->num_linenos - 1; i >= 0; i--) {
		if (PR_Get_Lineno_Addr (pr, &pr->linenos[i]) <= addr) {
			lineno = &pr->linenos[i];
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
	return PR_GetString(pr, pr->pr_functions[f->function].s_file);
}

const char *
PR_Get_Source_Line (progs_t *pr, unsigned long addr)
{
	pr_auxfunction_t *func;
	const char *fname;
	unsigned long line;
	char       *str;
	file_t     *file;

	pr_lineno_t *lineno = PR_Find_Lineno (pr, addr);
	if (!lineno || PR_Get_Lineno_Addr (pr, lineno) != addr)
		return 0;
	func = PR_Get_Lineno_Func (pr, lineno);
	fname = PR_Get_Source_File (pr, lineno);
	if (!func || !fname)
		return 0;
	line = PR_Get_Lineno_Line (pr, lineno);
	line += func->source_line;

	file = PR_Load_Source_File (pr, fname);

	str = Hunk_TempAlloc (strlen (fname) + 12);
	sprintf (str, "%s:%ld", fname, line);

	if (!file || line > file->num_lines)
		return str;

	str = Hunk_TempAlloc (strlen (str) + file->lines[line - 1].len + 2);
	sprintf (str, "%s:%ld:%.*s", fname, line,
			 (int)file->lines[line - 1].len, file->lines[line - 1].text);
	return str;
}

static const char *
file_get_key (void *_f, void *unused)
{
	return ((file_t*)_f)->name;
}

static void
file_free (void *_f, void *unused)
{
	file_t *f = (file_t*)_f;
	free (f->lines);
	free (f->text);
	free (f->name);
	free (f);
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
	pr_source_path = Cvar_Get ("pr_source_path", ".", CVAR_NONE, NULL,
							   "where to look (within gamedir) for source "
							   "files");
}
