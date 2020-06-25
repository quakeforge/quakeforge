/*
	cl_demo.c

	demo playback support

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
#include <time.h>
#include <ctype.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "nq/include/client.h"
#include "nq/include/host.h"

typedef struct {
	int         frames;
	double      time;
	double      fps;
} td_stats_t;

static int         demo_timeframes_isactive;
static int         demo_timeframes_index;
static dstring_t  *demoname;
static double     *demo_timeframes_array;
#define CL_TIMEFRAMES_ARRAYBLOCK 4096

static int         timedemo_count;
static int         timedemo_runs;
static td_stats_t *timedemo_data;

static void CL_FinishTimeDemo (void);
static void CL_TimeFrames_DumpLog (void);
static void CL_TimeFrames_AddTimestamp (void);
static void CL_TimeFrames_Reset (void);

cvar_t     *demo_gzip;
cvar_t     *demo_speed;
cvar_t     *demo_quit;
cvar_t     *demo_timeframes;

#define MAX_DEMMSG (MAX_MSGLEN)

/*
	DEMO CODE

	When a demo is playing back, all NET_SendMessages are skipped, and
	NET_GetMessages are read from the demo file.

	Whenever cl.time gets past the last received message, another message is
	read from the demo file.
*/


/*
	CL_WriteDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/
static void
CL_WriteDemoMessage (sizebuf_t *msg)
{
	float       f;
	int         len;
	int         i;

	len = LittleLong (msg->cursize);
	Qwrite (cls.demofile, &len, 4);
	for (i = 0; i < 3; i++) {
		f = LittleFloat (cl.viewangles[i]);
		Qwrite (cls.demofile, &f, 4);
	}
	Qwrite (cls.demofile, msg->data, msg->cursize);

	Qflush (cls.demofile);
}

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
	cls.demofile = NULL;
	CL_SetState (ca_disconnected);
	cls.demo_capture = 0;
	cls.demoplayback = 0;

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

void
CL_StopRecording (void)
{
	// write a disconnect message to the demo file
	SZ_Clear (net_message->message);
	MSG_WriteByte (net_message->message, svc_disconnect);
	CL_WriteDemoMessage (net_message->message);

	// finish up
	Qclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Sys_Printf ("Completed demo\n");
}

// decide if it is time to grab the next message
static int
check_next_demopacket (void)
{
	if (cls.state == ca_active) {	// always grab until fully connected
		if (cls.timedemo) {
			if (host_framecount == cls.td_lastframe)
				return 0;			// already read this frame's message
			cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
			if (host_framecount == cls.td_startframe + 1)
				cls.td_starttime = Sys_DoubleTime ();
		} else if (cl.time <= cl.mtime[0]) {
			return 0;				// don't need another message yet
		}
	}
	return 1;
}

// get the next message
static int
read_demopacket (void)
{
	int         i, r;
	float       f;

	Qread (cls.demofile, &net_message->message->cursize, 4);
	net_message->message->cursize =
		LittleLong (net_message->message->cursize);
	if (net_message->message->cursize > MAX_DEMMSG)
		Host_Error ("Demo message > MAX_DEMMSG: %d/%d",
					net_message->message->cursize, MAX_DEMMSG);
	VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
	for (i = 0; i < 3; i++) {
		r = Qread (cls.demofile, &f, 4);
		if (r != 4) {
			CL_StopPlayback ();
			return 0;
		}
		cl.mviewangles[0][i] = LittleFloat (f);
	}
	r = Qread (cls.demofile, net_message->message->data,
			   net_message->message->cursize);
	if (r != net_message->message->cursize) {
		CL_StopPlayback ();
		return 0;
	}
	return 1;
}

static int
CL_GetDemoMessage (void)
{
	if (!check_next_demopacket ())
		return 0;
	return read_demopacket ();
}

static int
CL_GetPacket (void)
{
	int         r;

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
	return r;
}

/*
	CL_GetMessage

	Handles recording and playback of demos, on top of NET_ code
*/
int
CL_GetMessage (void)
{
	if (cls.demoplayback) {
		int         ret = CL_GetDemoMessage ();

		if (!ret && demo_timeframes_isactive && cls.td_starttime) {
			CL_TimeFrames_AddTimestamp ();
		}
		return ret;
	}

	if (!CL_GetPacket ())
		return 0;

	if (cls.demorecording)
		CL_WriteDemoMessage (net_message->message);

	return 1;
}

/*
	CL_Stop_f

	stop recording a demo
*/
static void
CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording) {
		Sys_Printf ("Not recording a demo.\n");
		return;
	}
	CL_StopRecording ();
}

/*
	CL_Record_f

	record <demoname> <map> [cd track]
*/
static void
CL_Record_f (void)
{
	int         c;
	int         track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc ();
	if (c > 4) {
		Sys_Printf ("record [<demoname> [<map> [cd track]]]\n");
		return;
	}

	if (c >= 2 && strstr (Cmd_Argv (1), "..")) {
		Sys_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state >= ca_connected) {
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

// start up the map
	if (c > 2)
		Cmd_ExecuteString (va ("map %s", Cmd_Argv (2)), src_command);

	CL_Record (Cmd_Argv (1), track);
}

static dstring_t *
demo_default_name (const char *argv1)
{
	dstring_t  *name;
	const char *mapname;
	int         mapname_len;
	char        timestring[20];
	time_t      tim;

	name = dstring_new ();

	if (argv1) {
		dsprintf (name, "%s/%s", qfs_gamedir->dir.def, argv1);
		return name;
	}

	// Get time to a useable format
	time (&tim);
	strftime (timestring, 19, "%Y-%m-%d-%H-%M", localtime (&tim));

	// the leading path-name is to be removed from cl.worldmodel->name
	mapname = QFS_SkipPath (cl.worldmodel->name);

	// the map name is cut off after any "." because this would prevent
	// an extension being appended
	for (mapname_len = 0; mapname[mapname_len]; mapname_len++)
		if (mapname[mapname_len] == '.')
			break;

	dsprintf (name, "%s/%s-%.*s", qfs_gamedir->dir.def, timestring,
			  mapname_len, mapname);
	return name;
}

static void
demo_start_recording (int track)
{
	cls.forcetrack = track;
	Qprintf (cls.demofile, "%i\n", cls.forcetrack);
}

void
CL_Record (const char *argv1, int track)
{
	dstring_t  *name;

	name = demo_default_name (argv1);

	// open the demo file
#ifdef HAVE_ZLIB
	if (demo_gzip->int_val) {
		QFS_DefaultExtension (name, ".dem.gz");
		cls.demofile = QFS_WOpen (name->str, demo_gzip->int_val);
	} else
#endif
	{
		QFS_DefaultExtension (name, ".dem");
		cls.demofile = QFS_WOpen (name->str, 0);
	}

	if (!cls.demofile) {
		Sys_Printf ("ERROR: couldn't open.\n");
		dstring_delete (name);
		return;
	}

	Sys_Printf ("recording to %s.\n", name->str);
	dstring_delete (name);
	cls.demorecording = true;

	demo_start_recording (track);
}

static int
demo_check_dem (void)
{
	int         c, ret = 0;
	uint32_t    len, bytes;

	// .dem demo files begin with an ascii integer (possibly negative)
	// representing the forced bgm track, followed by a newline
	c = Qgetc (cls.demofile);
	while (isspace (c))	// hipnotic demos have leading whitespace :P
		c = Qgetc (cls.demofile);
	if (c == '-')
		c = Qgetc (cls.demofile);
	while (isdigit (c))
		c = Qgetc (cls.demofile);
	if (c != '\n')
		goto done;

	// After the bgm track comes the packet length plus 3 floats for view
	// angles (not included in the packet length)
	Qread (cls.demofile, &len, sizeof (len));
	len = LittleLong (len);
	if (len > MAX_DEMMSG)
		goto done;
	Qseek (cls.demofile, 3 * 4, SEEK_CUR);	// 3 * float (angles)

	// Normally, svc_serverinfo is the first command in the packet, but some
	// servers (eg, fitzquake) add in an svc_print command first. Allow
	// multiple svc_print commands (but nothing else) before the svc_serverinfo
	net_message->message->cursize = len;
	bytes = Qread (cls.demofile, net_message->message->data, len);
	if (bytes != len)
		goto done;
	MSG_BeginReading (net_message);
	while (!ret) {
		if (net_message->badread)
			goto done;
		c = MSG_ReadByte (net_message);
		switch (c) {
			case svc_print:
				MSG_ReadString (net_message);
				break;
			case svc_serverinfo:
				ret = 1;
				break;
			default:
				goto done;
		}
	}
done:
	Qseek (cls.demofile, 0, SEEK_SET);
	return ret;
}

static void
CL_StartDemo (void)
{
	dstring_t  *name;
	int         c;
	qboolean    neg = false;

	// disconnect from server
	CL_Disconnect ();

	// open the demo file
	name = dstring_strdup (demoname->str);
	QFS_DefaultExtension (name, ".dem");

	Sys_Printf ("Playing demo from %s.\n", name->str);
	cls.demofile = QFS_FOpenFile (name->str);
	if (!cls.demofile) {
		Sys_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;				// stop demo loop
		dstring_delete (name);
		return;
	}
	if (!demo_check_dem ()) {
		Sys_Printf ("%s is not a valid .dem file.\n", name->str);
		cls.demonum = -1;				// stop demo loop
		dstring_delete (name);
		Qclose (cls.demofile);
		cls.demofile = 0;
		return;
	}

	cls.demoplayback = true;
	CL_SetState (ca_connected);
	cls.forcetrack = 0;
	Key_SetKeyDest (key_demo);

	while ((c = Qgetc (cls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;
	dstring_delete (name);
}

/*
	CL_PlayDemo_f

	play [demoname]
*/
static void
CL_PlayDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	switch (Cmd_Argc ()) {
		case 1:
			if (!demoname->str[0])
				goto playdemo_error;
			// fall through
		case 2:
			cls.demo_capture = 0;
			break;
		case 3:
			if (!strcmp (Cmd_Argv (2), "rec")) {
				cls.demo_capture = 1;
				break;
			}
			// fall through
		default:
playdemo_error:
			Sys_Printf ("play <demoname> : plays a demo\n");
			return;
	}
	timedemo_runs = timedemo_count = 1;	// make sure looped timedemos stop

	if (Cmd_Argc () > 1)
		dstring_copystr (demoname, Cmd_Argv (1));
	CL_StartDemo ();
}

static void
CL_StartTimeDemo (void)
{
	CL_StartDemo ();

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_starttime = 0;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;				// get a new message this frame

	CL_TimeFrames_Reset ();
	if (demo_timeframes->int_val)
		demo_timeframes_isactive = 1;
}

static inline double
sqr (double x)
{
	return x * x;
}

static void
CL_FinishTimeDemo (void)
{
	int         frames;
	float       time;

	cls.timedemo = false;

	// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = Sys_DoubleTime () - cls.td_starttime;
	if (!time)
		time = 1;
	Sys_Printf ("%i frame%s %.4g seconds %.4g fps\n", frames,
				frames == 1 ? "" : "s", time, frames / time);

	CL_TimeFrames_DumpLog ();
	demo_timeframes_isactive = 0;

	timedemo_count--;
	if (timedemo_data) {
		timedemo_data[timedemo_count].frames = frames;
		timedemo_data[timedemo_count].time = time;
		timedemo_data[timedemo_count].fps = frames / time;
	}
	if (timedemo_count > 0) {
		CL_StartTimeDemo ();
	} else {
		if (--timedemo_runs > 0 && timedemo_data) {
			double      average = 0;
			double      variance = 0;
			double      min, max;
			int         i;

			min = max = timedemo_data[0].fps;
			for (i = 0; i < timedemo_runs; i++) {
				average += timedemo_data[i].fps;
				min = min (min, timedemo_data[i].fps);
				max = max (max, timedemo_data[i].fps);
			}
			average /= timedemo_runs;
			for (i = 0; i < timedemo_runs; i++)
				variance += sqr (timedemo_data[i].fps - average);
			variance /= timedemo_runs;
			Sys_Printf ("timedemo stats for %d runs:\n", timedemo_runs);
			Sys_Printf ("  average fps: %.3f\n", average);
			Sys_Printf ("  min/max fps: %.3f/%.3f\n", min, max);
			Sys_Printf ("std deviation: %.3f fps\n", sqrt (variance));
		}
		if (demo_quit->int_val)
			Cbuf_InsertText (host_cbuf, "quit\n");
	}
}

/*
	CL_TimeDemo_f

	timedemo [demoname]
*/
static void
CL_TimeDemo_f (void)
{
	int         count = 1;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 3) {
		Sys_Printf ("timedemo <demoname> [count]: gets demo speeds\n");
		return;
	}
	timedemo_runs = timedemo_count = 1;	// make sure looped timedemos stop

	if (Cmd_Argc () == 3)
		count = atoi (Cmd_Argv (2));
	if (timedemo_data) {
		free (timedemo_data);
		timedemo_data = 0;
	}
	timedemo_data = calloc (timedemo_runs, sizeof (td_stats_t));
	dstring_copystr (demoname, Cmd_Argv (1));
	CL_StartTimeDemo ();
	timedemo_runs = timedemo_count = max (count, 1);
	timedemo_data = calloc (timedemo_runs, sizeof (td_stats_t));
}

void
CL_Demo_Init (void)
{
	demoname = dstring_newstr ();
	demo_timeframes_isactive = 0;
	demo_timeframes_index = 0;
	demo_timeframes_array = NULL;

	demo_gzip = Cvar_Get ("demo_gzip", "0", CVAR_ARCHIVE, NULL,
						  "Compress demos using gzip. 0 = none, 1 = least "
						  "compression, 9 = most compression. Compressed "
						  " demos (1-9) will have .gz appended to the name");
	demo_speed = Cvar_Get ("demo_speed", "1.0", CVAR_NONE, NULL,
						   "adjust demo playback speed. 1.0 = normal, "
						   "< 1 slow-mo, > 1 timelapse");
	demo_quit = Cvar_Get ("demo_quit", "0", CVAR_NONE, NULL,
						  "automaticly quit after a timedemo has finished");
	demo_timeframes = Cvar_Get ("demo_timeframes", "0", CVAR_NONE, NULL,
								"write timestamps for every frame");
	Cmd_AddCommand ("record", CL_Record_f, "Record a demo, if no filename "
					"argument is given\n"
					"the demo will be called Year-Month-Day-Hour-Minute-"
					"Mapname");
	Cmd_AddCommand ("stop", CL_Stop_f, "Stop recording a demo");
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "Play a recorded demo");
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f, "Play a demo as fast as your "
					"hardware can. Useful for benchmarking.");
}

static void
CL_TimeFrames_Reset (void)
{
	demo_timeframes_index = 0;
	free (demo_timeframes_array);
	demo_timeframes_array = NULL;
}

static void
CL_TimeFrames_AddTimestamp (void)
{
	if (!(demo_timeframes_index % CL_TIMEFRAMES_ARRAYBLOCK))
		demo_timeframes_array = realloc
			(demo_timeframes_array, sizeof (demo_timeframes_array[0]) *
			 ((demo_timeframes_index / CL_TIMEFRAMES_ARRAYBLOCK) + 1) *
			 CL_TIMEFRAMES_ARRAYBLOCK);
	if (demo_timeframes_array == NULL)
		Sys_Error ("Unable to allocate timeframes buffer");
	demo_timeframes_array[demo_timeframes_index] = Sys_DoubleTime ();
	demo_timeframes_index++;
}

static void
CL_TimeFrames_DumpLog (void)
{
	const char *filename = "timeframes.txt";
	int         i;
	long        frame;
	QFile      *outputfile;

	if (demo_timeframes_isactive == 0)
		return;

	Sys_Printf ("Dumping Timed Frames log: %s\n", filename);
	outputfile = QFS_Open (filename, "w");
	if (!outputfile) {
		Sys_Printf ("Could not open: %s\n", filename);
		return;
	}
	for (i = 1; i < demo_timeframes_index; i++) {
		frame = (demo_timeframes_array[i] - demo_timeframes_array[i - 1]) * 1e6;
		Qprintf (outputfile, "%09ld\n", frame);
	}
	Qclose (outputfile);
}
