/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/progs.h>
#include <QF/quakefs.h>
#include <QF/sys.h>
#include <QF/zone.h>

#define MAX_EDICTS 1024

progs_t progs;
void *membase;
int memsize = 16*1024*1024;

edict_t *edicts;
int num_edicts;
int reserved_edicts;

void BI_Init (progs_t *progs);

extern int *read_result; //FIXME: eww

int
main ()
{
	func_t main_func;
	QFile *f;
	int len;

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	membase = malloc (memsize);
	SYS_CHECKMEM (membase);
	Memory_Init (membase, memsize);
	Cvar_Init ();
	Cmd_Init ();

	Cvar_Get ("pr_debug", "1", 0, 0, 0);
	Cvar_Get ("pr_boundscheck", "0", 0, 0, 0);
	Cvar_Get ("fs_basegame", ".", 0, 0, 0);
	Cvar_Get ("fs_userpath", ".", 0, 0, 0);
	Cvar_Get ("fs_sharepath", ".", 0, 0, 0);
	developer = Cvar_Get ("developer", "1", 0, 0, 0);

	PR_Init_Cvars ();
	COM_Filesystem_Init_Cvars ();
	COM_Filesystem_Init ();
	PR_Init ();
	PR_Obj_Progs_Init (&progs);
	BI_Init (&progs);

	progs.edicts = &edicts;
	progs.num_edicts = &num_edicts;
	progs.reserved_edicts = &reserved_edicts;
	progs.no_exec_limit = 1;
	progs.progs_name = "qwaq.dat";

	f = Qopen (progs.progs_name, "rb");
	if (f) {
		Qseek (f, 0, SEEK_END);
		len = Qtell (f);
		Qseek (f, 0, SEEK_SET);
		com_filesize = len;
		PR_LoadProgsFile (&progs, f, len, 0, 1024 * 1024);
		Qclose (f);
	}
	if (!progs.progs)
		Sys_Error ("couldn't load %s", "qwaq.dat");

	if (!PR_ResolveGlobals (&progs))
		PR_Error (&progs, "unable to load %s", progs.progs_name);
	PR_LoadStrings (&progs);
	PR_LoadDebug (&progs);
	PR_Check_Opcodes (&progs);
	PR_RelocateBuiltins (&progs);
	PR_InitRuntime (&progs);

	*progs.edicts = PR_InitEdicts (&progs, MAX_EDICTS);

	read_result = (int*)PR_GetGlobalPointer (&progs, "read_result");
	main_func = PR_GetFunctionIndex (&progs, "main");
	PR_ExecuteProgram (&progs, main_func);
	return G_FLOAT (&progs, OFS_RETURN);
}
