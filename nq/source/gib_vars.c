#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include "QF/cvar.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/cmd.h"
#include "QF/zone.h"
#include "QF/quakefs.h"
#include "gib.h"
#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_modules.h"
#include "gib_parse.h"
#include "gib_vars.h"
#include "gib_stack.h"

gib_var_t  *
GIB_Var_FindLocal (char *key)
{
	gib_var_t  *var;

	if (!(GIB_LOCALS))
		return 0;
	for (var = GIB_LOCALS; strcmp (key, var->key); var = var->next)
		if (!(var->next))
			return 0;
	return var;
}
gib_var_t  *
GIB_Var_FindGlobal (char *key)
{
	gib_var_t  *var;

	if (!(GIB_CURRENTMOD->vars))
		return 0;
	for (var = GIB_CURRENTMOD->vars; strcmp (key, var->key); var = var->next)
		if (!(var->next))
			return 0;
	return var;
}


void
GIB_Var_Set (char *key, char *value)
{
	gib_var_t  *var;

	if ((var = GIB_Var_FindLocal (key))) {
		free (var->value);
	} else {
		var = malloc (sizeof (gib_var_t));
		var->key = malloc (strlen (key) + 1);
		strcpy (var->key, key);
		var->next = GIB_LOCALS;
		GIB_LOCALS = var;
	}
	var->value = malloc (strlen (value) + 1);
	strcpy (var->value, value);
}

void
GIB_Var_FreeAll (gib_var_t * var)
{
	gib_var_t  *temp;

	for (; var; var = temp) {
		temp = var->next;
		free (var->key);
		free (var->value);
		free (var);
	}
}
