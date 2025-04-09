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
#include "world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"

progs_t     sv_pr_state;
sv_globals_t sv_globals;
sv_funcs_t sv_funcs;
sv_fields_t sv_fields;

edict_t sv_edicts[MAX_EDICTS];
sv_data_t sv_data[MAX_EDICTS];

char *sv_progs;
static cvar_t sv_progs_cvar = {
	.name = "sv_progs",
	.description =
		"Override the default game progs.",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_progs },
};
int sv_progs_zone;
static cvar_t sv_progs_zone_cvar = {
	.name = "sv_progs_zone",
	.description =
		"size of the zone for progs in kb",
	.default_value = "256",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_progs_zone },
};
int sv_progs_stack;
static cvar_t sv_progs_stack_cvar = {
	.name = "sv_progs_stack",
	.description =
		"size of the stack for progs in kb",
	.default_value = "256",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_progs_stack },
};
char *sv_progs_ext;
static cvar_t sv_progs_ext_cvar = {
	.name = "sv_progs_ext",
	.description =
		"extention mapping to use: none, id, qf",
	.default_value = "qf",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_progs_ext },
};
float pr_checkextensions;
static cvar_t pr_checkextensions_cvar = {
	.name = "pr_checkextensions",
	.description =
		"indicate the presence of the checkextentions qc function",
	.default_value = "1",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_float, .value = &pr_checkextensions },
};

float nomonsters;
static cvar_t nomonsters_cvar = {
	.name = "nomonsters",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &nomonsters },
};
float gamecfg;
static cvar_t gamecfg_cvar = {
	.name = "gamecfg",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &gamecfg },
};
float scratch1;
static cvar_t scratch1_cvar = {
	.name = "scratch1",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scratch1 },
};
float scratch2;
static cvar_t scratch2_cvar = {
	.name = "scratch2",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scratch2 },
};
float scratch3;
static cvar_t scratch3_cvar = {
	.name = "scratch3",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scratch3 },
};
float scratch4;
static cvar_t scratch4_cvar = {
	.name = "scratch4",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scratch4 },
};
float savedgamecfg;
static cvar_t savedgamecfg_cvar = {
	.name = "savedgamecfg",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &savedgamecfg },
};
float saved1;
static cvar_t saved1_cvar = {
	.name = "saved1",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &saved1 },
};
float saved2;
static cvar_t saved2_cvar = {
	.name = "saved2",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &saved2 },
};
float saved3;
static cvar_t saved3_cvar = {
	.name = "saved3",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &saved3 },
};
float saved4;
static cvar_t saved4_cvar = {
	.name = "saved4",
	.description =
		"No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &saved4 },
};

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
	if (deathmatch) {
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
	const char *fieldname = 0;

	if (Cmd_Argc () < 2) {
		Sys_Printf ("edict num [fieldname]\n");
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
	{ev_float,	80,	"teleport_time",	&sv_fields.teleport_time},
	{ev_float,	82,	"armorvalue",		&sv_fields.armorvalue},
	{ev_float,	83,	"waterlevel",		&sv_fields.waterlevel},
	{ev_float,	84,	"watertype",		&sv_fields.watertype},
	{ev_float,	85,	"ideal_yaw",		&sv_fields.ideal_yaw},
	{ev_float,	86,	"yaw_speed",		&sv_fields.yaw_speed},
	{ev_entity,	88,	"goalentity",		&sv_fields.goalentity},
	{ev_float,	89,	"spawnflags",		&sv_fields.spawnflags},
	{ev_float,	92,	"dmg_take",			&sv_fields.dmg_take},
	{ev_float,	93,	"dmg_save",			&sv_fields.dmg_save},
	{ev_entity,	94,	"dmg_inflictor",	&sv_fields.dmg_inflictor},
	{ev_entity,	95,	"owner",			&sv_fields.owner},
	{ev_vector,	96,	"movedir",			&sv_fields.movedir},
	{ev_string,	99,	"message",			&sv_fields.message},
	{ev_float,	100,	"sounds",		&sv_fields.sounds},
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
	{ev_int,		0,	"rotated_bbox",		&sv_fields.rotated_bbox},
	{ev_float,		0,	"alpha",			&sv_fields.alpha},
	{ev_float,		0,	"gravity",			&sv_fields.gravity},
	{ev_float,		0,	"items2",			&sv_fields.items2},
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
		case ev_ushort:
		case ev_invalid:
		case ev_type_count:
			break;
		case ev_float:
		case ev_vector:
		case ev_quaternion:
			*(float **)def->field = (float *) address;
			break;
		case ev_double:
			*(double **)def->field = (double *) address;
			break;
		case ev_string:
		case ev_entity:
		case ev_field:
		case ev_func:
		case ev_ptr:
		case ev_int:
		case ev_uint:
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
			*(pr_func_t *) def->field = G_FUNCTION (pr, def->offset);
		return 1;
	}
	for (; def->name; def++) {
		dfunc = PR_FindFunction (pr, def->name);
		if (dfunc) {
			*(pr_func_t *) def->field = dfunc - pr->pr_functions;
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
	for (i = 0; i < sv.max_edicts; i++) {
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
	const char *progs_name = "progs.dat";
	const char *range;

	if (strequal (sv_progs_ext, "qf")) {
		sv_range = PR_RANGE_QF;
		range = "QF";
	} else if (strequal (sv_progs_ext, "id")) {
		sv_range = PR_RANGE_ID;
		range = "ID";
	} else {
		sv_range = PR_RANGE_NONE;
		range = "None";
	}
	Sys_MaskPrintf (SYS_dev, "Using %s builtin extention mapping\n", range);

	memset (&sv_globals, 0, sizeof (sv_funcs));
	memset (&sv_funcs, 0, sizeof (sv_funcs));

	if (qfs_gamedir->gamecode && *qfs_gamedir->gamecode)
		progs_name = qfs_gamedir->gamecode;
	if (*sv_progs)
		progs_name = sv_progs;

	sv.edicts = sv_edicts;
	sv_pr_state.max_edicts = sv.max_edicts;
	sv_pr_state.zone_size = sv_progs_zone * 1024;
	sv_pr_state.stack_size = sv_progs_stack * 1024;
	sv.edicts = sv_edicts;

	PR_LoadProgs (&sv_pr_state, progs_name);
	if (!sv_pr_state.progs)
		Host_Error ("SV_LoadProgs: couldn't load %s", progs_name);
}

static void
SV_Progs_Shutdown (void *data)
{
	PR_Shutdown (&sv_pr_state);
}

void
SV_Progs_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (SV_Progs_Shutdown, 0);
	SV_Progs_Init_Cvars ();

	pr_gametype = "netquake";
	sv_pr_state.pr_edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.reserved_edicts = &svs.maxclients;
	sv_pr_state.unlink = SV_UnlinkEdict;
	sv_pr_state.parse_field = parse_field;
	sv_pr_state.prune_edict = prune_edict;
	sv_pr_state.bi_map = bi_map;
	sv_pr_state.resolve = resolve;

	PR_AddLoadFunc (&sv_pr_state, sv_init_edicts);
	PR_Init (&sv_pr_state);
	PR_AddLoadFunc (&sv_pr_state, sv_init_edicts);

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
	PR_Init_Cvars ();
	Cvar_Register (&sv_progs_cvar, 0, 0);
	Cvar_Register (&sv_progs_zone_cvar, 0, 0);
	Cvar_Register (&sv_progs_stack_cvar, 0, 0);
	Cvar_Register (&sv_progs_ext_cvar, 0, 0);
	Cvar_Register (&pr_checkextensions_cvar, 0, 0);

	Cvar_Register (&nomonsters_cvar, 0, 0);
	Cvar_Register (&gamecfg_cvar, 0, 0);
	Cvar_Register (&scratch1_cvar, 0, 0);
	Cvar_Register (&scratch2_cvar, 0, 0);
	Cvar_Register (&scratch3_cvar, 0, 0);
	Cvar_Register (&scratch4_cvar, 0, 0);
	Cvar_Register (&savedgamecfg_cvar, 0, 0);
	Cvar_Register (&saved1_cvar, 0, 0);
	Cvar_Register (&saved2_cvar, 0, 0);
	Cvar_Register (&saved3_cvar, 0, 0);
	Cvar_Register (&saved4_cvar, 0, 0);
}
