/*
	cd_linux.c

	Linux CD Audio support

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
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
static qboolean playing = false;
static qboolean wasPlaying = false;
static qboolean mus_enabled = false;
static qboolean playLooping = false;
static float cdvolume;
static byte remap[100];
static byte playTrack;
static byte maxTrack;
static int  cdfile = -1;

static cvar_t *mus_cddevice;
static cvar_t *bgmvolume;


static void
I_CDAudio_CloseDoor (void)
{
	if (cdfile == -1 || !mus_enabled)
		return;							// no cd init'd

	if (ioctl (cdfile, CDROMCLOSETRAY) == -1)
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromclosetray failed\n");
}

static void
I_CDAudio_Eject (void)
{
	if (cdfile == -1 || !mus_enabled)
		return;							// no cd init'd

	if (ioctl (cdfile, CDROMEJECT) == -1)
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromeject failed\n");
}

static int
I_CDAudio_GetAudioDiskInfo (void)
{
	struct cdrom_tochdr tochdr;

	cdValid = false;

	if (ioctl (cdfile, CDROMREADTOCHDR, &tochdr) == -1) {
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromreadtochdr failed\n");
		return -1;
	}

	if (tochdr.cdth_trk0 < 1) {
		Sys_MaskPrintf (SYS_snd, "CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = tochdr.cdth_trk1;

	return 0;
}

static void
I_CDAudio_Pause (void)
{
	if (cdfile == -1 || !mus_enabled)
		return;

	if (!playing)
		return;

	if (ioctl (cdfile, CDROMPAUSE) == -1)
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdrompause failed\n");

	wasPlaying = playing;
	playing = false;
}

static void
I_CDAudio_Stop (void)
{
	if (cdfile == -1 || !mus_enabled)
		return;

	if (!playing)
		return;

	if (ioctl (cdfile, CDROMSTOP) == -1)
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromstop failed (%d)\n",
						errno);

	wasPlaying = false;
	playing = false;
}

static void
I_CDAudio_Play (int track, qboolean looping)
{
	struct cdrom_tocentry entry0;
	struct cdrom_tocentry entry1;
	struct cdrom_msf msf;

	if (cdfile == -1 || !mus_enabled)
		return;

	if (!cdValid) {
		I_CDAudio_GetAudioDiskInfo ();
		if (!cdValid)
			return;
	}

	if (track < 0 || track >= (int) sizeof (remap)) {
		Sys_Printf ("CDAudio: invalid track number\n");
		return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack) {
		I_CDAudio_Stop ();
		return;
	}
	// don't try to play a non-audio track
	entry0.cdte_track = track;
	entry0.cdte_format = CDROM_MSF;
	if (ioctl (cdfile, CDROMREADTOCENTRY, &entry0) == -1) {
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromreadtocentry failed\n");
		return;
	}
	entry1.cdte_track = track + 1;
	entry1.cdte_format = CDROM_MSF;
	if (entry1.cdte_track > maxTrack) {
		entry1.cdte_track = CDROM_LEADOUT;
	}
	if (ioctl (cdfile, CDROMREADTOCENTRY, &entry1) == -1) {
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry0.cdte_ctrl == CDROM_DATA_TRACK) {
		Sys_Printf ("track %i is not audio\n", track);
		return;
	}

	if (playing) {
		if (playTrack == track)
			return;
		I_CDAudio_Stop ();
	}

	msf.cdmsf_min0 = entry0.cdte_addr.msf.minute;
	msf.cdmsf_sec0 = entry0.cdte_addr.msf.second;
	msf.cdmsf_frame0 = entry0.cdte_addr.msf.frame;

	msf.cdmsf_min1 = entry1.cdte_addr.msf.minute;
	msf.cdmsf_sec1 = entry1.cdte_addr.msf.second;
	msf.cdmsf_frame1 = entry1.cdte_addr.msf.frame;

	Sys_MaskPrintf (SYS_snd, "%2d:%02d:%02d %2d:%02d:%02d\n",
					msf.cdmsf_min0,
					msf.cdmsf_sec0,
					msf.cdmsf_frame0,
					msf.cdmsf_min1, msf.cdmsf_sec1, msf.cdmsf_frame1);

	if (ioctl (cdfile, CDROMPLAYMSF, &msf) == -1) {
		Sys_MaskPrintf (SYS_snd,
						"CDAudio: ioctl cdromplaytrkind failed (%s)\n",
						strerror (errno));
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		I_CDAudio_Pause ();
}

static void
I_CDAudio_Resume (void)
{
	if (cdfile == -1 || !mus_enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (ioctl (cdfile, CDROMRESUME) == -1)
		Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromresume failed\n");
	playing = true;
}

static void
I_CDAudio_Shutdown (void)
{
	if (cdfile != -1) {
		I_CDAudio_Stop ();
		close (cdfile);
		cdfile = -1;
	}
	mus_enabled = false;
}

static void
I_CD_f (void)
{
	const char *command;
	int         ret, n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (strequal (command, "on")) {
		mus_enabled = true;
		return;
	}

	if (strequal (command, "off")) {
		if (playing)
			I_CDAudio_Stop ();
		mus_enabled = false;
		return;
	}

	if (strequal (command, "reset")) {
		mus_enabled = true;
		if (playing)
			I_CDAudio_Stop ();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		I_CDAudio_GetAudioDiskInfo ();
		return;
	}

	if (strequal (command, "remap")) {
		ret = Cmd_Argc () - 2;
		if (ret <= 0) {
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Sys_Printf ("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi (Cmd_Argv (n + 1));
		return;
	}

	if (strequal (command, "close")) {
		I_CDAudio_CloseDoor ();
		return;
	}

	if (!cdValid) {
		I_CDAudio_GetAudioDiskInfo ();
		if (!cdValid) {
			Sys_Printf ("No CD in player.\n");
			return;
		}
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
		if (playing)
			I_CDAudio_Stop ();
		I_CDAudio_Eject ();
		cdValid = false;
		return;
	}

	if (strequal (command, "info")) {
		Sys_Printf ("%u tracks\n", maxTrack);
		if (playing)
			Sys_Printf ("Currently %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Sys_Printf ("Paused %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		Sys_Printf ("Volume is %g\n", cdvolume);
		return;
	}
}

static void
I_CDAudio_Update (void)
{
	struct cdrom_subchnl subchnl;
	static time_t lastchk;

	if (!mus_enabled)
		return;

	if (bgmvolume->value != cdvolume) {
		if (cdvolume) {
			Cvar_SetValue (bgmvolume, 0.0);
			cdvolume = bgmvolume->value;
			I_CDAudio_Pause ();
		} else {
			Cvar_SetValue (bgmvolume, 1.0);
			cdvolume = bgmvolume->value;
			I_CDAudio_Resume ();
		}
	}

	if (playing && lastchk < time (NULL)) {
		lastchk = time (NULL) + 2;		// two seconds between chks
		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl (cdfile, CDROMSUBCHNL, &subchnl) == -1) {
			Sys_MaskPrintf (SYS_snd, "CDAudio: ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED) {
			playing = false;
			if (playLooping)
				I_CDAudio_Play (playTrack, true);
		}
	}
}

static void
Mus_CDChange (cvar_t *mus_cdaudio)
{
	int         i;

	I_CDAudio_Shutdown ();
	if (strequal (mus_cdaudio->string, "none")) {
		return;
	}

	cdfile = open (mus_cdaudio->string, O_RDONLY | O_NONBLOCK);
	if (cdfile == -1) {
		Sys_MaskPrintf (SYS_snd,
						"Mus_CDInit: open device \"%s\" failed (error %i)\n",
						mus_cdaudio->string, errno);
		return;
	}

	if (I_CDAudio_GetAudioDiskInfo ()) {
		Sys_Printf ("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;

	mus_enabled = true;
}

static void
I_CDAudio_Init (void)
{
	mus_cddevice = Cvar_Get ("mus_cddevice", "/dev/cdrom", CVAR_NONE,
							 Mus_CDChange, "device to use for CD music");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, NULL,
						  "Volume of CD music");
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
	"Linux CD Audio output\n",
	"Copyright (C) 2001  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, linux)
{
	return &plugin_info;
}
