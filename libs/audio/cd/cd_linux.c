/*
	cd_linux.c

	(description)

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif


#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <linux/cdrom.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sound.h"

static qboolean cdValid = false;
static qboolean playing = false;
static qboolean wasPlaying = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static float cdvolume;
static byte remap[100];
static byte playTrack;
static byte maxTrack;

static int  cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";

static void
CDAudio_Eject (void)
{
	if (cdfile == -1 || !enabled)
		return;							// no cd init'd

	if (ioctl (cdfile, CDROMEJECT) == -1)
		Con_DPrintf ("CDAudio: ioctl cdromeject failed\n");
}


static void
CDAudio_CloseDoor (void)
{
	if (cdfile == -1 || !enabled)
		return;							// no cd init'd

	if (ioctl (cdfile, CDROMCLOSETRAY) == -1)
		Con_DPrintf ("CDAudio: ioctl cdromclosetray failed\n");
}

static int
CDAudio_GetAudioDiskInfo (void)
{
	struct cdrom_tochdr tochdr;

	cdValid = false;

	if (ioctl (cdfile, CDROMREADTOCHDR, &tochdr) == -1) {
		Con_DPrintf ("CDAudio: ioctl cdromreadtochdr failed\n");
		return -1;
	}

	if (tochdr.cdth_trk0 < 1) {
		Con_DPrintf ("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = tochdr.cdth_trk1;

	return 0;
}


void
CDAudio_Play (byte track, qboolean looping)
{
	struct cdrom_tocentry entry0;
	struct cdrom_tocentry entry1;
	struct cdrom_msf msf;

	if (cdfile == -1 || !enabled)
		return;

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack) {
		Con_DPrintf ("CDAudio: Bad track number %u.\n", track);
		return;
	}
	// don't try to play a non-audio track
	entry0.cdte_track = track;
	entry0.cdte_format = CDROM_MSF;
	if (ioctl (cdfile, CDROMREADTOCENTRY, &entry0) == -1) {
		Con_DPrintf ("CDAudio: ioctl cdromreadtocentry failed\n");
		return;
	}
	entry1.cdte_track = track + 1;
	entry1.cdte_format = CDROM_MSF;
	if (entry1.cdte_track > maxTrack) {
		entry1.cdte_track = CDROM_LEADOUT;
	}
	if (ioctl (cdfile, CDROMREADTOCENTRY, &entry1) == -1) {
		Con_DPrintf ("CDAudio: ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry0.cdte_ctrl == CDROM_DATA_TRACK) {
		Con_Printf ("track %i is not audio\n", track);
		return;
	}

	if (playing) {
		if (playTrack == track)
			return;
		CDAudio_Stop ();
	}

	msf.cdmsf_min0 = entry0.cdte_addr.msf.minute;
	msf.cdmsf_sec0 = entry0.cdte_addr.msf.second;
	msf.cdmsf_frame0 = entry0.cdte_addr.msf.frame;

	msf.cdmsf_min1 = entry1.cdte_addr.msf.minute;
	msf.cdmsf_sec1 = entry1.cdte_addr.msf.second;
	msf.cdmsf_frame1 = entry1.cdte_addr.msf.frame;

	Con_DPrintf ("%2d:%02d:%02d %2d:%02d:%02d\n",
				 msf.cdmsf_min0,
				 msf.cdmsf_sec0,
				 msf.cdmsf_frame0,
				 msf.cdmsf_min1, msf.cdmsf_sec1, msf.cdmsf_frame1);

	if (ioctl (cdfile, CDROMPLAYMSF, &msf) == -1) {
		Con_DPrintf ("CDAudio: ioctl cdromplaytrkind failed (%s)\n",
					 strerror (errno));
		return;
	}
	// if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
	// Con_DPrintf("CDAudio: ioctl cdromresume failed\n");

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void
CDAudio_Stop (void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

	if (ioctl (cdfile, CDROMSTOP) == -1)
		Con_DPrintf ("CDAudio: ioctl cdromstop failed (%d)\n", errno);

	wasPlaying = false;
	playing = false;
}

void
CDAudio_Pause (void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

	if (ioctl (cdfile, CDROMPAUSE) == -1)
		Con_DPrintf ("CDAudio: ioctl cdrompause failed\n");

	wasPlaying = playing;
	playing = false;
}


void
CDAudio_Resume (void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (ioctl (cdfile, CDROMRESUME) == -1)
		Con_DPrintf ("CDAudio: ioctl cdromresume failed\n");
	playing = true;
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

	if (strcasecmp (command, "on") == 0) {
		enabled = true;
		return;
	}

	if (strcasecmp (command, "off") == 0) {
		if (playing)
			CDAudio_Stop ();
		enabled = false;
		return;
	}

	if (strcasecmp (command, "reset") == 0) {
		enabled = true;
		if (playing)
			CDAudio_Stop ();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo ();
		return;
	}

	if (strcasecmp (command, "remap") == 0) {
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

	if (strcasecmp (command, "close") == 0) {
		CDAudio_CloseDoor ();
		return;
	}

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid) {
			Con_Printf ("No CD in player.\n");
			return;
		}
	}

	if (strcasecmp (command, "play") == 0) {
		CDAudio_Play ((byte) atoi (Cmd_Argv (2)), false);
		return;
	}

	if (strcasecmp (command, "loop") == 0) {
		CDAudio_Play ((byte) atoi (Cmd_Argv (2)), true);
		return;
	}

	if (strcasecmp (command, "stop") == 0) {
		CDAudio_Stop ();
		return;
	}

	if (strcasecmp (command, "pause") == 0) {
		CDAudio_Pause ();
		return;
	}

	if (strcasecmp (command, "resume") == 0) {
		CDAudio_Resume ();
		return;
	}

	if (strcasecmp (command, "eject") == 0) {
		if (playing)
			CDAudio_Stop ();
		CDAudio_Eject ();
		cdValid = false;
		return;
	}

	if (strcasecmp (command, "info") == 0) {
		Con_Printf ("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf ("Currently %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf ("Paused %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		Con_Printf ("Volume is %g\n", cdvolume);
		return;
	}
}

void
CDAudio_Update (void)
{
	struct cdrom_subchnl subchnl;
	static time_t lastchk;

	if (!enabled)
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

	if (playing && lastchk < time (NULL)) {
		lastchk = time (NULL) + 2;		// two seconds between chks
		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl (cdfile, CDROMSUBCHNL, &subchnl) == -1) {
			Con_DPrintf ("CDAudio: ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play (playTrack, true);
		}
	}
}

int
CDAudio_Init (void)
{
	int         i;

#if 0
	if (cls.state == ca_dedicated)
		return -1;
#endif

	if (COM_CheckParm ("-nocdaudio"))
		return -1;

	if ((i = COM_CheckParm ("-cddev")) != 0 && i < com_argc - 1) {
		strncpy (cd_dev, com_argv[i + 1], sizeof (cd_dev));
		cd_dev[sizeof (cd_dev) - 1] = 0;
	}

	if ((cdfile = open (cd_dev, O_RDONLY | O_NONBLOCK)) == -1) {
		Con_Printf ("CDAudio_Init: open of \"%s\" failed (%i)\n", cd_dev,
					errno);
		cdfile = -1;
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo ()) {
		Con_Printf ("CDAudio_Init: No CD in player.\n");
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

	Con_Printf ("CD Audio Initialized\n");

	return 0;
}


void
CDAudio_Shutdown (void)
{
	if (!initialized)
		return;
	CDAudio_Stop ();
	close (cdfile);
	cdfile = -1;
}
