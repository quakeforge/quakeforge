/*
	cd_ogg.c

	ogg music support

	Copyright (C) 1996-1997  Id Software, Inc.
	Modified Andrew Pilley, 2004

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
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/dstring.h"
#include "QF/qfplist.h"
#include "QF/sound.h"
#include "snd_render.h"

#include "compat.h"

static general_data_t plugin_info_general_data;
static general_funcs_t plugin_info_general_funcs;

static qboolean playing = false;
static qboolean wasPlaying = false;
static qboolean mus_enabled = false;

static cvar_t *bgmvolume;

static channel_t *cd_channel;
static sfx_t cd_sfx;

/* added by Andrew, for cd_ogg */
static cvar_t *mus_ogglist;				// contain the filename of the ogglist. 
static QFile *oggfile = NULL;			// filedescriptor with track map
static qboolean ogglistvalid = false;	// true if a valid ogg list has been
										// loaded
static plitem_t *tracklist = NULL;		// the parsed tracklist

/* end of added variables. */

/* does nothing */
static void
I_OGGMus_CloseDoor (void)
{
	Sys_DPrintf ("Entering I_OGGMus_CloseDoor\n");
	return;
}

/* does nothing. */
static void
I_OGGMus_Eject (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Eject\n");
	return;
}

/* we've opened the trackmap file from the quake resources
 * go through it, and make ourselves a tracklist map */
static int
I_OGGMus_GetAudioDiskInfo (void)
{
	/* read the track list, parse it a bit, map it into a list */
	int         size = -1;
	char       *buffile = NULL;

	Sys_DPrintf ("Entering I_OGGMus_GetAudioDiskInfo\n");
	ogglistvalid = false;

	if (oggfile == NULL) {
		return -1;
	}

	/* rewind the stream */
	Qseek (oggfile, 0, SEEK_SET);
	size = Qfilesize (oggfile);
	buffile = calloc (size + 10, sizeof (char));
	Qread (oggfile, buffile, size);
	tracklist = PL_GetPropertyList (buffile);
	if (!tracklist || tracklist->type != QFDictionary) {
		Sys_Printf ("Malformed or empty tracklist file. check mus_ogglist\n");
		return -1;
	}
	free (buffile);

	ogglistvalid = true;

	return 0;
}

/* pause playback of music? */
static void
I_OGGMus_Pause (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Pause\n");
	/* pause the ogg playback. */
	/* just kinda cheat and stop it for the time being */
	if (tracklist == NULL || !mus_enabled)
		return;

	if (!playing)
		return;

	wasPlaying = playing;
	playing = false;
}

/* stop playback of music. */
static void
I_OGGMus_Stop (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Stop\n");
	/* okay, stop playing oggs.  */
	if (tracklist == NULL || !mus_enabled)
		return;

	if (!playing)
		return;

	wasPlaying = false;
	playing = false;
}

/* start playing, if we've got a trackmap.
 * cry if we can't find a file to play */
static void
I_OGGMus_Play (int track, qboolean looping)
{
	plitem_t   *trackmap = NULL;
	dstring_t  *trackstring = dstring_new ();
	wavinfo_t  *info = 0;

	Sys_DPrintf ("Entering I_OGGMus_Play\n");
	/* alrighty. grab the list, map track to filename. grab filename from data
	   resources, attach sound to play, loop. */
	if (tracklist == NULL || !mus_enabled) {
		free (trackstring);
		return;
	}

	dsprintf (trackstring, "%i", track);
	trackmap = PL_ObjectForKey (tracklist, trackstring->str);
	if (trackmap == NULL || trackmap->type != QFString) {
		Sys_Printf ("No Track for track number %s.\n", trackstring->str);
		free (trackstring);
		return;
	}

	Sys_Printf ("Playing: %s.\n", (char *) trackmap->data);
	if (cd_channel->sfx) {
		cd_channel->sfx->close (cd_channel->sfx);
		memset (cd_channel, 0, sizeof (*cd_channel));
	}

	if (cd_sfx.name)
		free ((char *) cd_sfx.name);
	cd_sfx.name = strdup ((char *) trackmap->data);
	SND_Load (&cd_sfx);
	if (cd_sfx.wavinfo)
		info = cd_sfx.wavinfo (&cd_sfx);
	if (info)
		info->loopstart = 0;
	cd_channel->sfx = cd_sfx.open (&cd_sfx);
	if (cd_channel->sfx) {
		int         vol = bgmvolume->value * 255;

		cd_channel->master_vol = vol;
		cd_channel->leftvol = cd_channel->rightvol = cd_channel->master_vol;
	}

	free (trackstring);
	playing = true;
}

/* unpause. might just cheat and restart playing */
static void
I_OGGMus_Resume (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Resume\n");

	if (tracklist == NULL || !mus_enabled)
		return;

	if (!wasPlaying)
		return;
	playing = true;
}

static void
I_OGGMus_Shutdown (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Shutdown\n");
	/* clean up a bit, destroy the ogg if i have to */

	if (tracklist != NULL) {
		I_OGGMus_Stop ();
		PL_Free (tracklist);
		tracklist = NULL;
	}
	if (oggfile != NULL) {
		Qclose (oggfile);
		oggfile = NULL;
	}
	mus_enabled = false;
}

/* print out the current track map. */
static void
I_OGGMus_Info (void)
{
	plitem_t   *keylist = NULL;
	plitem_t   *currentmap = NULL;
	plitem_t   *currenttrack = NULL;
	int         iter = 0;

	Sys_DPrintf ("Entering I_OGGMus_Info\n");
	if (tracklist != NULL) {

		keylist = PL_D_AllKeys (tracklist);
		if (keylist == NULL) {
			Sys_DPrintf ("OGGMus: Didn't get valid plist_t array yet have "
						 "valid tracklist?\n");
			return;
		}
		Sys_DPrintf ("OGGMus: number of entries %i.\n",
					 ((plarray_t *) (keylist->data))->numvals);
		Sys_Printf ("\n" "Tracklist loaded from file:\n%s\n"
					"---------------------------\n", mus_ogglist->string);

		/* loop through the list with PL_ObjectAtIndex() */

		for (iter = 0;
			 iter < ((plarray_t *) (keylist->data))->numvals; iter++) {
			currentmap = PL_ObjectAtIndex (keylist, iter);
			if (currentmap == NULL) {
				Sys_DPrintf ("OGGMus: No track for track number %i.\n", iter);
				continue;
			}
			if (currentmap->type != QFString) {
				Sys_DPrintf ("OGGMus: Track fetched isn't a QFString for: %i\n",
							 iter);
				continue;
			}
			currenttrack = PL_ObjectForKey (tracklist,
											(char *) currentmap->data);
			Sys_Printf (" %s  -  %s\n", (char *) currentmap->data,
						(char *) currenttrack->data);

		}

		PL_Free (keylist);
	} else {
		Sys_Printf ("\n" "No Tracklist\n" "------------\n");
	}
}

static void
I_OGG_f (void)
{
	const char *command;

	Sys_DPrintf ("Entering I_OGG_f\n");
	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

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
		mus_enabled = true;
		if (playing)
			I_OGGMus_Stop ();
		I_OGGMus_GetAudioDiskInfo ();
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
		I_OGGMus_GetAudioDiskInfo ();
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

/* stubb out, since we don't need */
static void
I_OGGMus_Update (void)
{
}

/* called when the mus_ogglist cvar is changed */
static void
Mus_OggChange (cvar_t *mus_ogglist)
{
	int         size;

	Sys_DPrintf ("Entering Mus_OggChange\n");

	/* make sure we're not playing anything right now, and we've cleaned up */
	CDAudio_Shutdown ();

	if (strequal (mus_ogglist->string, "none")) {
		/* bail if we don't have one */
		return;
	}

	size = QFS_FOpenFile (mus_ogglist->string, &oggfile);
	if (size == -1) {
		Sys_Printf ("Mus_OGGInit: open of file \"%s\" failed\n",
					mus_ogglist->string);
		mus_enabled = false;
		return;
	}

	if (I_OGGMus_GetAudioDiskInfo () == -1) {
		/* check the info, read it in.  done */
		Sys_Printf ("Mus_OGGInit: invalid track map.\n");
		ogglistvalid = false;
		mus_enabled = false;
		return;
	}
	mus_enabled = true;
}

/* change volume on sound object */
static void
Mus_VolChange (cvar_t *bgmvolume)
{
	Sys_DPrintf ("Entering Mus_VolChange\n");
	if (cd_channel->sfx) {
		int         vol = bgmvolume->value * 255;

		cd_channel->master_vol = vol;
		cd_channel->leftvol = cd_channel->rightvol = cd_channel->master_vol;
	}
}

static void
I_OGGMus_Init (void)
{
	Sys_DPrintf ("Entering I_OGGMus_Init\n");

	cd_channel = S_AllocChannel ();

	/* check list file cvar, open list file, create map, close file. */

	mus_ogglist = Cvar_Get ("mus_ogglist", "tracklist.cfg", CVAR_NONE,
							Mus_OggChange,
							"filename of track to music file map");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, Mus_VolChange,
						  "Volume of CD music");
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
		"Copyright (C) 2001  contributors of the QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, file)
{
	return &plugin_info;
}
