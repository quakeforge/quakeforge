#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/progs.h>
#include <QF/sys.h>
#include <QF/zone.h>

progs_t progs;
void *membase;
int memsize = 16*1024*1024;

void BI_Init (progs_t *progs);

int
main ()
{
	func_t main_func;
	FILE *f;
	int len;

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	membase = malloc (memsize);
	Memory_Init (membase, memsize);
	Cvar_Init ();
	Cbuf_Init ();
	Cmd_Init ();

	PR_Init_Cvars ();
	PR_Init ();
	BI_Init (&progs);

	f = fopen ("qwaq.dat", "rb");
	if (f) {
		fseek (f, 0, SEEK_END);
		len = ftell (f);
		fseek (f, 0, SEEK_SET);
		progs.progs = Hunk_AllocName (len, "qwaq.dat");
		fread (progs.progs, 1, len, f);
		fclose (f);
		if (progs.progs)
			PR_LoadProgs (&progs, 0);
	}
	if (!progs.progs)
		Sys_Error ("couldn't load %s\n", "qwaq.dat");
	main_func = PR_GetFunctionIndex (&progs, "main");
	PR_ExecuteProgram (&progs, main_func);
	return 0;
}
