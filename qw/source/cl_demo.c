/*
	cl_demo.c

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

#include <sys/time.h>
#include <time.h>
	
#include "QF/console.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "cl_main.h"
#include "client.h"
#include "compat.h"
#include "host.h"
#include "pmove.h"

typedef struct {
	int         frames;
	double      time;
	double      fps;
} td_stats_t;

int     cl_timeframes_isactive;
int     cl_timeframes_index;
int     demotime_cached;
char    demoname[1024];
double *cl_timeframes_array;
#define CL_TIMEFRAMES_ARRAYBLOCK 4096

int     timedemo_count;
int     timedemo_runs;
td_stats_t *timedemo_data;


void CL_FinishTimeDemo (void);
void CL_TimeFrames_Reset (void);
void CL_TimeFrames_DumpLog (void);

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
	cls.demofile = NULL;
	CL_SetState (ca_disconnected);
	cls.demoplayback = 0;
	demotime_cached = 0;

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

#define dem_cmd		0
#define dem_read	1
#define dem_set		2

/*
	CL_WriteDemoCmd

	Writes the current user cmd
*/
void
CL_WriteDemoCmd (usercmd_t *pcmd)
{
	byte		c;
	float		fl;
	int			i;
	usercmd_t	cmd;

//	Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	fl = LittleFloat ((float) realtime);
	Qwrite (cls.demofile, &fl, sizeof (fl));

	c = dem_cmd;
	Qwrite (cls.demofile, &c, sizeof (c));

	// correct for byte order, bytes don't matter
	cmd = *pcmd;

	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleFloat (cmd.angles[i]);
	cmd.forwardmove = LittleShort (cmd.forwardmove);
	cmd.sidemove = LittleShort (cmd.sidemove);
	cmd.upmove = LittleShort (cmd.upmove);

	Qwrite (cls.demofile, &cmd, sizeof (cmd));

	for (i = 0; i < 3; i++) {
		fl = LittleFloat (cl.viewangles[i]);
		Qwrite (cls.demofile, &fl, 4);
	}

	Qflush (cls.demofile);
}

/*
	CL_WriteDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/
void
CL_WriteDemoMessage (sizebuf_t *msg)
{
	byte		c;
	float		fl;
	int			len;

//	Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat ((float) realtime);
	Qwrite (cls.demofile, &fl, sizeof (fl));

	c = dem_read;
	Qwrite (cls.demofile, &c, sizeof (c));

	len = LittleLong (msg->cursize);
	Qwrite (cls.demofile, &len, 4);
	Qwrite (cls.demofile, msg->data, msg->cursize);

	Qflush (cls.demofile);
}

qboolean
CL_GetDemoMessage (void)
{
	byte		c;
	float		demotime, f;
	static float cached_demotime;
	int			r, i, j;
	usercmd_t  *pcmd;

	// read the time from the packet
	if (demotime_cached) {
		demotime = cached_demotime;
		demotime_cached = 0;
	} else {
		Qread (cls.demofile, &demotime, sizeof (demotime));
		demotime = LittleFloat (demotime);
	}

	// decide if it is time to grab the next message        
	if (cls.timedemo) {
		if (cls.td_lastframe < 0)
			cls.td_lastframe = demotime;
		else if (demotime > cls.td_lastframe) {
			cls.td_lastframe = demotime;
			// rewind back to time
			demotime_cached = 1;
			cached_demotime = demotime;
			return 0;					// already read this frame's message
		}
		if (!cls.td_starttime && cls.state == ca_active) {
			cls.td_starttime = Sys_DoubleTime ();
			cls.td_startframe = host_framecount;
		}
		realtime = demotime;			// warp
	} else if (!cl.paused && cls.state >= ca_onserver) {
		// always grab until fully connected
		if (realtime + 1.0 < demotime) {
			// too far back
			realtime = demotime - 1.0;
			// rewind back to time
			demotime_cached = 1;
			cached_demotime = demotime;
			return 0;
		} else if (realtime < demotime) {
			// rewind back to time
			demotime_cached = 1;
			cached_demotime = demotime;
			return 0;					// don't need another message yet
		}
	} else
		realtime = demotime;			// we're warping

	if (cls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: cls.state != ca_active");

	// get the msg type
	Qread (cls.demofile, &c, sizeof (c));

	switch (c) {
		case dem_cmd:
			// user sent input
			net_message->message->cursize = -1;
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			pcmd = &cl.frames[i].cmd;
			r = Qread (cls.demofile, pcmd, sizeof (*pcmd));
			if (r != sizeof (*pcmd)) {
				CL_StopPlayback ();
				return 0;
			}
			// byte order stuff
			for (j = 0; j < 3; j++)
				pcmd->angles[j] = LittleFloat (pcmd->angles[j]);
			pcmd->forwardmove = LittleShort (pcmd->forwardmove);
			pcmd->sidemove = LittleShort (pcmd->sidemove);
			pcmd->upmove = LittleShort (pcmd->upmove);
			cl.frames[i].senttime = demotime;
			cl.frames[i].receivedtime = -1;	// we haven't gotten a reply yet
			cls.netchan.outgoing_sequence++;
			for (i = 0; i < 3; i++) {
				Qread (cls.demofile, &f, 4);
				cl.viewangles[i] = LittleFloat (f);
			}
			break;

		case dem_read:
			// get the next message
			Qread (cls.demofile, &net_message->message->cursize, 4);
			net_message->message->cursize = LittleLong
				(net_message->message->cursize);
//			Con_Printf("read: %ld bytes\n", net_message->message->cursize);
			if (net_message->message->cursize > MAX_MSGLEN + 8) //+8 for header
				Host_Error ("Demo message > MAX_MSGLEN + 8: %d/%d",
							net_message->message->cursize, MAX_MSGLEN + 8);
			r = Qread (cls.demofile, net_message->message->data,
					   net_message->message->cursize);
			if (r != net_message->message->cursize) {
				CL_StopPlayback ();
				return 0;
			}
			break;

		case dem_set:
			Qread (cls.demofile, &i, 4);
			cls.netchan.outgoing_sequence = LittleLong (i);
			Qread (cls.demofile, &i, 4);
			cls.netchan.incoming_sequence = LittleLong (i);
			break;

		default:
			Con_Printf ("Corrupted demo.\n");
			CL_StopPlayback ();
			return 0;
	}

	return 1;
}

/*
	CL_GetMessage

	Handles recording and playback of demos, on top of NET_ code
*/
qboolean
CL_GetMessage (void)
{
	if (cls.demoplayback)
		return CL_GetDemoMessage ();

	if (!NET_GetPacket ())
		return false;

	CL_WriteDemoMessage (net_message->message);

	return true;
}

/*
	CL_Stop_f

	stop recording a demo
*/
void
CL_Stop_f (void)
{
	if (!cls.demorecording) {
		Con_Printf ("Not recording a demo.\n");
		return;
	}
	// write a disconnect message to the demo file
	SZ_Clear (net_message->message);
	MSG_WriteLong (net_message->message, -1);  // -1 sequence means out of band
	MSG_WriteByte (net_message->message, svc_disconnect);
	MSG_WriteString (net_message->message, "EndOfDemo");
	CL_WriteDemoMessage (net_message->message);

	// finish up
	Qclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}

/*
	CL_WriteDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/
void
CL_WriteRecordDemoMessage (sizebuf_t *msg, int seq)
{
	byte		c;
	float		fl;
	int			len, i;

//	Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat ((float) realtime);
	Qwrite (cls.demofile, &fl, sizeof (fl));

	c = dem_read;
	Qwrite (cls.demofile, &c, sizeof (c));

	len = LittleLong (msg->cursize + 8);
	Qwrite (cls.demofile, &len, 4);

	i = LittleLong (seq);
	Qwrite (cls.demofile, &i, 4);
	Qwrite (cls.demofile, &i, 4);

	Qwrite (cls.demofile, msg->data, msg->cursize);

	Qflush (cls.demofile);
}

void
CL_WriteSetDemoMessage (void)
{
	byte		c;
	float		fl;
	int			len;

//	Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat ((float) realtime);
	Qwrite (cls.demofile, &fl, sizeof (fl));

	c = dem_set;
	Qwrite (cls.demofile, &c, sizeof (c));

	len = LittleLong (cls.netchan.outgoing_sequence);
	Qwrite (cls.demofile, &len, 4);
	len = LittleLong (cls.netchan.incoming_sequence);
	Qwrite (cls.demofile, &len, 4);

	Qflush (cls.demofile);
}

/*
	CL_Record_f

	record <demoname> <server>
*/

void
CL_Record (const char *argv1)
{
	char        buf_data[MAX_MSGLEN + 10];	// + 10 for header
	char        name[MAX_OSPATH];
	char       *s;
	char        timestring[20];
	char        mapname[MAX_OSPATH];

	int         n, i, j, k, l=0;
	int         seq = 1;
	entity_t   *ent;
	entity_state_t *es, blankes;
	player_info_t *player;
	sizebuf_t   buf;
	time_t      tim;

	if (!argv1) {
		// Get time to a useable format
		time (&tim);
		strftime (timestring, 19, "%Y-%m-%d-%H-%M", localtime (&tim));

		// the leading path-name is to be removed from cl.worldmodel->name
		for (k = 0; k <= strlen (cl.worldmodel->name); k++) {
			if (cl.worldmodel->name[k] == '/') {
				l = 0;
				mapname[l] = '\0';
				continue;
			}
			mapname[l] = cl.worldmodel->name[k];
			l++;
		}

		// the map name is cut off after any "." because this would prevent
		// ".qwd" from being appended

		for (k = 0; k <= strlen (mapname); k++)
			if (mapname[k] == '.')
				mapname[k] = '\0';

		snprintf (name, sizeof (name), "%s/%s-%s", com_gamedir,
				  timestring, mapname);
	} else {
		snprintf (name, sizeof (name), "%s/%s", com_gamedir, argv1);
	}

	// open the demo file
	COM_DefaultExtension (name, ".qwd");

	cls.demofile = Qopen (name, "wb");
	if (!cls.demofile) {
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Con_Printf ("recording to %s.\n", name);
	cls.demorecording = true;

/*-------------------------------------------------*/

	// serverdata
	// send the info about the new client to all connected clients
	memset (&buf, 0, sizeof (buf));
	buf.data = buf_data;
	buf.maxsize = sizeof (buf_data);

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);
	MSG_WriteString (&buf, gamedirfile);

	if (cl.spectator)
		MSG_WriteByte (&buf, cl.playernum | 128);
	else
		MSG_WriteByte (&buf, cl.playernum);

	// send full levelname
	MSG_WriteString (&buf, cl.levelname);

	// send the movevars
	MSG_WriteFloat (&buf, movevars.gravity);
	MSG_WriteFloat (&buf, movevars.stopspeed);
	MSG_WriteFloat (&buf, movevars.maxspeed);
	MSG_WriteFloat (&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat (&buf, movevars.accelerate);
	MSG_WriteFloat (&buf, movevars.airaccelerate);
	MSG_WriteFloat (&buf, movevars.wateraccelerate);
	MSG_WriteFloat (&buf, movevars.friction);
	MSG_WriteFloat (&buf, movevars.waterfriction);
	MSG_WriteFloat (&buf, movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0);			// none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("fullserverinfo \"%s\"\n",
							   Info_MakeString (cl.serverinfo, 0)));

	// flush packet
	CL_WriteRecordDemoMessage (&buf, seq++);
	SZ_Clear (&buf);

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}
	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}
	// spawnstatic
	for (i = 0; i < cl.num_statics; i++) {
		ent = cl_static_entities + i;

		MSG_WriteByte (&buf, svc_spawnstatic);

		for (j = 1; j < MAX_MODELS; j++)
			if (ent->model == cl.model_precache[j])
				break;
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);
		MSG_WriteCoordAngleV (&buf, ent->origin, ent->angles);

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// spawnstaticsound
	// static sounds are skipped in demos, life is hard

	// baselines
	memset (&blankes, 0, sizeof (blankes));
	for (i = 0; i < MAX_EDICTS; i++) {
		es = cl_baselines + i;

		if (memcmp (es, &blankes, sizeof (blankes))) {
			MSG_WriteByte (&buf, svc_spawnbaseline);
			MSG_WriteShort (&buf, i);

			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);
			MSG_WriteCoordAngleV (&buf, es->origin, es->angles);

			if (buf.cursize > MAX_MSGLEN / 2) {
				CL_WriteRecordDemoMessage (&buf, seq++);
				SZ_Clear (&buf);
			}
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("cmd spawn %i 0\n", cl.servercount));

	if (buf.cursize) {
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}
	// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++) {
		player = cl.players + i;
		if (!player->userinfo)
			continue;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, realtime - player->entertime);

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, Info_MakeString (player->userinfo, 0));

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char) i);
		MSG_WriteString (&buf, r_lightstyle[i].map);
	}

	for (i = 0; i < MAX_CL_STATS; i++) {
		MSG_WriteByte (&buf, svc_updatestatlong);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, cl.stats[i]);
		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

#if 0
	MSG_WriteByte (&buf, svc_updatestatlong);
	MSG_WriteByte (&buf, STAT_TOTALMONSTERS);
	MSG_WriteLong (&buf, cl.stats[STAT_TOTALMONSTERS]);

	MSG_WriteByte (&buf, svc_updatestatlong);
	MSG_WriteByte (&buf, STAT_SECRETS);
	MSG_WriteLong (&buf, cl.stats[STAT_SECRETS]);

	MSG_WriteByte (&buf, svc_updatestatlong);
	MSG_WriteByte (&buf, STAT_MONSTERS);
	MSG_WriteLong (&buf, cl.stats[STAT_MONSTERS]);
#endif

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("skins\n"));

	CL_WriteRecordDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage ();

	// done
}

void
CL_Record_f (void)
{
	if (Cmd_Argc () > 2) {
		/* we use a demo name like year-month-day-hours-minutes-mapname.qwd
		   if there is no argument */
		Con_Printf ("record [demoname]\n");
		return;
	}

	if (cls.state != ca_active) {
		Con_Printf ("You must be connected to record.\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f ();
	if (Cmd_Argc () == 2)
		CL_Record (Cmd_Argv (1));
	else
		CL_Record (0);
}

/*
	CL_ReRecord_f

	record <demoname>
*/
void
CL_ReRecord_f (void)
{
	char		name[MAX_OSPATH];
	int			c;

	c = Cmd_Argc ();
	if (c != 2) {
		Con_Printf ("rerecord <demoname>\n");
		return;
	}

	if (!*cls.servername) {
		Con_Printf ("No server to reconnect to...\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f ();

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, Cmd_Argv (1));

	// open the demo file
	COM_DefaultExtension (name, ".qwd");

	cls.demofile = Qopen (name, "wb");
	if (!cls.demofile) {
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Con_Printf ("recording to %s.\n", name);
	cls.demorecording = true;

	CL_Disconnect ();
	CL_BeginServerConnect ();
}

void
CL_StartDemo (void)
{
	char		name[MAX_OSPATH];

	// open the demo file
	strncpy (name, demoname, sizeof (name));
	COM_DefaultExtension (name, ".qwd");

	Con_Printf ("Playing demo from %s.\n", name);
	COM_FOpenFile (name, &cls.demofile);
	if (!cls.demofile) {
		Con_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;				// stop demo loop
		return;
	}

	cls.demoplayback = true;
	CL_SetState (ca_demostart);
	Netchan_Setup (&cls.netchan, net_from, 0);
	realtime = 0;
	demotime_cached = 0;
}

/*
	CL_PlayDemo_f

	play [demoname]
*/
void
CL_PlayDemo_f (void)
{
	if (Cmd_Argc () != 2) {
		Con_Printf ("play <demoname> : plays a demo\n");
		return;
	}
	timedemo_runs = timedemo_count = 1;	// make sure looped timedemos stop
	// disconnect from server
	CL_Disconnect ();

	strncpy (demoname, Cmd_Argv (1), sizeof (demoname));
	CL_StartDemo ();
}

void
CL_StartTimeDemo (void)
{
	CL_StartDemo ();

	if (cls.state != ca_demostart)
		return;

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_starttime = 0;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;				// get a new message this frame

	CL_TimeFrames_Reset ();
	if (cl_timeframes->int_val)
		cl_timeframes_isactive = 1;
}

static inline double
sqr (double x)
{
	return x * x;
}

void
CL_FinishTimeDemo (void)
{
	float		time;
	int			frames;

	cls.timedemo = false;

	// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = Sys_DoubleTime () - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.2f seconds %5.2f fps\n", frames, time,
				frames / time);

	CL_TimeFrames_DumpLog();
	cl_timeframes_isactive = 0;

	timedemo_count--;
	timedemo_data[timedemo_count].frames = time;
	timedemo_data[timedemo_count].time = time;
	timedemo_data[timedemo_count].fps = frames / time;
	if (timedemo_count > 0) {
		CL_StartTimeDemo ();
	} else {
		if (--timedemo_runs > 0) {
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
			Con_Printf ("timedemo stats for %d runs:\n", timedemo_runs);
			Con_Printf ("  average fps: %.3f\n", average);
			Con_Printf ("  min/max fps: %.3f/%.3f\n", min, max);
			Con_Printf ("std deviation: %.3f fps\n", sqrt (variance));
		}
		free (timedemo_data);
		timedemo_data = 0;
	}
}

/*
	CL_TimeDemo_f

	timedemo [demoname]
*/
void
CL_TimeDemo_f (void)
{
	if (Cmd_Argc () < 2 || Cmd_Argc () > 3) {
		Con_Printf ("timedemo <demoname> [count]: gets demo speeds\n");
		return;
	}
	timedemo_runs = timedemo_count = 1;	// make sure looped timedemos stop
	// disconnect from server
	CL_Disconnect ();

	if (Cmd_Argc () == 3) {
		timedemo_count = atoi (Cmd_Argv (2));
	} else {
		timedemo_count = 1;
	}
	timedemo_runs = timedemo_count = max (timedemo_count, 1);
	if (timedemo_data)
		free (timedemo_data);
	timedemo_data = calloc (timedemo_runs, sizeof (td_stats_t));
	strncpy (demoname, Cmd_Argv (1), sizeof (demoname));
	CL_StartTimeDemo ();
}

void
CL_TimeFrames_Init (void)
{
	cl_timeframes_isactive = 0;
	cl_timeframes_index = 0;
	cl_timeframes_array = NULL;
}

void
CL_TimeFrames_Reset (void)
{
	cl_timeframes_index = 0;
	free(cl_timeframes_array);
	cl_timeframes_array = NULL;
}

void
CL_TimeFrames_AddTimestamp (void)
{
	if (cl_timeframes_isactive) {
		if (!(cl_timeframes_index % CL_TIMEFRAMES_ARRAYBLOCK))
			cl_timeframes_array = realloc
				(cl_timeframes_array, sizeof(cl_timeframes_array[0]) *
				 ((cl_timeframes_index / CL_TIMEFRAMES_ARRAYBLOCK) + 1) *
				 CL_TIMEFRAMES_ARRAYBLOCK);
		if (cl_timeframes_array == NULL)
			Sys_Error ("Unable to allocate timeframes buffer");
		cl_timeframes_array[cl_timeframes_index] = Sys_DoubleTime ();
		cl_timeframes_index++;
	}
	return;
}

void CL_TimeFrames_DumpLog (void)
{
	char		e_path[MAX_OSPATH];
	char	   *filename = "timeframes.txt";
	int			i;
	long		frame;
	VFile	   *outputfile;

	if (cl_timeframes_isactive == 0)
		return;

	Qexpand_squiggle (fs_userpath->string, e_path);
	Con_Printf ("Dumping Timed Frames log: %s\n", filename);
	outputfile = Qopen (va ("%s/%s", e_path, filename), "w");
	if (!outputfile) {
		Con_Printf ("Could not open: %s\n", filename);
		return;
	}
	for (i = 1; i < cl_timeframes_index; i++) {
		frame = (cl_timeframes_array[i] - cl_timeframes_array[i - 1]) * 1e6;
		Qprintf (outputfile, "%09ld\n", frame);
	}
	Qclose (outputfile);
}
