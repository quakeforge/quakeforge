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
#include "QF/plist.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/general.h"
#include "QF/plugin/cd.h"

#include "compat.h"

/* Generic plugin structures */
static general_data_t plugin_info_general_data;
static general_funcs_t plugin_info_general_funcs;

/* global status variables. */
static bool	playing = false;
static bool	wasPlaying = false;
static bool	mus_enabled = false;
static bool	ogglistvalid = false;

/* sound resources */
static channel_t *cd_channel;
static plitem_t	 *tracklist = NULL;	// parsed tracklist, dictionary format
static plitem_t  *play_list;		// string or array of strings
static int        play_pos = -1;	// position in play_list (0 for string)
									// -1 = invalid (both)

static float bgmvolume;
static cvar_t bgmvolume_cvar = {
	.name = "bgmvolume",
	.description =
		"Volume of CD music",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &bgmvolume },
};
static char *mus_ogglist;
static cvar_t mus_ogglist_cvar = {
	.name = "mus_ogglist",
	.description =
		"filename of track to music file map",
	.default_value = "tracklist.cfg",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &mus_ogglist },
};


static void
set_volume (void)
{
	if (cd_channel) {
		S_ChannelSetVolume (cd_channel, bgmvolume);
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

	playing = false;
	wasPlaying = false;

	if (cd_channel) {
		S_ChannelFree (cd_channel);
		cd_channel = NULL;
	}
}

static void
I_OGGMus_Shutdown (void)
{
	if (tracklist) {
		I_OGGMus_Stop ();
		PL_Release (tracklist);
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
	int		 size;

	/* kill off the old tracklist, and make sure we're not playing anything */
	I_OGGMus_Shutdown ();

	ogglistvalid = false;
	mus_enabled = false;

	if (!mus_ogglist || strequal (mus_ogglist, "none")) {
		return -1;		// bail if we don't have a valid filename
	}

	oggfile = QFS_FOpenFile (mus_ogglist);
	if (!oggfile) {
		Sys_Printf ("Mus_OggInit: open of file \"%s\" failed\n",
			mus_ogglist);
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

	PL_Release (tracklist);
	tracklist = PL_GetPropertyList (buffile, 0);
	if (!tracklist || PL_Type (tracklist) != QFDictionary) {
		Sys_Printf ("Malformed or empty tracklist file. check mus_ogglist\n");
		return -1;
	}

	free (buffile);
	Qclose (oggfile);

	ogglistvalid = true;
	mus_enabled = true;
	return 0;
}

static void
I_OGGMus_SetPlayList (int track)
{
	const char *trackstring = va ("%i", track);
	int         i;

	play_list = PL_ObjectForKey (tracklist, trackstring);
	if (!play_list) {
		Sys_Printf ("No Track entry for track #%d.\n", track);
		return;
	}
	if (PL_Type (play_list) == QFString)
		return;
	if (PL_Type (play_list) != QFArray) {
		Sys_Printf ("Track entry for track #%d not string or array.\n", track);
		play_list = 0;
		return;
	}
	for (i = 0; i < PL_A_NumObjects (play_list); i++) {
		plitem_t   *item = PL_ObjectAtIndex (play_list, i);
		if (!item || PL_Type (item) != QFString) {
			Sys_Printf ("Bad subtract %d in track %d.\n", i, track);
			play_list = 0;
			return;
		}
	}
}

static void
I_OGGMus_PlayNext (int looping)
{
	const char *track;
	sfx_t      *sfx;

	if (!play_list)
		return;
	if (PL_Type (play_list) == QFString) {
		track = PL_String (play_list);
		play_pos = 0;
	} else {
		play_pos++;
		if (play_pos >= PL_A_NumObjects (play_list))
			play_pos = 0;
		track = PL_String (PL_ObjectAtIndex (play_list, play_pos));
		looping = 0;
	}

	if (cd_channel) {
		S_ChannelFree (cd_channel);
		cd_channel = 0;
	}

	if (!(cd_channel = S_AllocChannel ()))
		return;

	if (!(sfx = S_LoadSound (track)) || !S_ChannelSetSfx (cd_channel, sfx)) {
		S_ChannelFree (cd_channel);
		cd_channel = 0;
		return;
	}
	S_ChannelSetLooping (cd_channel, looping ? 1 : -1);
	set_volume ();
	Sys_Printf ("Playing: %s.\n", track);

	playing = true;
}

static void
I_OGGMus_Pause (void)
{
	if (!tracklist || !mus_enabled || !playing)
		return;

	if (cd_channel)
		S_ChannelSetPaused (cd_channel, 1);

	wasPlaying = playing;
	playing = false;
}

static void
I_OGGMus_Resume (void)
{
	if (!tracklist || !mus_enabled || !wasPlaying)
		return;

	set_volume ();
	S_ChannelSetPaused (cd_channel, 0);
	wasPlaying = false;
	playing = true;
}

/* start playing, if we've got a play_list.
 * cry if we can't find a file to play */
static void
I_OGGMus_Play (int track, bool looping)
{
	/* alrighty. grab the list, map track to filename. grab filename from data
	   resources, attach sound to play, loop. */

	if (!tracklist || !mus_enabled)
		return;

	if (playing)
		I_OGGMus_Stop ();

	if (!track)
		return;
	I_OGGMus_SetPlayList (track);
	I_OGGMus_PlayNext (looping);
}

/* print out the current track map, in numerical order. */
static void
I_OGGMus_Info (void)
{
	int			 count = 0, iter = 0, keycount = 0;
	const char	*trackstring;
	plitem_t	*currenttrack = NULL;

	if (!tracklist) {
		Sys_Printf ("\n" "No Tracklist\n" "------------\n");
		return;
	}
	if (!(keycount = PL_D_NumKeys (tracklist)))
		return;

	Sys_Printf ("\n" "Tracklist loaded from file:\n%s\n"
				"---------------------------\n", mus_ogglist);

	/* loop, and count up the Highest key number. */
	for (iter = 1, count = 0; count < keycount && iter <= 99 ; iter++) {
		trackstring = va ("%i", iter);
		if (!(currenttrack = PL_ObjectForKey (tracklist, trackstring))) {
			continue;
		}

		Sys_Printf (" %s  -  %s\n", trackstring, PL_String (currenttrack));
		count++;
	}
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
		I_OGGMus_Play (atoi (Cmd_Argv (2)), false);
		return;
	}

	if (strequal (command, "loop")) {
		I_OGGMus_Play (atoi (Cmd_Argv (2)), true);
		return;
	}

	if (strequal (command, "stop")) {
		I_OGGMus_Stop ();
		return;
	}

	if (strequal (command, "pause")) {
		I_OGGMus_Pause ();
		return;
	}

	if (strequal (command, "resume")) {
		I_OGGMus_Resume ();
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
	if (!cd_channel || S_ChannelGetState (cd_channel) > chan_done)
		return;
	// will get here only when multi-tracked
	I_OGGMus_Stop ();
	I_OGGMus_PlayNext (0);
}

/* called when the mus_ogglist cvar is changed */
static void
Mus_OggChange (void *data, const cvar_t *cvar)
{
	Load_Tracklist ();
}

/* change volume on sound object */
static void
Mus_VolChange (void *data, const cvar_t *bgmvolume)
{
	set_volume ();
}

static void
Mus_gamedir (int phase, void *data)
{
	if (phase)
		Load_Tracklist ();
}

static void
I_OGGMus_Init (void)
{
	/* check list file cvar, open list file, create map, close file. */
	Cvar_Register (&mus_ogglist_cvar, Mus_OggChange, 0);
	Cvar_Register (&bgmvolume_cvar, Mus_VolChange, 0);
	QFS_GamedirCallback (Mus_gamedir, 0);
}

static general_funcs_t plugin_info_general_funcs = {
	I_OGGMus_Init,
	I_OGGMus_Shutdown,
};

static cd_funcs_t plugin_info_cd_funcs = {
	0,
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
