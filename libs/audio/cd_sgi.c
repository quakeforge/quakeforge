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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <sys/types.h>
#include <dmedia/cdaudio.h>


#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/cd.h"

#include "compat.h"

static plugin_t plugin_info;
static plugin_data_t plugin_info_data;
static plugin_funcs_t plugin_info_funcs;
static general_data_t plugin_info_general_data;
static general_funcs_t plugin_info_general_funcs;

static cd_funcs_t plugin_info_cd_funcs;

static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static float cdvolume;
static byte remap[100];
static byte playTrack;

static char cd_dev[] = "/dev/cdrom";

static CDPLAYER *cdp = NULL;
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
I_SGI_Eject (void)
{
	if (cdp == NULL || !enabled)
		return;							// no cd init'd

	if (CDeject (cdp) == 0)
		Sys_MaskPrintf (SYS_snd, "I_SGI_Eject: CDeject failed\n");
}

static int
I_SGI_GetState (void)
{
	CDSTATUS	cds;

	if (cdp == NULL || !enabled)
		return -1;						// no cd init'd

	if (CDgetstatus (cdp, &cds) == 0) {
		Sys_MaskPrintf (SYS_snd, "CDAudio_GetStatus: CDgetstatus failed\n");
		return -1;
	}

	return cds.state;
}

static int
I_SGI_MaxTrack (void)
{
	CDSTATUS	cds;

	if (cdp == NULL || !enabled)
		return -1;						// no cd init'd

	if (CDgetstatus (cdp, &cds) == 0) {
		Sys_MaskPrintf (SYS_snd, "I_SGI_MaxTrack: CDgetstatus failed\n");
		return -1;
	}

	return cds.last;
}

void
I_SGI_Pause (void)
{
	if (cdp == NULL || !enabled || I_SGI_GetState () != CD_PLAYING)
		return;

	if (CDtogglepause (cdp) == 0)
		Sys_MaskPrintf (SYS_snd, "CDAudio_PAUSE: CDtogglepause failed (%d)\n", errno);
}

void
I_SGI_Play (int track, qboolean looping)
{
	int			maxtrack = I_SGI_MaxTrack ();

	if (!initialized || !enabled)
		return;

	/* cd == audio cd? */
	if (I_SGI_GetState () != CD_READY) {
		Sys_Printf ("CDAudio_Play: CD in player not an audio CD.\n");
		return;
	}

	if (maxtrack < 0) {
		Sys_MaskPrintf (SYS_snd,
						"CDAudio_Play: Error getting maximum track number\n");
		return;
	}

	if (track < 0 || track >= sizeof (remap)) {
		Sys_Printf ("CDAudio: invalid track number\n");
		return;
	}

	track = remap[track];

	if (track < 1 || track > maxtrack) {
		I_SGI_Stop ();
		return;
	}
	// don't try to play a non-audio track
/* mw: how to do this on irix? entry0.cdte_track = track;
	   entry0.cdte_format = CDROM_MSF; if ( ioctl(cdfile, CDROMREADTOCENTRY,
	   &entry0) == -1 ) { Sys_DPrintf("CDAudio: ioctl cdromreadtocentry
	   failed\n"); return; }

	   entry1.cdte_track = track + 1; entry1.cdte_format = CDROM_MSF; if
	   (entry1.cdte_track > maxTrack) { entry1.cdte_track = CDROM_LEADOUT; }

	   if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry1) == -1 ) {
	   Sys_DPrintf("CDAudio: ioctl cdromreadtocentry failed\n"); return; }

	   if (entry0.cdte_ctrl == CDROM_DATA_TRACK) { Sys_Printf("track %i is
	   not audio\n", track); return; }
*/

	if (I_SGI_GetState () == CD_PLAYING) {
		if (playTrack == track)
			return;

		I_SGI_Stop ();
	}

	if (CDplaytrack (cdp, track, cdvolume == 0.0 ? 0 : 1) == 0) {
		Sys_MaskPrintf (SYS_snd, "CDAudio_Play: CDplay failed (%d)\n", errno);
		return;
	}

	playLooping = looping;
	playTrack = track;
}

void
I_SGI_Resume (void)
{
	if (cdp == NULL || !enabled || I_SGI_GetState () != CD_PAUSED)
		return;

	if (CDtogglepause (cdp) == 0)
		Sys_MaskPrintf (SYS_snd, "CDAudio_Resume: CDtogglepause failed (%d)\n",
						errno);
}

void
I_SGI_Shutdown (void)
{
	if (!initialized)
		return;

	I_SGI_Stop ();
	CDclose (cdp);
	cdp = NULL;
	initialized = false;
}

void
I_SGI_Stop (void)
{
	if (cdp == NULL || !enabled || I_SGI_GetState () != CD_PLAYING)
		return;

	if (CDstop (cdp) == 0)
		Sys_MaskPrintf (SYS_snd, "I_SGI_Stop: CDStop failed (%d)\n", errno);
}

void
I_SGI_Update (void)
{
	if (!initialized || !enabled)
		return;

	if (bgmvolume != cdvolume) {
		if (cdvolume) {
			bgmvolume = 0.0;
			cdvolume = bgmvolume;
			CDAudio_Pause ();
		} else {
			bgmvolume = 1.0;
			cdvolume = bgmvolume;
			CDAudio_Resume ();
		}
	}

	if (I_SGI_GetState () != CD_PLAYING
		&& I_SGI_GetState () != CD_PAUSED && playLooping)
		CDAudio_Play (playTrack, true);
}

static void
I_SGI_f (void)
{
	const char       *command;
	int         ret, n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (strequal (command, "on")) {
		enabled = true;
		return;
	}

	if (strequal (command, "off")) {
		I_SGI_Stop ();
		enabled = false;
		return;
	}

	if (strequal (command, "reset")) {
		enabled = true;
		I_SGI_Stop ();

		for (n = 0; n < 100; n++)
			remap[n] = n;

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

	if (strequal (command, "play")) {
		CDAudio_Play (atoi (Cmd_Argv (2)), false);
		return;
	}

	if (strequal (command, "loop")) {
		CDAudio_Play (atoi (Cmd_Argv (2)), true);
		return;
	}

	if (strequal (command, "stop")) {
		I_SGI_Stop ();
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
		I_SGI_Stop ();
		I_SGI_Eject ();
		return;
	}

	if (strequal (command, "info")) {
		Sys_Printf ("%u tracks\n", I_SGI_MaxTrack ());
		if (I_SGI_GetState () == CD_PLAYING)
			Sys_Printf ("Currently %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		else if (I_SGI_GetState () == CD_PAUSED)
			Sys_Printf ("Paused %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);

		Sys_Printf ("Volume is %g\n", cdvolume);
		return;
	}
}

static void
I_SGI_Init (void)
{
	int         i;

	Cvar_Register (&bgmvolume_cvar, 0, 0);
	if ((i = COM_CheckParm ("-cddev")) != 0 && i < com_argc - 1) {
		strncpy (cd_dev, com_argv[i + 1], sizeof (cd_dev));
		cd_dev[sizeof (cd_dev) - 1] = 0;
	}

	cdp = CDopen (cd_dev, "r");

	if (cdp == NULL) {
		Sys_Printf ("CDAudio_Init: open of \"%s\" failed (%i)\n", cd_dev,
					errno);
		return ;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;

	initialized = true;
	enabled = true;

	Sys_Printf ("CD Audio Initialized\n");
}

static general_funcs_t plugin_info_general_funcs = {
	I_SGI_Init,
	I_SGI_Shutdown,
};

static cd_funcs_t plugin_info_cd_funcs = {
	0,
	I_SGI_f,
	I_SGI_Pause,
	I_SGI_Play,
	I_SGI_Resume,
	I_SGI_Update,
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
	"SGI CD Audio output\n",
	"Copyright (C) 2001  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, sgi)
{
	return &plugin_info;
}
