#include <QF/progs.h>

#define RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].int_var = EDICT_TO_PROG(p, e))
#define RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].int_var = PR_SetString((p), s))

void 
bi_fixme (progs_t *pr)
{
	PR_Error (pr, "unimplemented function\n");
}

void
bi_print (progs_t *pr)
{
	char *str;

	str = G_STRING (pr, (OFS_PARM0));
	fprintf (stdout, "%s", str);
}

builtin_t   builtins[] = {
	bi_fixme,
	bi_print,
};

void
BI_Init (progs_t *progs)
{
	progs->builtins = builtins;
	progs->numbuiltins = sizeof (builtins) / sizeof (builtins[0]);
}
