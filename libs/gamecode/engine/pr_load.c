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
#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/va.h"
#include "QF/vfs.h"

#include "compat.h"

static const char *
function_get_key (void *f, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	dfunction_t *func = (dfunction_t*)f;
	return PR_GetString (pr, func->s_name);
}

static const char *
var_get_key (void *d, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	ddef_t *def = (ddef_t*)d;
	return PR_GetString (pr, def->s_name);
}

void
PR_LoadProgsFile (progs_t * pr, const char *progsname)
{
	int         i;

	if (progsname)
		pr->progs = (dprograms_t *) COM_LoadHunkFile (progsname);
	else
		progsname = pr->progs_name;
	if (!pr->progs)
		return;

	pr->progs_size = com_filesize;
	Sys_DPrintf ("Programs occupy %iK.\n", com_filesize / 1024);

	// store prog crc
	pr->crc = CRC_Block ((byte *) pr->progs, com_filesize);

	// byte swap the header
	for (i = 0; i < sizeof (*pr->progs) / 4; i++)
		((int *) pr->progs)[i] = LittleLong (((int *) pr->progs)[i]);

	if (pr->progs->version != PROG_VERSION
		&& pr->progs->version != PROG_ID_VERSION) {
		if (pr->progs->version < 0x00fff000) {
			PR_Error (pr, "%s has unrecognised version number (%d)",
					  progsname, pr->progs->version);
		} else {
			PR_Error (pr,
					  "%s has unrecognised version number (%02x.%03x.%03x)"
					  " [%02x.%03x.%03x expected]",
					  progsname,
					  pr->progs->version >> 24,
					  (pr->progs->version >> 12) & 0xfff,
					  pr->progs->version & 0xfff,
					  PROG_VERSION >> 24,
					  (PROG_VERSION >> 12) & 0xfff,
					  PROG_VERSION & 0xfff);
		}
	}

	pr->progs_name = progsname;	//XXX is this safe?

	pr->zone = 0;		// caller sets up afterwards

	pr->pr_functions =
		(dfunction_t *) ((byte *) pr->progs + pr->progs->ofs_functions);
	pr->pr_strings = (char *) pr->progs + pr->progs->ofs_strings;
	pr->pr_stringsize = pr->progs->numstrings;
	pr->pr_globaldefs =
		(ddef_t *) ((byte *) pr->progs + pr->progs->ofs_globaldefs);
	pr->pr_fielddefs =
		(ddef_t *) ((byte *) pr->progs + pr->progs->ofs_fielddefs);
	pr->pr_statements =
		(dstatement_t *) ((byte *) pr->progs + pr->progs->ofs_statements);

	pr->pr_globals =
		(pr_type_t *) ((byte *) pr->progs + pr->progs->ofs_globals);

	// size of edict ascked for by progs
	pr->pr_edict_size = pr->progs->entityfields * 4;
	// size of engine data
	pr->pr_edict_size += sizeof (edict_t) - sizeof (pr_type_t);
	// round off to next highest whole word address (esp for Alpha)
	// this ensures that pointers in the engine data area are always
	// properly aligned
	pr->pr_edict_size += sizeof (void*) - 1;
	pr->pr_edict_size &= ~(sizeof (void*) - 1);

	pr->pr_edictareasize = 0;

	if (pr->function_hash) {
		Hash_FlushTable (pr->function_hash);
	} else {
		pr->function_hash = Hash_NewTable (1021, function_get_key, 0, pr);
	}
	if (pr->global_hash) {
		Hash_FlushTable (pr->global_hash);
	} else {
		pr->global_hash = Hash_NewTable (1021, var_get_key, 0, pr);
	}
	if (pr->field_hash) {
		Hash_FlushTable (pr->field_hash);
	} else {
		pr->field_hash = Hash_NewTable (1021, var_get_key, 0, pr);
	}

// byte swap the lumps
	for (i = 0; i < pr->progs->numstatements; i++) {
		pr->pr_statements[i].op = LittleShort (pr->pr_statements[i].op);
		pr->pr_statements[i].a = LittleShort (pr->pr_statements[i].a);
		pr->pr_statements[i].b = LittleShort (pr->pr_statements[i].b);
		pr->pr_statements[i].c = LittleShort (pr->pr_statements[i].c);
	}

	for (i = 0; i < pr->progs->numfunctions; i++) {
		pr->pr_functions[i].first_statement =
			LittleLong (pr->pr_functions[i].first_statement);
		pr->pr_functions[i].parm_start =
			LittleLong (pr->pr_functions[i].parm_start);
		pr->pr_functions[i].s_name = LittleLong (pr->pr_functions[i].s_name);
		pr->pr_functions[i].s_file = LittleLong (pr->pr_functions[i].s_file);
		pr->pr_functions[i].numparms =
			LittleLong (pr->pr_functions[i].numparms);
		pr->pr_functions[i].locals = LittleLong (pr->pr_functions[i].locals);
		Hash_Add (pr->function_hash, &pr->pr_functions[i]);
	}

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		pr->pr_globaldefs[i].type = LittleShort (pr->pr_globaldefs[i].type);
		pr->pr_globaldefs[i].ofs = LittleShort (pr->pr_globaldefs[i].ofs);
		pr->pr_globaldefs[i].s_name = LittleLong (pr->pr_globaldefs[i].s_name);
		Hash_Add (pr->global_hash, &pr->pr_globaldefs[i]);
	}

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		pr->pr_fielddefs[i].type = LittleShort (pr->pr_fielddefs[i].type);
		if (pr->pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			PR_Error (pr, "PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr->pr_fielddefs[i].ofs = LittleShort (pr->pr_fielddefs[i].ofs);
		pr->pr_fielddefs[i].s_name = LittleLong (pr->pr_fielddefs[i].s_name);
		Hash_Add (pr->field_hash, &pr->pr_fielddefs[i]);
	}

	for (i = 0; i < pr->progs->numglobals; i++)
		((int *) pr->pr_globals)[i] = LittleLong (((int *) pr->pr_globals)[i]);
}

/*
	PR_LoadProgs
*/
void
PR_LoadProgs (progs_t *pr, const char *progsname)
{
	PR_LoadProgsFile (pr, progsname);
	if (!pr->progs)
		return;

	if (!progsname)
		progsname = "(preloaded)";

	if (!PR_ResolveGlobals (pr))
		PR_Error (pr, "unable to load %s", progsname);

	// initialise the strings managment code
	PR_LoadStrings (pr);

	PR_LoadDebug (pr);

	PR_Check_Opcodes (pr);
}

void
PR_Init_Cvars (void)
{
	pr_boundscheck =
		Cvar_Get ("pr_boundscheck", "1", CVAR_NONE, NULL,
				  "Server progs bounds checking");
	pr_deadbeef_ents = Cvar_Get ("pr_deadbeef_ents", "0", CVAR_NONE, NULL,
							"set to clear unallocated memory to 0xdeadbeef");
	pr_deadbeef_locals = Cvar_Get ("pr_deadbeef_locals", "0", CVAR_NONE, NULL,
								   "set to clear uninitialized local vars to "
								   "0xdeadbeef");
	PR_Debug_Init_Cvars ();
}

void
PR_Init (void)
{
	PR_Opcode_Init ();
	PR_Debug_Init ();
}

void
PR_Error (progs_t *pr, const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Sys_Error ("%s: %s", pr->progs_name, string);
}
