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
static const char rcsid[] =
	"$Id$";


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
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include <xmmsctrl.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "compat.h"

static plugin_t plugin_info;
static plugin_data_t plugin_info_data;
static plugin_funcs_t plugin_info_funcs;
static general_data_t plugin_info_general_data;
static general_funcs_t plugin_info_general_funcs;

//static cd_data_t      plugin_info_cd_data;
static cd_funcs_t plugin_info_cd_funcs;

static char *xmms_cmd = "xmms";
static char *xmms_args[] = {"xmms", 0};

// don't need either of these now
// static int xmmsPid = '0';
//static int sigNo = '2';


static qboolean playing = false;

// no idea why i have wasPlaying, prolly cos this code was based on 
// cd_linux.c :/
static qboolean wasPlaying = false;
static qboolean musEnabled = true;

static void I_XMMS_f (void);


/* static float cdvolume; */
/* static byte remap[100]; */
/* static byte playTrack; */
/* static byte maxTrack; */

// FIXME: All of this code assumes that the xmms_remote_* functions succeed
// FIXME: All of this code assumes that we want to alter session 0 :/

static void
I_XMMS_Running (void)
{

	int         res;
	int         i;
	int         fd_size = getdtablesize ();

	if (!xmms_remote_is_running (0)) {

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
				if (execvp (xmms_cmd, xmms_args)) {
					// Oh dear, can we even use Sys_DPrinf ?, we are child so
					// we have access to them ? but wouldn't it just try
					// rendering it 
					// and screw stuff up ? better not find out

					exit (1);			// Well, we can't just hang about
										// causing trouble can we ?
				}

				break;
			case -1:					// ICH !
				// inform user
				Sys_DPrintf ("XMMSAudio: error, can't fork !?\n");
				break;
			default:					// Parent
				// don't need now :/
//				xmmsPid = res; // so we can kill it later 
				break;
		}

		return;

	}

	return;

}


static void
I_XMMS_Stop (void)						// stop playing
{

	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();
	if (!xmms_remote_is_playing (0))
		return;							// check that its actually playing

	xmms_remote_stop (0);				// stop it

	wasPlaying = playing;
	playing = false;
	return;
}

// Play, don't use track and looping atm, :/
static void
I_XMMS_Play (byte track, qboolean looping)
{
	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// Check its on
	/* i think this will fix some wierdness */
	if (xmms_remote_is_paused (0)) {
		xmms_remote_pause (0);
		return;
	}
	if (xmms_remote_is_playing (0))
		return;

	xmms_remote_play (0);

	wasPlaying = playing;
	playing = true;
	return;
}

static void
I_XMMS_Pause (void)
{
	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// It runnin ?
	if (!xmms_remote_is_playing (0))
		return;

	xmms_remote_pause (0);

	wasPlaying = playing;
	playing = false;
	return;

}

static void
I_XMMS_Resume (void)
{

	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// Is it on ? if not, make it so
	/* i think this will fix some wierdness */
	if (xmms_remote_is_paused (0)) {
		xmms_remote_pause (0);
		return;
	}
	if (xmms_remote_is_playing (0))
		return;

	xmms_remote_play (0);

	wasPlaying = playing;
	playing = true;
	return;
}

static void
I_XMMS_Prev (void)
{
	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// Running ?

	xmms_remote_playlist_prev (0);

	return;

}

static void
I_XMMS_Next (void)
{
	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// Running or not ?

	xmms_remote_playlist_next (0);

	return;

}

static void
I_XMMS_Update (void)
{

	return;
}

static void
I_XMMS_Init (void)
{
	I_XMMS_Running ();
	Cmd_AddCommand ("xmms", I_XMMS_f, "Control the XMMS player.\n"
					"Commands:\n"
					"resume - Will resume playback after pause.\n"
					"off - Stops control and playback of XMMS.\n"
					"on - Starts XMMS if not running, or enables playback.\n"
					"pause - Pause the XMMS playback.\n"
					"play - Begins playing tracks according to the playlist.\n"
					"stop - Stops the currently playing track.\n"
					"next - Plays the next track in the playlist.\n"
					"prev - Plays the previous track in the playlist.\n"
					"shuffle - Toggle shuffling the playlist.\n"
					"repeat - Toggle repeating of the playlist.");
	return;

}

static void
I_XMMS_Shutdown (void)
{
	return;
}

static void
I_XMMS_Kill (void)
{
	xmms_remote_quit (0);
	return;
}

static void
I_XMMS_On (void)
{
	musEnabled = true;
	I_XMMS_Running ();
	return;
}

static void
I_XMMS_Off (void)
{
	musEnabled = false;
	I_XMMS_Kill ();
	return;
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

	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// It even running ?
	shuf = xmms_remote_is_shuffle (0);
	xmms_remote_toggle_shuffle (0);
	if (shuf == 1)
		Sys_Printf ("XMMSAudio: Shuffling Disabled\n");
	else if (shuf == 0)
		Sys_Printf ("XMMSAudio: Shuffling Enabled\n");
	else
		return;							// ACH !

	return;

}

static void								// toggles playlist repeating
I_XMMS_Repeat (void)
{
	// Similar situation as with I_XMMS_Shuffle();
	// same code too :)

	int         rep;

	// don't try if "xmms off" has been called
	if (!musEnabled)
		return;
	I_XMMS_Running ();					// It even running ?
	rep = xmms_remote_is_repeat (0);
	xmms_remote_toggle_repeat (0);
	if (rep == 1)
		Sys_Printf ("XMMSAudio: Repeat Disabled\n");
	else if (rep == 0)
		Sys_Printf ("XMMSAudio: Repeat Enabled\n");
	else
		return;							// ACH !

	return;
}

static void
I_XMMS_f (void)
{

	const char *command;

/*	int         ret;
	int         n;  */

	if (Cmd_Argc () < 2)
		return;
	command = Cmd_Argv (1);

	if (strequal (command, "play")) {
		I_XMMS_Play ((byte) atoi (Cmd_Argv (2)), false);
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

	if (strequal (command, "help")) {
		Sys_Printf ("Try \"help xmms\".\n");
		return;
	}
	return;

}

plugin_t   *
cd_xmms_PluginInfo (void)
{
	plugin_info.type = qfp_cd;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "Linux XMMS (CD) Audio output"
		"Copyright (C) 2001  contributors of the QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors\n";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
//  plugin_info_data.cd = &plugin_info_cd_data;
	plugin_info_data.input = NULL;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.cd = &plugin_info_cd_funcs;
	plugin_info_funcs.input = NULL;

	plugin_info_general_funcs.p_Init = I_XMMS_Init;
	plugin_info_general_funcs.p_Shutdown = I_XMMS_Shutdown;

	plugin_info_cd_funcs.pCDAudio_Pause = I_XMMS_Pause;
	plugin_info_cd_funcs.pCDAudio_Play = I_XMMS_Play;
	plugin_info_cd_funcs.pCDAudio_Resume = I_XMMS_Resume;
	plugin_info_cd_funcs.pCDAudio_Update = I_XMMS_Update;
	plugin_info_cd_funcs.pCD_f = I_XMMS_f;

	return &plugin_info;
}
