#include "defs.h"
#include "QF/cvar.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

qboolean	Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}

cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (var_name, var->name))
			return var;

	return NULL;
}

void Cvar_RegisterVariable (cvar_t *variable)
{
	char	value[512];

// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->name))
	{
		printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}
	
// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}
		
// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;

// copy the value off, because future sets will Z_Free it
	strcpy (value, variable->string);
	variable->string = (char *)malloc (1);	
	
// set it through the function to be consistant
	Cvar_Set (variable->name, value);
}

void Cvar_Set (char *var_name, char *value)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		printf ("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	
	free (var->string);	// free the old value string
	
	var->string = (char *)malloc (strlen(value)+1);
	strcpy (var->string, value);
	var->value = atof (var->string);
}

void Cvar_SetValue (char *var_name, float value)
{
	char	val[32];
	
	sprintf (val, "%f",value);
	Cvar_Set (var_name, val);
}

char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}

float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}
