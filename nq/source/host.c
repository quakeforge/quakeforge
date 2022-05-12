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
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "buildnum.h"
#include "compat.h"

#include "client/chase.h"
#include "client/world.h"

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

float host_mem_size;
static cvar_t host_mem_size_cvar = {
	.name = "host_mem_size",
	.description =
		"Amount of memory (in MB) to allocate for the "
		PACKAGE_NAME
		" heap",
	.default_value = "40",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_float, .value = &host_mem_size },
};

float host_framerate;
static cvar_t host_framerate_cvar = {
	.name = "host_framerate",
	.description =
		"set for slow motion",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &host_framerate },
};
int host_speeds;
static cvar_t host_speeds_cvar = {
	.name = "host_speeds",
	.description =
		"set for running times",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &host_speeds },
};
int max_edicts;
static cvar_t max_edicts_cvar = {
	.name = "max_edicts",
	.description =
		"maximum server edicts",
	.default_value = "1024",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &max_edicts },
};

float sys_ticrate;
static cvar_t sys_ticrate_cvar = {
	.name = "sys_ticrate",
	.description =
		"None",
	.default_value = "0.05",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sys_ticrate },
};
int serverprofile;
static cvar_t serverprofile_cvar = {
	.name = "serverprofile",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &serverprofile },
};

int cl_quakerc;
static cvar_t cl_quakerc_cvar = {
	.name = "cl_quakerc",
	.description =
		"exec quake.rc on startup",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_quakerc },
};

float fraglimit;
static cvar_t fraglimit_cvar = {
	.name = "fraglimit",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_float, .value = &fraglimit },
};
int timelimit;
static cvar_t timelimit_cvar = {
	.name = "timelimit",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &timelimit },
};
int teamplay;
static cvar_t teamplay_cvar = {
	.name = "teamplay",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &teamplay },
};
float noexit;
static cvar_t noexit_cvar = {
	.name = "noexit",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_float, .value = &noexit },
};
float samelevel;
static cvar_t samelevel_cvar = {
	.name = "samelevel",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &samelevel },
};

int skill;
static cvar_t skill_cvar = {
	.name = "skill",
	.description =
		"0 - 3",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &skill },
};
int coop;
static cvar_t coop_cvar = {
	.name = "coop",
	.description =
		"0 or 1",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &coop },
};
int deathmatch;
static cvar_t deathmatch_cvar = {
	.name = "deathmatch",
	.description =
		"0, 1, or 2",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &deathmatch },
};

int pausable;
static cvar_t pausable_cvar = {
	.name = "pausable",
	.description =
		"None",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &pausable },
};

float temp1;
static cvar_t temp1_cvar = {
	.name = "temp1",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &temp1 },
};
int cl_usleep;
static cvar_t cl_usleep_cvar = {
	.name = "cl_usleep",
	.description =
		"Turn this on to save cpu when fps limited. May affect frame rate "
		"adversely depending on local machine/os conditions",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_usleep },
};

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

	if (net_is_dedicated)
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

	if (net_is_dedicated)
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
		if (i != (com_argc - 1)) {
			svs.maxclients = atoi (com_argv[i + 1]);
		} else
			svs.maxclients = 8;
	}
	cls.state = ca_disconnected;
	net_is_dedicated = i;

	i = COM_CheckParm ("-listen");
	if (i) {
		if (net_is_dedicated)
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
		deathmatch = 1.0;
	else
		deathmatch = 0.0;
}

static void
Host_InitLocal (void)
{
	Host_InitCommands ();

	Cvar_Register (&host_framerate_cvar, 0, 0);
	Cvar_Register (&host_speeds_cvar, 0, 0);
	Cvar_Register (&max_edicts_cvar, 0, 0);

	Cvar_Register (&sys_ticrate_cvar, 0, 0);
	Cvar_Register (&serverprofile_cvar, 0, 0);

	Cvar_Register (&cl_quakerc_cvar, 0, 0);

	Cvar_Register (&fraglimit_cvar, Cvar_Info, &fraglimit);
	Cvar_Register (&timelimit_cvar, Cvar_Info, &timelimit);
	Cvar_Register (&teamplay_cvar, Cvar_Info, &teamplay);
	Cvar_Register (&samelevel_cvar, 0, 0);
	Cvar_Register (&noexit_cvar, Cvar_Info, &noexit);
	Cvar_Register (&skill_cvar, 0, 0);
	Cvar_Register (&deathmatch_cvar, 0, 0);
	Cvar_Register (&coop_cvar, 0, 0);
	Cvar_Register (&pausable_cvar, 0, 0);
	Cvar_Register (&temp1_cvar, 0, 0);
	Cvar_Register (&cl_usleep_cvar, 0, 0);

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
		timescale = max (0, demo_speed);
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

	if (host_framerate > 0)
		host_frametime = host_framerate;
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
write_capture (tex_t *tex, void *data)
{
	QFile      *file = QFS_Open (va (0, "%s/qfmv%06d.png",
									 qfs_gamedir->dir.shots,
									 cls.demo_capture++), "wb");
	if (file) {
		WritePNG (file, tex);
		Qclose (file);
	}
	free (tex);
}

static void
Host_ClientFrame (void)
{
	static double time1 = 0, time2 = 0, time3 = 0;
	int         pass1, pass2, pass3;

	// fetch results from server
	if (cls.state >= ca_connected)
		CL_ReadFromServer ();

	// update video
	if (host_speeds)
		time1 = Sys_DoubleTime ();

	r_data->inhibit_viewmodel = (chase_active
								 || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
								 || cl.stats[STAT_HEALTH] <= 0);
	r_data->frametime = host_frametime;

	CL_UpdateScreen (cl.time);

	if (host_speeds)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		mleaf_t    *l;
		byte       *asl = 0;
		vec4f_t     origin;

		origin = Transform_GetWorldPosition (cl.viewstate.camera_transform);
		l = Mod_PointInLeaf ((vec_t*)&origin, cl_world.scene->worldmodel);//FIXME
		if (l)
			asl = l->ambient_sound_level;
		S_Update (cl.viewstate.camera_transform, asl);
		R_DecayLights (host_frametime);
	} else
		S_Update (0, 0);

	CDAudio_Update ();

	if (host_speeds) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Sys_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	if (cls.demo_capture) {
		r_funcs->capture_screen (write_capture, 0);
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
		if (cl_usleep && sleeptime > 0.002) // minimum sleep time
			usleep ((unsigned long) (sleeptime * 1000000 / 2));
#endif
		return;
	}
	host_time += host_frametime;	//FIXME is this needed? vcr stuff

	if (net_is_dedicated) {
		Con_ProcessInput ();
	} else {
		IN_ProcessEvents ();
	}

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

	if (!net_is_dedicated) {
		// Whether or not the server is active, if this is not a dedicated
		// server, then the client always needs to be able to process input
		// and send commands to the server before the server runs a frame.
		CL_SendCmd ();
	}

	if (sv.active) {
		Host_ServerFrame ();
	}

	if (!net_is_dedicated) {
		Host_ClientFrame ();
	}

	host_framecount++;
	fps_count++;
}

void
Host_Frame (float time)
{
	double        time1, time2;
	static double timetotal;
	int           c;
	double        m;
	static int    timecount;

	if (!serverprofile) {
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

	Sys_Printf ("serverprofile: %2i clients %5.3g msec\n", c, m);
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

	Cvar_Register (&host_mem_size_cvar, 0, 0);
	if (mem_parm)
		Cvar_Set ("host_mem_size", com_argv[mem_parm + 1]);

	if (COM_CheckParm ("-minmemory"))
		host_mem_size = minimum_memory / (1024 * 1024.0);


	mem_size = ((size_t) host_mem_size * 1024 * 1024);

	if (mem_size < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   mem_size / (float) 0x100000);

	mem_base = Sys_Alloc (mem_size);

	if (!mem_base)
		Sys_Error ("Can't allocate %zd", mem_size);

	Sys_PageIn (mem_base, mem_size);
	memhunk_t  *hunk = Memory_Init (mem_base, mem_size);

	Sys_Printf ("%4.1f megabyte heap\n", host_mem_size);
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
		Cmd_Exec_File (cbuf, fs_usercfg, 0);
		Cmd_StuffCmds (cbuf);
		COM_Check_quakerc ("startdemos", cbuf);
	} else {
		if (!skip_quakerc) {
			Cbuf_InsertText (cbuf, "exec quake.rc\n");
		}
		Cmd_Exec_File (cbuf, fs_usercfg, 0);
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
	Sys_RegisterShutdown (Host_Shutdown, 0);
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

	NET_Init (host_cbuf);

	Mod_Init ();

	SV_Init ();

	if (!net_is_dedicated)
		CL_Init (host_cbuf);

	if (con_module) {
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = Host_Quit_f;
		//FIXME need to rethink cbuf connections (they can form a stack)
		Cbuf_DeleteStack (con_module->data->console->cbuf);
		con_module->data->console->cbuf = host_cbuf;
	}

	CL_UpdateScreen (cl.time);
	CL_UpdateScreen (cl.time);

	Host_ExecConfig (host_cbuf, isDedicated || !cl_quakerc);

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
