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
#include <QF/zone.h>

void *membase;
int memsize = 16*1024*1024;

extern char *type_name[];

progs_t progs;

int
main (int argc, char **argv)
{
	int i;

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	membase = malloc (memsize);
	Memory_Init (membase, memsize);
	Cvar_Init ();
	Cbuf_Init ();
	Cmd_Init ();

	Cvar_Get ("fs_basegame", ".", 0, 0, 0);
	Cvar_Get ("fs_userpath", "/", 0, 0, 0);
	Cvar_Get ("fs_sharepath", "/", 0, 0, 0);

	PR_Init_Cvars ();
	COM_Filesystem_Init_Cvars ();
	COM_Filesystem_Init ();
	PR_Init ();

	if (argc < 2) {
		fprintf (stderr, "usage: qfdefs <progs> ...\n");
		return 1;
	}
	while (--argc) {
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
		for (i = 0; i < progs.progs->numglobaldefs; i++) {
			ddef_t *def = &progs.pr_globaldefs[i];
			printf ("%-8s %d %-5d %s\n", type_name[def->type & ~DEF_SAVEGLOBAL], (def->type & DEF_SAVEGLOBAL) != 0, def->ofs, PR_GetString (&progs, def->s_name));
		}
	}
	return 0;
}
