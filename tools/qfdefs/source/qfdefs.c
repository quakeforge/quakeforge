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
	Cbuf_Init ();
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
		PR_LoadProgsFile (&progs, *++argv);
		if (!progs.progs) {
			fprintf (stderr, "failed to load %s\n", *argv);
			return 1;
		}
		if (progs.progs->version < 0x100)
			printf ("%s: version %d\n", *argv, progs.progs->version);
		else
			printf ("%s: version %x.%03x.%03x\n", *argv,
					(progs.progs->version >> 24) & 0xff,
					(progs.progs->version >> 12) & 0xfff,
					progs.progs->version & 0xfff);
		if (progs.progs->crc == nq_crc) {
			printf ("%s: netquake crc\n", *argv);
			Init_Defs (nq_global_defs, nq_field_defs);
			globals = nq_global_defs;
			fields = nq_field_defs;
		} else if (progs.progs->crc == qw_crc) {
			printf ("%s: quakeworld crc\n", *argv);
			Init_Defs (qw_global_defs, qw_field_defs);
			globals = qw_global_defs;
			fields = qw_field_defs;
		} else {
			printf ("%s: unknown crc %d\n", *argv, progs.progs->crc);
			continue;
		}
		for (def = globals; def->name; def++)
			if (!PR_FindGlobal (&progs, def->name))
				break;
		if (!def->name)
			printf ("%s: all system globals accounted for\n", *argv);
		else {
			printf ("%s: some system globals missing\n", *argv);
			fix_missing_globals (&progs, globals);
			fix++;
		}
		for (def = fields; def->name; def++)
			if (!ED_FindField (&progs, def->name))
				break;
		if (!def->name)
			printf ("%s: all system fields accounted for\n", *argv);
		else {
			printf ("%s: some system fields missing\n", *argv);
		}
		if (fix) {
			//XXX FIXME endian fixups
			FILE *f = fopen (va ("%s.new", *argv), "wb");
			fwrite (progs.progs, progs.progs_size, 1, f);
			fclose (f);
		}
	}
	return 0;
}
