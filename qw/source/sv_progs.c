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
#include "progs.h"
#include "server.h"
#include "world.h"

int eval_alpha, eval_scale, eval_glowsize, eval_glowcolor, eval_colormod;
progs_t	    sv_pr_state;
cvar_t     *r_skyname;
cvar_t     *sv_progs;

func_t	EndFrame;
func_t	SpectatorConnect;
func_t	SpectatorDisconnect;
func_t	SpectatorThink;

void
FindEdictFieldOffsets (progs_t *pr)
{
	dfunction_t *f;

	if (pr == &sv_pr_state) {
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

		eval_alpha = FindFieldOffset (&sv_pr_state, "alpha");
		eval_scale = FindFieldOffset (&sv_pr_state, "scale");
		eval_glowsize = FindFieldOffset (&sv_pr_state, "glow_size");
		eval_glowcolor = FindFieldOffset (&sv_pr_state, "glow_color");
		eval_colormod = FindFieldOffset (&sv_pr_state, "colormod");
	}
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
	// If skyname is set, we want to allow skyboxes and set what
	// the skybox name should be.  "qlsky" is supported since
	// at least one other map uses it already.  --KB
	if (strcasecmp (key, "sky") == 0 ||		// LordHavoc: added "sky" key 
											// (Quake2 and DarkPlaces use 
											// this)
		strcasecmp (key, "skyname") == 0 ||
		strcasecmp (key, "qlsky") == 0) {
		Info_SetValueForKey (svs.info, "skybox",
							 "1", MAX_SERVERINFO_STRING);
		Cvar_Set (r_skyname, value);
		return 1;
	}
	return 0;
}

void
SV_LoadProgs (void)
{
	PR_LoadProgs (&sv_pr_state, sv_progs->string);
	if (!sv_pr_state.progs)
		SV_Error ("SV_LoadProgs: couldn't load %s", sv_progs->string);
}

void
SV_Progs_Init (void)
{
	sv_pr_state.edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.time = &sv.time;
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
