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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"
#include "host.h"
#include "server.h"
#include "sv_progs.h"
#include "world.h"

progs_t     sv_pr_state;
sv_globals_t sv_globals;
sv_funcs_t sv_funcs;
sv_fields_t sv_fields;

sv_data_t sv_data[MAX_EDICTS];

cvar_t     *sv_progs;
cvar_t     *sv_progs_zone;
cvar_t     *sv_progs_ext;
cvar_t     *pr_checkextensions;

cvar_t     *nomonsters;
cvar_t     *gamecfg;
cvar_t     *scratch1;
cvar_t     *scratch2;
cvar_t     *scratch3;
cvar_t     *scratch4;
cvar_t     *savedgamecfg;
cvar_t     *saved1;
cvar_t     *saved2;
cvar_t     *saved3;
cvar_t     *saved4;

static int sv_range;

static unsigned
bi_map (progs_t *pr, unsigned binum)
{
	unsigned    range;

	if (sv_range != PR_RANGE_NONE) {
		range = (binum & PR_RANGE_MASK) >> PR_RANGE_SHIFT;

		if (!range && binum > PR_RANGE_ID_MAX)
			binum |= sv_range << PR_RANGE_SHIFT;
	}
	return binum;
}

static int
prune_edict (progs_t *pr, edict_t *ent)
{
	// remove things from different skill levels or deathmatch
	if (deathmatch->int_val) {
		if (((int) SVfloat (ent, spawnflags)
			& SPAWNFLAG_NOT_DEATHMATCH)) {
			return 1;
		}
	} else if ((current_skill == 0
				&& ((int) SVfloat (ent, spawnflags)
					& SPAWNFLAG_NOT_EASY))
			   || (current_skill == 1
				   && ((int) SVfloat (ent, spawnflags)
					   & SPAWNFLAG_NOT_MEDIUM))
			   || (current_skill >= 2
				   && ((int) SVfloat (ent, spawnflags)
					   & SPAWNFLAG_NOT_HARD))) {
		return 1;
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

	i = atoi (Cmd_Argv (1));
	Sys_Printf ("\n EDICT %i:\n", i);
	ED_PrintNum (&sv_pr_state, i);
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
	if (strequal (key, "sky")
		|| strequal (key, "skyname")
		|| strequal (key, "qlsky")
		|| strequal (key, "fog"))
		return 1;
	if (strequal (key, "mapversion"))		// ignore HL(?) version field
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

static sv_def_t nq_self[] = {
	{ev_entity,	28,	"self",				&sv_globals.self},
	{ev_entity,	28,	".self",			&sv_globals.self},
	{ev_void,	0,	0},
};

static sv_def_t nq_defs[] = {
	{ev_entity,	29,	"other",				&sv_globals.other},
	{ev_entity,	30,	"world",				&sv_globals.world},
	{ev_float,	31,	"time",					&sv_globals.time},
	{ev_float,	32,	"frametime",			&sv_globals.frametime},
	{ev_float,	33,	"force_retouch",		&sv_globals.force_retouch},
	{ev_string,	34,	"mapname",				&sv_globals.mapname},
	{ev_float,	35,	"deathmatch",			&sv_globals.deathmatch},
	{ev_float,	36,	"coop",					&sv_globals.coop},
	{ev_float,	37,	"teamplay",				&sv_globals.teamplay},
	{ev_float,	38,	"serverflags",			&sv_globals.serverflags},
	{ev_float,	39,	"total_secrets",		&sv_globals.total_secrets},
	{ev_float,	40,	"total_monsters",		&sv_globals.total_monsters},
	{ev_float,	41,	"found_secrets",		&sv_globals.found_secrets},
	{ev_float,	42,	"killed_monsters",		&sv_globals.killed_monsters},
	//parm1-16 form an array
	{ev_float,	43,	"parm1",				&sv_globals.parms},
	{ev_vector,	59,	"v_forward",			&sv_globals.v_forward},
	{ev_vector,	62,	"v_up",					&sv_globals.v_up},
	{ev_vector,	65,	"v_right",				&sv_globals.v_right},
	{ev_float,	68,	"trace_allsolid",		&sv_globals.trace_allsolid},
	{ev_float,	69,	"trace_startsolid",		&sv_globals.trace_startsolid},
	{ev_float,	70,	"trace_fraction",		&sv_globals.trace_fraction},
	{ev_vector,	71,	"trace_endpos",			&sv_globals.trace_endpos},
	{ev_vector,	74,	"trace_plane_normal",	&sv_globals.trace_plane_normal},
	{ev_float,	77,	"trace_plane_dist",		&sv_globals.trace_plane_dist},
	{ev_entity,	78,	"trace_ent",			&sv_globals.trace_ent},
	{ev_float,	79,	"trace_inopen",			&sv_globals.trace_inopen},
	{ev_float,	80,	"trace_inwater",		&sv_globals.trace_inwater},
	{ev_entity,	81,	"msg_entity",			&sv_globals.msg_entity},
	{ev_void,	0,	0},
};

static sv_def_t nq_funcs[] = {
	{ev_func,	82,	"main",					&sv_funcs.main},
	{ev_func,	83,	"StartFrame",			&sv_funcs.StartFrame},
	{ev_func,	84,	"PlayerPreThink",		&sv_funcs.PlayerPreThink},
	{ev_func,	85,	"PlayerPostThink",		&sv_funcs.PlayerPostThink},
	{ev_func,	86,	"ClientKill",			&sv_funcs.ClientKill},
	{ev_func,	87,	"ClientConnect",		&sv_funcs.ClientConnect},
	{ev_func,	88,	"PutClientInServer",	&sv_funcs.PutClientInServer},
	{ev_func,	89,	"ClientDisconnect",		&sv_funcs.ClientDisconnect},
	{ev_func,	90,	"SetNewParms",			&sv_funcs.SetNewParms},
	{ev_func,	91,	"SetChangeParms",		&sv_funcs.SetChangeParms},
	{ev_void,	0,	0},
};

static sv_def_t nq_fields[] = {
	{ev_float,	0,	"modelindex",		&sv_fields.modelindex},
	{ev_vector,	1,	"absmin",			&sv_fields.absmin},
	{ev_vector,	4,	"absmax",			&sv_fields.absmax},
	{ev_float,	7,	"ltime",			&sv_fields.ltime},
	{ev_float,	8,	"movetype",			&sv_fields.movetype},
	{ev_float,	9,	"solid",			&sv_fields.solid},
	{ev_vector,	10,	"origin",			&sv_fields.origin},
	{ev_vector,	13,	"oldorigin",		&sv_fields.oldorigin},
	{ev_vector,	16,	"velocity",			&sv_fields.velocity},
	{ev_vector,	19,	"angles",			&sv_fields.angles},
	{ev_vector,	22,	"avelocity",		&sv_fields.avelocity},
	{ev_vector,	25,	"punchangle",		&sv_fields.punchangle},
	{ev_string,	28,	"classname",		&sv_fields.classname},
	{ev_string,	29,	"model",			&sv_fields.model},
	{ev_float,	30,	"frame",			&sv_fields.frame},
	{ev_float,	31,	"skin",				&sv_fields.skin},
	{ev_float,	32,	"effects",			&sv_fields.effects},
	{ev_vector,	33,	"mins",				&sv_fields.mins},
	{ev_vector,	36,	"maxs",				&sv_fields.maxs},
	{ev_vector,	39,	"size",				&sv_fields.size},
	{ev_func,	42,	"touch",			&sv_fields.touch},
	{ev_func,	43,	"use",				&sv_fields.use},
	{ev_func,	44,	"think",			&sv_fields.think},
	{ev_func,	45,	"blocked",			&sv_fields.blocked},
	{ev_float,	46,	"nextthink",		&sv_fields.nextthink},
	{ev_entity,	47,	"groundentity",		&sv_fields.groundentity},
	{ev_float,	48,	"health",			&sv_fields.health},
	{ev_float,	49,	"frags",			&sv_fields.frags},
	{ev_float,	50,	"weapon",			&sv_fields.weapon},
	{ev_string,	51,	"weaponmodel",		&sv_fields.weaponmodel},
	{ev_float,	52,	"weaponframe",		&sv_fields.weaponframe},
	{ev_float,	53,	"currentammo",		&sv_fields.currentammo},
	{ev_float,	54,	"ammo_shells",		&sv_fields.ammo_shells},
	{ev_float,	55,	"ammo_nails",		&sv_fields.ammo_nails},
	{ev_float,	56,	"ammo_rockets",		&sv_fields.ammo_rockets},
	{ev_float,	57,	"ammo_cells",		&sv_fields.ammo_cells},
	{ev_float,	58,	"items",			&sv_fields.items},
	{ev_float,	59,	"takedamage",		&sv_fields.takedamage},
	{ev_entity,	60,	"chain",			&sv_fields.chain},
	{ev_float,	61,	"deadflag",			&sv_fields.deadflag},
	{ev_vector,	62,	"view_ofs",			&sv_fields.view_ofs},
	{ev_float,	65,	"button0",			&sv_fields.button0},
	{ev_float,	66,	"button1",			&sv_fields.button1},
	{ev_float,	67,	"button2",			&sv_fields.button2},
	{ev_float,	68,	"impulse",			&sv_fields.impulse},
	{ev_float,	69,	"fixangle",			&sv_fields.fixangle},
	{ev_vector,	70,	"v_angle",			&sv_fields.v_angle},
	{ev_float,	73,	"idealpitch",		&sv_fields.idealpitch},
	{ev_string,	74,	"netname",			&sv_fields.netname},
	{ev_entity,	75,	"enemy",			&sv_fields.enemy},
	{ev_float,	76,	"flags",			&sv_fields.flags},
	{ev_float,	77,	"colormap",			&sv_fields.colormap},
	{ev_float,	78,	"team",				&sv_fields.team},
	{ev_float,	79,	"max_health",		&sv_fields.max_health},
	{ev_float,	80,	"teleport_time",	&sv_fields.teleport_time},
	{ev_float,	81,	"armortype",		&sv_fields.armortype},
	{ev_float,	82,	"armorvalue",		&sv_fields.armorvalue},
	{ev_float,	83,	"waterlevel",		&sv_fields.waterlevel},
	{ev_float,	84,	"watertype",		&sv_fields.watertype},
	{ev_float,	85,	"ideal_yaw",		&sv_fields.ideal_yaw},
	{ev_float,	86,	"yaw_speed",		&sv_fields.yaw_speed},
	{ev_entity,	87,	"aiment",			&sv_fields.aiment},
	{ev_entity,	88,	"goalentity",		&sv_fields.goalentity},
	{ev_float,	89,	"spawnflags",		&sv_fields.spawnflags},
	{ev_string,	90,	"target",			&sv_fields.target},
	{ev_string,	91,	"targetname",		&sv_fields.targetname},
	{ev_float,	92,	"dmg_take",			&sv_fields.dmg_take},
	{ev_float,	93,	"dmg_save",			&sv_fields.dmg_save},
	{ev_entity,	94,	"dmg_inflictor",	&sv_fields.dmg_inflictor},
	{ev_entity,	95,	"owner",			&sv_fields.owner},
	{ev_vector,	96,	"movedir",			&sv_fields.movedir},
	{ev_string,	99,	"message",			&sv_fields.message},
	{ev_float,	100,	"sounds",		&sv_fields.sounds},
	{ev_string,	101,	"noise",		&sv_fields.noise},
	{ev_string,	102,	"noise1",		&sv_fields.noise1},
	{ev_string,	103,	"noise2",		&sv_fields.noise2},
	{ev_string,	104,	"noise3",		&sv_fields.noise3},
	{ev_void,	0,	0},
};

static sv_def_t nq_opt_defs[] = {
	{ev_entity, 0,	"newmis",			&sv_globals.newmis},
	{ev_void,	0,	0},
};

static sv_def_t nq_opt_funcs[] = {
	{ev_func,	0,	"EndFrame",				&sv_funcs.EndFrame},
	{ev_void,	0,	0},
};

static sv_def_t nq_opt_fields[] = {
	{ev_integer,	0,	"rotated_bbox",		&sv_fields.rotated_bbox},
	{ev_float,		0,	"alpha",			&sv_fields.alpha},
	{ev_float,		0,	"gravity",			&sv_fields.gravity},
	// Quake 2 fields?
	{ev_float,		0,	"dmg",				&sv_fields.dmg},
	{ev_float,		0,	"dmgtime",			&sv_fields.dmgtime},
	{ev_float,		0,	"air_finished",		&sv_fields.air_finished},
	{ev_float,		0,	"pain_finished",	&sv_fields.pain_finished},
	{ev_float,		0,	"radsuit_finished",	&sv_fields.radsuit_finished},
	{ev_float,		0,	"speed",			&sv_fields.speed},
	{ev_float,		0,	"basevelocity",		&sv_fields.basevelocity},
	{ev_float,		0,	"drawPercent",		&sv_fields.drawPercent},
	{ev_float,		0,	"mass",				&sv_fields.mass},
	{ev_float,		0,	"light_level",		&sv_fields.light_level},
	{ev_float,		0,	"items2",			&sv_fields.items2},
	{ev_float,		0,	"pitch_speed",		&sv_fields.pitch_speed},
	{ev_float,		0,	"lastruntime",		&sv_fields.lastruntime},
	{ev_void,		0,	0},
};

static const pr_uint_t nq_crc = 5927;

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
			*(int **)def->field = (int *) address;
			break;
	}
}

static int
resolve_globals (progs_t *pr, sv_def_t *def, int mode)
{
	ddef_t     *ddef;
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
	ddef_t     *ddef;
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
	if (pr->progs->crc == nq_crc) {
		resolve_globals (pr, nq_self, 2);
		resolve_globals (pr, nq_defs, 2);
		resolve_functions (pr, nq_funcs, 2);
		resolve_fields (pr, nq_fields, 2);
	} else {
		if (!resolve_globals (pr, nq_self, 0))
			ret = 0;
		if (!resolve_globals (pr, nq_defs, 1))
			ret = 0;
		if (!resolve_functions (pr, nq_funcs, 1))
			ret = 0;
		if (!resolve_fields (pr, nq_fields, 1))
			ret = 0;
	}
	resolve_globals (pr, nq_opt_defs, 0);
	resolve_functions (pr, nq_opt_funcs, 0);
	resolve_fields (pr, nq_opt_fields, 0);
	// progs engine needs these globals anyway
	sv_pr_state.globals.self = sv_globals.self;
	sv_pr_state.globals.time = sv_globals.time;
	return ret;
}

void
SV_LoadProgs (void)
{
	const char *progs_name = "progs.dat";
	const char *range;
	int         i;

	if (strequal (sv_progs_ext->string, "qf")) {
		sv_range = PR_RANGE_QF;
		range = "QF";
	} else if (strequal (sv_progs_ext->string, "id")) {
		sv_range = PR_RANGE_ID;
		range = "ID";
	} else {
		sv_range = PR_RANGE_NONE;
		range = "None";
	}
	Sys_MaskPrintf (SYS_DEV, "Using %s builtin extention mapping\n", range);

	memset (&sv_globals, 0, sizeof (sv_funcs));
	memset (&sv_funcs, 0, sizeof (sv_funcs));

	if (qfs_gamedir->gamecode && *qfs_gamedir->gamecode)
		progs_name = qfs_gamedir->gamecode;
	if (*sv_progs->string)
		progs_name = sv_progs->string;

	sv_pr_state.max_edicts = sv.max_edicts;
	sv_pr_state.zone_size = sv_progs_zone->int_val * 1024;
	PR_LoadProgs (&sv_pr_state, progs_name);
	if (!sv_pr_state.progs)
		Host_Error ("SV_LoadProgs: couldn't load %s", progs_name);

	memset (sv_data, 0, sizeof (sv_data));

	// init the data field of the edicts
	for (i = 0; i < sv.max_edicts; i++) {
		edict_t    *ent = EDICT_NUM (&sv_pr_state, i);
		ent->entnum = i;
		ent->edata = &sv_data[i];
		SVdata (ent)->edict = ent;
	}
}

void
SV_Progs_Init (void)
{
	pr_gametype = "netquake";
	sv_pr_state.edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.reserved_edicts = &svs.maxclients;
	sv_pr_state.unlink = SV_UnlinkEdict;
	sv_pr_state.parse_field = parse_field;
	sv_pr_state.prune_edict = prune_edict;
	sv_pr_state.bi_map = bi_map;
	sv_pr_state.resolve = resolve;

	SV_PR_Cmds_Init ();

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
	sv_progs = Cvar_Get ("sv_progs", "", CVAR_NONE, NULL,
						 "Override the default game progs.");
	sv_progs_zone = Cvar_Get ("sv_progs_zone", "256", CVAR_NONE, NULL,
							  "size of the zone for progs in kb");
	sv_progs_ext = Cvar_Get ("sv_progs_ext", "qf", CVAR_NONE, NULL,
							 "extention mapping to use: "
							 "none, id, qf");
	pr_checkextensions = Cvar_Get ("pr_checkextensions", "1", CVAR_ROM, NULL,
								   "indicate the presence of the "
								   "checkextentions qc function");

	nomonsters = Cvar_Get ("nomonsters", "0", CVAR_NONE, NULL,
						   "No Description");
	gamecfg = Cvar_Get ("gamecfg", "0", CVAR_NONE, NULL, "No Description");
	scratch1 = Cvar_Get ("scratch1", "0", CVAR_NONE, NULL, "No Description");
	scratch2 = Cvar_Get ("scratch2", "0", CVAR_NONE, NULL, "No Description");
	scratch3 = Cvar_Get ("scratch3", "0", CVAR_NONE, NULL, "No Description");
	scratch4 = Cvar_Get ("scratch4", "0", CVAR_NONE, NULL, "No Description");
	savedgamecfg = Cvar_Get ("savedgamecfg", "0", CVAR_ARCHIVE, NULL,
							 "No Description");
	saved1 = Cvar_Get ("saved1", "0", CVAR_ARCHIVE, NULL, "No Description");
	saved2 = Cvar_Get ("saved2", "0", CVAR_ARCHIVE, NULL, "No Description");
	saved3 = Cvar_Get ("saved3", "0", CVAR_ARCHIVE, NULL, "No Description");
	saved4 = Cvar_Get ("saved4", "0", CVAR_ARCHIVE, NULL, "No Description");
}
