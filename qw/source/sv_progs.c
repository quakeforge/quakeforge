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

#include "cmd.h"
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

func_t	EndFrame;
func_t	SpectatorConnect;
func_t	SpectatorDisconnect;
func_t	SpectatorThink;

static int reserved_edicts = MAX_CLIENTS;

int
ED_Prune_Edict (progs_t *pr, edict_t *ent)
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

int
ED_Parse_Extra_Fields (progs_t *pr, char *key, char *value)
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

	sv_funcs.main =
		ED_FindFunction (&sv_pr_state, "main") - sv_pr_state.pr_functions;
	sv_funcs.StartFrame =
		ED_FindFunction (&sv_pr_state, "StartFrame") - sv_pr_state.pr_functions;
	sv_funcs.PlayerPreThink =
		ED_FindFunction (&sv_pr_state, "PlayerPreThink") - sv_pr_state.pr_functions;
	sv_funcs.PlayerPostThink =
		ED_FindFunction (&sv_pr_state, "PlayerPostThink") - sv_pr_state.pr_functions;
	sv_funcs.ClientKill =
		ED_FindFunction (&sv_pr_state, "ClientKill") - sv_pr_state.pr_functions;
	sv_funcs.ClientConnect =
		ED_FindFunction (&sv_pr_state, "ClientConnect") - sv_pr_state.pr_functions;
	sv_funcs.PutClientInServer =
		ED_FindFunction (&sv_pr_state, "PutClientInServer") - sv_pr_state.pr_functions;
	sv_funcs.ClientDisconnect =
		ED_FindFunction (&sv_pr_state, "ClientDisconnect") - sv_pr_state.pr_functions;
	sv_funcs.SetNewParms =
		ED_FindFunction (&sv_pr_state, "SetNewParms") - sv_pr_state.pr_functions;
	sv_funcs.SetChangeParms =
		ED_FindFunction (&sv_pr_state, "SetChangeParms") - sv_pr_state.pr_functions;

	sv_fields.modelindex = ED_FindField (&sv_pr_state, "modelindex")->ofs;
	sv_fields.absmin = ED_FindField (&sv_pr_state, "absmin")->ofs;
	sv_fields.absmax = ED_FindField (&sv_pr_state, "absmax")->ofs;
	sv_fields.ltime = ED_FindField (&sv_pr_state, "ltime")->ofs;
	sv_fields.lastruntime = ED_FindField (&sv_pr_state, "lastruntime")->ofs;
	sv_fields.movetype = ED_FindField (&sv_pr_state, "movetype")->ofs;
	sv_fields.solid = ED_FindField (&sv_pr_state, "solid")->ofs;
	sv_fields.origin = ED_FindField (&sv_pr_state, "origin")->ofs;
	sv_fields.oldorigin = ED_FindField (&sv_pr_state, "oldorigin")->ofs;
	sv_fields.velocity = ED_FindField (&sv_pr_state, "velocity")->ofs;
	sv_fields.angles = ED_FindField (&sv_pr_state, "angles")->ofs;
	sv_fields.avelocity = ED_FindField (&sv_pr_state, "avelocity")->ofs;
	sv_fields.classname = ED_FindField (&sv_pr_state, "classname")->ofs;
	sv_fields.model = ED_FindField (&sv_pr_state, "model")->ofs;
	sv_fields.frame = ED_FindField (&sv_pr_state, "frame")->ofs;
	sv_fields.skin = ED_FindField (&sv_pr_state, "skin")->ofs;
	sv_fields.effects = ED_FindField (&sv_pr_state, "effects")->ofs;
	sv_fields.mins = ED_FindField (&sv_pr_state, "mins")->ofs;
	sv_fields.maxs = ED_FindField (&sv_pr_state, "maxs")->ofs;
	sv_fields.size = ED_FindField (&sv_pr_state, "size")->ofs;
	sv_fields.touch = ED_FindField (&sv_pr_state, "touch")->ofs;
	sv_fields.think = ED_FindField (&sv_pr_state, "think")->ofs;
	sv_fields.blocked = ED_FindField (&sv_pr_state, "blocked")->ofs;
	sv_fields.nextthink = ED_FindField (&sv_pr_state, "nextthink")->ofs;
	sv_fields.groundentity = ED_FindField (&sv_pr_state, "groundentity")->ofs;
	sv_fields.health = ED_FindField (&sv_pr_state, "health")->ofs;
	sv_fields.frags = ED_FindField (&sv_pr_state, "frags")->ofs;
	sv_fields.weapon = ED_FindField (&sv_pr_state, "weapon")->ofs;
	sv_fields.weaponmodel = ED_FindField (&sv_pr_state, "weaponmodel")->ofs;
	sv_fields.weaponframe = ED_FindField (&sv_pr_state, "weaponframe")->ofs;
	sv_fields.currentammo = ED_FindField (&sv_pr_state, "currentammo")->ofs;
	sv_fields.ammo_shells = ED_FindField (&sv_pr_state, "ammo_shells")->ofs;
	sv_fields.ammo_nails = ED_FindField (&sv_pr_state, "ammo_nails")->ofs;
	sv_fields.ammo_rockets = ED_FindField (&sv_pr_state, "ammo_rockets")->ofs;
	sv_fields.ammo_cells = ED_FindField (&sv_pr_state, "ammo_cells")->ofs;
	sv_fields.items = ED_FindField (&sv_pr_state, "items")->ofs;
	sv_fields.takedamage = ED_FindField (&sv_pr_state, "takedamage")->ofs;
	sv_fields.chain = ED_FindField (&sv_pr_state, "chain")->ofs;
	sv_fields.view_ofs = ED_FindField (&sv_pr_state, "view_ofs")->ofs;
	sv_fields.button0 = ED_FindField (&sv_pr_state, "button0")->ofs;
	sv_fields.button1 = ED_FindField (&sv_pr_state, "button1")->ofs;
	sv_fields.button2 = ED_FindField (&sv_pr_state, "button2")->ofs;
	sv_fields.impulse = ED_FindField (&sv_pr_state, "impulse")->ofs;
	sv_fields.fixangle = ED_FindField (&sv_pr_state, "fixangle")->ofs;
	sv_fields.v_angle = ED_FindField (&sv_pr_state, "v_angle")->ofs;
	sv_fields.netname = ED_FindField (&sv_pr_state, "netname")->ofs;
	sv_fields.enemy = ED_FindField (&sv_pr_state, "enemy")->ofs;
	sv_fields.flags = ED_FindField (&sv_pr_state, "flags")->ofs;
	sv_fields.colormap = ED_FindField (&sv_pr_state, "colormap")->ofs;
	sv_fields.team = ED_FindField (&sv_pr_state, "team")->ofs;
	sv_fields.teleport_time = ED_FindField (&sv_pr_state, "teleport_time")->ofs;
	sv_fields.armorvalue = ED_FindField (&sv_pr_state, "armorvalue")->ofs;
	sv_fields.waterlevel = ED_FindField (&sv_pr_state, "waterlevel")->ofs;
	sv_fields.watertype = ED_FindField (&sv_pr_state, "watertype")->ofs;
	sv_fields.ideal_yaw = ED_FindField (&sv_pr_state, "ideal_yaw")->ofs;
	sv_fields.yaw_speed = ED_FindField (&sv_pr_state, "yaw_speed")->ofs;
	sv_fields.goalentity = ED_FindField (&sv_pr_state, "goalentity")->ofs;
	sv_fields.spawnflags = ED_FindField (&sv_pr_state, "spawnflags")->ofs;
	sv_fields.dmg_take = ED_FindField (&sv_pr_state, "dmg_take")->ofs;
	sv_fields.dmg_save = ED_FindField (&sv_pr_state, "dmg_save")->ofs;
	sv_fields.dmg_inflictor = ED_FindField (&sv_pr_state, "dmg_inflictor")->ofs;
	sv_fields.owner = ED_FindField (&sv_pr_state, "owner")->ofs;
	sv_fields.message = ED_FindField (&sv_pr_state, "message")->ofs;
	sv_fields.sounds = ED_FindField (&sv_pr_state, "sounds")->ofs;

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
	sv_fields.glowsize = ED_GetFieldIndex (&sv_pr_state, "glow_size");
	sv_fields.glowcolor = ED_GetFieldIndex (&sv_pr_state, "glow_color");
	sv_fields.colormod = ED_GetFieldIndex (&sv_pr_state, "colormod");
}

void
SV_Progs_Init (void)
{
	sv_pr_state.edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.time = &sv.time;
	sv_pr_state.reserved_edicts = &reserved_edicts;
	sv_pr_state.unlink = SV_UnlinkEdict;
	sv_pr_state.flush = SV_FlushSignon;

	Cmd_AddCommand ("edict", ED_PrintEdict_f, "Report information on a given edict in the game. (edict (edict number))");
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f, "Display information on all edicts in the game.");
	Cmd_AddCommand ("edictcount", ED_Count_f, "Display summary information on the edicts in the game.");
	Cmd_AddCommand ("profile", PR_Profile_f, "FIXME: Report information about QuakeC Stuff (???) No Description");
}

void
SV_Progs_Init_Cvars (void)
{
	r_skyname =
		Cvar_Get ("r_skyname", "", CVAR_SERVERINFO, "name of skybox");
	sv_progs = Cvar_Get ("sv_progs", "qwprogs.dat", CVAR_ROM,
						 "Allows selectable game progs if you have several "
						 "of them in the gamedir");
}
