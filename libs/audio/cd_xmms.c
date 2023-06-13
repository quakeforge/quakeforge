/*
	xmms_cd.c

	XMMS Player support, disguised as a cdrom.

	Copyright (C) 2001 Alexis Paul Musgrave

	Author: Alexis Paul Musgrave
	Date: 2001/09/28

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
#include <xmmsctrl.h>

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

static const char *xmms_cmd = "xmms";
static const char *xmms_args[] = {"xmms", 0};
// Session number, gets set to 0 in I_XMMS_Init if not set
static int sessionNo;

// don't need either of these now
//static int xmmsPid = '0';
//static int sigNo = '2';

static bool playing = false;

// no idea why I have wasPlaying, pbly this code was based on cd_linux.c :/
static bool wasPlaying = false;
static bool musEnabled = true;

static void I_XMMS_Running(void);
static void I_XMMS_Stop(void);
static void I_XMMS_Play(int, bool);
static void I_XMMS_Pause(void);
static void I_XMMS_Resume(void);
static void I_XMMS_Next(void);
static void I_XMMS_Prev(void);
static void I_XMMS_Update(void);
static void XMMS_SessionChg(cvar_t *);
static void I_XMMS_Init(void);
static void I_XMMS_Shutdown(void);
static void I_XMMS_Kill(void);
static void I_XMMS_On(void);
static void I_XMMS_Off(void);
static void I_XMMS_Shuffle(void);
static void I_XMMS_Repeat(void);
static void I_XMMS_Pos(int);
static void I_XMMS_Info(void);
static void I_XMMS_f(void);
plugin_t *cd_xmms_PluginInfo (void);

//static float cdvolume;
//static byte remap[100];
//static byte playTrack;
//static byte maxTrack;

// FIXME: All of this code assumes that the xmms_remote_* functions succeed
// FIXME: shouldn't I use gint for all the xmms stuff like
// FIXME (cont): /usr/include/xmms/xmmsctrl.h says?

static void
I_XMMS_Running (void)
{
	int         res, i;
	int         fd_size = getdtablesize ();

	if (!xmms_remote_is_running (sessionNo)) {
		// this method is used over a system() call, so that we know child's
		// pid (not that is particularly important) but so we can close
		// unneeded descriptors

		res = fork ();

		switch (res) {
			case 0:					// Child
				// Well, we don't want the child to be running about with
				// 27001 still open

				for (i = 3; i < fd_size; i++)
					close (i);

				// run xmms
				if (execvp (xmms_cmd, (char **)xmms_args)) {
					// Oh dear, can we even use Sys_DPrinf? We are child so
					// we have access to them? But wouldn't it just try
					// rendering it and screw stuff up? Better not find out.

					exit (1);			// Well, we can't just hang about
										// causing trouble can we ?
				}
				break;
			case -1:					// ICH!
				// inform user
				Sys_MaskPrintf (SYS_snd, "XMMSAudio: error, can't fork!?\n");
				break;
			default:					// Parent
				// don't need now :/
//				xmmsPid = res;			// so we can kill it later
				break;
		}
		return;
	}
}


static void
I_XMMS_Stop (void)						// stop playing
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	if (!xmms_remote_is_playing (sessionNo))
		return;							// check that its actually playing

	xmms_remote_stop (sessionNo);		// stop it

	wasPlaying = playing;
	playing = false;
}

// Play
// start it playing, (unless disabled)
static void
I_XMMS_Play (int track, bool looping)		// looping for compatability
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	// I think this will fix some wierdness
	if (xmms_remote_is_paused (sessionNo)) {
		xmms_remote_pause (sessionNo);
		return;
	}

	if (track >= 0)
		xmms_remote_set_playlist_pos (sessionNo, track);

	if (xmms_remote_is_playing (sessionNo))
		return;

	xmms_remote_play (sessionNo);

	wasPlaying = playing;
	playing = true;
	return;
}

static void
I_XMMS_Pause (void)
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	if (!xmms_remote_is_playing (sessionNo))
		return;

	xmms_remote_pause (sessionNo);

	wasPlaying = playing;
	playing = false;
}

static void
I_XMMS_Resume (void)
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	// i think this will fix some wierdness
	if (xmms_remote_is_paused (sessionNo)) {
		xmms_remote_pause (sessionNo);
		return;
	}
	if (xmms_remote_is_playing (sessionNo))
		return;

	xmms_remote_play (sessionNo);
	wasPlaying = playing;
	playing = true;
}

static void
I_XMMS_Prev (void)
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	xmms_remote_playlist_prev (sessionNo);
}

static void
I_XMMS_Next (void)
{
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	xmms_remote_playlist_next (sessionNo);
}

static void
I_XMMS_Update (void)
{
}

static void
XMMS_SessionChg(cvar_t *xmms_session)
{
	sessionNo = xmms_session->int_val;
}

static void
I_XMMS_Init (void)
{
	cvar_t *tmp;

	I_XMMS_Running ();
	Cmd_AddCommand ("xmms", I_XMMS_f, "Control the XMMS player.\n"
					"Commands:\n"
					"resume - Will resume playback after pause.\n"
					"off - Stops control and playback of XMMS.\n"
					"on - Starts XMMS if not running, or enables playback.\n"
					"pause - Pause the XMMS playback.\n"
					"play (position) - Begins playing tracks (from position) "
					"according to the playlist.\n"
					"stop - Stops the currently playing track.\n"
					"next - Plays the next track in the playlist.\n"
					"prev - Plays the previous track in the playlist.\n"
					"shuffle - Toggle shuffling the playlist.\n"
					"repeat - Toggle repeating of the playlist.\n"
					"pos - Set playlist position.\n"
					"info - Get information about currently playing song.");

	tmp = Cvar_Get ("xmms_session", "0", CVAR_NONE, XMMS_SessionChg,
					"XMMS Session number to use");

	sessionNo = tmp->int_val;
}

static void
I_XMMS_Shutdown (void)
{
}

static void
I_XMMS_Kill (void)
{
	xmms_remote_quit (sessionNo);
}

static void
I_XMMS_On (void)
{
	musEnabled = true;
	I_XMMS_Running ();
}

static void
I_XMMS_Off (void)
{
	musEnabled = false;
	I_XMMS_Kill ();
}

static void								// Toggle Shuffling
I_XMMS_Shuffle (void)
{
	int         shuf;

	// for some reason, it reports shuffle wrong,
	// probably because it relies on a timer that doesn't time out straight
	// after change, and before the check

	// SO, we check before, and assuming it works, we know that it will be the
	// opposite of what it _WAS_, if you get my meaning :/

	if (!musEnabled)
		return;
	I_XMMS_Running ();
	shuf = xmms_remote_is_shuffle (sessionNo);
	xmms_remote_toggle_shuffle (sessionNo);
	if (shuf == 1) {
		Sys_Printf ("XMMSAudio: Shuffling Disabled\n");
	} else if (shuf == 0) {
		Sys_Printf ("XMMSAudio: Shuffling Enabled\n");
	}
}

static void								// toggles playlist repeating
I_XMMS_Repeat (void)
{
	// Similar situation as with I_XMMS_Shuffle() -- same code too :)
	int         rep;

	if (!musEnabled)
		return;
	I_XMMS_Running ();
	rep = xmms_remote_is_repeat (sessionNo);
	xmms_remote_toggle_repeat (sessionNo);
	if (rep == 1) {
		Sys_Printf ("XMMSAudio: Repeat Disabled\n");
	} else if (rep == 0) {
		Sys_Printf ("XMMSAudio: Repeat Enabled\n");
	}
}

static void // sets playlist position
I_XMMS_Pos (int track)
{
	if (!musEnabled)
		return;
	if (track < 0)
		return; // -ve track numbers are dumb
	xmms_remote_set_playlist_pos (sessionNo, track);
}

static void // returns info about track playing and list progress
I_XMMS_Info (void) // this is untested with really long tracks, prolly works
{
	int		pos;
	char   *title;
	unsigned int ctime;	// -ve times are dumb, will this help ?
	unsigned int cmin;	// current no of mins
	byte	csecs;		// current no of secs
	unsigned int ttime;	// total track time
	unsigned int tmin;	// total no of mins
	byte	tsecs;		// total no of secs
	unsigned int len;	// playlist length

	if (!musEnabled)
		return;
	I_XMMS_Running ();

	pos = xmms_remote_get_playlist_pos (sessionNo); // get playlist position
	len = xmms_remote_get_playlist_length (sessionNo); // playlist length
	title = xmms_remote_get_playlist_title (sessionNo, pos); // get track title
	ctime = xmms_remote_get_output_time (sessionNo); // get track elapsed time
	ttime = xmms_remote_get_playlist_time (sessionNo, pos);

	// The time returned by xmms_remote_get_output_time is in milliseconds
	// elapsed, so, divide by (60*1000) to get mins (its an int, no decimals)
	// and divide by 1000 mod 60 to get seconds. its a byte, no decimals too.
	cmin = ctime / 60000;
	csecs = ctime / 1000 % 60;

	tmin = ttime / 60000;
	tsecs = ttime / 1000 % 60;

	// tell the user.
	Sys_Printf ("XMMS:    %d/%d        %s        %d:%02d/%d:%02d\n",
				pos, len, title, cmin, csecs, tmin, tsecs);
	free (title);
}

static void
I_XMMS_f (void)
{
	const char *command;

	if (Cmd_Argc () < 2)
		return;
	command = Cmd_Argv (1);

	if (strequal (command, "play")) {
		I_XMMS_Play ((atoi (Cmd_Argv (2)) -1), 0);
		return;
	}

	if (strequal (command, "on")) {
		I_XMMS_On ();
		return;
	}

	if (strequal (command, "off")) {
		I_XMMS_Off ();
		return;
	}

	if (strequal (command, "stop")) {
		I_XMMS_Stop ();
		return;
	}

	if (strequal (command, "pause")) {
		I_XMMS_Pause ();
		return;
	}

	if (strequal (command, "resume")) {
		I_XMMS_Resume ();
		return;
	}

	if (strequal (command, "next")) {
		I_XMMS_Next ();
		return;
	}

	if (strequal (command, "prev")) {
		I_XMMS_Prev ();
		return;
	}

	if (strequal (command, "shuffle")) {
		I_XMMS_Shuffle ();
		return;
	}

	if (strequal (command, "repeat")) {
		I_XMMS_Repeat ();
		return;
	}

	if (strequal (command, "pos")) {
		I_XMMS_Pos (atoi (Cmd_Argv (2)) - 1);
		return;
	}

	if (strequal (command, "info")) {
		I_XMMS_Info ();
		return;
	}

	if (strequal (command, "help")) {
		Sys_Printf ("Try \"help xmms\".\n");
		return;
	}
}

static general_funcs_t plugin_info_general_funcs = {
	I_XMMS_Init,
	I_XMMS_Shutdown,
};

static cd_funcs_t plugin_info_cd_funcs = {
	0,
	I_XMMS_f,
	I_XMMS_Pause,
	I_XMMS_Play,
	I_XMMS_Resume,
	I_XMMS_Update,
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
	"Linux XMMS Audio output\n",
	"Copyright (C) 2001  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors\n",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO (cd, xmms)
{
	return &plugin_info;
}
