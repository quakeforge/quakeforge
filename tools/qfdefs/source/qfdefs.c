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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include "QF/progs.h"
#include <QF/vfs.h>
#include <QF/sys.h>
#include <QF/va.h>
#include <QF/zone.h>

#include "def.h"

void *membase;
int memsize = 16*1024*1024;

progs_t progs;

int
main (int argc, char **argv)
{
	def_t *globals, *fields, *def;

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	membase = malloc (memsize);
	Memory_Init (membase, memsize);
	Cvar_Init ();
	Cmd_Init ();

	Cvar_Get ("fs_basegame", "", 0, 0, 0);
	Cvar_Get ("fs_userpath", "", 0, 0, 0);
	Cvar_Get ("fs_sharepath", "", 0, 0, 0);

	PR_Init_Cvars ();
	COM_Filesystem_Init_Cvars ();
	COM_Filesystem_Init ();
	PR_Init ();

	if (argc < 2) {
		fprintf (stderr, "usage: qfdefs <progs> ...\n");
		return 1;
	}
	while (--argc) {
		int fix = 0;
		int size;
		VFile *file;

		progs.progs_name = *++argv;
		size = COM_FOpenFile (progs.progs_name, &file);
		if (size != -1)
			PR_LoadProgsFile (&progs, file, size, 0, 0);
		if (size == -1 || !progs.progs) {
			fprintf (stderr, "failed to load %s\n", progs.progs_name);
			return 1;
		}
		if (progs.progs->version < 0x100)
			printf ("%s: version %d\n", progs.progs_name, progs.progs->version);
		else
			printf ("%s: version %x.%03x.%03x\n", progs.progs_name,
					(progs.progs->version >> 24) & 0xff,
					(progs.progs->version >> 12) & 0xfff,
					progs.progs->version & 0xfff);
		if (progs.progs->crc == nq_crc) {
			printf ("%s: netquake crc\n", progs.progs_name);
			Init_Defs (nq_global_defs, nq_field_defs);
			globals = nq_global_defs;
			fields = nq_field_defs;
		} else if (progs.progs->crc == qw_crc) {
			printf ("%s: quakeworld crc\n", progs.progs_name);
			Init_Defs (qw_global_defs, qw_field_defs);
			globals = qw_global_defs;
			fields = qw_field_defs;
		} else {
			printf ("%s: unknown crc %d\n", progs.progs_name, progs.progs->crc);
			continue;
		}
		for (def = globals; def->name; def++)
			if (!PR_FindGlobal (&progs, def->name))
				break;
		if (!def->name)
			printf ("%s: all system globals accounted for\n", progs.progs_name);
		else {
			printf ("%s: some system globals missing\n", progs.progs_name);
			fix_missing_globals (&progs, globals);
			fix++;
		}
		for (def = fields; def->name; def++)
			if (!ED_FindField (&progs, def->name))
				break;
		if (!def->name)
			printf ("%s: all system fields accounted for\n", progs.progs_name);
		else {
			printf ("%s: some system fields missing\n", progs.progs_name);
		}
		if (fix) {
			//XXX FIXME endian fixups
			FILE *f = fopen (va ("%s.new", progs.progs_name), "wb");
			fwrite (progs.progs, progs.progs_size, 1, f);
			fclose (f);
		}
	}
	return 0;
}
