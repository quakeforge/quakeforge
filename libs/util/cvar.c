/*
	cvar.c

	dynamic variable tracking

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#define USER_RO_CVAR "User-created READ-ONLY Cvar"
#define USER_CVAR "User-created cvar"

static exprenum_t developer_enum;
static exprtype_t developer_type = {
	.name = "developer",
	.size = sizeof (int),
	.binops = cexpr_flag_binops,
	.unops = cexpr_flag_unops,
	.data = &developer_enum,
	.get_string = cexpr_enum_get_string,
};

#define SYS_DEVELOPER(dev) (SYS_##dev & ~SYS_dev),
static int developer_values[] = {
	SYS_dev,
#include "QF/sys_developer.h"
};
#undef SYS_DEVELOPER
#define SYS_DEVELOPER(dev) {#dev, &developer_type, developer_values + SYS_DeveloperID_##dev + 1},
static exprsym_t developer_symbols[] = {
	{"dev", &developer_type, developer_values + 0},
#include "QF/sys_developer.h"
	{}
};
#undef SYS_DEVELOPER
static exprtab_t developer_symtab = {
	developer_symbols,
};
static exprenum_t developer_enum = {
	&developer_type,
	&developer_symtab,
};

VISIBLE int developer;
static cvar_t developer_cvar = {
	.name = "developer",
	.description =
		"set to enable extra debugging information",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &developer_type, .value = &developer },
};
static const char		*cvar_null_string = "";
static hashtab_t		*cvar_hash;
static hashtab_t		*user_cvar_hash;
static hashtab_t		*calias_hash;

static cvar_t *
cvar_create (const char *name, const char *value)
{
	cvar_t     *var = calloc (1, sizeof (cvar_t) + sizeof (char *));
	var->name = strdup (name);
	var->description = cvar_null_string;
	var->default_value = cvar_null_string;
	var->flags = CVAR_USER_CREATED;
	var->value.value = var + 1;
	*(char **)var->value.value = strdup (value);

	Hash_Add (user_cvar_hash, var);
	return var;
}

static void
cvar_destroy (cvar_t *var)
{
	if (!(var->flags & CVAR_USER_CREATED)) {
		Sys_Error ("Attempt to destroy non-user cvar");
	}
	Hash_Free (user_cvar_hash, Hash_Del (user_cvar_hash, var->name));
}

static void
cvar_invoke (cvar_t *var)
{
	if (var->listeners) {
		LISTENER_INVOKE (var->listeners, var);
	}
}

VISIBLE cvar_t *
Cvar_FindVar (const char *var_name)
{
	cvar_t     *var = Hash_Find (cvar_hash, var_name);
	if (!var) {
		var = Hash_Find (user_cvar_hash, var_name);
	}
	return var;
}

VISIBLE cvar_t *
Cvar_FindAlias (const char *alias_name)
{
	cvar_alias_t *alias;

	alias = (cvar_alias_t *) Hash_Find (calias_hash, alias_name);
	if (alias)
		return alias->cvar;
	return 0;
}

VISIBLE cvar_t *
Cvar_MakeAlias (const char *name, cvar_t *cvar)
{
	cvar_alias_t *alias;
	cvar_t			*var;

	if (Cmd_Exists (name)) {
		Sys_Printf ("Cvar_MakeAlias: %s is a command\n", name);
		return 0;
	}
	if (Cvar_FindVar (name)) {
		Sys_Printf ("Cvar_MakeAlias: %s is a cvar\n",
					name);
		return 0;
	}
	var = Cvar_FindAlias (name);
	if (var && var != cvar) {
		Sys_Printf ("Cvar_MakeAlias: %s is an alias to %s\n",
					name, var->name);
		return 0;
	}
	if (!var) {
		alias = (cvar_alias_t *) calloc (1, sizeof (cvar_alias_t));

		alias->name = strdup (name);
		alias->cvar = cvar;
		Hash_Add (calias_hash, alias);
	}
	return cvar;
}

VISIBLE cvar_t *
Cvar_RemoveAlias (const char *name)
{
	cvar_alias_t *alias;
	cvar_t      *var;

	alias = (cvar_alias_t *) Hash_Find (calias_hash, name);
	if (!alias) {
		Sys_Printf ("Cvar_RemoveAlias: %s is not an alias\n", name);
		return 0;
	}
	var = alias->cvar;
	Hash_Free (calias_hash, Hash_Del (calias_hash, name));
	return var;
}

static float
cvar_value (cvar_t *var)
{
	if (!var->value.type) {
		return atof (*(char **)var->value.value);
	} else if (var->value.type == &cexpr_int) {
		return *(int *)var->value.value;
	} else if (var->value.type == &cexpr_float) {
		return *(float *)var->value.value;
	}
	return 0;
}

VISIBLE float
Cvar_Value (const char *var_name)
{
	cvar_t     *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var)
		return 0;
	return cvar_value (var);
}

static const char *
cvar_string (const cvar_t *var)
{
	if (!var->value.type) {
		return *(char **)var->value.value;
	} else if (var->value.type->get_string) {
		return var->value.type->get_string (&var->value, 0);
	}
	return cvar_null_string;
}

VISIBLE const char *
Cvar_String (const char *var_name)
{
	cvar_t     *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		var = Cvar_FindAlias (var_name);
	if (!var)
		return cvar_null_string;
	return cvar_string (var);
}

VISIBLE const char *
Cvar_VarString (const cvar_t *var)
{
	return cvar_string (var);
}

typedef struct {
	const char *match;
	size_t      match_len;
	int         num_matches;
} cvar_count_ctx_t;

static void
cvar_match_count (void *ele, void *data)
{
	cvar_count_ctx_t *ctx = data;
	const cvar_t *cvar = ele;

	if (strncmp (cvar->name, ctx->match, ctx->match_len) == 0) {
		ctx->num_matches++;
	}
}

VISIBLE int
Cvar_CompleteCountPossible (const char *partial)
{
	cvar_count_ctx_t ctx = {
		.match = partial,
		.match_len = strlen (partial),
		.num_matches = 0,
	};

	Hash_ForEach (cvar_hash, cvar_match_count, &ctx);
	Hash_ForEach (user_cvar_hash, cvar_match_count, &ctx);
	// this is a bit of a hack, but both cvar_alias_t and cvar_t have
	// name in the first file, so it will work out as that's the only
	// criteron for a match
	Hash_ForEach (calias_hash, cvar_match_count, &ctx);

	return ctx.num_matches;
}

typedef struct {
	const char *match;
	size_t      match_len;
	const char **list;
	int         index;
} cvar_copy_ctx_t;

static void
cvar_match_copy (void *ele, void *data)
{
	cvar_copy_ctx_t *ctx = data;
	const cvar_t *cvar = ele;

	if (strncmp (cvar->name, ctx->match, ctx->match_len) == 0) {
		ctx->list[ctx->index++] = cvar->name;
	}
}

static int
cvar_cmp_name (const void *_a, const void *_b)
{
	const char * const *a = _a;
	const char * const *b = _b;
	return strcmp (*a, *b);
}

VISIBLE const char **
Cvar_CompleteBuildList (const char *partial)
{
	int         num_matches = Cvar_CompleteCountPossible (partial);

	cvar_copy_ctx_t ctx = {
		.match = partial,
		.match_len = strlen (partial),
		.list = malloc((num_matches + 1) * sizeof (char *)),
		.index = 0,
	};

	Hash_ForEach (cvar_hash, cvar_match_copy, &ctx);
	Hash_ForEach (user_cvar_hash, cvar_match_copy, &ctx);
	// this is a bit of a hack, but both cvar_alias_t and cvar_t have
	// name in the first file, so it will work out as that's the only
	// criteron for a match
	Hash_ForEach (calias_hash, cvar_match_copy, &ctx);
	ctx.list[ctx.index] = 0;
	heapsort (ctx.list, ctx.index, sizeof (char *), cvar_cmp_name);
	return ctx.list;
}

VISIBLE void
Cvar_AddListener (cvar_t *cvar, cvar_listener_t listener, void *data)
{
	if (!cvar->listeners) {
		cvar->listeners = malloc (sizeof (*cvar->listeners));
		LISTENER_SET_INIT (cvar->listeners, 8);
	}
	LISTENER_ADD (cvar->listeners, listener, data);
}

VISIBLE void
Cvar_RemoveListener (cvar_t *cvar, cvar_listener_t listener, void *data)
{
	if (cvar->listeners) {
		LISTENER_REMOVE (cvar->listeners, listener, data);
	}
}

static int
cvar_setvar (cvar_t *var, const char *value)
{
	int         changed = 0;

	if (!var->value.type) {
		char      **str_value = var->value.value;
		changed = !*str_value || !strequal (*str_value, value);
		if (var->validator) {
			changed = changed && var->validator (var);
		}
		if (changed) {
			free (*str_value);
			*str_value = strdup (value);
		}
	} else {
		exprenum_t *enm = var->value.type->data;
		exprctx_t   context = {
			.memsuper = new_memsuper (),
			.symtab = enm ? enm->symtab : 0,
			.msg_prefix = var->name,
		};
		if (context.symtab && !context.symtab->tab) {
			cexpr_init_symtab (context.symtab, &context);
		}
		context.result = cexpr_value (var->value.type, &context);
		if (!cexpr_eval_string (value, &context)) {
			changed = memcmp (context.result->value, var->value.value,
							  var->value.type->size) != 0;
			if (var->validator) {
				changed = changed && var->validator (var);
			}
			if (changed) {
				memcpy (var->value.value, context.result->value,
						var->value.type->size);
			}
		}
		delete_memsuper (context.memsuper);
	}

	if (changed) {
		cvar_invoke (var);
	}
	return changed;
}

VISIBLE void
Cvar_SetVar (cvar_t *var, const char *value)
{
	if (var->flags & CVAR_ROM) {
		Sys_MaskPrintf (SYS_dev, "Cvar \"%s\" is read-only, cannot modify\n",
						var->name);
		return;
	}
	cvar_setvar (var, value);
}

VISIBLE void
Cvar_Set (const char *var_name, const char *value)
{
	cvar_t     *var;

	var = Cvar_FindVar (var_name);

	if (!var)
		return;

	Cvar_SetVar (var, value);
}

/*
	Cvar_Command

	Handles variable inspection and changing from the console
*/
VISIBLE bool
Cvar_Command (void)
{
	cvar_t     *v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv (0));
	if (!v)
		v = Cvar_FindAlias (Cmd_Argv (0));
	if (!v)
		return false;

	// perform a variable print or set
	if (Cmd_Argc () == 1) {
		Sys_Printf ("\"%s\" is \"%s\"\n", v->name, cvar_string (v));
		return true;
	}

	Cvar_SetVar (v, Cmd_Argv (1));
	return true;
}

static void
cvar_write_variable (void *ele, void *data)
{
	cvar_t     *cvar = ele;
	QFile      *f = data;

	if (cvar->flags & CVAR_ARCHIVE) {
		Qprintf (f, "seta %s \"%s\"\n", cvar->name, cvar_string (cvar));
	}
}

/*
	Cvar_WriteVariables

	Writes lines containing "seta variable value" for all variables
	with the archive flag set to true.
*/
VISIBLE void
Cvar_WriteVariables (QFile *f)
{
	Hash_ForEach (cvar_hash, cvar_write_variable, f);
	Hash_ForEach (user_cvar_hash, cvar_write_variable, f);
}

static void
cvar_write_config (void *ele, void *data)
{
	cvar_t     *cvar = ele;
	plitem_t   *cfg = data;

	if (cvar->flags & CVAR_ARCHIVE) {
		PL_D_AddObject (cfg, cvar->name, PL_NewString (cvar_string (cvar)));
	}
}

VISIBLE void
Cvar_SaveConfig (plitem_t *config)
{
	plitem_t   *cvars = PL_NewDictionary (0);
	PL_D_AddObject (config, "cvars", cvars);
	Hash_ForEach (cvar_hash, cvar_write_config, cvars);
	Hash_ForEach (user_cvar_hash, cvar_write_config, cvars);
}

VISIBLE void
Cvar_LoadConfig (plitem_t *config)
{
	plitem_t   *cvars = PL_ObjectForKey (config, "cvars");

	if (!cvars) {
		return;
	}
	for (int i = 0, count = PL_D_NumKeys (cvars); i < count; i++) {
		const char *cvar_name = PL_KeyAtIndex (cvars, i);
		const char *value = PL_String (PL_ObjectForKey (cvars, cvar_name));
		if (value) {
			cvar_t      *var = Cvar_FindVar (cvar_name);
			if (var) {
				Cvar_SetVar (var, value);
				var->flags |= CVAR_ARCHIVE;
			} else {
				var = cvar_create (cvar_name, value);
				var->flags |= CVAR_ARCHIVE;
			}
		}
	}
}

static void
set_cvar (const char *cmd, int orflags)
{
	cvar_t     *var;
	const char *value;
	const char *var_name;

	if (Cmd_Argc () != 3) {
		Sys_Printf ("usage: %s <cvar> <value>\n", cmd);
		return;
	}

	var_name = Cmd_Argv (1);
	value = Cmd_Argv (2);
	var = Cvar_FindVar (var_name);

	if (!var)
		var = Cvar_FindAlias (var_name);

	if (var) {
		if (var->flags & CVAR_ROM) {
			Sys_MaskPrintf (SYS_dev,
							"Cvar \"%s\" is read-only, cannot modify\n",
							var_name);
		} else {
			Cvar_SetVar (var, value);
			var->flags |= orflags;
		}
	} else {
		var = cvar_create (var_name, value);
		var->flags |= orflags;
	}
}

static void
cvar_set_f (void)
{
	set_cvar ("set", CVAR_NONE);
}

static void
cvar_setrom_f (void)
{
	set_cvar ("setrom", CVAR_ROM);
}

static void
cvar_seta_f (void)
{
	set_cvar ("seta", CVAR_ARCHIVE);
}

static void
cvar_inc_f (void)
{
	cvar_t     *var;
	float       inc = 1;
	const char *name;

	switch (Cmd_Argc ()) {
		default:
		case 1:
			Sys_Printf ("inc <cvar> [amount] : increment cvar\n");
			return;
		case 3:
			inc = atof (Cmd_Argv (2));
		case 2:
			name = Cmd_Argv (1);
			var = Cvar_FindVar (name);
			if (!var)
				var = Cvar_FindAlias (name);
			if (!var) {
				Sys_Printf ("Unknown variable \"%s\"\n", name);
				return;
			}
			if (var->flags & CVAR_ROM) {
				Sys_Printf ("Variable \"%s\" is read-only\n", name);
				return;
			}
			break;
	}
	if (var->value.type == &cexpr_float) {
		*(float *) var->value.value += inc;
	} else if (var->value.type == &cexpr_int) {
		*(int *) var->value.value += inc;
	} else {
		Sys_Printf ("Variable \"%s\" cannot be incremented\n", name);
	}
}

void
Cvar_Toggle (cvar_t *var)
{
	if ((var->flags & CVAR_ROM)
		|| (var->value.type != &cexpr_int && var->value.type != &cexpr_bool)) {
		Sys_Printf ("Variable \"%s\" cannot be toggled\n", Cmd_Argv (1));
		return;
	}

	if (var->value.type == &cexpr_int) {
		*(int *) var->value.value = !*(int *) var->value.value;
	} else {
		*(bool *) var->value.value = !*(bool *) var->value.value;
	}
	cvar_invoke (var);
}

static void
cvar_toggle_f (void)
{
	cvar_t     *var;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("toggle <cvar> : toggle a cvar on/off\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv (1));
	if (!var)
		var = Cvar_FindAlias (Cmd_Argv (1));
	if (!var) {
		Sys_Printf ("Unknown variable \"%s\"\n", Cmd_Argv (1));
		return;
	}
	Cvar_Toggle (var);
}

static void
cvar_cycle_f (void)
{
	int         i;
	const char *name;
	cvar_t     *var;

	if (Cmd_Argc () < 3) {
		Sys_Printf ("cycle <cvar> <value list>: cycle cvar through a list of "
					"values\n");
		return;
	}

	name = Cmd_Argv (1);
	var = Cvar_FindVar (name);
	if (!var)
		var = Cvar_FindAlias (name);
	if (!var) {
		var = cvar_create (name, Cmd_Argv (Cmd_Argc () - 1));
	}

	// loop through the args until you find one that matches the current cvar
	// value. yes, this will get stuck on a list that contains the same value
	// twice. it's not worth dealing with, and i'm not even sure it can be
	// dealt with -- johnfitz
	for (i = 2; i < Cmd_Argc (); i++) {
		// zero is assumed to be a string, even though it could actually be
		// zero. The worst case is that the first time you call this command,
		// it won't match on zero when it should, but after that, it will be
		// comparing string that all had the same source (the user) so it will
		// work.
		if (!strcmp (Cmd_Argv (i), cvar_string (var)))
			break;
	}

	if (i == Cmd_Argc ())
		Cvar_SetVar (var, Cmd_Argv (2));	// no match
	else if (i + 1 == Cmd_Argc ())
		Cvar_SetVar (var, Cmd_Argv (2));	// matched last value in list
	else
		Cvar_SetVar (var, Cmd_Argv (i + 1));	// matched earlier in list
}

VISIBLE void
Cvar_Reset (cvar_t *var)
{
	Cvar_SetVar (var, var->default_value);
}

static void
cvar_reset_f (void)
{
	cvar_t     *var;
	const char *name;

	switch (Cmd_Argc ()) {
		default:
		case 1:
			Sys_Printf ("reset <cvar> : reset cvar to default\n");
			break;
		case 2:
			name = Cmd_Argv (1);
			var = Cvar_FindVar (name);
			if (!var)
				var = Cvar_FindAlias (name);
			if (!var)
				Sys_Printf ("Unknown variable \"%s\"\n", name);
			else
				Cvar_Reset (var);
			break;
	}
}

static void
cvar_reset_var (void *ele, void *data)
{
	cvar_t     *var = ele;
	if (!(var->flags & CVAR_ROM))
		Cvar_Reset (var);
}

static void
cvar_resetall_f (void)
{
	Hash_ForEach (cvar_hash, cvar_reset_var, 0);
}

static int
cvar_cmp (const void *_a, const void *_b)
{
	const cvar_t * const *a = _a;
	const cvar_t * const *b = _b;
	return strcmp ((*a)->name, (*b)->name);
}

static void
cvar_cvarList_f (void)
{
	int         showhelp = 0;
	if (Cmd_Argc () > 1) {
		showhelp = 1;
		if (strequal (Cmd_Argv (1), "cfg"))
			showhelp++;
	}

	void      **cvar_list = Hash_GetList (cvar_hash);
	int         num_vars = Hash_NumElements (cvar_hash);
	heapsort (cvar_list, num_vars, sizeof (void *), cvar_cmp);

	for (cvar_t **cvar = (cvar_t **) cvar_list; *cvar; cvar++) {
		cvar_t     *var = *cvar;
		const char *flags = va (0, "%c%c%c%c",
								var->flags & CVAR_ROM ? 'r' : ' ',
								var->flags & CVAR_ARCHIVE ? '*' : ' ',
								var->flags & CVAR_USERINFO ? 'u' : ' ',
								var->flags & CVAR_SERVERINFO ? 's' : ' ');
		if (showhelp == 2)
			Sys_Printf ("//%s %s\n%s \"%s\"\n\n", flags, var->description,
						var->name, cvar_string (var));
		else if (showhelp)
			Sys_Printf ("%s %-20s : %s\n", flags, var->name, var->description);
		else
			Sys_Printf ("%s %s\n", flags, var->name);
	}

	Sys_Printf ("------------\n%d variables\n", num_vars);
}

static void
cvar_free_memory (void *ele, void *data)
{
	const cvar_t *cvar = ele;
	if (!cvar->value.type) {
		char      **str_value = cvar->value.value;
		free (*str_value);
		*str_value = 0;
	} else if (cvar->value.type->data) {
		exprenum_t *enm = cvar->value.type->data;
		if (enm->symtab && enm->symtab->tab) {
			Hash_DelTable (enm->symtab->tab);
			enm->symtab->tab = 0;
		}
	}

	if (cvar->listeners) {
		DARRAY_CLEAR (cvar->listeners);
		free (cvar->listeners);
	}
}

static void
cvar_shutdown (void *data)
{
	Hash_ForEach (cvar_hash, cvar_free_memory, 0);
	Hash_DelTable (cvar_hash);
	Hash_DelTable (user_cvar_hash);
	Hash_DelTable (calias_hash);
}

static const char *
cvar_get_key (const void *c, void *unused)
{
	cvar_t *cvar = (cvar_t*)c;
	return cvar->name;
}

static void
cvar_free (void *c, void *unused)
{
	cvar_t *cvar = (cvar_t*)c;

	free (*(char **) cvar->value.value);
	free ((char *) cvar->name);
	free (cvar);
}

static void
calias_free (void *c, void *unused)
{
	cvar_alias_t *calias = (cvar_alias_t *) c;
	free (calias->name);
	free (calias);
}

static const char *
calias_get_key (const void *c, void *unused)
{
	cvar_alias_t *calias = (cvar_alias_t *) c;
	return calias->name;
}

VISIBLE void
Cvar_Init_Hash (void)
{
	cvar_hash = Hash_NewTable (1021, cvar_get_key, 0, 0, 0);
	user_cvar_hash = Hash_NewTable (1021, cvar_get_key, cvar_free, 0, 0);
	calias_hash = Hash_NewTable (1021, calias_get_key, calias_free, 0, 0);

	Sys_RegisterShutdown (cvar_shutdown, 0);

}

VISIBLE void
Cvar_Init (void)
{
	Cvar_Register (&developer_cvar, 0, 0);

	Cmd_AddCommand ("set", cvar_set_f, "Set the selected variable, useful on "
					"the command line (+set variablename setting)");
	Cmd_AddCommand ("setrom", cvar_setrom_f, "Set the selected variable and "
					"make it read-only, useful on the command line. "
					"(+setrom variablename setting)");
	Cmd_AddCommand ("seta", cvar_seta_f, "Set the selected variable, and make "
					"it archived, useful on the command line (+seta "
					"variablename setting)");
	Cmd_AddCommand ("toggle", cvar_toggle_f, "Toggle a cvar on or off");
	Cmd_AddCommand ("cvarlist", cvar_cvarList_f, "List all cvars");
	Cmd_AddCommand ("cycle", cvar_cycle_f,
					"Cycle a cvar through a list of values");
	Cmd_AddCommand ("inc", cvar_inc_f, "Increment a cvar");
	Cmd_AddCommand ("reset", cvar_reset_f, "Reset a cvar");
	Cmd_AddCommand ("resetall", cvar_resetall_f, "Reset all cvars");
}

VISIBLE void
Cvar_Register (cvar_t *var, cvar_listener_t listener, void *data)
{
	cvar_t     *user_var;

	if (Cmd_Exists (var->name)) {
		Sys_Printf ("Cvar_Get: %s is a command\n", var->name);
		return;
	}
	if (var->flags & CVAR_REGISTERED) {
		Sys_Error ("Cvar %s already registered", var->name);
	}

	if ((user_var = Hash_Find (user_cvar_hash, var->name))) {
		cvar_setvar (var, cvar_string (user_var));
		cvar_destroy (user_var);
	} else {
		cvar_setvar (var, var->default_value);
	}
	var->flags |= CVAR_REGISTERED;

	if (listener) {
		Cvar_AddListener (var, listener, data);
	}

	Hash_Add (cvar_hash, var);

	cvar_invoke (var);
}

/*
	Cvar_SetFlags

	sets a Cvar's flags simply and easily
*/
VISIBLE void
Cvar_SetFlags (cvar_t *var, int cvarflags)
{
	if (var == NULL)
		return;

	var->flags = cvarflags;
}

typedef struct {
	cvar_select_t select;
	void       *data;
	const cvar_t **list;
	int         index;
} cvar_select_ctx_t;

static void
cvar_select (void *ele, void *data)
{
	const cvar_t *cvar = ele;
	cvar_select_ctx_t *ctx = data;

	if (ctx->select (cvar, ctx->data)) {
		ctx->list[ctx->index++] = cvar;
	}
}

VISIBLE const cvar_t **
Cvar_Select (cvar_select_t select, void *data)
{
	int         num_cvars = Hash_NumElements (cvar_hash)
							+ Hash_NumElements (user_cvar_hash);
	cvar_select_ctx_t ctx = {
		.select = select,
		.data = data,
		.list = malloc ((num_cvars + 1) * sizeof (cvar_t *)),
		.index = 0,
	};
	Hash_ForEach (cvar_hash, cvar_select, &ctx);
	Hash_ForEach (user_cvar_hash, cvar_select, &ctx);
	ctx.list[num_cvars] = 0;
	return ctx.list;
}
