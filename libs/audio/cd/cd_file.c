/*
	cd_ogg.c

	ogg music support

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2004 Andrew Pilley
	Copyright (C) 2004 Bill Currie <taniwha@quakeforge.net>
	Copyright (C) 2004 Jeff Teunissen <deek@quakeforge.net>

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

static __attribute__ ((unused)) const char  rcsid[] =
	"$Id$";

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

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "snd_render.h"

/* Generic plugin structures */
static general_data_t plugin_info_general_data;
static general_funcs_t plugin_info_general_funcs;

/* global status variables. */
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	mus_enabled = false;
static qboolean	ogglistvalid = false;

/* sound resources */
static channel_t *cd_channel;
static sfx_t	 *cd_sfx;
static int		  current_track;	// current track, used when pausing
static plitem_t	 *tracklist = NULL;	// parsed tracklist, dictionary format

static cvar_t	 *bgmvolume;		// volume cvar
static cvar_t	 *mus_ogglist;	// tracklist cvar


static void
set_volume (void)
{
	if (cd_channel && cd_channel->sfx) {
		int		vol = bgmvolume->value * 255;

		cd_channel->master_vol = vol;
		cd_channel->leftvol = cd_channel->rightvol = cd_channel->master_vol;
	}
}

static void
I_OGGMus_CloseDoor (void)
{
}

static void
I_OGGMus_Eject (void)
{
}

/* stop playback of music */
static void
I_OGGMus_Stop (void)
{
	if (!tracklist || !mus_enabled || !playing)
		return;
	
	current_track = -1;
	playing = false;
	wasPlaying = false;

	if (cd_sfx) {
		cd_sfx->close (cd_sfx);
		cd_channel->sfx = NULL;
	}
}

static void
I_OGGMus_Shutdown (void)
{
	if (tracklist) {
		I_OGGMus_Stop ();
		PL_Free (tracklist);
		tracklist = NULL;
	}
	mus_enabled = false;
}

/* we've opened the trackmap file from the quake resources
 * go through it, and make ourselves a tracklist map */
static int
Load_Tracklist (void)
{
	QFile	*oggfile = NULL;
	char	*buffile = NULL;
	int		 size = -1;

	/* kill off the old tracklist, and make sure we're not playing anything */
	I_OGGMus_Shutdown ();

	ogglistvalid = false;
	mus_enabled = false;

	if (!mus_ogglist || strequal (mus_ogglist->string, "none")) {
		return -1;		// bail if we don't have a valid filename
	}

	size = QFS_FOpenFile (mus_ogglist->string, &oggfile);
	if (size < 0) {
		Sys_Printf ("Mus_OggInit: open of file \"%s\" failed\n",
			mus_ogglist->string);
		return -1;
	}

	if (!oggfile) {
		return -1;
	}

	/* rewind the stream */
	Qseek (oggfile, 0, SEEK_SET);
	size = Qfilesize (oggfile);
	buffile = calloc (size+10, sizeof (char));
	Qread (oggfile, buffile, size);

	tracklist = PL_GetPropertyList (buffile);
	if (!tracklist || tracklist->type != QFDictionary) {
		Sys_Printf ("Malformed or empty tracklist file. check mus_ogglist\n");
		return -1;
	}

	free (buffile);
	Qclose (oggfile);

	ogglistvalid = true;
	mus_enabled = true;
	return 0;
}

/* pause playback of music */
static void
I_OGGMus_Pause (void)
{
	/* just kinda cheat and stop it for the time being */
	if (!tracklist || !mus_enabled || !playing)
		return;
	
	if (cd_sfx) {
		cd_sfx->close (cd_sfx);
		cd_channel->sfx = NULL;
	}

	wasPlaying = playing;
	playing = false;
}


/* start playing, if we've got a trackmap.
 * cry if we can't find a file to play */
static void
I_OGGMus_Play (int track, qboolean looping)
{
	plitem_t	*trackmap = NULL;
	wavinfo_t	*info = 0;
	const char	*trackstring;

	/* alrighty. grab the list, map track to filename. grab filename from data
	   resources, attach sound to play, loop. */

	if (!cd_channel && mus_enabled) {		// Shouldn't happen!
		Sys_Printf ("OGGMus: on fire.\n");
		mus_enabled = false;
	}

	if (!tracklist || !mus_enabled)
		return;

	trackstring = va ("%i", track);
	trackmap = PL_ObjectForKey (tracklist, trackstring);
	if (!trackmap || trackmap->type != QFString) {
		Sys_Printf ("No Track entry for track #%s.\n", trackstring);
		return;
	}

	Sys_Printf ("Playing: %s.\n", (char *) trackmap->data);
	if (cd_channel->sfx) {
		cd_channel->sfx->close (cd_channel->sfx);
		memset (cd_channel, 0, sizeof (*cd_channel));
	}

	if (!(cd_sfx = S_LoadSound ((char *) trackmap->data)))
		return;

	if (cd_sfx->wavinfo)
		info = cd_sfx->wavinfo (cd_sfx);
	if (info) {
		if (looping == true)
			info->loopstart = 0;
		else
			info->loopstart = -1;
	}
	cd_channel->sfx = cd_sfx->open (cd_sfx);
	set_volume ();

	playing = true;
	current_track = track;
}

/* unpause. might just cheat and restart playing */
static void
I_OGGMus_Resume (void)
{
	if (!tracklist || !mus_enabled || !wasPlaying)
		return;

	I_OGGMus_Play (current_track, true);
	playing = true;
}

/* print out the current track map. */
static void
I_OGGMus_Info (void)
{
	plitem_t	*keylist = NULL;
	plitem_t	*currentmap = NULL;
	plitem_t	*currenttrack = NULL;
	int			 iter = 0;

	if (!tracklist) {
		Sys_Printf ("\n" "No Tracklist\n" "------------\n");
		return;
	}
	if (!(keylist = PL_D_AllKeys (tracklist))) {
		Sys_DPrintf ("OGGMus: Didn't get valid plist_t array, yet have "
					 "valid tracklist?\n");
		return;
	}

	Sys_DPrintf ("OGGMus: number of entries %i.\n",
				 ((plarray_t *) (keylist->data))->numvals);
	Sys_Printf ("\n" "Tracklist loaded from file:\n%s\n"
				"---------------------------\n", mus_ogglist->string);

	/* loop through the list with PL_ObjectAtIndex () */
	for (iter = 0; iter < ((plarray_t *) (keylist->data))->numvals; iter++) {
		if (!(currentmap = PL_ObjectAtIndex (keylist, iter))) {
			Sys_DPrintf ("OGGMus: No track for track number %i.\n", iter);
			continue;
		}

		if (currentmap->type != QFString) {
			Sys_DPrintf ("OGGMus: Track data isn't a string for: %i\n", iter);
			continue;
		}

		currenttrack = PL_ObjectForKey (tracklist, (char *) currentmap->data);
		Sys_Printf (" %s  -  %s\n", (char *) currentmap->data,
					(char *) currenttrack->data);
	}
	PL_Free (keylist);
}

static void
I_OGG_f (void)
{
	const char *command;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (!cd_channel) {
		Sys_Printf ("OGGMus: Don't have a channel.\n");
		mus_enabled = false;
		return;
	}

	if (strequal (command, "on")) {
		mus_enabled = true;
		return;
	}

	if (strequal (command, "off")) {
		if (playing)
			I_OGGMus_Stop ();
		mus_enabled = false;
		return;
	}

	if (strequal (command, "reset")) {
		Load_Tracklist ();
		return;
	}

	if (strequal (command, "remap")) {
		Sys_Printf ("OGGMus: remap does nothing.\n");
		return;
	}

	if (strequal (command, "close")) {
		I_OGGMus_CloseDoor ();
		return;
	}

	if (!tracklist) {
		Load_Tracklist ();
		if (!tracklist) {
			Sys_Printf ("Can't initialize tracklist.\n");
			return;
		}
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
		I_OGGMus_Stop ();
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
		if (playing)
			I_OGGMus_Stop ();
		I_OGGMus_Eject ();
		return;
	}

	if (strequal (command, "info")) {
		I_OGGMus_Info ();
		return;
	}
}

static void
I_OGGMus_Update (void)
{
}

/* called when the mus_ogglist cvar is changed */
static void
Mus_OggChange (cvar_t *ogglist)
{
	mus_ogglist = ogglist;
	Load_Tracklist ();
}

/* change volume on sound object */
static void
Mus_VolChange (cvar_t *bgmvolume)
{
	set_volume ();
}

static void
Mus_gamedir (void)
{
	Mus_OggChange (mus_ogglist);
}

static void
I_OGGMus_Init (void)
{
	cd_channel = S_AllocChannel ();
	if (!cd_channel) // We can't fail to load yet... so just disable everything
		Sys_Printf ("OGGMus: Failed to allocate sound channel.\n");

	/* check list file cvar, open list file, create map, close file. */
	mus_ogglist = Cvar_Get ("mus_ogglist", "tracklist.cfg", CVAR_NONE,
							Mus_OggChange,
							"filename of track to music file map");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, Mus_VolChange,
							"Volume of CD music");
	QFS_GamedirCallback (Mus_gamedir);
}

static general_funcs_t plugin_info_general_funcs = {
	I_OGGMus_Init,
	I_OGGMus_Shutdown,
};

static cd_funcs_t plugin_info_cd_funcs = {
	I_OGG_f,
	I_OGGMus_Pause,
	I_OGGMus_Play,
	I_OGGMus_Resume,
	I_OGGMus_Update,
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
	"OGG Music output\n",
	"Copyright (C) 2004 Andrew Pilley\n"
	"Copyright (C) 2004 Members of the QuakeForge Project\n"
	"See the file \"AUTHORS\" for more information.\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, file)
{
	return &plugin_info;
}
