#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "cvar.h"
#include "console.h"
#include "qargs.h"
#include "cmd.h"
#include "zone.h"
#include "quakefs.h"
#include "gib.h"
#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_modules.h"
#include "gib_parse.h"
#include "gib_vars.h"
#include "gib_error.h"
#include "gib_stack.h"

char       *gib_subargv[256];
int         gib_subargc;

char       *
GIB_Argv (int i)
{
	return gib_instack[gib_insp - 1].argv[i];
}

int
GIB_Argc (void)
{
	return gib_instack[gib_insp - 1].argc;
}

void
GIB_Strip_Arg (char *arg)
{
	if (arg[0] == '{' || arg[0] == '\'' || arg[0] == '\"') {
		arg[strlen (arg) - 1] = 0;
		memmove (arg, arg + 1, strlen (arg));
	}
}

int
GIB_Execute_Block (char *block, int retflag)
{
	int         len, i, ret;
	char       *code;

	i = 0;

	while ((len = GIB_Get_Inst (block + i)) > 0) {
		code = malloc (len + 1);
		strncpy (code, block + i, len);
		code[len] = 0;
		if ((ret = GIB_Interpret_Inst (code))) {
			free (code);
			return ret;
		}
		if ((ret = GIB_Execute_Inst ())) {
			free (code);
			if (ret == GIB_E_RETURN && retflag)
				return 0;
			return ret;
		}
		i += len + 1;
		free (code);
	}
	if (len == -1)
		return GIB_E_PARSE;
	return 0;
}

int
GIB_Execute_Inst (void)
{
	int         ret;

	ret = gib_instack[gib_insp - 1].instruction->func ();
	GIB_InStack_Pop ();
	return ret;
}

int
GIB_Interpret_Inst (char *inst)
{
	char       *buffer;
	char       *buffer2;
	char       *buffer3;
	int         i, n, len, ret, gib_argc;
	char       *gib_argv[256];
	gib_inst_t *ginst;


	buffer = malloc (strlen (inst) + 1);
	i = 0;

	while (isspace (inst[i]))
		i++;

	for (n = 0; i <= strlen (inst); i++) {
		if (inst[i] == '\n' || inst[i] == '\t')
			buffer[n] = ' ';
		else
			buffer[n] = inst[i];
		n++;
	}


	buffer2 = malloc (2048);
	buffer3 = malloc (2048);
	ret = GIB_ExpandVars (buffer, buffer2, 2048);
	if (ret)
		return ret;
	ret = GIB_ExpandBackticks (buffer2, buffer3, 2048);
	if (ret)
		return ret;
	gib_argc = 0;
	for (i = 0; buffer3[i] != ' '; i++);
	gib_argv[0] = malloc (i + 1);
	strncpy (gib_argv[0], buffer3, i);
	gib_argv[0][i] = 0;
	for (n = 0;; n++) {
		for (; isspace (buffer3[i]); i++);
		if (buffer3[i] == 0)
			break;
		if ((len = GIB_Get_Arg (buffer3 + i)) < 0)
			return GIB_E_PARSE;
		else {
			gib_argv[n + 1] = malloc (len + 1);
			strncpy (gib_argv[n + 1], buffer3 + i, len);
			gib_argv[n + 1][len] = 0;
			GIB_ExpandEscapes (gib_argv[n + 1]);
			i += len;
		}
	}
	gib_argc = n;

	free (buffer);
	free (buffer2);
	free (buffer3);

	for (i = 1; i <= n; i++)
		GIB_Strip_Arg (gib_argv[i]);
	if (!(ginst = GIB_Find_Instruction (gib_argv[0])))
		return GIB_E_ILLINST;
	GIB_InStack_Push (ginst, gib_argc, gib_argv);

	for (i = 0; i <= n; i++)
		free (gib_argv[i]);
	return 0;
}

int
GIB_Run_Sub (gib_module_t * mod, gib_sub_t * sub)
{
	int         ret, i;
	char        buf[256];

	GIB_SubStack_Push (mod, sub, 0);

	for (i = 0; i <= gib_subargc; i++) {
		snprintf (buf, sizeof (buf), "arg%i", i);
		GIB_Var_Set (buf, gib_subargv[i]);
	}

	snprintf (buf, sizeof (buf), "%i", gib_subargc);
	GIB_Var_Set ("argc", buf);

	ret = GIB_Execute_Sub ();

	if (GIB_LOCALS)
		GIB_Var_FreeAll (GIB_LOCALS);

	GIB_SubStack_Pop ();
	return ret;
}

int
GIB_Execute_Sub (void)
{
	return GIB_Execute_Block (GIB_CURRENTSUB->code, 1);
}

int
GIB_Run_Inst (char *inst)
{
	int         ret;

	ret = GIB_Interpret_Inst (inst);
	if (ret) {
		GIB_InStack_Pop ();
		return ret;
	}
	return GIB_Execute_Inst ();
}
