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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <SDL.h>

#include "QF/compat.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/sound.h"

static qboolean cdValid = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;

static SDL_CD  *cd_id;
static float	cdvolume = 1.0;

static void CD_f (void);


static void
CDAudio_Eject (void)
{
	if (!cd_id || !enabled)
		return;

	if (SDL_CDEject (cd_id))
		Con_DPrintf ("Unable to eject CD-ROM tray.\n");
}

void
CDAudio_Play (byte track, qboolean looping)
{
	/* Initialize cd_stat to avoid warning */
	/* XXX - Does this default value make sense? */
	CDstatus    cd_stat = CD_ERROR;

	if (!cd_id || !enabled)
		return;

	if (!cdValid) {
		if (!CD_INDRIVE (cd_stat = SDL_CDStatus (cd_id)) || (!cd_id->numtracks))
			return;
		cdValid = true;
	}

	if ((track < 1) || (track >= cd_id->numtracks)) {
		CDAudio_Stop ();
		return;
	}
	track--;							/* Convert track from person to SDL
										   value */
	if (cd_stat == CD_PLAYING) {
		if (cd_id->cur_track == track)
			return;
		CDAudio_Stop ();
	}

	if (SDL_CDPlay (cd_id, cd_id->track[track].offset,
					cd_id->track[track].length)) {
		Con_DPrintf ("CDAudio_Play: Unable to play track: %d\n", track + 1);
		return;
	}
	playLooping = looping;
}


void
CDAudio_Stop (void)
{
	int			cdstate;

	if (!cd_id || !enabled)
		return;
	cdstate = SDL_CDStatus (cd_id);
	if ((cdstate != CD_PLAYING) && (cdstate != CD_PAUSED))
		return;

	if (SDL_CDStop (cd_id))
		Con_DPrintf ("CDAudio_Stop: Failed to stop track.\n");
}


void
CDAudio_Pause (void)
{
	if (!cd_id || !enabled)
		return;
	if (SDL_CDStatus (cd_id) != CD_PLAYING)
		return;

	if (SDL_CDPause (cd_id))
		Con_DPrintf ("CDAudio_Pause: Failed to pause track.\n");
}


void
CDAudio_Resume (void)
{
	if (!cd_id || !enabled)
		return;
	if (SDL_CDStatus (cd_id) != CD_PAUSED)
		return;

	if (SDL_CDResume (cd_id))
		Con_DPrintf ("CDAudio_Resume: Failed tp resume track.\n");
}

void
CDAudio_Update (void)
{
	if (!cd_id || !enabled)
		return;
	if (bgmvolume->value != cdvolume) {
		if (cdvolume) {
			Cvar_SetValue (bgmvolume, 0.0);
			CDAudio_Pause ();
		} else {
			Cvar_SetValue (bgmvolume, 1.0);
			CDAudio_Resume ();
		}
		cdvolume = bgmvolume->value;
		return;
	}
	if (playLooping && (SDL_CDStatus (cd_id) != CD_PLAYING)
		&& (SDL_CDStatus (cd_id) != CD_PAUSED))
		CDAudio_Play (cd_id->cur_track + 1, true);
}


int
CDAudio_Init (void)
{
#ifdef UQUAKE
	if (cls.state == ca_dedicated)
		return -1;
#endif

	if (COM_CheckParm ("-nocdaudio"))
		return -1;

	if (SDL_Init (SDL_INIT_CDROM) < 0) {
		Con_Printf ("Couldn't initialize SDL CD-AUDIO: %s\n", SDL_GetError ());
		return -1;
	}
	cd_id = SDL_CDOpen (0);
	if (!cd_id) {
		Con_Printf ("CDAudio_Init: Unable to open default CD-ROM drive: %s\n",
					SDL_GetError ());
		return -1;
	}

	initialized = true;
	enabled = true;
	cdValid = true;

	if (!CD_INDRIVE (SDL_CDStatus (cd_id))) {
		Con_Printf ("CDAudio_Init: No CD in drive.\n");
		cdValid = false;
	}
	if (!cd_id->numtracks) {
		Con_Printf ("CDAudio_Init: CD contains no audio tracks.\n");
		cdValid = false;
	}
	
	Cmd_AddCommand ("cd", CD_f, "Control the CD player.\n"
		"Commands:\n"
		"eject - Eject the CD.\n"
		"info - Reports information on the CD.\n"
		"loop (track number) - Loops the specified track.\n"
		"remap (track1) (track2) ... - Remap the current track order.\n"
		"reset - Causes the CD audio to re-initialize.\n"
		"resume - Will resume playback after pause.\n"
		"off - Shuts down the CD audio system..\n"
		"on - Re-enables the CD audio system after a cd off command.\n"
		"pause - Pause the CD playback.\n"
		"play (track number) - Plays the specified track one time.\n"
		"stop - Stops the currently playing track.");
	
	Con_Printf ("CD Audio Initialized.\n");
	return 0;
}


void
CDAudio_Shutdown (void)
{
	if (!cd_id)
		return;
	CDAudio_Stop ();
	SDL_CDClose (cd_id);
	cd_id = NULL;
}


#define CD_f_DEFINED

static void
CD_f (void)
{
	char       *command;
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
			CDAudio_Stop ();
		enabled = false;
		return;
	}
	if (strequal (command, "play")) {
		CDAudio_Play (atoi (Cmd_Argv (2)), false);
		return;
	}
	if (strequal (command, "loop")) {
		CDAudio_Play (atoi (Cmd_Argv (2)), true);
		return;
	}
	if (strequal (command, "stop")) {
		CDAudio_Stop ();
		return;
	}
	if (strequal (command, "pause")) {
		CDAudio_Pause ();
		return;
	}
	if (strequal (command, "resume")) {
		CDAudio_Resume ();
		return;
	}
	if (strequal (command, "eject")) {
		CDAudio_Eject ();
		return;
	}
	if (strequal (command, "info")) {
		if (!cd_id)
			return;
		cdstate = SDL_CDStatus (cd_id);
		Con_Printf ("%d tracks\n", cd_id->numtracks);
		if (cdstate == CD_PLAYING)
			Con_Printf ("Currently %s track %d\n",
						playLooping ? "looping" : "playing",
						cd_id->cur_track + 1);
		else if (cdstate == CD_PAUSED)
			Con_Printf ("Paused %s track %d\n",
						playLooping ? "looping" : "playing",
						cd_id->cur_track + 1);
		return;
	}
}
