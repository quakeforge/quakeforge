#include <stdlib.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/progs.h>
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

extern char *type_name[];

extern float *read_result; //FIXME: eww

int
main ()
{
	func_t main_func;
	FILE *f;
	int len;
	int i;

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

	progs.edicts = &edicts;
	progs.num_edicts = &num_edicts;
	progs.reserved_edicts = &reserved_edicts;
	progs.no_exec_limit = 1;

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

	*progs.edicts = PR_InitEdicts (&progs, MAX_EDICTS);
	for (i = 0; i < progs.progs->numstatements; i++)
		PR_PrintStatement (&progs, &progs.pr_statements[i]);
#if 0
	printf ("\n");
	for (i = 0; i < progs.progs->numfunctions; i++) {
		dfunction_t *func = &progs.pr_functions[i];
		int j;

		printf ("%d %d %d %d %s %s %d", func->first_statement, func->parm_start, func->locals, func->profile, PR_GetString (&progs, func->s_name), PR_GetString (&progs, func->s_file), func->numparms);
		for (j = 0; j < func->numparms; j++)
			printf (" %d", func->parm_size[j]);
		printf ("\n");
	}
	printf ("\n");
	for (i = 0; i < progs.progs->numglobaldefs; i++) {
		ddef_t *def = &progs.pr_globaldefs[i];

		printf ("%s %d %d %s\n", type_name[def->type & ~DEF_SAVEGLOBAL], (def->type & DEF_SAVEGLOBAL) != 0, def->ofs, PR_GetString (&progs, def->s_name));
	}
	printf ("\n");
	for (i = 0; i < progs.progs->numfielddefs; i++) {
		ddef_t *def = &progs.pr_fielddefs[i];

		printf ("%s %d %d %s\n", type_name[def->type & ~DEF_SAVEGLOBAL], (def->type & DEF_SAVEGLOBAL) != 0, def->ofs, PR_GetString (&progs, def->s_name));
	}
	printf ("\n");
#endif
	read_result = (float*)PR_GetGlobalPointer (&progs, "read_result");
	main_func = PR_GetFunctionIndex (&progs, "main");
	PR_ExecuteProgram (&progs, main_func);
	return G_FLOAT (&progs, OFS_RETURN);
}
