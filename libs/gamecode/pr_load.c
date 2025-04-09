/*
	pr_edict.c

	entity dictionary

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "QF/progs/pr_type.h"

#include "compat.h"

VISIBLE int pr_boundscheck;
static cvar_t pr_boundscheck_cvar = {
	.name = "pr_boundscheck",
	.description =
		"Server progs bounds checking",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pr_boundscheck },
};
int pr_deadbeef_ents;
static cvar_t pr_deadbeef_ents_cvar = {
	.name = "pr_deadbeef_ents",
	.description =
		"set to clear unallocated memory to 0xdeadbeef",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pr_deadbeef_ents },
};
int pr_deadbeef_locals;
static cvar_t pr_deadbeef_locals_cvar = {
	.name = "pr_deadbeef_locals",
	.description =
		"set to clear uninitialized local vars to 0xdeadbeef",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pr_deadbeef_locals },
};
int pr_faultchecks;
static cvar_t pr_faultchecks_cvar = {
	.name = "pr_faultchecks",
	.description =
		"capture and handle division by 0 in progs",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pr_faultchecks },
};

static const char *
function_get_key (const void *f, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	dfunction_t *func = (dfunction_t*)f;
	return PR_GetString (pr, func->name);
}

static const char *
var_get_key (const void *d, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	pr_def_t *def = (pr_def_t*)d;
	return PR_GetString (pr, def->name);
}

static const char *
type_get_key (const void *t, void *_pr)
{
	progs_t    *pr = (progs_t *) _pr;
	__auto_type type = (qfot_type_t *) t;
	return PR_GetString (pr, type->encoding);
}

static void
file_error (progs_t *pr, const char *path)
{
	Sys_Printf ("failed to load %s\n", path);
}

static void *
load_file (progs_t *pr, const char *path, off_t *size)
{
	void *data = QFS_LoadHunkFile (QFS_FOpenFile (path));
	*size = qfs_filesize;
	return data;
}

static void *
allocate_progs_mem (progs_t *pr, int size)
{
	return Hunk_AllocName (0, size, pr->progs_name);
}

static void
free_progs_mem (progs_t *pr, void *mem)
{
}

static int
align_size (int size, int align)
{
	size += align - 1;
	size &= ~(align - 1);
	return size;
}

VISIBLE void
PR_LoadProgsFile (progs_t *pr, QFile *file, int size)
{
	qfZoneScoped (true);
	size_t      i;
	int         mem_size;
	int         offset_tweak;
	dprograms_t progs;
	byte       *base;
	byte       *heap;
	ddef_t     *global_ddefs;
	ddef_t     *field_ddefs;
	// absolute minimum alignment is 4 bytes
	int         edict_alignment = __alignof__(pr_lvec4_t);

	if (!pr->file_error)
		pr->file_error = file_error;
	if (!pr->load_file)
		pr->load_file = load_file;
	if (!pr->allocate_progs_mem)
		pr->allocate_progs_mem = allocate_progs_mem;
	if (!pr->free_progs_mem)
		pr->free_progs_mem = free_progs_mem;

	PR_Resources_Clear (pr);
	if (pr->progs)
		pr->free_progs_mem (pr, pr->progs);
	pr->progs = 0;

	if (Qread (file, &progs, sizeof (progs)) != sizeof (progs))
		PR_Error (pr, "error reading header");

	// store prog crc
	pr->crc = CRC_Block ((byte*)&progs, sizeof (progs));

	pr->denorm_found = 0;

	// byte swap the header
	for (i = 0; i < sizeof (progs) / 4; i++)
		((int *) &progs)[i] = LittleLong (((int *) &progs)[i]);

	if (progs.version != PROG_VERSION
		&& progs.version != PROG_V6P_VERSION
		&& progs.version != PROG_ID_VERSION) {
		if (progs.version < 0x00fff000) {
			PR_Error (pr, "%s has unrecognised version number (%u)",
					  pr->progs_name, progs.version);
		} else {
			PR_Error (pr,
					  "%s has unrecognised version number (%02x.%03x.%03x)"
					  " [%02x.%03x.%03x expected]",
					  pr->progs_name,
					  progs.version >> 24,
					  (progs.version >> 12) & 0xfff,
					  progs.version & 0xfff,
					  PROG_VERSION >> 24,
					  (PROG_VERSION >> 12) & 0xfff,
					  PROG_VERSION & 0xfff);
		}
	}
	if (progs.version == PROG_VERSION) {
		// ensure SIMD types can be aligned (qfcc will align them within
		// the progs memory map, so the engine needs to ensure the progs memory
		// is aligned)
		edict_alignment = __alignof__(pr_lvec4_t);
	}
	// edict size is in words
	edict_alignment /= 4;

	// Some compilers (eg, FTE) put extra data between the header and the
	// strings section. What's worse, they de-align the data.
	offset_tweak = progs.strings.offset % sizeof (pr_int_t);
	offset_tweak = (sizeof (pr_int_t) - offset_tweak) % sizeof (pr_int_t);

	// size of progs themselves
	pr->progs_size = size + offset_tweak;
	Sys_MaskPrintf (SYS_dev, "Programs occupy %iK.\n", size / 1024);

	pr->progs_size = align_size (pr->progs_size, 64);
	pr->zone_size = align_size (pr->zone_size, 64);
	pr->stack_size = align_size (pr->stack_size, 64);

	// size of edict asked for by progs, but at least 1
	pr->pr_edict_size = max (1, progs.entityfields);
	pr->pr_edict_size = align_size (pr->pr_edict_size, edict_alignment);
	pr->pr_edict_area_size = pr->max_edicts * pr->pr_edict_size;
	pr->pr_edict_area_size = align_size (pr->pr_edict_area_size, 64);

	pr->edict_parse = 0;

	mem_size = pr->pr_edict_area_size * sizeof (pr_type_t);
	mem_size += pr->progs_size + pr->zone_size + pr->stack_size;
	// space for return buffer
	mem_size += PR_MAX_RETURN * sizeof (pr_type_t);

	// +1 for a nul terminator
	pr->progs = pr->allocate_progs_mem (pr, mem_size + 1);
	if (!pr->progs)
		return;
	// Place a nul at the end of progs memory to ensure any unterminated
	// strings within progs memory don't run off the end.
	((byte *) pr->progs)[mem_size] = 0;

	memcpy (pr->progs, &progs, sizeof (progs));
	base = (byte *) (pr->progs + 1) + offset_tweak;
	Qread (file, base, size - sizeof (progs));
	CRC_ProcessBlock (base, &pr->crc, size - sizeof (progs));

	pr->pr_edict_area = (pr_type_t *)((byte *) pr->progs + pr->progs_size);

	base -= sizeof (progs);	// offsets are from file start
	heap = (byte *) &pr->pr_edict_area[pr->pr_edict_area_size];

	pr->zone = 0;
	if (pr->zone_size) {
		//FIXME zone_size needs to be at least as big as memzone_t, but
		//memzone_t is opaque so its size is unknown
		pr->zone = (memzone_t *) heap;
	}

	pr->pr_functions = (dfunction_t *) (base + pr->progs->functions.offset);
	pr->pr_strings = (char *) base + pr->progs->strings.offset;
	pr->pr_stringsize = (heap - base) + pr->zone_size;
	global_ddefs = (ddef_t *) (base + pr->progs->globaldefs.offset);
	field_ddefs = (ddef_t *) (base + pr->progs->fielddefs.offset);
	pr->pr_statements = (dstatement_t *) (base + pr->progs->statements.offset);

	pr->pr_globals = (pr_type_t *) (base + pr->progs->globals.offset);
	pr->stack = (pr_type_t *) (heap + pr->zone_size);
	pr->stack_bottom = pr->stack - pr->pr_globals;
	pr->globals_size = (pr_type_t *) ((byte *) pr->stack + pr->stack_size)
						- pr->pr_globals;
	if (pr->globals.stack && pr->stack_bottom) {
		*pr->globals.stack = pr->globals_size;
	}
	pr->pr_return_buffer = pr->pr_globals + pr->globals_size;

	if (pr->zone) {
		PR_Zone_Init (pr);
	}

	Hash_FlushTable (pr->function_hash);
	Hash_FlushTable (pr->global_hash);
	Hash_FlushTable (pr->field_hash);
	Hash_FlushTable (pr->type_hash);

// byte swap the lumps
	for (i = 0; i < pr->progs->statements.count; i++) {
		pr->pr_statements[i].op = LittleShort (pr->pr_statements[i].op);
		pr->pr_statements[i].a = LittleShort (pr->pr_statements[i].a);
		pr->pr_statements[i].b = LittleShort (pr->pr_statements[i].b);
		pr->pr_statements[i].c = LittleShort (pr->pr_statements[i].c);
	}

	for (i = 0; i < pr->progs->functions.count; i++) {
		pr->pr_functions[i].first_statement =
			LittleLong (pr->pr_functions[i].first_statement);
		pr->pr_functions[i].params_start =
			LittleLong (pr->pr_functions[i].params_start);
		pr->pr_functions[i].name = LittleLong (pr->pr_functions[i].name);
		pr->pr_functions[i].file = LittleLong (pr->pr_functions[i].file);
		pr->pr_functions[i].numparams =
			LittleLong (pr->pr_functions[i].numparams);
		pr->pr_functions[i].locals = LittleLong (pr->pr_functions[i].locals);
		if (pr->pr_functions[i].name)
			Hash_Add (pr->function_hash, &pr->pr_functions[i]);
	}

	if (pr->pr_globaldefs) {
		free (pr->pr_globaldefs);
	}
	pr->pr_globaldefs = calloc (pr->progs->globaldefs.count, sizeof (pr_def_t));

	for (i = 0; i < pr->progs->globaldefs.count; i++) {
		pr_ushort_t safe_type = global_ddefs[i].type & ~DEF_SAVEGLOBAL;
		global_ddefs[i].type = LittleShort (global_ddefs[i].type);
		global_ddefs[i].ofs = LittleShort (global_ddefs[i].ofs);
		global_ddefs[i].name = LittleLong (global_ddefs[i].name);

		pr->pr_globaldefs[i].type = global_ddefs[i].type;
		pr->pr_globaldefs[i].size = pr_type_size[safe_type];
		pr->pr_globaldefs[i].ofs = global_ddefs[i].ofs;
		pr->pr_globaldefs[i].name = global_ddefs[i].name;
		Hash_Add (pr->global_hash, &pr->pr_globaldefs[i]);
	}

	if (pr->pr_fielddefs) {
		free (pr->pr_fielddefs);
	}
	pr->pr_fielddefs = calloc (pr->progs->fielddefs.count, sizeof (pr_def_t));
	for (i = 0; i < pr->progs->fielddefs.count; i++) {
		field_ddefs[i].type = LittleShort (field_ddefs[i].type);
		if (field_ddefs[i].type & DEF_SAVEGLOBAL)
			PR_Error (pr, "PR_LoadProgs: DEF_SAVEGLOBAL on field def %zd", i);
		field_ddefs[i].ofs = LittleShort (field_ddefs[i].ofs);
		field_ddefs[i].name = LittleLong (field_ddefs[i].name);

		pr->pr_fielddefs[i].type = field_ddefs[i].type;
		pr->pr_fielddefs[i].ofs = field_ddefs[i].ofs;
		pr->pr_fielddefs[i].name = field_ddefs[i].name;
		Hash_Add (pr->field_hash, &pr->pr_fielddefs[i]);
	}

	for (i = 0; i < pr->progs->globals.count; i++)
		((int *) pr->pr_globals)[i] = LittleLong (((int *) pr->pr_globals)[i]);

	pr_def_t   *types_def = PR_FindGlobal (pr, ".type_encodings");
	if (types_def) {
		__auto_type encodings = &G_STRUCT (pr, qfot_type_encodings_t,
										   types_def->ofs);
		pr->type_encodings = encodings->types;
		qfot_type_t *type;
		for (pr_ptr_t type_ptr = 4; type_ptr < encodings->size;
			 type_ptr += type->size) {
			type = &G_STRUCT (pr, qfot_type_t, pr->type_encodings + type_ptr);
			Hash_Add (pr->type_hash, type);
		}
	} else {
		pr->type_encodings = 0;
	}

	pr_def_t   *xdefs_def = PR_FindGlobal (pr, ".xdefs");
	if (xdefs_def) {
		pr_xdefs_t *xdefs = &G_STRUCT (pr, pr_xdefs_t, xdefs_def->ofs);
		xdef_t     *xdef = &G_STRUCT (pr, xdef_t, xdefs->xdefs);
		pr_def_t   *def;
		for (def = pr->pr_globaldefs, i = 0; i < pr->progs->globaldefs.count;
			 i++, xdef++, def++) {
			def->ofs = xdef->ofs;
			def->type_encoding = xdef->type;
		}
		for (def = pr->pr_fielddefs, i = 0; i < pr->progs->fielddefs.count;
			 i++, xdef++, def++) {
			def->ofs = xdef->ofs;
			def->type_encoding = xdef->type;
		}
	} else {
		pr_def_t   *def;
		for (def = pr->pr_globaldefs, i = 0; i < pr->progs->globaldefs.count;
			 i++, def++) {
			def->size = pr_type_size[def->type & ~DEF_SAVEGLOBAL];
		}
		for (def = pr->pr_fielddefs, i = 0; i < pr->progs->fielddefs.count;
			 i++, def++) {
			def->size = pr_type_size[def->type & ~DEF_SAVEGLOBAL];
		}
	}
	pr->pr_trace = 0;
	pr->pr_trace_depth = 0;
	pr->pr_xfunction = 0;
	pr->pr_xstatement = 0;
	pr->pr_depth = 0;
	pr->localstack_used = 0;
	pr->pr_argc = 0;
}

VISIBLE void
PR_AddLoadFunc (progs_t *pr, int (*func)(progs_t *))
{
	if (pr->num_load_funcs == pr->max_load_funcs) {
		int       n;
		pr->max_load_funcs += 8;
		n = pr->max_load_funcs;
		pr->load_funcs = realloc (pr->load_funcs,
								  n * sizeof (pr_load_func_t *));
		SYS_CHECKMEM (pr->load_funcs);
	}
	pr->load_funcs[pr->num_load_funcs++] = func;
}

VISIBLE void
PR_AddLoadFinishFunc (progs_t *pr, int (*func)(progs_t *))
{
	if (pr->num_load_finish_funcs == pr->max_load_finish_funcs) {
		int       n;
		pr->max_load_finish_funcs += 8;
		n = pr->max_load_finish_funcs;
		pr->load_finish_funcs = realloc (pr->load_finish_funcs,
										 n * sizeof (pr_load_func_t *));
		SYS_CHECKMEM (pr->load_finish_funcs);
	}
	pr->load_finish_funcs[pr->num_load_finish_funcs++] = func;
}

static int
pr_run_ctors (progs_t *pr)
{
	pr_uint_t   fnum;
	dfunction_t *func;

	for (fnum = 0; fnum < pr->progs->functions.count; fnum++) {
		func = pr->pr_functions + fnum;
		if (strequal (PR_GetString (pr, func->name), ".ctor"))
			PR_ExecuteProgram (pr, fnum);
	}
	return 1;
}

static int (*load_funcs_1[])(progs_t *) = {
	PR_LoadStrings,
	PR_RelocateBuiltins,
	PR_LoadDebug,
	0,
};

static int (*load_funcs_2[])(progs_t *) = {
	PR_ResolveGlobals,
	PR_Check_Opcodes,
	0,
};

static int
run_load_funcs (progs_t *pr, int (**load_funcs)(progs_t *))
{
	int         (**lf)(progs_t *);

	for (lf = load_funcs; *lf; lf++)
		if (!(*lf)(pr))
			return 0;
	return 1;
}

VISIBLE int
PR_RunLoadFuncs (progs_t *pr)
{
	qfZoneScoped (true);
	int         i;

	memset (&pr->globals, 0, sizeof (pr->globals));
	pr->fields.nextthink = -1;
	pr->fields.frame = -1;
	pr->fields.think = -1;
	pr->fields.this = -1;

	if (!run_load_funcs(pr, load_funcs_1))
		return 0;
	if (pr->resolve)
		if (!pr->resolve (pr))
			return 0;
	if (!run_load_funcs(pr, load_funcs_2))
		return 0;

	for (i = 0; i < pr->num_load_funcs; i++)
		if (!pr->load_funcs[i] (pr))
			return 0;

	return 1;
}

VISIBLE int
PR_RunPostLoadFuncs (progs_t *pr)
{
	if (!pr_run_ctors (pr))
		return 0;

	while (pr->num_load_finish_funcs)
		if (!pr->load_finish_funcs[--pr->num_load_finish_funcs] (pr))
			return 0;

	return 1;
}

/*
	PR_LoadProgs
*/
VISIBLE void
PR_LoadProgs (progs_t *pr, const char *progsname)
{
	QFile      *file;
	file = QFS_FOpenFile (progsname);

	pr->progs_name = progsname;
	if (file) {
		PR_LoadProgsFile (pr, file, qfs_filesize);
		Qclose (file);
	}
	if (!pr->progs)
		return;

	if (!PR_RunLoadFuncs (pr)) {
		PR_Error (pr, "unable to load %s", progsname);
	}
	if (!pr->debug_handler && !PR_RunPostLoadFuncs (pr)) {
		PR_Error (pr, "unable to load %s", progsname);
	}
}

VISIBLE void
PR_Init_Cvars (void)
{
	Cvar_Register (&pr_boundscheck_cvar, 0, 0);
	Cvar_Register (&pr_deadbeef_ents_cvar, 0, 0);
	Cvar_Register (&pr_deadbeef_locals_cvar, 0, 0);
	Cvar_Register (&pr_faultchecks_cvar, 0, 0);
	PR_Debug_Init_Cvars ();
}

VISIBLE void
PR_Init (progs_t *pr)
{
	PR_Resources_Init (pr);
	PR_Strings_Init (pr);
	PR_Debug_Init (pr);
	pr->function_hash = Hash_NewTable (1021, function_get_key, 0, pr,
									   pr->hashctx);
	pr->global_hash = Hash_NewTable (1021, var_get_key, 0, pr, pr->hashctx);
	pr->field_hash = Hash_NewTable (1021, var_get_key, 0, pr, pr->hashctx);
	pr->type_hash = Hash_NewTable (1021, type_get_key, 0, pr, pr->hashctx);
}

VISIBLE void
PR_Shutdown (progs_t *pr)
{
	PR_Resources_Shutdown (pr);
	PR_Builtins_Shutdown (pr);

	free (pr->load_funcs);
	free (pr->load_finish_funcs);
	pr->num_load_funcs = pr->max_load_funcs = 0;
	pr->num_load_finish_funcs = pr->max_load_finish_funcs = 0;

	Hash_DelTable (pr->function_hash);
	Hash_DelTable (pr->global_hash);
	Hash_DelTable (pr->field_hash);
	Hash_DelTable (pr->type_hash);
	pr->function_hash = 0;
	pr->global_hash = 0;
	pr->field_hash = 0;
	pr->type_hash = 0;

	if (pr->progs) {
		pr->free_progs_mem (pr, pr->progs);
	}
	free (pr->pr_globaldefs);
	free (pr->pr_fielddefs);
	pr->progs = 0;
	pr->pr_globaldefs = 0;
	pr->pr_fielddefs = 0;
}

VISIBLE void
PR_Error (progs_t *pr, const char *error, ...)
{
	va_list     argptr;
	dstring_t  *string = dstring_new ();//FIXME leaks when debugging

	va_start (argptr, error);
	dvsprintf (string, error, argptr);
	va_end (argptr);

	if (pr->debug_handler) {
		pr->debug_handler (prd_error, string->str, pr->debug_data);
		// not expected to return, but if so, behave as if there was no handler
	}
	Sys_Error ("%s: %s", pr->progs_name, string->str);
}
