/*
	host.c

	host init

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

#ifdef HAVE_UNISTD_H
# include "unistd.h"
#endif

#include "QF/cbuf.h"
#include "QF/cdaudio.h"
#include "QF/draw.h"
#include "QF/idparse.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/png.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/gib.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "buildnum.h"
#include "compat.h"

#include "client/chase.h"

#include "nq/include/host.h"
#include "nq/include/server.h"
#include "nq/include/sv_progs.h"


/*
  A server can always be started, even if the system started out as a client
  to a remote system.

  A client can NOT be started if the system started as a dedicated server.

  Memory is cleared/released when a server or client begins, not when they end.
*/

SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
		SERVER_PLUGIN_LIST
};

qboolean	host_initialized;			// true if into command execution

quakeparms_t host_parms;

cbuf_t     *host_cbuf;

double		host_frametime;
double		host_time;
double		realtime;					// without any filtering or bounding
double		oldrealtime;				// last frame run

double      con_frametime;
double      con_realtime;
double      oldcon_realtime;

int			host_framecount;
size_t      host_hunklevel;
int         host_in_game;
size_t      minimum_memory;

client_t   *host_client;				// current client

jmp_buf		host_abortserver;

cvar_t     *host_mem_size;

cvar_t     *host_framerate;
cvar_t     *host_speeds;
cvar_t     *max_edicts;

cvar_t     *sys_ticrate;
cvar_t     *serverprofile;

cvar_t     *cl_quakerc;

cvar_t     *fraglimit;
cvar_t     *timelimit;
cvar_t     *teamplay;
cvar_t     *noexit;
cvar_t     *samelevel;

cvar_t     *skill;
cvar_t     *coop;
cvar_t     *deathmatch;

cvar_t     *pausable;

cvar_t     *temp1;
cvar_t     *cl_usleep;

static int  cl_usleep_cache;

static void
cl_usleep_f (cvar_t *var)
{
	cl_usleep_cache = var->int_val;
}


void
Host_EndGame (const char *message, ...)
{
	static dstring_t *str;
	va_list     argptr;

	if (!str)
		str = dstring_new ();

	va_start (argptr, message);
	dvsprintf (str, message, argptr);
	va_end (argptr);
	Sys_MaskPrintf (SYS_dev, "Host_EndGame: %s\n", str->str);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s", str->str);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
	Host_Error

	This shuts down both the client and server
*/
void
Host_Error (const char *error, ...)
{
	static dstring_t *str;
	static qboolean inerror = false;
	va_list     argptr;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");

	if (!str)
		str = dstring_new ();

	inerror = true;

	cl.loading = false;

	va_start (argptr, error);
	dvsprintf (str, error, argptr);
	va_end (argptr);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s", str->str);		// dedicated servers exit

	Sys_Printf ("Host_Error: %s\n", str->str);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	longjmp (host_abortserver, 1);
}

static void
Host_FindMaxClients (void)
{
	int         i;

	svs.maxclients = 1;

	i = COM_CheckParm ("-dedicated");
	if (i) {
		cls.state = ca_dedicated;
		if (i != (com_argc - 1)) {
			svs.maxclients = atoi (com_argv[i + 1]);
		} else
			svs.maxclients = 8;
	} else
		cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i) {
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i + 1]);
		else
			svs.maxclients = 8;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients =
		Hunk_AllocName (0, svs.maxclientslimit * sizeof (client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValue (deathmatch, 1.0);
	else
		Cvar_SetValue (deathmatch, 0.0);
}

static void
Host_InitLocal (void)
{
	Host_InitCommands ();

	host_framerate =
		Cvar_Get ("host_framerate", "0", CVAR_NONE, NULL,
				"set for slow motion");
	host_speeds =
		Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL,
				"set for running times");
	max_edicts = Cvar_Get ("max_edicts", "1024", CVAR_NONE, NULL,
						   "maximum server edicts");

	sys_ticrate = Cvar_Get ("sys_ticrate", "0.05", CVAR_NONE, NULL, "None");
	serverprofile = Cvar_Get ("serverprofile", "0", CVAR_NONE, NULL, "None");

	cl_quakerc = Cvar_Get ("cl_quakerc", "1", CVAR_NONE, NULL,
						   "exec quake.rc on startup");

	fraglimit = Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO, Cvar_Info,
						  "None");
	timelimit = Cvar_Get ("timelimit", "0", CVAR_SERVERINFO, Cvar_Info,
						  "None");
	teamplay = Cvar_Get ("teamplay", "0", CVAR_SERVERINFO, Cvar_Info, "None");
	samelevel = Cvar_Get ("samelevel", "0", CVAR_NONE, NULL, "None");
	noexit = Cvar_Get ("noexit", "0", CVAR_SERVERINFO, Cvar_Info, "None");
	skill = Cvar_Get ("skill", "1", CVAR_NONE, NULL, "0 - 3");
	deathmatch = Cvar_Get ("deathmatch", "0", CVAR_NONE, NULL, "0, 1, or 2");
	coop = Cvar_Get ("coop", "0", CVAR_NONE, NULL, "0 or 1");
	pausable = Cvar_Get ("pausable", "1", CVAR_NONE, NULL, "None");
	temp1 = Cvar_Get ("temp1", "0", CVAR_NONE, NULL, "None");
	cl_usleep = Cvar_Get ("cl_usleep", "1", CVAR_ARCHIVE, cl_usleep_f,
						  "Turn this on to save cpu when fps limited. "
						  "May affect frame rate adversely depending on "
						  "local machine/os conditions");

	Host_FindMaxClients ();

	host_time = 1.0;				// so a think at time 0 won't get called
}

/*
	SV_ClientPrintf

	Sends text across to be displayed
*/
void
SV_ClientPrintf (const char *fmt, ...)
{
	static dstring_t *str;
	va_list     argptr;

	if (!str)
		str = dstring_new ();

	va_start (argptr, fmt);
	dvsprintf (str, fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, str->str);
}

/*
	SV_BroadcastPrintf

	Sends text to all active clients
*/
void
SV_BroadcastPrintf (const char *fmt, ...)
{
	static dstring_t *str;
	va_list     argptr;

	if (!str)
		str = dstring_new ();

	va_start (argptr, fmt);
	dvsprintf (str, fmt, argptr);
	va_end (argptr);

	for (unsigned i = 0; i < svs.maxclients; i++)
		if (svs.clients[i].active && svs.clients[i].spawned) {
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, str->str);
		}
}

/*
	Host_ClientCommands

	Send text over to the client to be executed
*/
void
Host_ClientCommands (const char *fmt, ...)
{
	static dstring_t *str;
	va_list     argptr;

	if (!str)
		str = dstring_new ();

	va_start (argptr, fmt);
	dvsprintf (str, fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, str->str);
}

/*
	SV_DropClient

	Called when the player is getting totally kicked off the host
	if (crash = true), don't bother sending signofs
*/
void
SV_DropClient (qboolean crash)
{
	client_t   *client;
	unsigned    i;
	pr_uint_t   saveSelf;

	if (!crash) {
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection)) {
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection,
							 &host_client->message);
		}

		if (host_client->edict && host_client->spawned) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = *sv_globals.self;
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, host_client->edict);
			PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientDisconnect);
			*sv_globals.self = saveSelf;
		}

		Sys_Printf ("Client %s removed\n", host_client->name);
	}
	// break the net connection
	Sys_MaskPrintf (SYS_net, "dropping client\n");
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

	// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

	// send notification to all clients
	for (i = 0, client = svs.clients; i < svs.maxclients;
		 i++, client++) {
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
	Host_ShutdownServer

	This happens only at the end of a game, not between levels
*/
void
Host_ShutdownServer (qboolean crash)
{
	byte        message[4];
	double      start;
	unsigned    count, i;
	sizebuf_t   buf;

	if (!sv.active)
		return;

	sv.active = false;

	// stop all client sounds immediately
	if (cls.state >= ca_connected)
		CL_Disconnect ();

	// flush any pending messages - like the score!!!
	start = Sys_DoubleTime ();
	do {
		count = 0;
		for (i = 0, host_client = svs.clients; i < svs.maxclients;
			 i++, host_client++) {
			if (host_client->active && host_client->message.cursize) {
				if (NET_CanSendMessage (host_client->netconnection)) {
					NET_SendMessage (host_client->netconnection,
									 &host_client->message);
					SZ_Clear (&host_client->message);
				} else {
					NET_GetMessage (host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime () - start) > 3.0)
			break;
	}
	while (count);

	// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte (&buf, svc_disconnect);
	count = NET_SendToAll (&buf, 5.0);
	if (count)
		Sys_Printf
			("Host_ShutdownServer: NET_SendToAll failed for %u clients\n",
			 count);

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++)
		if (host_client->active)
			SV_DropClient (crash);

	// clear structures
	memset (&sv, 0, sizeof (sv));
	memset (svs.clients, 0, svs.maxclientslimit * sizeof (client_t));
}

/*
	Host_ClearMemory

	This clears all the memory used by both the client and server, but does
	not reinitialize anything.
*/
void
Host_ClearMemory (void)
{
	Sys_MaskPrintf (SYS_dev, "Clearing memory\n");
	CL_ClearMemory ();
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToLowMark (0, host_hunklevel);

	cls.signon = 0;
	memset (&sv, 0, sizeof (sv));
	memset (&cl, 0, sizeof (cl));
}

/*
	Host_FilterTime

	Returns false if the time is too short to run a frame
*/
static float
Host_FilterTime (float time)
{
	float       timedifference;
	float       timescale = 1.0;

	con_realtime += time;

	if (cls.demoplayback) {
		timescale = max (0, demo_speed->value);
		time *= timescale;
	}

	realtime += time;

	//FIXME not having the framerate cap is nice, but it breaks net play
	timedifference = (timescale / 72.0) - (realtime - oldrealtime);

	if (!cls.demoplayback && (timedifference > 0))
		return timedifference;                   // framerate is too high

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	con_frametime = con_realtime - oldcon_realtime;
	oldcon_realtime = con_realtime;

	if (host_framerate->value > 0)
		host_frametime = host_framerate->value;
	else	// don't allow really long or short frames
		host_frametime = bound (0.000, host_frametime, 0.1);

	return 0;
}

void
Host_ServerFrame (void)
{
	*sv_globals.frametime = sv_frametime = host_frametime;

	// set the time and clear the general datagram
	SV_ClearDatagram ();

	SV_CheckForNewClients ();

	// read client messages
	SV_RunClients ();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || host_in_game)) {
		SV_Physics ();
		sv.time += host_frametime;
	}

	// send all messages to the clients
	SV_SendClientMessages ();
}

static void
Host_ClientFrame (void)
{
	static double time1 = 0, time2 = 0, time3 = 0;
	int         pass1, pass2, pass3;

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

	// fetch results from server
	if (cls.state >= ca_connected)
		CL_ReadFromServer ();

	// update video
	if (host_speeds->int_val)
		time1 = Sys_DoubleTime ();

	r_data->inhibit_viewmodel = (chase_active->int_val
								 || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
								 || cl.stats[STAT_HEALTH] <= 0);
	r_data->frametime = host_frametime;

	CL_UpdateScreen (cl.time);

	if (host_speeds->int_val)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		mleaf_t    *l;
		byte       *asl = 0;

		l = Mod_PointInLeaf (r_data->origin, cl.worldmodel);
		if (l)
			asl = l->ambient_sound_level;
		S_Update (r_data->origin, r_data->vpn, r_data->vright, r_data->vup,
				  asl);
		r_funcs->R_DecayLights (host_frametime);
	} else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin, 0);

	CDAudio_Update ();

	if (host_speeds->int_val) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Sys_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}
}

/*
	Host_Frame

	Runs all active servers
*/
static void
_Host_Frame (float time)
{
	static int  first = 1;
	float       sleeptime;

	if (setjmp (host_abortserver))
		return;							// something bad happened, or the
										// server disconnected

	rand ();							// keep the random time dependent

	if (cls.demo_capture)
		time = 1.0 / 30;		//FIXME fixed 30fps atm

	// decide the simulation time
	if ((sleeptime = Host_FilterTime (time)) != 0) {
		// don't run too fast, or packet will flood outs
#ifdef HAVE_USLEEP
		if (cl_usleep_cache && sleeptime > 0.002) // minimum sleep time
			usleep ((unsigned long) (sleeptime * 1000000 / 2));
#endif
		return;
	}

	if (cls.state != ca_dedicated)
		IN_ProcessEvents ();

	if (cls.state == ca_dedicated)
		Con_ProcessInput ();

	GIB_Thread_Execute ();
	cmd_source = src_command;
	Cbuf_Execute_Stack (host_cbuf);

	if (first) {
		first = 0;

		if (isDedicated) {
			if (!sv.active)
				Cmd_ExecuteString ("map start", src_command);
			if (!sv.active)
				Sys_Error ("Could not initialize server");
		} else {
			Con_NewMap ();
		}

		CL_UpdateScreen (cl.time);
	}

	NET_Poll ();

	if (sv.active) {
		CL_SendCmd ();
		Host_ServerFrame ();
	}

	if (cls.state != ca_dedicated)
		Host_ClientFrame ();
	else
		host_time += host_frametime;	//FIXME is this needed? vcr stuff

	if (cls.demo_capture) {
		tex_t      *tex = r_funcs->SCR_CaptureBGR ();
		WritePNGqfs (va (0, "%s/qfmv%06d.png", qfs_gamedir->dir.shots,
						 cls.demo_capture++),
					 tex->data, tex->width, tex->height);
		free (tex);
	}

	host_framecount++;
	fps_count++;
}

void
Host_Frame (float time)
{
	double        time1, time2;
	static double timetotal;
	int           c, m;
	static int    timecount;

	if (!serverprofile->int_val) {
		_Host_Frame (time);
		return;
	}

	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal * 1000 / timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (unsigned i = 0; i < svs.maxclients; i++) {
		if (svs.clients[i].active)
			c++;
	}

	Sys_Printf ("serverprofile: %2i clients %2i msec\n", c, m);
}


#define	VCR_SIGNATURE	0x56435231
// "VCR1"

static void
Host_InitVCR (quakeparms_t *parms)
{
	char       *p;
	int         i, len, n;

	if (COM_CheckParm ("-playback")) {
		if (com_argc != 2)
			Sys_Error ("No other parameters allowed with -playback");

		vcrFile = QFS_Open ("quake.vcr", "rbz");
		if (!vcrFile)
			Sys_Error ("playback file not found");

		Qread (vcrFile, &i, sizeof (int));

		if (i != VCR_SIGNATURE)
			Sys_Error ("Invalid signature in vcr file");

		Qread (vcrFile, &com_argc, sizeof (int));
		com_argv = malloc (com_argc * sizeof (char *));
		SYS_CHECKMEM (com_argv);

		com_argv[0] = parms->argv[0];
		for (i = 0; i < com_argc; i++) {
			Qread (vcrFile, &len, sizeof (int));

			p = malloc (len);
			SYS_CHECKMEM (p);
			Qread (vcrFile, p, len);
			com_argv[i + 1] = p;
		}
		com_argc++;						/* add one for arg[0] */
		parms->argc = com_argc;
		parms->argv = com_argv;
	}

	if ((n = COM_CheckParm ("-record")) != 0) {
		vcrFile = QFS_Open ("quake.vcr", "wb");

		i = VCR_SIGNATURE;
		Qwrite (vcrFile, &i, sizeof (int));

		i = com_argc - 1;
		Qwrite (vcrFile, &i, sizeof (int));

		for (i = 1; i < com_argc; i++) {
			if (i == n) {
				len = 10;
				Qwrite (vcrFile, &len, sizeof (int));

				Qwrite (vcrFile, "-playback", len);
				continue;
			}
			len = strlen (com_argv[i]) + 1;
			Qwrite (vcrFile, &len, sizeof (int));

			Qwrite (vcrFile, com_argv[i], len);
		}
	}

}

static memhunk_t *
Host_Init_Memory (void)
{
	int         mem_parm = COM_CheckParm ("-mem");
	size_t      mem_size;
	void       *mem_base;

	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	host_mem_size = Cvar_Get ("host_mem_size", "40", CVAR_NONE, NULL,
							  "Amount of memory (in MB) to allocate for the "
							  PACKAGE_NAME " heap");
	if (mem_parm)
		Cvar_Set (host_mem_size, com_argv[mem_parm + 1]);

	if (COM_CheckParm ("-minmemory"))
		Cvar_SetValue (host_mem_size, minimum_memory / (1024 * 1024.0));

	Cvar_SetFlags (host_mem_size, host_mem_size->flags | CVAR_ROM);

	mem_size = ((size_t) host_mem_size->value * 1024 * 1024);

	if (mem_size < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   mem_size / (float) 0x100000);

	mem_base = Sys_Alloc (mem_size);

	if (!mem_base)
		Sys_Error ("Can't allocate %zd", mem_size);

	Sys_PageIn (mem_base, mem_size);
	memhunk_t  *hunk = Memory_Init (mem_base, mem_size);

	Sys_Printf ("%4.1f megabyte heap\n", host_mem_size->value);
	return hunk;
}

static void
Host_ExecConfig (cbuf_t *cbuf, int skip_quakerc)
{
	// quakeforge.cfg overrides quake.rc as it contains quakeforge-specific
	// commands. If it doesn't exist, then this is the first time quakeforge
	// has been used in this installation, thus any existing legacy config
	// should be used to set up defaults on the assumption that the user has
	// things set up to work with another (hopefully compatible) client
	if (CL_ReadConfiguration ("quakeforge.cfg")) {
		Cmd_Exec_File (cbuf, fs_usercfg->string, 0);
		Cmd_StuffCmds (cbuf);
		COM_Check_quakerc ("startdemos", cbuf);
	} else {
		if (!skip_quakerc) {
			Cbuf_InsertText (cbuf, "exec quake.rc\n");
		}
		Cmd_Exec_File (cbuf, fs_usercfg->string, 0);
		// Reparse the command line for + commands.
		// (sets still done, but it doesn't matter)
		// (Note, no non-base commands exist yet)
		if (skip_quakerc || !COM_Check_quakerc ("stuffcmds", 0)) {
			Cmd_StuffCmds (cbuf);
		}
	}
}

void
Host_Init (void)
{
	Sys_Printf ("Host_Init\n");

	host_cbuf = Cbuf_New (&id_interp);
	cmd_source = src_command;

	Sys_Init ();
	GIB_Init (true);
	GIB_Key_Init ();
	COM_ParseConfig (host_cbuf);

	memhunk_t  *hunk = Host_Init_Memory ();

	PI_Init ();

	Game_Init (hunk);

	if (!isDedicated)
		CL_InitCvars ();

	if (isDedicated) {
		PI_RegisterPlugins (server_plugin_list);
		Con_Init ("server");
	}

	Host_InitVCR (&host_parms);
	Host_InitLocal ();

	NET_Init ();

	Mod_Init ();

	SV_Init ();

	if (cls.state != ca_dedicated)
		CL_Init (host_cbuf);

	if (con_module) {
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = Host_Quit_f;
		con_module->data->console->cbuf = host_cbuf;
	}

	CL_UpdateScreen (cl.time);
	CL_UpdateScreen (cl.time);

	Host_ExecConfig (host_cbuf, isDedicated || !cl_quakerc->int_val);

	Hunk_AllocName (0, 0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark (0);

	Sys_Printf ("\nVersion %s (build %04d)\n\n", PACKAGE_VERSION,
				build_number ());

	Sys_Printf ("\x80\x81\x81\x82 %s initialized \x80\x81\x81\x82\n",
			    PACKAGE_NAME);

	host_initialized = true;

	CL_UpdateScreen (cl.time);
}

/*
	Host_Shutdown

	FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be
	better to run quit through here before final handoff to the sys code.
*/
void
Host_Shutdown (void *data)
{
	static qboolean isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;
}
