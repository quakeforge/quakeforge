/*
	cd_sgi.c

	audio cd playback support for sgi irix machines

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

#include <sys/types.h>
#include <errno.h>
#include <dmedia/cdaudio.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/sound.h"

static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static float cdvolume;
static byte remap[100];
static byte playTrack;

static char cd_dev[64] = "/dev/cdrom";

static CDPLAYER *cdp = NULL;

static void
CDAudio_Eject (void)
{
	if (cdp == NULL || !enabled)
		return;							// no cd init'd

	if (CDeject (cdp) == 0)
		Con_DPrintf ("CDAudio_Eject: CDeject failed\n");
}

static int
CDAudio_GetState (void)
{
	CDSTATUS    cds;

	if (cdp == NULL || !enabled)
		return -1;						// no cd init'd

	if (CDgetstatus (cdp, &cds) == 0) {
		Con_DPrintf ("CDAudio_GetStatus: CDgetstatus failed\n");
		return -1;
	}

	return cds.state;
}

static int
CDAudio_MaxTrack (void)
{
	CDSTATUS    cds;

	if (cdp == NULL || !enabled)
		return -1;						// no cd init'd

	if (CDgetstatus (cdp, &cds) == 0) {
		Con_DPrintf ("CDAudio_MaxTrack: CDgetstatus failed\n");
		return -1;
	}

	return cds.last;
}

void
CDAudio_Play (byte track, qboolean looping)
{
	int         maxtrack = CDAudio_MaxTrack ();

	if (!initialized || !enabled)
		return;

	/* cd == audio cd? */
	if (CDAudio_GetState () != CD_READY) {
		Con_Printf ("CDAudio_Play: CD in player not an audio CD.\n");
		return;
	}

	if (maxtrack < 0) {
		Con_DPrintf ("CDAudio_Play: Error getting maximum track number\n");
		return;
	}

	track = remap[track];

	if (track < 1 || track > maxtrack) {
		CDAudio_Stop ();
		return;
	}
	// don't try to play a non-audio track
	/* mw: how to do this on irix? entry0.cdte_track = track;
	   entry0.cdte_format = CDROM_MSF; if ( ioctl(cdfile, CDROMREADTOCENTRY,
	   &entry0) == -1 ) { Con_DPrintf("CDAudio: ioctl cdromreadtocentry
	   failed\n"); return; }

	   entry1.cdte_track = track + 1; entry1.cdte_format = CDROM_MSF; if
	   (entry1.cdte_track > maxTrack) { entry1.cdte_track = CDROM_LEADOUT; }

	   if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry1) == -1 ) {
	   Con_DPrintf("CDAudio: ioctl cdromreadtocentry failed\n"); return; }

	   if (entry0.cdte_ctrl == CDROM_DATA_TRACK) { Con_Printf("track %i is
	   not audio\n", track); return; } */

	if (CDAudio_GetState () == CD_PLAYING) {
		if (playTrack == track)
			return;

		CDAudio_Stop ();
	}

	if (CDplaytrack (cdp, track, cdvolume == 0.0 ? 0 : 1) == 0) {
		Con_DPrintf ("CDAudio_Play: CDplay failed (%d)\n", errno);
		return;
	}

	playLooping = looping;
	playTrack = track;
}

void
CDAudio_Stop (void)
{
	if (cdp == NULL || !enabled || CDAudio_GetState () != CD_PLAYING)
		return;

	if (CDstop (cdp) == 0)
		Con_DPrintf ("CDAudio_Stop: CDStop failed (%d)\n", errno);
}

void
CDAudio_Pause (void)
{
	if (cdp == NULL || !enabled || CDAudio_GetState () != CD_PLAYING)
		return;

	if (CDtogglepause (cdp) == 0)
		Con_DPrintf ("CDAudio_PAUSE: CDtogglepause failed (%d)\n", errno);
}

void
CDAudio_Resume (void)
{
	if (cdp == NULL || !enabled || CDAudio_GetState () != CD_PAUSED)
		return;

	if (CDtogglepause (cdp) == 0)
		Con_DPrintf ("CDAudio_Resume: CDtogglepause failed (%d)\n", errno);
}

static void
CD_f (void)
{
	char       *command;
	int         ret;
	int         n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (strequal (command, "on")) {
		enabled = true;
		return;
	}

	if (strequal (command, "off")) {
		CDAudio_Stop ();
		enabled = false;
		return;
	}

	if (strequal (command, "reset")) {
		enabled = true;
		CDAudio_Stop ();

		for (n = 0; n < 100; n++)
			remap[n] = n;

		return;
	}

	if (strequal (command, "remap")) {
		ret = Cmd_Argc () - 2;

		if (ret <= 0) {
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf ("  %u -> %u\n", n, remap[n]);
			return;
		}

		for (n = 1; n <= ret; n++)
			remap[n] = atoi (Cmd_Argv (n + 1));

		return;
	}

	if (strequal (command, "play")) {
		CDAudio_Play ((byte) atoi (Cmd_Argv (2)), false);
		return;
	}

	if (strequal (command, "loop")) {
		CDAudio_Play ((byte) atoi (Cmd_Argv (2)), true);
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
		CDAudio_Stop ();
		CDAudio_Eject ();
		return;
	}

	if (strequal (command, "info")) {
		Con_Printf ("%u tracks\n", CDAudio_MaxTrack ());
		if (CDAudio_GetState () == CD_PLAYING)
			Con_Printf ("Currently %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		else if (CDAudio_GetState () == CD_PAUSED)
			Con_Printf ("Paused %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);

		Con_Printf ("Volume is %g\n", cdvolume);
		return;
	}
}

void
CDAudio_Update (void)
{
	if (!initialized || !enabled)
		return;

	if (bgmvolume->value != cdvolume) {
		if (cdvolume) {
			Cvar_SetValue (bgmvolume, 0.0);
			cdvolume = bgmvolume->value;
			CDAudio_Pause ();
		} else {
			Cvar_SetValue (bgmvolume, 1.0);
			cdvolume = bgmvolume->value;
			CDAudio_Resume ();
		}
	}

	if (CDAudio_GetState () != CD_PLAYING &&
		CDAudio_GetState () != CD_PAUSED && playLooping)
			CDAudio_Play (playTrack, true);
}

int
CDAudio_Init (void)
{
	int         i;

	if (COM_CheckParm ("-nocdaudio"))
		return -1;

	if ((i = COM_CheckParm ("-cddev")) != 0 && i < com_argc - 1) {
		strncpy (cd_dev, com_argv[i + 1], sizeof (cd_dev));
		cd_dev[sizeof (cd_dev) - 1] = 0;
	}

	cdp = CDopen (cd_dev, "r");

	if (cdp == NULL) {
		Con_Printf ("CDAudio_Init: open of \"%s\" failed (%i)\n",
					cd_dev, errno);
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;

	initialized = true;
	enabled = true;

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

	Con_Printf ("CD Audio Initialized\n");

	return 0;
}

void
CDAudio_Shutdown (void)
{
	if (!initialized)
		return;

	CDAudio_Stop ();
	CDclose (cdp);
	cdp = NULL;
	initialized = false;
}

/* end of file */
