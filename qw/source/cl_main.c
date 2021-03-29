/*
	cl_main.c

	Client main loop

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

#ifdef HAVE_WINDOWS_H
# include "winquake.h"
#endif
#include <sys/types.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_NETINET_IN_H
# define model_t sun_model_t
# include <netinet/in.h>
# undef model_t
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_WINSOCK_H
# include <winsock.h>
#endif
#ifdef HAVE_RPC_TYPES_H
# include <rpc/types.h>
#endif

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>

#include "qfalloca.h"

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/png.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/ruamoko.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/gib.h"

#include "QF/plugin/console.h"

#include "buildnum.h"
#include "compat.h"
#include "sbar.h"

#include "client/temp_entities.h"
#include "client/view.h"

#include "qw/bothdefs.h"
#include "qw/pmove.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_chat.h"
#include "qw/include/cl_demo.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_http.h"
#include "qw/include/cl_input.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_parse.h"
#include "qw/include/cl_pred.h"
#include "qw/include/cl_skin.h"
#include "qw/include/cl_slist.h"
#include "qw/include/client.h"
#include "qw/include/game.h"
#include "qw/include/host.h"
#include "netchan.h"

CLIENT_PLUGIN_PROTOS
static plugin_list_t client_plugin_list[] = {
	CLIENT_PLUGIN_LIST
};

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean    noclip_anglehack;			// remnant from old quake

cbuf_t     *cl_cbuf;
cbuf_t     *cl_stbuf;

cvar_t     *cl_mem_size;

cvar_t     *rcon_password;

cvar_t     *rcon_address;

cvar_t     *cl_writecfg;
cvar_t     *cl_allow_cmd_pkt;
cvar_t     *cl_cmd_pkt_adr;
cvar_t     *cl_paranoid;

cvar_t     *cl_timeout;

cvar_t     *cl_draw_locs;
cvar_t     *cl_shownet;
cvar_t     *cl_autoexec;
cvar_t     *cl_quakerc;
cvar_t     *cl_maxfps;
cvar_t     *cl_usleep;

cvar_t     *cl_cshift_bonus;
cvar_t     *cl_cshift_contents;
cvar_t     *cl_cshift_damage;
cvar_t     *cl_cshift_powerup;

cvar_t     *cl_model_crcs;

cvar_t     *lookspring;

cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

cvar_t     *cl_predict_players;
cvar_t     *cl_solid_players;

cvar_t     *localid;

cvar_t     *cl_port;
cvar_t     *cl_autorecord;

cvar_t     *cl_fb_players;

static qboolean allowremotecmd = true;

/*  info mirrors */
cvar_t     *password;
cvar_t     *spectator;
cvar_t     *cl_name;
cvar_t     *team;
cvar_t     *rate;
cvar_t     *noaim;
cvar_t     *msg;

/* GIB events */
gib_event_t *cl_player_health_e, *cl_chat_e;

static int  cl_usleep_cache;

client_static_t cls;
client_state_t cl;

entity_state_t cl_baselines[MAX_EDICTS];

double      connect_time = -1;			// for connection retransmits

quakeparms_t host_parms;
qboolean    host_initialized;			// true if into command execution
qboolean    nomaster;

double      host_frametime;
double      realtime;					// without any filtering or bounding
double      oldrealtime;				// last frame run
int         host_framecount;

double      con_frametime;
double      con_realtime;
double      oldcon_realtime;

int         host_hunklevel;

cvar_t     *host_speeds;
cvar_t     *hud_fps;
cvar_t     *hud_ping;
cvar_t     *hud_pl;
cvar_t     *hud_time;

int         fps_count;

jmp_buf     host_abort;

char       *server_version = NULL;		// version of server we connected to

char        emodel_name[] = "emodel";
char        pmodel_name[] = "pmodel";
char        prespawn_name[] = "prespawn %i 0 %i";
char        modellist_name[] = "modellist %i %i";
char        soundlist_name[] = "soundlist %i %i";

extern cvar_t *hud_scoreboard_uid;
static netadr_t cl_cmd_packet_address;

static void
CL_Quit_f (void)
{
	if (!con_module)
		Sys_Printf ("I hope you wanted to quit\n");
	CL_Disconnect ();
	Sys_Quit ();
}

static void
CL_Version_f (void)
{
	Sys_Printf ("%s Version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	Sys_Printf ("Binary: " __TIME__ " " __DATE__ "\n");
}

/*
	CL_SendConnectPacket

	called by CL_Connect_f and CL_CheckResend
*/
static void
CL_SendConnectPacket (void)
{
	dstring_t  *data;
	double      t1, t2;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//		 Now, adds lookup time to the connect time.
//		 Should I add it to realtime instead?!?!

	if (cls.state != ca_disconnected)
		return;

	t1 = Sys_DoubleTime ();

	if (!NET_StringToAdr (cls.servername->str, &cls.server_addr)) {
		Sys_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (cls.server_addr.port == 0)
		cls.server_addr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	cls.qport = qport->int_val;

	data = dstring_new ();
	dsprintf (data, "%c%c%c%cconnect %i %i %i \"%s\"\n",
			  255, 255, 255, 255, PROTOCOL_VERSION, cls.qport, cls.challenge,
			  Info_MakeString (cls.userinfo, 0));
	Netchan_SendPacket (strlen (data->str), data->str, cls.server_addr);
	dstring_delete (data);
}

/*
	CL_CheckForResend

	Resend a connect message if the last one has timed out
*/
static void
CL_CheckForResend (void)
{
	static const char *getchallenge = "\377\377\377\377getchallenge\n";
	double      t1, t2;

	if (connect_time == -1)
		return;
	if (cls.state != ca_disconnected)
		return;
	if (connect_time && realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername->str, &cls.server_addr)) {
		Sys_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (cls.server_addr.port == 0)
		cls.server_addr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	VID_SetCaption (va (0, "Connecting to %s", cls.servername->str));
	Sys_Printf ("Connecting to %s...\n", cls.servername->str);
	Netchan_SendPacket (strlen (getchallenge), (void *) getchallenge,
						cls.server_addr);
}

void
CL_BeginServerConnect (void)
{
	connect_time = 0;
	CL_CheckForResend ();
}

static void
CL_Connect_f (void)
{
	const char *server;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect ();
	CL_Chat_Flush_Ignores ();

	dstring_copystr (cls.servername, server);
	CL_BeginServerConnect ();
}

/*
	CL_Rcon_f

	Send the rest of the command line over as
	an unconnected command.
*/
static void
CL_Rcon_f (void)
{
	static dstring_t *message;
	netadr_t    to;

	if (!message)
		message = dstring_new ();

	dsprintf (message, "\377\377\377\377rcon %s %s", rcon_password->string,
			  Cmd_Args (1));

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!rcon_address->string[0]) {
			Sys_Printf ("You must either be connected, or set the "
						"'rcon_address' cvar to issue rcon commands\n");
			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (27500);
	}

	Netchan_SendPacket (strlen (message->str) + 1, message->str, to);
}

void
CL_ClearState (void)
{
	int         i;

	S_StopAllSounds ();

	// wipe the entire cl structure
	if (cl.serverinfo)
		Info_Destroy (cl.serverinfo);
	if (cl.players)
		free (cl.players);
	memset (&cl, 0, sizeof (cl));
	cl.players = calloc (MAX_CLIENTS, sizeof (player_info_t));
	r_data->force_fullscreen = 0;

	cl.maxclients = MAX_CLIENTS;

	// Note: we should probably hack around this and give diff values for
	// diff gamedirs
	cl.fpd = FPD_DEFAULT;
	cl.fbskins = FBSKINS_DEFAULT;

	for (i = 0; i < UPDATE_BACKUP; i++)
		cl.frames[i].packet_entities.entities = qw_entstates.frame[i];
	cl.serverinfo = Info_ParseString ("", MAX_INFO_STRING, 0);

	CL_Init_Entity (&cl.viewent);

	Sys_MaskPrintf (SYS_DEV, "Clearing memory\n");
	VID_ClearMemory ();
	Mod_ClearAll ();
	if (host_hunklevel)					// FIXME: check this...
		Hunk_FreeToLowMark (host_hunklevel);

	CL_ClearEnts ();
	CL_ClearTEnts ();

	r_funcs->R_ClearState ();

	SZ_Clear (&cls.netchan.message);

	if (centerprint)
		dstring_clearstr (centerprint);
}

/*
	CL_StopCshifts

	Cleans the Cshifts, so your screen doesn't stay red after a timedemo :)
*/
static void
CL_StopCshifts (void)
{
	int i;

	for (i = 0; i < NUM_CSHIFTS; i++)
		cl.cshifts[i].percent = 0;
	for (i = 0; i < MAX_CL_STATS; i++)
		cl.stats[i] = 0;
}

static void
CL_RemoveQFInfoKeys (void)
{
	Info_RemoveKey (cls.userinfo, "*cap");
	Info_RemoveKey (cls.userinfo, "*qf_version");
	Info_RemoveKey (cls.userinfo, "*qsg_version");
}

/*
	CL_Disconnect

	Sends a disconnect message to the server
	This is also called on Host_Error, so it shouldn't cause any errors
*/
void
CL_Disconnect (void)
{
	byte        final[10];
	int	i;

	connect_time = -1;

	VID_SetCaption ("Disconnected");

	// stop sounds (especially looping!)
	S_StopAllSounds ();

	// Clean the Cshifts
	CL_StopCshifts ();

	// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state != ca_disconnected) {
		if (cls.demorecording)
			CL_StopRecording ();

		final[0] = clc_stringcmd;
		strcpy ((char *) final + 1, "drop");
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);

		CL_SetState (ca_disconnected);

		cls.demoplayback = cls.demorecording = cls.timedemo = false;
		cls.demoplayback2 = false;

		CL_RemoveQFInfoKeys ();
	}
	Cam_Reset ();

	if (cls.download) {
		CL_HTTP_Reset ();
		dstring_clearstr (cls.downloadname);
		dstring_clearstr (cls.downloadurl);
		dstring_clearstr (cls.downloadtempname);
		Qclose (cls.download);
		cls.download = NULL;
	}

	CL_StopUpload ();

	Team_ResetTimers ();

	// remove player info strings
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].userinfo)
			Info_Destroy (cl.players[i].userinfo);
		memset (&cl.players[i], 0, sizeof (cl.players[i]));
	}
	cl.worldmodel = NULL;
	cl.validsequence = 0;
}

void
CL_Disconnect_f (void)
{
	CL_Disconnect ();
}

/*
	CL_User_f

	user <name or userid>

	Dump userdata / masterdata for a user
*/
static void
CL_User_f (void)
{
	int         uid, i;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("Usage: user <username / userid>\n");
		return;
	}

	uid = atoi (Cmd_Argv (1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name->value[0])
			continue;
		if (cl.players[i].userid == uid
			|| !strcmp (cl.players[i].name->value, Cmd_Argv (1))) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Sys_Printf ("User not in server.\n");
}

/*
	CL_Users_f

	Dump userids for all current players
*/
static void
CL_Users_f (void)
{
	int         c, i;

	c = 0;
	Sys_Printf ("userid frags name\n");
	Sys_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name && cl.players[i].name->value[0]) {
			Sys_Printf ("%6i %4i %s\n", cl.players[i].userid,
						cl.players[i].frags, cl.players[i].name->value);
			c++;
		}
	}

	Sys_Printf ("%i total users\n", c);
}

/*
	CL_FullServerinfo_f

	Sent by server when serverinfo changes
*/
static void
CL_FullServerinfo_f (void)
{
	const char *p;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	Sys_MaskPrintf (SYS_DEV, "Cmd_Argv (1): '%s'\n", Cmd_Argv (1));
	Info_Destroy (cl.serverinfo);
	cl.serverinfo = Info_ParseString (Cmd_Argv (1), MAX_SERVERINFO_STRING, 0);
	Sys_MaskPrintf (SYS_DEV, "cl.serverinfo: '%s'\n",
					Info_MakeString (cl.serverinfo, 0));

	if ((p = Info_ValueForKey (cl.serverinfo, "*qf_version")) && *p) {
		if (server_version == NULL)
			Sys_Printf ("QuakeForge v%s Server\n", p);
		server_version = strdup (p);
	} else if ((p = Info_ValueForKey (cl.serverinfo, "*version")) && *p) {
		if (server_version == NULL)
			Sys_Printf ("QuakeWorld v%s Server\n", p);
		server_version = strdup (p);
	}

	if ((p = Info_ValueForKey (cl.serverinfo, "*qsg_version")) && *p) {
		if ((cl.stdver = atof (p)))
			Sys_Printf ("Server supports QSG v%s protocol\n", p);
		else
			Sys_Printf ("Invalid QSG Protocol number: %s", p);
	}

	cl.chase = cl.sv_cshifts = cl.no_pogo_stick = cl.teamplay = 0;
	if ((p = Info_ValueForKey (cl.serverinfo, "chase")) && *p) {
		cl.chase = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "cshifts")) && *p) {
		cl.sv_cshifts = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "no_pogo_stick")) && *p) {
		cl.no_pogo_stick = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "teamplay")) && *p) {
		cl.teamplay = atoi (p);
		Sbar_DMO_Init_f (hud_scoreboard_uid); // HUD setup, cl.teamplay changed
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "watervis")) && *p) {
		cl.watervis = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "fpd")) && *p) {
		cl.fpd = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "fbskins")) && *p) {
		cl.fbskins = atoi (p);
	}

	// R_LoadSkys does the right thing with null pointers.
	r_funcs->R_LoadSkys (Info_ValueForKey (cl.serverinfo, "sky"));
}

static void
CL_AddQFInfoKeys (void)
{
	// set the capabilities info. single char flags (possibly with modifiers)
	// defined capabilities (* = not implemented):
	// z   client can accept gzipped files.
	// h   http transfers
	// f   * ftp transfers
	// a   * audio channel (voice chat)
	// i   * irc
	// p   pogo stick control
	// t   team messages
	static const char *cap = "pt"
#ifdef HAVE_ZLIB
		"z"
#endif
#ifdef HAVE_LIBCURL
		"h"
#endif
		;

	Info_SetValueForStarKey (cls.userinfo, "*cap", cap, 0);
	Info_SetValueForStarKey (cls.userinfo, "*qf_version", PACKAGE_VERSION, 0);
	Info_SetValueForStarKey (cls.userinfo, "*qsg_version", QW_QSG_VERSION, 0);
}

/*
	CL_FullInfo_f

	Allow clients to change userinfo
	Casey was here :)
*/
static void
CL_FullInfo_f (void)
{
	info_t     *info;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("fullinfo <complete info string>\n");
		return;
	}

	info = Info_ParseString (Cmd_Argv (1), MAX_INFO_STRING, 0);
	Info_AddKeys (cls.userinfo, info);
	Info_Destroy (info);
}

/*
	CL_SetInfo_f

	Allow clients to change userinfo
*/
static void
CL_SetInfo_f (void)
{
	if (Cmd_Argc () == 1) {
		Info_Print (cls.userinfo);
		return;
	}
	if (Cmd_Argc () != 3) {
		Sys_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (strcaseequal (Cmd_Argv (1), pmodel_name)
		|| strcaseequal (Cmd_Argv (1), emodel_name))
		return;

	Info_SetValueForKey (cls.userinfo, Cmd_Argv (1), Cmd_Argv (2),
						 (!strequal (Cmd_Argv (1), "name")) |
						 (strequal (Cmd_Argv (2), "team") << 1));
	if (cls.state >= ca_connected)
		CL_Cmd_ForwardToServer ();
}

/*
	CL_Packet_f

	packet <destination> <contents>
	Contents allows \n escape character
*/
static void
CL_Packet_f (void)
{
	char       *send;
	char       *out;
	const char *in;
	int         i, l;
	netadr_t    adr;

	if (Cmd_Argc () != 3) {
		Sys_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv (1), &adr)) {
		Sys_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv (2);
	send = malloc (strlen (in) + 4 + 1);
	out = send + 4;
	send[0] = send[1] = send[2] = send[3] = 0xff;

	l = strlen (in);
	for (i = 0; i < l; i++) {
		if (in[i] == '\\' && in[i + 1] == 'n') {
			*out++ = '\n';
			i++;
		} else
			*out++ = in[i];
	}
	*out = 0;

	Netchan_SendPacket (out - send, send, adr);
	free (send);
}

/*
	CL_NextDemo

	Called to play the next demo in the demo loop
*/
void
CL_NextDemo (void)
{
	if (cls.demonum == -1)
		return;							// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
			cls.demonum = -1;
			return;
		}
	}

	Cbuf_InsertText (cl_cbuf, va (0, "playdemo %s\n", cls.demos[cls.demonum]));
	cls.demonum++;
}

/*
	CL_Changing_f

	Sent as just a hint to the client that they should
	drop to full console
*/
static void
CL_Changing_f (void)
{
	if (cls.download)					// don't change when downloading
		return;

	S_StopAllSounds ();
	cl.intermission = 0;
	r_data->force_fullscreen = 0;
	CL_SetState (ca_connected);			// not active anymore, but not
										// disconnected
	Sys_Printf ("\nChanging map...\n");
}

/*
	CL_Reconnect_f

	The server is changing levels
*/
static void
CL_Reconnect_f (void)
{
	if (cls.download)					// don't change when downloading
		return;
	if (cls.demoplayback)
		return;

	S_StopAllSounds ();

	if (cls.state == ca_connected) {
		Sys_Printf ("reconnecting...\n");
		VID_SetCaption ("Reconnecting");
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername->str) {
		Sys_Printf ("No server to which to reconnect...\n");
		return;
	}

	CL_Disconnect ();
	CL_BeginServerConnect ();
}

/*
	CL_ConnectionlessPacket

	Responses to broadcasts, etc
*/
static void
CL_ConnectionlessPacket (void)
{
	const char *s;
	int         c;

	MSG_BeginReading (net_message);
	MSG_ReadLong (net_message);					// skip the -1

	c = MSG_ReadByte (net_message);
	if (net_message->badread)
		return;
	if (!cls.demoplayback
		&& (cl_paranoid->int_val
			|| !NET_CompareAdr (net_from, cls.server_addr)))
		Sys_Printf ("%s: ", NET_AdrToString (net_from));
	if (c == S2C_CONNECTION) {
		Sys_Printf ("connection\n");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Sys_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (&cls.netchan, net_from, cls.qport, NC_QPORT_SEND);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		CL_SetState (ca_connected);
		Sys_Printf ("Connected.\n");
		allowremotecmd = false;			// localid required now for remote
										// cmds
		return;
	}
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND) {
		char       *cmdtext;
		int         len;

		Sys_Printf ("client command\n");

		if (!cl_allow_cmd_pkt->int_val
			|| (!NET_CompareBaseAdr (net_from, net_local_adr)
				&& !NET_CompareBaseAdr (net_from, net_loopback_adr)
				&& (!cl_cmd_pkt_adr->string[0]
					|| !NET_CompareBaseAdr (net_from,
											cl_cmd_packet_address)))) {
			Sys_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		if (cl_cmd_pkt_adr->string[0]
			&& NET_CompareBaseAdr (net_from, cl_cmd_packet_address))
			allowremotecmd = false; // force password checking
		s = MSG_ReadString (net_message);

		cmdtext = alloca (strlen (s) + 1);
		strcpy (cmdtext, s);

		s = MSG_ReadString (net_message);

		while (*s && isspace ((byte) *s))
			s++;
		len = strlen (s);
		while (len && isspace ((byte) s[len - 1]))
			len--;

		if (!allowremotecmd && (!*localid->string ||
								(int) strlen (localid->string) > len ||
								strncmp (localid->string, s, len))) {
			if (!*localid->string) {
				Sys_Printf ("===========================\n");
				Sys_Printf ("Command packet received from local host, but no "
							"localid has been set.  You may need to upgrade "
							"your server browser.\n");
				Sys_Printf ("===========================\n");
				return;
			}
			Sys_Printf ("===========================\n");
			Sys_Printf
				("Invalid localid on command packet received from local host. "
				 "\n|%s| != |%s|\n"
				 "You may need to reload your server browser and %s.\n", s,
				 localid->string, PACKAGE_NAME);
			Sys_Printf ("===========================\n");
			Cvar_Set (localid, "");
			return;
		}

		Sys_Printf ("%s\n", cmdtext);
		Cbuf_AddText (cl_cbuf, cmdtext);
		allowremotecmd = false;
		return;
	}
	// print command from somewhere
	if (c == A2C_PRINT) {
		s = MSG_ReadString (net_message);
		if (SL_CheckStatus (NET_AdrToString (net_from), s))
		{
			Sys_Printf ("status response\n");
			return;
		} else if (!cls.demoplayback
				   && (cl_paranoid->int_val
					   || !NET_CompareAdr (net_from, cls.server_addr))) {
			Sys_Printf ("print\n");
		}
		Sys_Printf ("%s", s);
		return;
	}
	// ping from somewhere
	if (c == A2A_PING) {
		char        data[6];

		Sys_Printf ("ping\n");

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		Netchan_SendPacket (6, &data, net_from);
		return;
	}

	if (c == S2C_CHALLENGE) {
		Sys_Printf ("challenge");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Sys_Printf ("\nDup challenge received.  Ignored.\n");
			return;
		}

		s = MSG_ReadString (net_message);
		cls.challenge = atoi (s);
		if (strstr (s, "QF")) {
			Sys_Printf (": QuakeForge server detected\n");
			CL_AddQFInfoKeys ();
		} else if (strstr (s, "EXT")) {
			CL_AddQFInfoKeys ();
		} else {
			Sys_Printf ("\n");
		}
		CL_SendConnectPacket ();
		return;
	}

	if (c == M2C_MASTER_REPLY)
	{
		Sys_Printf ("Master Server Reply\n");
		MSG_ReadByte (net_message);
		s = MSG_ReadString (net_message);
		MSL_ParseServerList (s);
		return;
	}
	if (c == svc_disconnect) {
		if (cls.demoplayback)
			Host_EndGame ("End of demo");
		else
			Sys_Printf ("svc_disconnect\n");
		return;
	}

	Sys_Printf ("unknown:  %c\n", c);
}

void
CL_ReadPackets (void)
{
	while (CL_GetMessage ()) {

		if (net_message->message->cursize == -1)
			continue;

		if (cls.demoplayback && net_packetlog->int_val)
			Log_Incoming_Packet (net_message->message->data,
								 net_message->message->cursize, 0, 0);

		// remote command packet
		if (*(int *) net_message->message->data == -1) {
			CL_ConnectionlessPacket ();
			continue;
		}
		if (*(char *) net_message->message->data == A2A_ACK)
		{
			SL_CheckPing (NET_AdrToString (net_from));
			continue;
		}
		if (net_message->message->cursize < 8 && !cls.demoplayback2) {
			Sys_Printf ("%s: Runt packet\n", NET_AdrToString (net_from));
			continue;
		}

		// packet from server
		if (!cls.demoplayback &&
			!NET_CompareAdr (net_from, cls.netchan.remote_address)) {
			Sys_MaskPrintf (SYS_DEV,
							"%s:sequenced packet without connection\n",
							NET_AdrToString (net_from));
			continue;
		}
		if (!cls.demoplayback2) {
			if (!Netchan_Process (&cls.netchan))
				continue;					// wasn't accepted for some reason
		} else {
			MSG_BeginReading (net_message);
		}
		if (cls.state != ca_disconnected)
			CL_ParseServerMessage ();
	}

	// check timeout
	if (!cls.demoplayback && cls.state >= ca_connected
		&& realtime - cls.netchan.last_received > cl_timeout->value) {
		Sys_Printf ("\nServer connection timed out.\n");
		CL_Disconnect ();
		return;
	}

}

static void
CL_Download_f (void)
{
	if (cls.state == ca_disconnected || cls.demoplayback) {
		Sys_Printf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Sys_Printf ("Usage: download <datafile>\n");
		return;
	}

	dstring_copystr (cls.downloadname, Cmd_Argv (1));
	dstring_copystr (cls.downloadtempname, Cmd_Argv (1));

	QFS_StripExtension (cls.downloadname->str, cls.downloadtempname->str);
	dstring_appendstr (cls.downloadtempname, ".tmp");

	cls.download = QFS_WOpen (cls.downloadname->str, 0);
	if (cls.download) {
		cls.downloadtype = dl_single;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, va (0, "download %s\n", Cmd_Argv (1)));
	} else {
		Sys_Printf ("error downloading %s: %s\n", Cmd_Argv (1),
					strerror (errno));
	}
}

static void
Force_CenterView_f (void)
{
	cl.viewstate.angles[PITCH] = 0;
}

static void
CL_PRotate_f (void)
{
	if ((cl.fpd & FPD_LIMIT_PITCH) || Cmd_Argc() < 2)
		return;

	cl.viewstate.angles[PITCH] += atoi (Cmd_Argv (1));

	if (cl.viewstate.angles[PITCH] < -70)
		cl.viewstate.angles[PITCH] = -70;
	else if (cl.viewstate.angles[PITCH] > 80)
		cl.viewstate.angles[PITCH] = 80;
}

static void
CL_Rotate_f (void)
{
	if ((cl.fpd & FPD_LIMIT_YAW) || Cmd_Argc() < 2)
		return;

	cl.viewstate.angles[YAW] += atoi (Cmd_Argv (1));
	cl.viewstate.angles[YAW] = anglemod (cl.viewstate.angles[YAW]);
}

void
CL_SetState (cactive_t state)
{
	static const char *state_names[] = {
		"ca_disconnected",
		"ca_demostate",
		"ca_connected",
		"ca_onserver",
		"ca_active",
	};
	cactive_t   old_state = cls.state;

	Sys_MaskPrintf (SYS_DEV, "CL_SetState (%s)\n", state_names[state]);
	cls.state = state;
	if (old_state != state) {
		if (old_state == ca_active) {
			// leaving active state
			IN_ClearStates ();
			Key_SetKeyDest (key_console);

			// Auto demo recorder stops here
			if (cl_autorecord->int_val && cls.demorecording)
				CL_StopRecording ();
		} else if (state == ca_active) {
			// entering active state
			VID_SetCaption (cls.servername->str);
			IN_ClearStates ();
			Key_SetKeyDest (key_game);

			// Auto demo recorder starts here
			if (cl_autorecord->int_val && !cls.demoplayback
				&& !cls.demorecording)
				CL_Record (0, -1);
		}
	}
	if (con_module)
		con_module->data->console->force_commandline = (state != ca_active);
}

void
CL_Init (void)
{
	byte       *basepal, *colormap;

	basepal = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/palette.lmp"));
	if (!basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	colormap = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/colormap.lmp"));
	if (!colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");

	W_LoadWadFile ("gfx.wad");
	VID_Init (basepal, colormap);
	IN_Init (cl_cbuf);
	Mod_Init ();
	R_Init ();
	r_data->lightstyle = cl.lightstyle;

	PI_RegisterPlugins (client_plugin_list);
	Con_Init ("client");
	if (con_module) {
		con_module->data->console->dl_name = cls.downloadname;
		con_module->data->console->dl_percent = &cls.downloadpercent;
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = CL_Quit_f;
		con_module->data->console->cbuf = cl_cbuf;
	}

	S_Init (&cl.viewentity, &host_frametime);
	CDAudio_Init ();

	Sbar_Init ();

	CL_Input_Init ();
	CL_Ents_Init ();
	CL_TEnts_Init ();
	CL_ClearState ();
	Pmove_Init ();

	SL_Init ();

	CL_Skin_Init ();
	Locs_Init ();
	V_Init ();

	Info_SetValueForStarKey (cls.userinfo, "*ver", QW_VERSION, 0);

	centerprint = dstring_newstr ();
	cls.servername = dstring_newstr ();
	cls.downloadtempname = dstring_newstr ();
	cls.downloadname = dstring_newstr ();
	cls.downloadurl = dstring_newstr ();
	cl.serverinfo = Info_ParseString ("", MAX_INFO_STRING, 0);
	cl.players = calloc (MAX_CLIENTS, sizeof (player_info_t));

	// register our commands
	Cmd_AddCommand ("version", CL_Version_f, "Report version information");
	Cmd_AddCommand ("changing", CL_Changing_f, "Used when maps are changing");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "Disconnect from server");
	Cmd_AddCommand ("snap", CL_RSShot_f, "Take a screenshot and upload it to "
					"the server");
	Cmd_AddCommand ("maplist", Con_Maplist_f, "List maps available");
	Cmd_AddCommand ("skinlist", Con_Skinlist_f, "List skins available");
	Cmd_AddCommand ("skyboxlist", Con_Skyboxlist_f, "List skyboxes available");
	Cmd_AddCommand ("demolist", Con_Demolist_QWD_f, "List demos available");
	if (!con_module)
		Cmd_AddCommand ("quit", CL_Quit_f, "Exit the program");
	Cmd_AddCommand ("connect", CL_Connect_f, "Connect to a server 'connect "
					"hostname:port'");
	Cmd_AddCommand ("reconnect", CL_Reconnect_f, "Reconnect to the last "
					"server");
	Cmd_AddCommand ("rcon", CL_Rcon_f, "Issue set of commands to the current "
					"connected server or the one set in rcon_address");
	Cmd_AddCommand ("packet", CL_Packet_f, "Send a packet with specified "
					"contents to the destination");
	Cmd_AddCommand ("user", CL_User_f, "Queries the user for his setinfo "
					"information");
	Cmd_AddCommand ("users", CL_Users_f, "Report information on connected "
					"players and retrieve user ids");
	Cmd_AddCommand ("setinfo", CL_SetInfo_f, "Sets information about your "
					"QuakeWorld user.\n Used without a key it will list all "
					"of your current settings.\n Specifying a non-existent "
					"key and a value will create the new key.\n"
					"Special Keys:\n b_switch - Determines the highest weapon "
					"that Quake should switch to upon a backpack pickup.\n "
					"w_switch - Determines the highest weapon that Quake "
					"should switch to upon a weapon pickup.");
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f, "Used by GameSpy and Qlist to "
					"set setinfo variables");
	Cmd_AddCommand ("fullserverinfo", CL_FullServerinfo_f, "Used by GameSpy "
					"and Qlist to obtain server variables");
	Cmd_AddCommand ("download", CL_Download_f, "Manually download a quake "
					"file from the server");
	Cmd_AddCommand ("nextul", CL_NextUpload, "Tells the client to send the "
					"next upload");
	Cmd_AddCommand ("stopul", CL_StopUpload, "Tells the client to stop "
					"uploading");
	Cmd_AddCommand ("force_centerview", Force_CenterView_f, "force the view "
					"to be level");
	Cmd_AddCommand ("rotate", CL_Rotate_f, "Look left or right a given amount. Usage: rotate <degrees>");
	Cmd_AddCommand ("protate", CL_PRotate_f, "Look up or down a given amount. Usage: protate <degrees>");
	// forward to server commands
	Cmd_AddCommand ("kill", CL_Cmd_ForwardToServer, "Suicide :)");
	Cmd_AddCommand ("pause", CL_Cmd_ForwardToServer, "Pause the game");
	Cmd_AddCommand ("say", CL_Cmd_ForwardToServer, "Say something to all "
					"other players");
	Cmd_AddCommand ("say_team", CL_Cmd_ForwardToServer, "Say something to "
					"only people on your team");
	Cmd_AddCommand ("serverinfo", CL_Cmd_ForwardToServer, "Report the current "
					"server info");
	cl_player_health_e = GIB_Event_New ("player.health");
	cl_chat_e = GIB_Event_New ("chat");

	CL_SetState (ca_disconnected);
}

static void
cl_usleep_f (cvar_t *var)
{
	cl_usleep_cache = var->int_val;
}

static void
cl_cmd_pkt_adr_f (cvar_t *var)
{
	if (var->string[0])
		NET_StringToAdr (var->string, &cl_cmd_packet_address);
	else
		memset (&cl_cmd_packet_address, 0, sizeof (cl_cmd_packet_address));
}

static void
CL_Init_Cvars (void)
{
	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	CL_Cam_Init_Cvars ();
	CL_Input_Init_Cvars ();
	CL_Prediction_Init_Cvars ();
	Game_Init_Cvars ();
	Pmove_Init_Cvars ();
	Team_Init_Cvars ();
	V_Init_Cvars ();

	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL,
						   "Toggle the display of a graph showing network "
						   "performance");
	r_netgraph_alpha = Cvar_Get ("r_netgraph_alpha", "0.5", CVAR_ARCHIVE, NULL,
								 "Net graph translucency");
	r_netgraph_box = Cvar_Get ("r_netgraph_box", "1", CVAR_ARCHIVE, NULL,
							   "Draw box around net graph");

	cls.userinfo = Info_ParseString ("", MAX_INFO_STRING, 0);

	cl_model_crcs = Cvar_Get ("cl_model_crcs", "1", CVAR_ARCHIVE, NULL,
							  "Controls setting of emodel and pmodel info "
							  "vars. Required by some servers, but clearing "
							  "this can make the difference between "
							  "connecting and not connecting on some others.");
	cl_allow_cmd_pkt = Cvar_Get ("cl_allow_cmd_pkt", "1", CVAR_NONE, NULL,
								 "enables packets from the likes of gamespy");
	cl_cmd_pkt_adr = Cvar_Get ("cl_cmd_pkt_adr", "", CVAR_NONE,
							   cl_cmd_pkt_adr_f,
							   "allowed address for non-local command packet");
	cl_paranoid = Cvar_Get ("cl_paranoid", "1", CVAR_NONE, NULL,
							"print source address of connectionless packets"
							" even when coming from the server being connected"
							" to.");
	cl_autoexec = Cvar_Get ("cl_autoexec", "0", CVAR_ROM, NULL,
							"exec autoexec.cfg on gamedir change");
	cl_quakerc = Cvar_Get ("cl_quakerc", "1", CVAR_NONE, NULL,
						   "exec quake.rc on startup");
	cl_cshift_bonus = Cvar_Get ("cl_cshift_bonus", "1", CVAR_ARCHIVE, NULL,
								"Show bonus flash on item pickup");
	cl_cshift_contents = Cvar_Get ("cl_cshift_content", "1", CVAR_ARCHIVE,
								   NULL, "Shift view colors for contents "
								   "(water, slime, etc)");
	cl_cshift_damage = Cvar_Get ("cl_cshift_damage", "1", CVAR_ARCHIVE, NULL,
								 "Shift view colors on damage");
	cl_cshift_powerup = Cvar_Get ("cl_cshift_powerup", "1", CVAR_ARCHIVE, NULL,
								  "Shift view colors for powerups");
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL,
								 "turn `run' speed multiplier");
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL,
							 "backward speed");
	cl_fb_players = Cvar_Get ("cl_fb_players", "0", CVAR_ARCHIVE, NULL, "fullbrightness of player models. "
							"server must allow (via fbskins serverinfo).");
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, NULL,
								"forward speed");
	cl_movespeedkey = Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, NULL,
								"move `run' speed multiplier");
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, NULL,
							  "look up/down speed");
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, NULL,
							 "strafe speed");
	cl_upspeed = Cvar_Get ("cl_upspeed", "200", CVAR_NONE, NULL,
						   "swim/fly up/down speed");
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, NULL,
							"turning speed");
	cl_writecfg = Cvar_Get ("cl_writecfg", "1", CVAR_NONE, NULL,
							"write config files?");
	cl_draw_locs = Cvar_Get ("cl_draw_locs", "0", CVAR_NONE, NULL,
						   "Draw location markers.");
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL,
						   "show network packets. 0=off, 1=basic, 2=verbose");
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL,
						  "maximum frames rendered in one second. 0 == 72");
	cl_timeout = Cvar_Get ("cl_timeout", "60", CVAR_ARCHIVE, NULL, "server "
						   "connection timeout (since last packet received)");
	host_speeds = Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL,
							"display host processing times");
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL, "Snap view "
						   "to center when moving and no mlook/klook");
	m_forward = Cvar_Get ("m_forward", "1", CVAR_ARCHIVE, NULL,
						  "mouse forward/back speed");
	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL,
						"mouse pitch (up/down) multipier");
	m_side = Cvar_Get ("m_side", "0.8", CVAR_ARCHIVE, NULL,
					   "mouse strafe speed");
	m_yaw = Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE, NULL,
					  "mouse yaw (left/right) multiplier");
	rcon_password = Cvar_Get ("rcon_password", "", CVAR_NONE, NULL,
							  "remote control password");
	rcon_address = Cvar_Get ("rcon_address", "", CVAR_NONE, NULL, "server IP "
							 "address when client not connected - for "
							 "sending rcon commands");
	hud_fps = Cvar_Get ("hud_fps", "0", CVAR_ARCHIVE, NULL,
						"display realtime frames per second");
	Cvar_MakeAlias ("show_fps", hud_fps);
	hud_ping = Cvar_Get ("hud_ping", "0", CVAR_ARCHIVE, NULL,
						 "display current ping to server");
	hud_pl = Cvar_Get ("hud_pl", "0", CVAR_ARCHIVE, NULL,
					   "display current packet loss to server");
	hud_time = Cvar_Get ("hud_time", "0", CVAR_ARCHIVE, NULL,
						 "Display the current time, 1 24hr, 2 AM/PM");
	cl_predict_players = Cvar_Get ("cl_predict_players", "1", CVAR_NONE, NULL,
								   "If this is 0, no player prediction is "
								   "done");
	cl_solid_players = Cvar_Get ("cl_solid_players", "1", CVAR_NONE, NULL,
								 "Are players solid? If off, you can walk "
								 "through them with difficulty");
	localid = Cvar_Get ("localid", "", CVAR_NONE, NULL, "Used by "
								 "gamespy+others to authenticate when sending "
								 "commands to the client");
	// info mirrors
	cl_name = Cvar_Get ("name", "unnamed", CVAR_ARCHIVE | CVAR_USERINFO,
						Cvar_Info, "Player name");
	password = Cvar_Get ("password", "", CVAR_USERINFO, Cvar_Info,
						 "Server password");
	spectator = Cvar_Get ("spectator", "", CVAR_USERINFO, Cvar_Info,
						  "Set to 1 before connecting to become a spectator");
	team = Cvar_Get ("team", "", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					 "Team player is on.");
	rate = Cvar_Get ("rate", "10000", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					 "Amount of bytes per second server will send/download "
					 "to you");
	msg = Cvar_Get ("msg", "1", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					"Determines the type of messages reported 0 is maximum, "
					"4 is none");
	noaim = Cvar_Get ("noaim", "0", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					  "Auto aim off switch. Set to 1 to turn off.");
	cl_port = Cvar_Get ("cl_port", PORT_CLIENT, CVAR_NONE, Cvar_Info,
						"UDP Port for client to use.");
	cl_usleep = Cvar_Get ("cl_usleep", "1", CVAR_ARCHIVE, cl_usleep_f,
						  "Turn this on to save cpu when fps limited. "
						  "May affect frame rate adversely depending on "
						  "local machine/os conditions");
	cl_autorecord = Cvar_Get ("cl_autorecord", "0", CVAR_ARCHIVE, NULL, "Turn "
							  "this on, if you want to record every game");
}

/*
	Host_EndGame

	Call this to drop to a console without exiting the qwcl
*/
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
	Sys_Printf ("\n===========================\n");
	Sys_Printf ("Host_EndGame: %s\n", str->str);
	Sys_Printf ("===========================\n\n");

	CL_Disconnect ();

	longjmp (host_abort, 1);
}

/*
	Host_Error

	This shuts down the client and exits qwcl
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

	va_start (argptr, error);
	dvsprintf (str, error, argptr);
	va_end (argptr);

	Sys_Printf ("Host_Error: %s", str->str);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	if (host_initialized) {
		longjmp (host_abort, 1);
	} else {
		Sys_Error ("Host_Error: %s", str->str);
	}
}

/*
	Host_WriteConfiguration

	Writes key bindings and archived cvars to config.cfg
*/
void
Host_WriteConfiguration (void)
{
	QFile      *f;

	if (host_initialized && cl_writecfg->int_val) {
		const char *path = va (0, "%s/config.cfg", qfs_gamedir->dir.def);

		f = QFS_WOpen (path, 0);
		if (!f) {
			Sys_Printf ("Couldn't write config.cfg.\n");
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f);

		Qclose (f);
	}
}

/*
	Host_SimulationTime

	This determines if enough time has passed to run a simulation frame, or
	returns the amount of time that has to be waited
*/
static inline float
Host_SimulationTime (float time)
{
	float       fps, timedifference;
	float		timescale = 1.0;

	con_realtime += time;

	if (cls.demoplayback) {
		timescale = max (0, demo_speed->value);
		time *= timescale;
	}

	realtime += time;
	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cls.demoplayback)
		return 0;

	if (cl_maxfps->value <= 0)
		fps = 72;
	else
		fps = min (cl_maxfps->value, 72);

	timedifference = (timescale / fps) - (realtime - oldrealtime);

	if (timedifference > 0)
		return timedifference;					// framerate is too high
	return 0;
}

int         nopacketcount;

/*
	Host_Frame

	Runs all active servers
*/
void
Host_Frame (float time)
{
	static double	time1 = 0;
	static double	time2 = 0;
	static double	time3 = 0;
	float			sleeptime;
	int				pass1, pass2, pass3;

	if (setjmp (host_abort))
		// something bad happened, or the server disconnected
		return;

	if (cls.demo_capture)
		time = 1.0 / 30;		//FIXME fixed 30fps atm

	// decide the simulation time
	if ((sleeptime = Host_SimulationTime (time)) != 0) {
#ifdef HAVE_USLEEP
		if (cl_usleep_cache && sleeptime > 0.002) // minimum sleep time
			usleep ((unsigned long) (sleeptime * 1000000 / 2));
#endif
		return;					// framerate is too high
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
	host_frametime = min (host_frametime, 0.2);

	cl.viewstate.frametime = host_frametime;

	con_frametime = con_realtime - oldcon_realtime;
	oldcon_realtime = con_realtime;

	IN_ProcessEvents ();

	// process gib threads

	GIB_Thread_Execute ();

	// process console commands
	Cbuf_Execute_Stack (cl_cbuf);
	Cbuf_Execute_Stack (cl_stbuf);

	// fetch results from server
	CL_ReadPackets ();
	if (*cls.downloadurl->str)
		CL_HTTP_Update ();

	if (cls.demoplayback2) {
		player_state_t *self, *oldself;

		self = &cl.frames[cl.parsecount
						  & UPDATE_MASK].playerstate[cl.playernum];
		oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1)
							 & UPDATE_MASK].playerstate[cl.playernum];
		self->messagenum = cl.parsecount;
		VectorCopy (oldself->pls.es.origin, self->pls.es.origin);
		VectorCopy (oldself->pls.es.velocity, self->pls.es.velocity);
		VectorCopy (oldself->viewangles, self->viewangles);

		CL_ParseClientdata ();

		cls.netchan.outgoing_sequence = cl.parsecount + 1;
	}

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected) {
		CL_CheckForResend ();
	} else {
		CL_SendCmd ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction (false);

		// do client side motion prediction
		CL_PredictMove ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction (cl_predict_players->int_val);

		// build a refresh entity list
		CL_EmitEntities ();
	}

	// update video
	if (host_speeds->int_val)
		time1 = Sys_DoubleTime ();

	r_data->inhibit_viewmodel = (!Cam_DrawViewModel ()
								 || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
								 || cl.stats[STAT_HEALTH] <= 0);
	r_data->frametime = host_frametime;

	CL_UpdateScreen (realtime);

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

static int
check_quakerc (void)
{
	const char *l, *p;
	int ret = 1;
	QFile *f;

	f = QFS_FOpenFile ("quake.rc");
	if (!f)
		return 1;
	while ((l = Qgetline (f))) {
		if ((p = strstr (l, "stuffcmds"))) {
			if (p == l) {				// only known case so far
				ret = 0;
				break;
			}
		}
	}
	Qclose (f);
	return ret;
}

static void
CL_Init_Memory (void)
{
	int         mem_parm = COM_CheckParm ("-mem");
	int         mem_size;
	void       *mem_base;

	cl_mem_size = Cvar_Get ("cl_mem_size", "32", CVAR_NONE, NULL,
							"Amount of memory (in MB) to allocate for the "
							PACKAGE_NAME " heap");
	if (mem_parm)
		Cvar_Set (cl_mem_size, com_argv[mem_parm + 1]);

	if (COM_CheckParm ("-minmemory"))
		Cvar_SetValue (cl_mem_size, MINIMUM_MEMORY / (1024 * 1024.0));

	Cvar_SetFlags (cl_mem_size, cl_mem_size->flags | CVAR_ROM);

	mem_size = (int) (cl_mem_size->value * 1024 * 1024);

	if (mem_size < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   mem_size / (float) 0x100000);

	mem_base = Sys_Alloc (mem_size);

	if (!mem_base)
		Sys_Error ("Can't allocate %d", mem_size);

	Sys_PageIn (mem_base, mem_size);
	Memory_Init (mem_base, mem_size);

	Sys_Printf ("%4.1f megabyte heap.\n", cl_mem_size->value);
}

static void
CL_Autoexec (int phase)
{
	int         cmd_warncmd_val = cmd_warncmd->int_val;

	if (!phase)
		return;
	Cbuf_AddText (cl_cbuf, "cmd_warncmd 0\n");
	Cbuf_AddText (cl_cbuf, "exec config.cfg\n");
	Cbuf_AddText (cl_cbuf, "exec frontend.cfg\n");

	if (cl_autoexec->int_val) {
		Cbuf_AddText (cl_cbuf, "exec autoexec.cfg\n");
	}

	Cbuf_AddText (cl_cbuf, va (0, "cmd_warncmd %d\n", cmd_warncmd_val));
}

void
Host_Init (void)
{
	cl_cbuf = Cbuf_New (&id_interp);
	cl_stbuf = Cbuf_New (&id_interp);

	Sys_Init ();
	GIB_Init (true);
	COM_ParseConfig ();

	CL_Init_Memory ();

	pr_gametype = "quakeworld";

	QFS_Init ("qw");
	QFS_GamedirCallback (CL_Autoexec);
	PI_Init ();

	Netchan_Init_Cvars ();

	PR_Init_Cvars ();		// FIXME location

	CL_Init_Cvars ();

	CL_Chat_Init ();

	CL_Cmd_Init ();
	Game_Init ();

	NET_Init (cl_port->int_val);
	Netchan_Init ();
	net_realtime = &realtime;
	{
		static const char *sound_precache[MAX_MODELS];
		int i;

		for (i = 0; i < MAX_MODELS; i++)
			sound_precache[i] = cl.sound_name[i];
		Net_Log_Init (sound_precache);
	}
	CL_HTTP_Init ();

	CL_Demo_Init ();

	CL_Init ();

	CL_UpdateScreen (realtime);
	CL_UpdateScreen (realtime);

	if (cl_quakerc->int_val)
		Cbuf_InsertText (cl_cbuf, "exec quake.rc\n");
	Cmd_Exec_File (cl_cbuf, fs_usercfg->string, 0);
	// Reparse the command line for + commands.
	// (Note, no non-base commands exist yet)
	if (!cl_quakerc->int_val || check_quakerc ())
		Cmd_StuffCmds (cl_cbuf);

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	Sys_Printf ("\nClient version %s (build %04d)\n\n", PACKAGE_VERSION,
				build_number ());
	Sys_Printf ("\x80\x81\x81\x82 %s initialized \x80\x81\x81\x82\n",
				PACKAGE_NAME);

	// make sure all + commands have been executed
	Cbuf_Execute_Stack (cl_cbuf);

	host_initialized = true;

	CL_UpdateScreen (realtime);
	Con_NewMap ();							// force the menus to be loaded
	CL_UpdateScreen (realtime);
	CL_UpdateScreen (realtime);

	if (connect_time == -1) {
		Cbuf_AddText (cl_cbuf, "echo Type connect <internet address> or use a "
					  "server browser to connect to a game.\n");
	}
	Cbuf_AddText (cl_cbuf, "set cmd_warncmd 1\n");
}

void
Host_Shutdown (void *data)
{
	static qboolean isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	SL_Shutdown ();

	Host_WriteConfiguration ();

	CL_HTTP_Shutdown ();
}
