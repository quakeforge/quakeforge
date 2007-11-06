/*
	cl_demo.c

	@description@

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "client.h"
#include "compat.h"
#include "host.h"

char        demoname[1024];
int         timedemo_count;

static void        CL_FinishTimeDemo (void);

cvar_t     *demo_gzip;
cvar_t     *demo_speed;

/*
DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
*/


/*
	CL_StopPlayback

	Called when a demo file runs out, or the user starts a game
*/
void
CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	Qclose (cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;
	CL_SetState (ca_disconnected);

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}


/*
	CL_WriteDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/
static void
CL_WriteDemoMessage (void)
{
	int         len;
	int         i;
	float       f;

	len = LittleLong (net_message->message->cursize);
	Qwrite (cls.demofile, &len, 4);
	for (i = 0; i < 3; i++) {
		f = LittleFloat (cl.viewangles[i]);
		Qwrite (cls.demofile, &f, 4);
	}
	Qwrite (cls.demofile, net_message->message->data,
			net_message->message->cursize);
	Qflush (cls.demofile);
}


/*
	CL_GetMessage

	Handles recording and playback of demos, on top of NET_ code
*/
int
CL_GetMessage (void)
{
	int         r, i;
	float       f;

	if (cls.demoplayback) {
		// decide if it is time to grab the next message        
		if (cls.signon == SIGNONS)		// always grab until fully connected
		{
			if (cls.timedemo) {
				if (host_framecount == cls.td_lastframe)
					return 0;			// already read this frame's message
				cls.td_lastframe = host_framecount;
				// if this is the second frame, grab the real td_starttime
				// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			} else if ( /* cl.time > 0 && */ cl.time <= cl.mtime[0]) {
				return 0;				// don't need another message yet
			}
		}
		// get the next message
		Qread (cls.demofile, &net_message->message->cursize, 4);
		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		for (i = 0; i < 3; i++) {
			r = Qread (cls.demofile, &f, 4);
			cl.mviewangles[0][i] = LittleFloat (f);
		}

		net_message->message->cursize =
			LittleLong (net_message->message->cursize);
		if (net_message->message->cursize > MAX_MSGLEN)
			Sys_Error ("Demo message > MAX_MSGLEN");
		r =
			Qread (cls.demofile, net_message->message->data,
				   net_message->message->cursize);
		if (r != net_message->message->cursize) {
			CL_StopPlayback ();
			return 0;
		}

		return 1;
	}

	while (1) {
		r = NET_GetMessage (cls.netcon);

		if (r != 1 && r != 2)
			return r;

		// discard nop keepalive message
		if (net_message->message->cursize == 1
			&& net_message->message->data[0] == svc_nop)
			Sys_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();

	return r;
}


/*
	CL_Stop_f

	stop recording a demo
*/
void
CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording) {
		Sys_Printf ("Not recording a demo.\n");
		return;
	}
// write a disconnect message to the demo file
	SZ_Clear (net_message->message);
	MSG_WriteByte (net_message->message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
	Qclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Sys_Printf ("Completed demo\n");
}


/*
	CL_Record_f

	record <demoname> <map> [cd track]
*/
void
CL_Record_f (void)
{
	int         c;
	char        name[MAX_OSPATH];
	int         track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc ();
	if (c != 2 && c != 3 && c != 4) {
		Sys_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr (Cmd_Argv (1), "..")) {
		Sys_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected) {
		Sys_Printf
			("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
	}
// write the forced cd track number, or -1
	if (c == 4) {
		track = atoi (Cmd_Argv (3));
		Sys_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	} else
		track = -1;

	snprintf (name, sizeof (name), "%s/%s",
			  qfs_gamedir->dir.def, Cmd_Argv (1));

// start the map up
//
	if (c > 2)
		Cmd_ExecuteString (va ("map %s", Cmd_Argv (2)), src_command);

// open the demo file
//
#ifdef HAVE_ZLIB
	if (demo_gzip->int_val) {
		QFS_DefaultExtension (name, ".dem.gz");
		cls.demofile = QFS_WOpen (name, demo_gzip->int_val);
	} else
#endif
	{
		QFS_DefaultExtension (name, ".dem");
		cls.demofile = QFS_WOpen (name, 0);
	}

	if (!cls.demofile) {
		Sys_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Sys_Printf ("recording to %s.\n", name);
	cls.demorecording = true;

	cls.forcetrack = track;
	Qprintf (cls.demofile, "%i\n", cls.forcetrack);

}


static void
CL_StartDemo (void)
{
	char        name[256];
	int         c;
	qboolean    neg = false;

// disconnect from server
//
	CL_Disconnect ();

// open the demo file
//
	strncpy (name, demoname, sizeof (name));
	QFS_DefaultExtension (name, ".dem");

	Sys_Printf ("Playing demo from %s.\n", name);
	QFS_FOpenFile (name, &cls.demofile);
	if (!cls.demofile) {
		Sys_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;				// stop demo loop
		return;
	}

	cls.demoplayback = true;
	CL_SetState (ca_connected);
	cls.forcetrack = 0;
	key_dest = key_game;
	game_target = IMT_0;

	while ((c = Qgetc (cls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;
// ZOID, fscanf is evil
//  fscanf (cls.demofile, "%i\n", &cls.forcetrack);
}
/*
	CL_PlayDemo_f

	play [demoname]
*/
void
CL_PlayDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("play <demoname> : plays a demo\n");
		return;
	}
	strncpy (demoname, Cmd_Argv (1), sizeof (demoname));
	CL_StartDemo ();
}

static void
CL_StartTimeDemo (void)
{
	CL_StartDemo ();

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;				// get a new message this frame
}

static void
CL_FinishTimeDemo (void)
{
	int         frames;
	float       time;

	cls.timedemo = false;

	// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Sys_Printf ("%i frame%s %.4g seconds %.4g fps\n", frames,
				frames == 1 ? "" : "s", time, frames / time);
	if (--timedemo_count > 0)
		CL_StartTimeDemo ();
}


/*
	CL_TimeDemo_f

	timedemo [demoname]
*/
void
CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 3) {
		Sys_Printf ("timedemo <demoname> [count]: gets demo speeds\n");
		return;
	}
	if (Cmd_Argc () == 3) {
		timedemo_count = atoi (Cmd_Argv (2));
	} else {
		timedemo_count = 1;
	}
	strncpy (demoname, Cmd_Argv (1), sizeof (demoname));
	CL_StartTimeDemo ();
}

void
CL_Demo_Init (void)
{
	demo_gzip = Cvar_Get ("demo_gzip", "0", CVAR_ARCHIVE, NULL,
						  "Compress demos using gzip. 0 = none, 1 = least "
						  "compression, 9 = most compression. Compressed "
						  " demos (1-9) will have .gz appended to the name");
	demo_speed = Cvar_Get ("demo_speed", "1.0", CVAR_NONE, NULL,
						   "adjust demo playback speed. 1.0 = normal, "
						   "< 1 slow-mo, > 1 timelapse");
}
