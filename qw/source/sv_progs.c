/*
	sv_progs.c

	Quick QuakeC server code

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/include/server.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_pr_cpqw.h"
#include "qw/include/sv_pr_qwe.h"
#include "world.h"

progs_t     sv_pr_state;
sv_globals_t sv_globals;
sv_funcs_t sv_funcs;
sv_fields_t sv_fields;

edict_t sv_edicts[MAX_EDICTS];
sv_data_t sv_data[MAX_EDICTS];

cvar_t     *r_skyname;
cvar_t     *sv_progs;
cvar_t     *sv_progs_zone;
cvar_t     *sv_progs_ext;
cvar_t     *pr_checkextensions;
cvar_t     *sv_old_entity_free;
cvar_t     *sv_hide_version_info;

static int reserved_edicts = MAX_CLIENTS;

static int sv_range;

static unsigned
bi_map (progs_t *pr, unsigned binum)
{
	unsigned    range;
	unsigned    base = sv_range << PR_RANGE_SHIFT;

	if (sv_range != PR_RANGE_NONE) {
		range = (binum & PR_RANGE_MASK) >> PR_RANGE_SHIFT;

		if (!range) {
			if (binum > PR_RANGE_ID_MAX) {
				binum |= base;
			} else {
				if (PR_FindBuiltinNum (pr, binum | base))
					binum |= base;
			}
		}
	}
	return binum;
}

static void
free_edict (progs_t *pr, edict_t *ent)
{
	if (sv_old_entity_free->int_val) {
		E_fld (ent, sv_fields.model).entity_var = 0;
		E_fld (ent, sv_fields.takedamage).float_var = 0;
		E_fld (ent, sv_fields.modelindex).float_var = 0;
		E_fld (ent, sv_fields.colormap).float_var = 0;
		E_fld (ent, sv_fields.skin).float_var = 0;
		E_fld (ent, sv_fields.frame).float_var = 0;
		E_fld (ent, sv_fields.nextthink).float_var = -1;
		E_fld (ent, sv_fields.solid).float_var = 0;
		memset (&E_fld (ent, sv_fields.origin).vector_var, 0, 3*sizeof (float));
		memset (&E_fld (ent, sv_fields.angles).vector_var, 0, 3*sizeof (float));
	} else {
		ED_ClearEdict (pr, ent, 0);
	}
}

static int
prune_edict (progs_t *pr, edict_t *ent)
{
	if (!sv_globals.skill) {
		if (((int) SVfloat (ent, spawnflags) & SPAWNFLAG_NOT_DEATHMATCH))
			return 1;
	} else {
		// remove things from different skill levels or deathmatch
		if (deathmatch->int_val) {
			if (((int) SVfloat (ent, spawnflags) & SPAWNFLAG_NOT_DEATHMATCH)) {
				return 1;
			}
		} else if ((*sv_globals.skill == 0
					&& ((int) SVfloat (ent, spawnflags)
						& SPAWNFLAG_NOT_EASY))
				   || (*sv_globals.skill == 1
					   && ((int) SVfloat (ent, spawnflags)
						   & SPAWNFLAG_NOT_MEDIUM))
				   || (*sv_globals.skill >= 2
					   && ((int) SVfloat (ent, spawnflags)
						   & SPAWNFLAG_NOT_HARD))) {
			return 1;
		}
	}
	return 0;
}

static void
ED_PrintEdicts_f (void)
{
	ED_PrintEdicts (&sv_pr_state, Cmd_Argv (1));
}

/*
	ED_PrintEdict_f

	For debugging, prints a single edict
*/
static void
ED_PrintEdict_f (void)
{
	int         i;
	const char *fieldname = 0;

	if (Cmd_Argc () < 2) {
		SV_Printf ("edict num [fieldname]\n");
		return;
	}
	if (Cmd_Argc () >= 3) {
		fieldname = Cmd_Argv (2);
	}
	i = atoi (Cmd_Argv (1));
	ED_PrintNum (&sv_pr_state, i, fieldname);
}

static void
ED_Count_f (void)
{
	ED_Count (&sv_pr_state);
}

static void
PR_Profile_f (void)
{
	if (!sv_pr_state.progs) {
		Sys_Printf ("no progs loaded\n");
		return;
	}
	PR_Profile (&sv_pr_state);
}

static void
watch_f (void)
{
	PR_Debug_Watch (&sv_pr_state, Cmd_Argc () < 2 ? 0 : Cmd_Args (1));
}

static void
print_f (void)
{
	PR_Debug_Print (&sv_pr_state, Cmd_Argc () < 2 ? 0 : Cmd_Args (1));
}

static int
parse_field (progs_t *pr, const char *key, const char *value)
{
	/*
		If skyname is set, set what the map thinks the skybox name should
		be. "qlsky" is supported since at least one other map uses it.
	*/
	if (strequal (key, "skyname")		// QuakeForge
		|| strequal (key, "sky")		// Q2/DarkPlaces
		|| strequal (key, "qlsky")) {	// QuakeLives
		Info_SetValueForKey (svs.info, "sky", value, 0);
		return 1;
	}
	if (strequal (key, "fog"))
		return 1;
	if (*key == '_')						// ignore _fields
		return 1;
	return 0;
}

typedef struct sv_def_s {
	etype_t    type;
	unsigned short offset;
	const char *name;
	void      *field;
} sv_def_t;

static sv_def_t qw_self[] = {
	{ev_entity,	28,	"self",				&sv_globals.self},
	{ev_entity,	28,	".self",			&sv_globals.self},
	{ev_void,	0,	0},
};

static sv_def_t qw_defs[] = {
	{ev_entity,	29,	"other",				&sv_globals.other},
	{ev_entity,	30,	"world",				&sv_globals.world},
	{ev_float,	31,	"time",					&sv_globals.time},
	{ev_float,	32,	"frametime",			&sv_globals.frametime},
	{ev_entity,	33,	"newmis",				&sv_globals.newmis},
	{ev_float,	34,	"force_retouch",		&sv_globals.force_retouch},
	{ev_string,	35,	"mapname",				&sv_globals.mapname},
	{ev_float,	36,	"serverflags",			&sv_globals.serverflags},
	{ev_float,	37,	"total_secrets",		&sv_globals.total_secrets},
	{ev_float,	38,	"total_monsters",		&sv_globals.total_monsters},
	{ev_float,	39,	"found_secrets",		&sv_globals.found_secrets},
	{ev_float,	40,	"killed_monsters",		&sv_globals.killed_monsters},
	// parm1-parm16 actually form an array
	{ev_float,	41,	"parm1",				&sv_globals.parms},
	{ev_vector,	57,	"v_forward",			&sv_globals.v_forward},
	{ev_vector,	60,	"v_up",					&sv_globals.v_up},
	{ev_vector,	63,	"v_right",				&sv_globals.v_right},
	{ev_float,	66,	"trace_allsolid",		&sv_globals.trace_allsolid},
	{ev_float,	67,	"trace_startsolid",		&sv_globals.trace_startsolid},
	{ev_float,	68,	"trace_fraction",		&sv_globals.trace_fraction},
	{ev_vector,	69,	"trace_endpos",			&sv_globals.trace_endpos},
	{ev_vector,	72,	"trace_plane_normal",	&sv_globals.trace_plane_normal},
	{ev_float,	75,	"trace_plane_dist",		&sv_globals.trace_plane_dist},
	{ev_entity,	76,	"trace_ent",			&sv_globals.trace_ent},
	{ev_float,	77,	"trace_inopen",			&sv_globals.trace_inopen},
	{ev_float,	78,	"trace_inwater",		&sv_globals.trace_inwater},
	{ev_entity,	79,	"msg_entity",			&sv_globals.msg_entity},
	{ev_void,	0,	0},
};

static sv_def_t qw_funcs[] = {
	{ev_func,	81,	"StartFrame",			&sv_funcs.StartFrame},
	{ev_func,	82,	"PlayerPreThink",		&sv_funcs.PlayerPreThink},
	{ev_func,	83,	"PlayerPostThink",		&sv_funcs.PlayerPostThink},
	{ev_func,	84,	"ClientKill",			&sv_funcs.ClientKill},
	{ev_func,	85,	"ClientConnect",		&sv_funcs.ClientConnect},
	{ev_func,	86,	"PutClientInServer",	&sv_funcs.PutClientInServer},
	{ev_func,	87,	"ClientDisconnect",		&sv_funcs.ClientDisconnect},
	{ev_func,	88,	"SetNewParms",			&sv_funcs.SetNewParms},
	{ev_func,	89,	"SetChangeParms",		&sv_funcs.SetChangeParms},
	{ev_void,	0,	0},
};

static sv_def_t qw_fields[] = {
	{ev_float,	0,	"modelindex",		&sv_fields.modelindex},
	{ev_vector,	1,	"absmin",			&sv_fields.absmin},
	{ev_vector,	4,	"absmax",			&sv_fields.absmax},
	{ev_float,	7,	"ltime",			&sv_fields.ltime},
	{ev_float,	8,	"lastruntime",		&sv_fields.lastruntime},
	{ev_float,	9,	"movetype",			&sv_fields.movetype},
	{ev_float,	10,	"solid",			&sv_fields.solid},
	{ev_vector,	11,	"origin",			&sv_fields.origin},
	{ev_vector,	14,	"oldorigin",		&sv_fields.oldorigin},
	{ev_vector,	17,	"velocity",			&sv_fields.velocity},
	{ev_vector,	20,	"angles",			&sv_fields.angles},
	{ev_vector,	23,	"avelocity",		&sv_fields.avelocity},
	{ev_string,	26,	"classname",		&sv_fields.classname},
	{ev_string,	27,	"model",			&sv_fields.model},
	{ev_float,	28,	"frame",			&sv_fields.frame},
	{ev_float,	29,	"skin",				&sv_fields.skin},
	{ev_float,	30,	"effects",			&sv_fields.effects},
	{ev_vector,	31,	"mins",				&sv_fields.mins},
	{ev_vector,	34,	"maxs",				&sv_fields.maxs},
	{ev_vector,	37,	"size",				&sv_fields.size},
	{ev_func,	40,	"touch",			&sv_fields.touch},
	{ev_func,	42,	"think",			&sv_fields.think},
	{ev_func,	43,	"blocked",			&sv_fields.blocked},
	{ev_float,	44,	"nextthink",		&sv_fields.nextthink},
	{ev_entity,	45,	"groundentity",		&sv_fields.groundentity},
	{ev_float,	46,	"health",			&sv_fields.health},
	{ev_float,	47,	"frags",			&sv_fields.frags},
	{ev_float,	48,	"weapon",			&sv_fields.weapon},
	{ev_string,	49,	"weaponmodel",		&sv_fields.weaponmodel},
	{ev_float,	50,	"weaponframe",		&sv_fields.weaponframe},
	{ev_float,	51,	"currentammo",		&sv_fields.currentammo},
	{ev_float,	52,	"ammo_shells",		&sv_fields.ammo_shells},
	{ev_float,	53,	"ammo_nails",		&sv_fields.ammo_nails},
	{ev_float,	54,	"ammo_rockets",		&sv_fields.ammo_rockets},
	{ev_float,	55,	"ammo_cells",		&sv_fields.ammo_cells},
	{ev_float,	56,	"items",			&sv_fields.items},
	{ev_float,	57,	"takedamage",		&sv_fields.takedamage},
	{ev_entity,	58,	"chain",			&sv_fields.chain},
	{ev_vector,	60,	"view_ofs",			&sv_fields.view_ofs},
	{ev_float,	63,	"button0",			&sv_fields.button0},
	{ev_float,	64,	"button1",			&sv_fields.button1},
	{ev_float,	65,	"button2",			&sv_fields.button2},
	{ev_float,	66,	"impulse",			&sv_fields.impulse},
	{ev_float,	67,	"fixangle",			&sv_fields.fixangle},
	{ev_vector,	68,	"v_angle",			&sv_fields.v_angle},
	{ev_string,	71,	"netname",			&sv_fields.netname},
	{ev_entity,	72,	"enemy",			&sv_fields.enemy},
	{ev_float,	73,	"flags",			&sv_fields.flags},
	{ev_float,	74,	"colormap",			&sv_fields.colormap},
	{ev_float,	75,	"team",				&sv_fields.team},
	{ev_float,	77,	"teleport_time",	&sv_fields.teleport_time},
	{ev_float,	79,	"armorvalue",		&sv_fields.armorvalue},
	{ev_float,	80,	"waterlevel",		&sv_fields.waterlevel},
	{ev_float,	81,	"watertype",		&sv_fields.watertype},
	{ev_float,	82,	"ideal_yaw",		&sv_fields.ideal_yaw},
	{ev_float,	83,	"yaw_speed",		&sv_fields.yaw_speed},
	{ev_entity,	85,	"goalentity",		&sv_fields.goalentity},
	{ev_float,	86,	"spawnflags",		&sv_fields.spawnflags},
	{ev_float,	89,	"dmg_take",			&sv_fields.dmg_take},
	{ev_float,	90,	"dmg_save",			&sv_fields.dmg_save},
	{ev_entity,	91,	"dmg_inflictor",	&sv_fields.dmg_inflictor},
	{ev_entity,	92,	"owner",			&sv_fields.owner},
	{ev_string,	96,	"message",			&sv_fields.message},
	{ev_float,	97,	"sounds",			&sv_fields.sounds},
	{ev_void,	0,	0},
};

static sv_def_t qw_opt_defs[] = {
	{ev_float,	0,	"skill", 	&sv_globals.skill},
	{ev_void,	0,	0},
};

static sv_def_t qw_opt_funcs[] = {
	{ev_func,	0,	"SpectatorConnect",		&sv_funcs.SpectatorConnect},
	{ev_func,	0,	"SpectatorThink",		&sv_funcs.SpectatorThink},
	{ev_func,	0,	"SpectatorDisconnect",	&sv_funcs.SpectatorDisconnect},
	{ev_func,	0,	"UserInfoCallback",		&sv_funcs.UserInfoCallback},
	{ev_func,	0,	"EndFrame",				&sv_funcs.EndFrame},
	{ev_void,	0,	0},
};

static sv_def_t qw_opt_fields[] = {
	{ev_integer,	0,	"rotated_bbox",		&sv_fields.rotated_bbox},
	{ev_float,		0,	"alpha",			&sv_fields.alpha},
	{ev_float,		0,	"scale",			&sv_fields.scale},
	{ev_float,		0,	"glow_size",		&sv_fields.glow_size},
	{ev_float,		0,	"glow_color",		&sv_fields.glow_color},
	{ev_vector,		0,	"colormod",			&sv_fields.colormod},

	{ev_float,		0,	"gravity",			&sv_fields.gravity},
	{ev_float,		0,	"maxspeed",			&sv_fields.maxspeed},
	{ev_void,		0,	0},
};

static const pr_uint_t qw_crc = 54730;

static void
set_address (sv_def_t *def, void *address)
{
	switch (def->type) {
		case ev_void:
		case ev_short:
		case ev_invalid:
		case ev_type_count:
			break;
		case ev_float:
		case ev_vector:
		case ev_quat:
			*(float **)def->field = (float *) address;
			break;
		case ev_double:
			*(double **)def->field = (double *) address;
			break;
		case ev_string:
		case ev_entity:
		case ev_field:
		case ev_func:
		case ev_pointer:
		case ev_integer:
		case ev_uinteger:
			*(pr_int_t **)def->field = (pr_int_t *) address;
			break;
		case ev_long:
		case ev_ulong:
			*(pr_long_t **)def->field = (pr_long_t *) address;
			break;
	}
}

static int
resolve_globals (progs_t *pr, sv_def_t *def, int mode)
{
	pr_def_t   *ddef;
	int         ret = 1;

	if (mode == 2) {
		for (; def->name; def++)
			set_address (def, &G_FLOAT (pr, def->offset));
		return 1;
	}
	for (; def->name; def++) {
		ddef = PR_FindGlobal (pr, def->name);
		if (ddef) {
			set_address (def, &G_FLOAT(pr, ddef->ofs));
		} else if (mode) {
			PR_Undefined (pr, "global", def->name);
			ret = 0;
		}
	}
	return ret;
}

static int
resolve_functions (progs_t *pr, sv_def_t *def, int mode)
{
	dfunction_t *dfunc;
	int         ret = 1;

	if (mode == 2) {
		for (; def->name; def++)
			*(func_t *) def->field = G_FUNCTION (pr, def->offset);
		return 1;
	}
	for (; def->name; def++) {
		dfunc = PR_FindFunction (pr, def->name);
		if (dfunc) {
			*(func_t *) def->field = dfunc - pr->pr_functions;
		} else if (mode) {
			PR_Undefined (pr, "function", def->name);
			ret = 0;
		}
	}
	return ret;
}

static int
resolve_fields (progs_t *pr, sv_def_t *def, int mode)
{
	pr_def_t   *ddef;
	int         ret = 1;

	if (mode == 2) {
		for (; def->name; def++)
			*(int *) def->field = def->offset;
		return 1;
	}
	for (; def->name; def++) {
		*(int *)def->field = -1;
		ddef = PR_FindField (pr, def->name);
		if (ddef) {
			*(int *)def->field = ddef->ofs;
		} else if (mode) {
			PR_Undefined (pr, "field", def->name);
			ret = 0;
		}
	}
	return ret;
}

static int
resolve (progs_t *pr)
{
	int         ret = 1;
	if (pr->progs->crc == qw_crc) {
		resolve_globals (pr, qw_self, 2);
		resolve_globals (pr, qw_defs, 2);
		resolve_functions (pr, qw_funcs, 2);
		resolve_fields (pr, qw_fields, 2);
	} else {
		if (!resolve_globals (pr, qw_self, 0))
			ret = 0;
		if (!resolve_globals (pr, qw_defs, 1))
			ret = 0;
		if (!resolve_functions (pr, qw_funcs, 1))
			ret = 0;
		if (!resolve_fields (pr, qw_fields, 1))
			ret = 0;
	}
	resolve_globals (pr, qw_opt_defs, 0);
	resolve_functions (pr, qw_opt_funcs, 0);
	resolve_fields (pr, qw_opt_fields, 0);
	// progs engine needs these globals anyway
	sv_pr_state.globals.self = sv_globals.self;
	sv_pr_state.globals.ftime = sv_globals.time;//FIXME double time
	return ret;
}

static int
sv_init_edicts (progs_t *pr)
{
	int         i;

	memset (sv_edicts, 0, sizeof (sv_edicts));
	memset (sv_data, 0, sizeof (sv_data));

	// init the data field of the edicts
	for (i = 0; i < MAX_EDICTS; i++) {
		edict_t    *ent = EDICT_NUM (&sv_pr_state, i);
		ent->pr = &sv_pr_state;
		ent->entnum = i;
		ent->edict = EDICT_TO_PROG (&sv_pr_state, ent);
		ent->edata = &sv_data[i];
		SVdata (ent)->edict = ent;
	}

	return 1;
}

void
SV_LoadProgs (void)
{
	const char *progs_name = "qwprogs.dat";
	const char *range;

	if (strequal (sv_progs_ext->string, "qf")) {
		sv_range = PR_RANGE_QF;
		range = "QF";
	} else if (strequal (sv_progs_ext->string, "id")) {
		sv_range = PR_RANGE_ID;
		range = "ID";
	} else if (strequal (sv_progs_ext->string, "qwe")
			   || strequal (sv_progs_ext->string, "ktpro")) {
		sv_range = PR_RANGE_QWE;
		range = "QWE/KTPro";
	} else if (strequal (sv_progs_ext->string, "cpqw")) {
		sv_range = PR_RANGE_CPQW;
		range = "CPQW";
	} else {
		sv_range = PR_RANGE_NONE;
		range = "None";
	}
	Sys_MaskPrintf (SYS_dev, "Using %s builtin extention mapping\n", range);

	memset (&sv_globals, 0, sizeof (sv_funcs));
	memset (&sv_funcs, 0, sizeof (sv_funcs));
	Info_RemoveKey (svs.info, "sky");
	if (*r_skyname->string)
		Info_SetValueForKey (svs.info, "sky", r_skyname->string, 0);

	sv_cbuf->unknown_command = 0;
	ucmd_unknown = 0;

	if (qfs_gamedir->gamecode && *qfs_gamedir->gamecode)
		progs_name = qfs_gamedir->gamecode;
	if (*sv_progs->string)
		progs_name = sv_progs->string;

	sv_pr_state.max_edicts = MAX_EDICTS;
	sv_pr_state.zone_size = sv_progs_zone->int_val * 1024;
	sv.edicts = sv_edicts;

	PR_LoadProgs (&sv_pr_state, progs_name);
	if (!sv_pr_state.progs)
		Sys_Error ("SV_LoadProgs: couldn't load %s", progs_name);
}

void
SV_Progs_Init (void)
{
	SV_Progs_Init_Cvars ();

	pr_gametype = "quakeworld";
	sv_pr_state.pr_edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.reserved_edicts = &reserved_edicts;
	sv_pr_state.unlink = SV_UnlinkEdict;
	sv_pr_state.flush = SV_FlushSignon;
	sv_pr_state.parse_field = parse_field;
	sv_pr_state.prune_edict = prune_edict;
	sv_pr_state.free_edict = free_edict; // eww, I hate the need for this :(
	sv_pr_state.bi_map = bi_map;
	sv_pr_state.resolve = resolve;

	PR_AddLoadFunc (&sv_pr_state, sv_init_edicts);
	PR_Init (&sv_pr_state);

	SV_PR_Cmds_Init ();
	SV_PR_QWE_Init (&sv_pr_state);
	SV_PR_CPQW_Init (&sv_pr_state);

	Cmd_AddCommand ("edict", ED_PrintEdict_f, "Report information on a given "
					"edict in the game. (edict (edict number))");
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f,
					"Display information on all edicts in the game.");
	Cmd_AddCommand ("edictcount", ED_Count_f,
					"Display summary information on the edicts in the game.");
	Cmd_AddCommand ("profile", PR_Profile_f, "FIXME: Report information about "
					"QuakeC Stuff (\?\?\?) No Description");
	Cmd_AddCommand ("watch", watch_f, "set watchpoint");
	Cmd_AddCommand ("print", print_f, "print value at location");
}

void
SV_Progs_Init_Cvars (void)
{
	PR_Init_Cvars ();

	r_skyname = Cvar_Get ("r_skyname", "", CVAR_NONE, NULL,
						 "Default name of skybox if none given by map");
	sv_progs = Cvar_Get ("sv_progs", "", CVAR_NONE, NULL,
						 "Override the default game progs.");
	sv_progs_zone = Cvar_Get ("sv_progs_zone", "256", CVAR_NONE, NULL,
							  "size of the zone for progs in kB");
	sv_progs_ext = Cvar_Get ("sv_progs_ext", "qf", CVAR_NONE, NULL,
							 "extention mapping to use: "
							 "none, id, qf, qwe, ktpro, cpqw");
	pr_checkextensions = Cvar_Get ("pr_checkextensions", "1", CVAR_ROM, NULL,
								   "indicate the presence of the "
								   "checkextentions qc function");
	sv_old_entity_free = Cvar_Get ("sv_old_entity_free", "1", CVAR_NONE, NULL,
								   "set this for buggy mods that rely on the"
								   " old behaviour of entity freeing (eg,"
								   " *TF)");
	sv_hide_version_info = Cvar_Get ("sv_hide_version_info", "0", CVAR_ROM,
									 NULL, "hide QuakeForge specific "
									 "serverinfo strings from terminally "
									 "stupid progs (eg, braindead TF "
									 "variants)");
}
