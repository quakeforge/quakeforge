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

gib_instack_t *gib_instack = 0;
gib_substack_t *gib_substack = 0;

int gib_insp = 0;
int gib_subsp = 0;

void GIB_InStack_Push (gib_inst_t *instruction, int argc, char **argv)
{
	int i;

	gib_instack = realloc(gib_instack, sizeof(gib_instack_t) * (gib_insp + 1));
	gib_instack[gib_insp].argv = malloc(argc + 1);
	
	for (i = 0; i <= argc; i++)
	{
		gib_instack[gib_insp].argv[i] = malloc(strlen(argv[i]) + 1);
		strcpy(gib_instack[gib_insp].argv[i], argv[i]);
	}

	gib_instack[gib_insp].argc = argc;
	gib_instack[gib_insp].instruction = instruction;
	gib_insp++;
}

void GIB_InStack_Pop (void)
{
	int i;

	gib_insp--;
	
	for (i = 0; i <= gib_instack[gib_insp].argc; i++)
		free(gib_instack[gib_insp].argv[i]);

	free(gib_instack[gib_insp].argv);

	gib_instack = realloc(gib_instack, sizeof(gib_instack_t) * gib_insp);
}

void GIB_SubStack_Push (gib_module_t *mod, gib_sub_t *sub, gib_var_t *local)
{
	gib_substack = realloc(gib_substack, sizeof(gib_substack_t) * (gib_subsp + 1));
	gib_substack[gib_subsp].mod = mod;
	gib_substack[gib_subsp].sub = sub;
	gib_substack[gib_subsp].local = local;
	gib_subsp++;
}

void GIB_SubStack_Pop (void)
{
	gib_instack = realloc(gib_instack, sizeof(gib_instack_t) * (--gib_subsp));
}