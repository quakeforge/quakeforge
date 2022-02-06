#include "QF/progs.h"

int
call_progs_main (progs_t *pr, int argc, const char **argv)
{
	int         i;
	dfunction_t *dfunc;
	pr_func_t   progs_main = 0;
	string_t   *pr_argv;

	if ((dfunc = PR_FindFunction (pr, "main"))) {
		progs_main = dfunc - pr->pr_functions;
	} else {
		PR_Undefined (pr, "function", "main");
		return -1;
	}

	PR_PushFrame (pr);
	pr_argv = PR_Zone_Malloc (pr, (argc + 1) * 4);
	for (i = 0; i < argc; i++)
		pr_argv[i] = PR_SetTempString (pr, argv[1 + i]);
	pr_argv[i] = 0;
	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = argc;
	P_POINTER (pr, 1) = PR_SetPointer (pr, pr_argv);
	pr->pr_argc = 2;
	PR_ExecuteProgram (pr, progs_main);
	PR_PopFrame (pr);
	PR_Zone_Free (pr, pr_argv);
	return R_INT (pr);
}
