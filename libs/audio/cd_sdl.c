/*
	cd_sdl.c

	SDL CD audio routines

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

#include <SDL.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/cd.h"

#include "compat.h"

static plugin_t		plugin_info;
static plugin_data_t	plugin_info_data;
static plugin_funcs_t	plugin_info_funcs;
static general_data_t	plugin_info_general_data;
static general_funcs_t	plugin_info_general_funcs;
static cd_funcs_t		plugin_info_cd_funcs;

static qboolean cdValid = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;

static SDL_CD  *cd_id;
static float	cdvolume = 1.0;

static float bgmvolume;
static cvar_t bgmvolume_cvar = {
	.name = "bgmvolume",
	.description =
		"Volume of CD music",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &bgmvolume },
};


static void
I_CDAudio_Eject (void)
{
	if (!cd_id || !enabled)
		return;

	if (SDL_CDEject (cd_id))
		Sys_MaskPrintf (SYS_snd, "Unable to eject CD-ROM tray.\n");
}

static void
I_CDAudio_Pause (void)
{
	if (!cd_id || !enabled)
		return;
	if (SDL_CDStatus (cd_id) != CD_PLAYING)
		return;

	if (SDL_CDPause (cd_id))
		Sys_MaskPrintf (SYS_snd, "CDAudio_Pause: Failed to pause track.\n");
}

static void
I_CDAudio_Stop (void)
{
	int			cdstate;

	if (!cd_id || !enabled)
		return;
	cdstate = SDL_CDStatus (cd_id);
	if ((cdstate != CD_PLAYING) && (cdstate != CD_PAUSED))
		return;

	if (SDL_CDStop (cd_id))
		Sys_MaskPrintf (SYS_snd, "CDAudio_Stop: Failed to stop track.\n");
}

static void
I_CDAudio_Play (int track, qboolean looping)
{
	/* Initialize cd_stat to avoid warning */
	/* XXX - Does this default value make sense? */
	CDstatus    cd_stat = CD_ERROR;

	if (!cd_id || !enabled)
		return;

	if (!cdValid) {
		cd_stat = SDL_CDStatus (cd_id);
		if (!CD_INDRIVE (cd_stat) || (!cd_id->numtracks))
			return;
		cdValid = true;
	}

	if ((track < 1) || (track >= cd_id->numtracks)) {
		I_CDAudio_Stop ();
		return;
	}
	track--;						// Convert track from person to SDL value
	if (cd_stat == CD_PLAYING) {
		if (cd_id->cur_track == track)
			return;
		I_CDAudio_Stop ();
	}

	if (SDL_CDPlay (cd_id, cd_id->track[track].offset,
					cd_id->track[track].length)) {
		Sys_MaskPrintf (SYS_snd, "CDAudio_Play: Unable to play track: %d\n",
						track + 1);
		return;
	}
	playLooping = looping;
}

static void
I_CDAudio_Resume (void)
{
	if (!cd_id || !enabled)
		return;
	if (SDL_CDStatus (cd_id) != CD_PAUSED)
		return;

	if (SDL_CDResume (cd_id))
		Sys_MaskPrintf (SYS_snd, "CDAudio_Resume: Failed to resume track.\n");
}

static void
I_CDAudio_Shutdown (void)
{
	if (!cd_id)
		return;
	I_CDAudio_Stop ();
	SDL_CDClose (cd_id);
	cd_id = NULL;
}

static void
I_CDAudio_Update (void)
{
	if (!cd_id || !enabled)
		return;
	if (bgmvolume != cdvolume) {
		if (cdvolume) {
			bgmvolume = 0.0;
			I_CDAudio_Pause ();
		} else {
			bgmvolume = 1.0;
			I_CDAudio_Resume ();
		}
		cdvolume = bgmvolume;
		return;
	}
	if (playLooping && (SDL_CDStatus (cd_id) != CD_PLAYING)
		&& (SDL_CDStatus (cd_id) != CD_PAUSED))
		I_CDAudio_Play (cd_id->cur_track + 1, true);
}

#define CD_f_DEFINED

static void
I_CD_f (void)
{
	const char *command;
	int			cdstate;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);
	if (strequal (command, "on")) {
		enabled = true;
	}
	if (strequal (command, "off")) {
		if (!cd_id)
			return;
		cdstate = SDL_CDStatus (cd_id);
		if ((cdstate == CD_PLAYING) || (cdstate == CD_PAUSED))
			I_CDAudio_Stop ();
		enabled = false;
		return;
	}
	if (strequal (command, "play")) {
		I_CDAudio_Play (atoi (Cmd_Argv (2)), false);
		return;
	}
	if (strequal (command, "loop")) {
		I_CDAudio_Play (atoi (Cmd_Argv (2)), true);
		return;
	}
	if (strequal (command, "stop")) {
		I_CDAudio_Stop ();
		return;
	}
	if (strequal (command, "pause")) {
		I_CDAudio_Pause ();
		return;
	}
	if (strequal (command, "resume")) {
		I_CDAudio_Resume ();
		return;
	}
	if (strequal (command, "eject")) {
		I_CDAudio_Eject ();
		return;
	}
	if (strequal (command, "info")) {
		if (!cd_id)
			return;
		cdstate = SDL_CDStatus (cd_id);
		Sys_Printf ("%d tracks\n", cd_id->numtracks);
		if (cdstate == CD_PLAYING)
			Sys_Printf ("Currently %s track %d\n",
						playLooping ? "looping" : "playing",
						cd_id->cur_track + 1);
		else if (cdstate == CD_PAUSED)
			Sys_Printf ("Paused %s track %d\n",
						playLooping ? "looping" : "playing",
						cd_id->cur_track + 1);
		return;
	}
}

static void
I_CDAudio_Init (void)
{
	if (SDL_Init (SDL_INIT_CDROM) < 0) {
		Sys_Printf ("Couldn't initialize SDL CD-AUDIO: %s\n", SDL_GetError ());
		return; // was -1
	}
	cd_id = SDL_CDOpen (0);
	if (!cd_id) {
		Sys_Printf ("CDAudio_Init: Unable to open default CD-ROM drive: %s\n",
					SDL_GetError ());
		return; // was -1
	}

	initialized = true;
	enabled = true;
	cdValid = true;

	if (!CD_INDRIVE (SDL_CDStatus (cd_id))) {
		Sys_Printf ("CDAudio_Init: No CD in drive.\n");
		cdValid = false;
	}
	if (!cd_id->numtracks) {
		Sys_Printf ("CDAudio_Init: CD contains no audio tracks.\n");
		cdValid = false;
	}

	Cvar_Register (&bgmvolume_cvar, 0, 0);

	Sys_Printf ("CD Audio Initialized.\n");
}

static general_funcs_t plugin_info_general_funcs = {
	I_CDAudio_Init,
	I_CDAudio_Shutdown,
};

static cd_funcs_t plugin_info_cd_funcs = {
	0,
	I_CD_f,
	I_CDAudio_Pause,
	I_CDAudio_Play,
	I_CDAudio_Resume,
	I_CDAudio_Update,
};

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	&plugin_info_cd_funcs,
	0,
	0,
	0,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	0,
	0,
	0,
};

static plugin_t plugin_info = {
	qfp_cd,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"SDL CD Audio output\n",
	"Copyright (C) 2001  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, sdl)
{
	return &plugin_info;
}
