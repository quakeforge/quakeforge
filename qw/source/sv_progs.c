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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
#include "string.h"
#endif
#ifdef HAVE_STRINGS_H
#include "strings.h"
#endif

#include "QF/cmd.h"
#include "compat.h"
#include "QF/cvar.h"

#include "server.h"
#include "progdefs.h"
#include "sv_progs.h"
#include "world.h"

sv_globals_t sv_globals;
sv_funcs_t sv_funcs;
sv_fields_t sv_fields;

progs_t	    sv_pr_state;
cvar_t     *r_skyname;
cvar_t     *sv_progs;
cvar_t     *pr_checkextentions;

func_t	EndFrame;
func_t	SpectatorConnect;
func_t	SpectatorDisconnect;
func_t	SpectatorThink;

static int reserved_edicts = MAX_CLIENTS;

static int
prune_edict (progs_t *pr, edict_t *ent)
{
	if (((int) SVFIELD (ent, spawnflags, float) & SPAWNFLAG_NOT_DEATHMATCH))
		return 1;
	return 0;
}

void
ED_PrintEdicts_f (void)
{
	ED_PrintEdicts (&sv_pr_state);
}

/*
	ED_PrintEdict_f

	For debugging, prints a single edicy
*/
void
ED_PrintEdict_f (void)
{
	int         i;

	i = atoi (Cmd_Argv (1));
	Con_Printf ("\n EDICT %i:\n", i);
	ED_PrintNum (&sv_pr_state, i);
}

void
ED_Count_f (void)
{
	ED_Count (&sv_pr_state);
}

void
PR_Profile_f (void)
{
	PR_Profile (&sv_pr_state);
}

static int
parse_field (progs_t *pr, char *key, char *value)
{
	/*
		If skyname is set, we want to allow skyboxes and set what the skybox
		name should be.  "qlsky" is supported since at least one other map
		uses it already.
	*/
	if (strcaseequal (key, "skyname")		// QuakeForge
		|| strcaseequal (key, "sky")		// Q2/DarkPlaces
		|| strcaseequal (key, "qlsky")) {	// QuakeLives
		Info_SetValueForKey (svs.info, "skybox", "1", MAX_SERVERINFO_STRING);
		Cvar_Set (r_skyname, value);
		return 1;
	}
	return 0;
}

void
SV_LoadProgs (void)
{
	dfunction_t *f;

	PR_LoadProgs (&sv_pr_state, sv_progs->string);
	if (!sv_pr_state.progs)
		SV_Error ("SV_LoadProgs: couldn't load %s", sv_progs->string);
	if (sv_pr_state.progs->crc != PROGHEADER_CRC)
		SV_Error ("You must have the qwprogs.dat from QuakeWorld installed");
	// progs engine needs these globals anyway
	sv_globals.self = sv_pr_state.globals.self;
	sv_globals.time = sv_pr_state.globals.time;

	(void *) sv_globals.other = PR_GetGlobalPointer (&sv_pr_state, "other");
	(void *) sv_globals.world = PR_GetGlobalPointer (&sv_pr_state, "world");
	(void *) sv_globals.frametime = PR_GetGlobalPointer (&sv_pr_state, "frametime");
	(void *) sv_globals.newmis = PR_GetGlobalPointer (&sv_pr_state, "newmis");
	(void *) sv_globals.force_retouch = PR_GetGlobalPointer (&sv_pr_state, "force_retouch");
	(void *) sv_globals.mapname = PR_GetGlobalPointer (&sv_pr_state, "mapname");
	(void *) sv_globals.serverflags = PR_GetGlobalPointer (&sv_pr_state, "serverflags");
	(void *) sv_globals.total_secrets = PR_GetGlobalPointer (&sv_pr_state, "total_secrets");
	(void *) sv_globals.total_monsters = PR_GetGlobalPointer (&sv_pr_state, "total_monsters");
	(void *) sv_globals.found_secrets = PR_GetGlobalPointer (&sv_pr_state, "found_secrets");
	(void *) sv_globals.killed_monsters = PR_GetGlobalPointer (&sv_pr_state, "killed_monsters");
	(void *) sv_globals.parms = PR_GetGlobalPointer (&sv_pr_state, "parm1");
	(void *) sv_globals.v_forward = PR_GetGlobalPointer (&sv_pr_state, "v_forward");
	(void *) sv_globals.v_up = PR_GetGlobalPointer (&sv_pr_state, "v_up");
	(void *) sv_globals.v_right = PR_GetGlobalPointer (&sv_pr_state, "v_right");
	(void *) sv_globals.trace_allsolid = PR_GetGlobalPointer (&sv_pr_state, "trace_allsolid");
	(void *) sv_globals.trace_startsolid = PR_GetGlobalPointer (&sv_pr_state, "trace_startsolid");
	(void *) sv_globals.trace_fraction = PR_GetGlobalPointer (&sv_pr_state, "trace_fraction");
	(void *) sv_globals.trace_endpos = PR_GetGlobalPointer (&sv_pr_state, "trace_endpos");
	(void *) sv_globals.trace_plane_normal = PR_GetGlobalPointer (&sv_pr_state, "trace_plane_normal");
	(void *) sv_globals.trace_plane_dist = PR_GetGlobalPointer (&sv_pr_state, "trace_plane_dist");
	(void *) sv_globals.trace_ent = PR_GetGlobalPointer (&sv_pr_state, "trace_ent");
	(void *) sv_globals.trace_inopen = PR_GetGlobalPointer (&sv_pr_state, "trace_inopen");
	(void *) sv_globals.trace_inwater = PR_GetGlobalPointer (&sv_pr_state, "trace_inwater");
	(void *) sv_globals.msg_entity = PR_GetGlobalPointer (&sv_pr_state, "msg_entity");

	sv_funcs.main = PR_GetFunctionIndex (&sv_pr_state, "main");
	sv_funcs.StartFrame = PR_GetFunctionIndex (&sv_pr_state, "StartFrame");
	sv_funcs.PlayerPreThink = PR_GetFunctionIndex (&sv_pr_state, "PlayerPreThink");
	sv_funcs.PlayerPostThink = PR_GetFunctionIndex (&sv_pr_state, "PlayerPostThink");
	sv_funcs.ClientKill = PR_GetFunctionIndex (&sv_pr_state, "ClientKill");
	sv_funcs.ClientConnect = PR_GetFunctionIndex (&sv_pr_state, "ClientConnect");
	sv_funcs.PutClientInServer = PR_GetFunctionIndex (&sv_pr_state, "PutClientInServer");
	sv_funcs.ClientDisconnect = PR_GetFunctionIndex (&sv_pr_state, "ClientDisconnect");
	sv_funcs.SetNewParms = PR_GetFunctionIndex (&sv_pr_state, "SetNewParms");
	sv_funcs.SetChangeParms = PR_GetFunctionIndex (&sv_pr_state, "SetChangeParms");

	sv_fields.modelindex = PR_GetFieldOffset (&sv_pr_state, "modelindex");
	sv_fields.absmin = PR_GetFieldOffset (&sv_pr_state, "absmin");
	sv_fields.absmax = PR_GetFieldOffset (&sv_pr_state, "absmax");
	sv_fields.ltime = PR_GetFieldOffset (&sv_pr_state, "ltime");
	sv_fields.lastruntime = PR_GetFieldOffset (&sv_pr_state, "lastruntime");
	sv_fields.movetype = PR_GetFieldOffset (&sv_pr_state, "movetype");
	sv_fields.solid = PR_GetFieldOffset (&sv_pr_state, "solid");
	sv_fields.origin = PR_GetFieldOffset (&sv_pr_state, "origin");
	sv_fields.oldorigin = PR_GetFieldOffset (&sv_pr_state, "oldorigin");
	sv_fields.velocity = PR_GetFieldOffset (&sv_pr_state, "velocity");
	sv_fields.angles = PR_GetFieldOffset (&sv_pr_state, "angles");
	sv_fields.avelocity = PR_GetFieldOffset (&sv_pr_state, "avelocity");
	sv_fields.classname = PR_GetFieldOffset (&sv_pr_state, "classname");
	sv_fields.model = PR_GetFieldOffset (&sv_pr_state, "model");
	sv_fields.frame = PR_GetFieldOffset (&sv_pr_state, "frame");
	sv_fields.skin = PR_GetFieldOffset (&sv_pr_state, "skin");
	sv_fields.effects = PR_GetFieldOffset (&sv_pr_state, "effects");
	sv_fields.mins = PR_GetFieldOffset (&sv_pr_state, "mins");
	sv_fields.maxs = PR_GetFieldOffset (&sv_pr_state, "maxs");
	sv_fields.size = PR_GetFieldOffset (&sv_pr_state, "size");
	sv_fields.touch = PR_GetFieldOffset (&sv_pr_state, "touch");
	sv_fields.think = PR_GetFieldOffset (&sv_pr_state, "think");
	sv_fields.blocked = PR_GetFieldOffset (&sv_pr_state, "blocked");
	sv_fields.nextthink = PR_GetFieldOffset (&sv_pr_state, "nextthink");
	sv_fields.groundentity = PR_GetFieldOffset (&sv_pr_state, "groundentity");
	sv_fields.health = PR_GetFieldOffset (&sv_pr_state, "health");
	sv_fields.frags = PR_GetFieldOffset (&sv_pr_state, "frags");
	sv_fields.weapon = PR_GetFieldOffset (&sv_pr_state, "weapon");
	sv_fields.weaponmodel = PR_GetFieldOffset (&sv_pr_state, "weaponmodel");
	sv_fields.weaponframe = PR_GetFieldOffset (&sv_pr_state, "weaponframe");
	sv_fields.currentammo = PR_GetFieldOffset (&sv_pr_state, "currentammo");
	sv_fields.ammo_shells = PR_GetFieldOffset (&sv_pr_state, "ammo_shells");
	sv_fields.ammo_nails = PR_GetFieldOffset (&sv_pr_state, "ammo_nails");
	sv_fields.ammo_rockets = PR_GetFieldOffset (&sv_pr_state, "ammo_rockets");
	sv_fields.ammo_cells = PR_GetFieldOffset (&sv_pr_state, "ammo_cells");
	sv_fields.items = PR_GetFieldOffset (&sv_pr_state, "items");
	sv_fields.takedamage = PR_GetFieldOffset (&sv_pr_state, "takedamage");
	sv_fields.chain = PR_GetFieldOffset (&sv_pr_state, "chain");
	sv_fields.view_ofs = PR_GetFieldOffset (&sv_pr_state, "view_ofs");
	sv_fields.button0 = PR_GetFieldOffset (&sv_pr_state, "button0");
	sv_fields.button1 = PR_GetFieldOffset (&sv_pr_state, "button1");
	sv_fields.button2 = PR_GetFieldOffset (&sv_pr_state, "button2");
	sv_fields.impulse = PR_GetFieldOffset (&sv_pr_state, "impulse");
	sv_fields.fixangle = PR_GetFieldOffset (&sv_pr_state, "fixangle");
	sv_fields.v_angle = PR_GetFieldOffset (&sv_pr_state, "v_angle");
	sv_fields.netname = PR_GetFieldOffset (&sv_pr_state, "netname");
	sv_fields.enemy = PR_GetFieldOffset (&sv_pr_state, "enemy");
	sv_fields.flags = PR_GetFieldOffset (&sv_pr_state, "flags");
	sv_fields.colormap = PR_GetFieldOffset (&sv_pr_state, "colormap");
	sv_fields.team = PR_GetFieldOffset (&sv_pr_state, "team");
	sv_fields.teleport_time = PR_GetFieldOffset (&sv_pr_state, "teleport_time");
	sv_fields.armorvalue = PR_GetFieldOffset (&sv_pr_state, "armorvalue");
	sv_fields.waterlevel = PR_GetFieldOffset (&sv_pr_state, "waterlevel");
	sv_fields.watertype = PR_GetFieldOffset (&sv_pr_state, "watertype");
	sv_fields.ideal_yaw = PR_GetFieldOffset (&sv_pr_state, "ideal_yaw");
	sv_fields.yaw_speed = PR_GetFieldOffset (&sv_pr_state, "yaw_speed");
	sv_fields.goalentity = PR_GetFieldOffset (&sv_pr_state, "goalentity");
	sv_fields.spawnflags = PR_GetFieldOffset (&sv_pr_state, "spawnflags");
	sv_fields.dmg_take = PR_GetFieldOffset (&sv_pr_state, "dmg_take");
	sv_fields.dmg_save = PR_GetFieldOffset (&sv_pr_state, "dmg_save");
	sv_fields.dmg_inflictor = PR_GetFieldOffset (&sv_pr_state, "dmg_inflictor");
	sv_fields.owner = PR_GetFieldOffset (&sv_pr_state, "owner");
	sv_fields.message = PR_GetFieldOffset (&sv_pr_state, "message");
	sv_fields.sounds = PR_GetFieldOffset (&sv_pr_state, "sounds");

	// Zoid, find the spectator functions
	SpectatorConnect = SpectatorThink = SpectatorDisconnect = 0;

	if ((f = ED_FindFunction (&sv_pr_state, "SpectatorConnect")) != NULL)
		SpectatorConnect = (func_t) (f - sv_pr_state.pr_functions);
	if ((f = ED_FindFunction (&sv_pr_state, "SpectatorThink")) != NULL)
		SpectatorThink = (func_t) (f - sv_pr_state.pr_functions);
	if ((f = ED_FindFunction (&sv_pr_state, "SpectatorDisconnect")) != NULL)
		SpectatorDisconnect = (func_t) (f - sv_pr_state.pr_functions);

	// 2000-01-02 EndFrame function by Maddes/FrikaC
	EndFrame = 0;
	if ((f = ED_FindFunction (&sv_pr_state, "EndFrame")) != NULL)
		EndFrame = (func_t) (f - sv_pr_state.pr_functions);

	sv_fields.alpha = ED_GetFieldIndex (&sv_pr_state, "alpha");
	sv_fields.scale = ED_GetFieldIndex (&sv_pr_state, "scale");
	sv_fields.glow_size = ED_GetFieldIndex (&sv_pr_state, "glow_size");
	sv_fields.glow_color = ED_GetFieldIndex (&sv_pr_state, "glow_color");
	sv_fields.colormod = ED_GetFieldIndex (&sv_pr_state, "colormod");
}

extern builtin_t sv_builtins[];
extern int sv_numbuiltins;

void
SV_Progs_Init (void)
{
	sv_pr_state.edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.time = &sv.time;
	sv_pr_state.reserved_edicts = &reserved_edicts;
	sv_pr_state.unlink = SV_UnlinkEdict;
	sv_pr_state.flush = SV_FlushSignon;
	sv_pr_state.builtins = sv_builtins;
	sv_pr_state.numbuiltins = sv_numbuiltins;
	sv_pr_state.parse_field = parse_field;
	sv_pr_state.prune_edict = prune_edict;

	Cmd_AddCommand ("edict", ED_PrintEdict_f, "Report information on a given edict in the game. (edict (edict number))");
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f, "Display information on all edicts in the game.");
	Cmd_AddCommand ("edictcount", ED_Count_f, "Display summary information on the edicts in the game.");
	Cmd_AddCommand ("profile", PR_Profile_f, "FIXME: Report information about QuakeC Stuff (???) No Description");
}

void
SV_Progs_Init_Cvars (void)
{
	r_skyname =
		Cvar_Get ("r_skyname", "", CVAR_SERVERINFO, Cvar_Info,
				"name of skybox");
	sv_progs = Cvar_Get ("sv_progs", "qwprogs.dat", CVAR_ROM, NULL,
						 "Allows selectable game progs if you have several "
						 "of them in the gamedir");
	pr_checkextentions = Cvar_Get ("sv_progs", "1", CVAR_ROM, NULL,
								   "indicate the presence of the "
								   "checkextentions qc function");
}
