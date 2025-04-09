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

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <ctype.h>
#include <errno.h>

#include "qfalloca.h"

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/png.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/ruamoko.h"
#include "QF/plist.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/gib.h"

#include "QF/plugin/console.h"
#include "QF/scene/light.h"
#include "QF/scene/transform.h"
#include "QF/ui/font.h"//FIXME

#include "buildnum.h"
#include "compat.h"
#include "csqc.h"

#include "client/effects.h"
#include "client/particles.h"
#include "client/hud.h"
#include "client/screen.h"
#include "client/temp_entities.h"
#include "client/view.h"
#include "client/world.h"

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

bool        noclip_anglehack;			// remnant from old quake

cbuf_t     *cl_cbuf;
cbuf_t     *cl_stbuf;

float cl_mem_size;
static cvar_t cl_mem_size_cvar = {
	.name = "cl_mem_size",
	.description =
		"Amount of memory (in MB) to allocate for the "
		PACKAGE_NAME
		" heap",
	.default_value = "32",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_float, .value = &cl_mem_size },
};

char *rcon_password;
static cvar_t rcon_password_cvar = {
	.name = "rcon_password",
	.description =
		"Set the password for rcon 'root' commands",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &rcon_password },
};

char *rcon_address;
static cvar_t rcon_address_cvar = {
	.name = "rcon_address",
	.description =
		"server IP address when client not connected - for sending rcon "
		"commands",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &rcon_address },
};

int cl_writecfg;
static cvar_t cl_writecfg_cvar = {
	.name = "cl_writecfg",
	.description =
		"write config files?",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_writecfg },
};
int cl_allow_cmd_pkt;
static cvar_t cl_allow_cmd_pkt_cvar = {
	.name = "cl_allow_cmd_pkt",
	.description =
		"enables packets from the likes of gamespy",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_allow_cmd_pkt },
};
char *cl_cmd_pkt_adr;
static cvar_t cl_cmd_pkt_adr_cvar = {
	.name = "cl_cmd_pkt_adr",
	.description =
		"allowed address for non-local command packet",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &cl_cmd_pkt_adr },
};
int cl_paranoid;
static cvar_t cl_paranoid_cvar = {
	.name = "cl_paranoid",
	.description =
		"print source address of connectionless packets even when coming from "
		"the server being connected to.",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_paranoid },
};

float cl_timeout;
static cvar_t cl_timeout_cvar = {
	.name = "cl_timeout",
	.description =
		"server connection timeout (since last packet received)",
	.default_value = "60",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &cl_timeout },
};

int cl_draw_locs;
static cvar_t cl_draw_locs_cvar = {
	.name = "cl_draw_locs",
	.description =
		"Draw location markers.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_draw_locs },
};
int cl_shownet;
static cvar_t cl_shownet_cvar = {
	.name = "cl_shownet",
	.description =
		"show network packets. 0=off, 1=basic, 2=verbose",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_shownet },
};
int cl_player_shadows;
static cvar_t cl_player_shadows_cvar = {
	.name = "cl_player_shadows",
	.description =
		"Show player shadows instead of weapon shadows",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_player_shadows },
};
int cl_autoexec;
static cvar_t cl_autoexec_cvar = {
	.name = "cl_autoexec",
	.description =
		"exec autoexec.cfg on gamedir change",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &cl_autoexec },
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
float cl_maxfps;
static cvar_t cl_maxfps_cvar = {
	.name = "cl_maxfps",
	.description =
		"maximum frames rendered in one second. 0 == 72",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &cl_maxfps },
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

int cl_model_crcs;
static cvar_t cl_model_crcs_cvar = {
	.name = "cl_model_crcs",
	.description =
		"Controls setting of emodel and pmodel info vars. Required by some "
		"servers, but clearing this can make the difference between connecting"
		" and not connecting on some others.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_model_crcs },
};

int cl_predict_players;
static cvar_t cl_predict_players_cvar = {
	.name = "cl_predict_players",
	.description =
		"If this is 0, no player prediction is done",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_predict_players },
};
int cl_solid_players;
static cvar_t cl_solid_players_cvar = {
	.name = "cl_solid_players",
	.description =
		"Are players solid? If off, you can walk through them with difficulty",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_solid_players },
};

char *localid;
static cvar_t localid_cvar = {
	.name = "localid",
	.description =
		"Used by gamespy+others to authenticate when sending commands to the "
		"client",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &localid },
};

int cl_port;
static cvar_t cl_port_cvar = {
	.name = "cl_port",
	.description =
		"UDP Port for client to use.",
	.default_value = PORT_CLIENT,
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_port },
};
int cl_autorecord;
static cvar_t cl_autorecord_cvar = {
	.name = "cl_autorecord",
	.description =
		"Turn this on, if you want to record every game",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_autorecord },
};

float cl_fb_players;
static cvar_t cl_fb_players_cvar = {
	.name = "cl_fb_players",
	.description =
		"fullbrightness of player models. server must allow (via fbskins "
		"serverinfo).",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &cl_fb_players },
};

static bool allowremotecmd = true;

/*  info mirrors */
char *password;
static cvar_t password_cvar = {
	.name = "password",
	.description =
		"Set the server password for players",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &password },
};
int spectator;
static cvar_t spectator_cvar = {
	.name = "spectator",
	.description =
		"Set to 1 before connecting to become a spectator",
	.default_value = "",
	.flags = CVAR_USERINFO,
	.value = { .type = &cexpr_int, .value = &spectator },
};
char *cl_name;
static cvar_t cl_name_cvar = {
	.name = "_cl_name",
	.description =
		"Player name",
	.default_value = "player",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0, .value = &cl_name },
};
float team;
static cvar_t team_cvar = {
	.name = "team",
	.description =
		"Team player is on.",
	.default_value = "",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_float, .value = &team },
};
float rate;
static cvar_t rate_cvar = {
	.name = "rate",
	.description =
		"Amount of bytes per second server will send/download to you",
	.default_value = "10000",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_float, .value = &rate },
};
int noaim;
static cvar_t noaim_cvar = {
	.name = "noaim",
	.description =
		"Auto aim off switch. Set to 1 to turn off.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_int, .value = &noaim },
};
int msg;
static cvar_t msg_cvar = {
	.name = "msg",
	.description =
		"Determines the type of messages reported 0 is maximum, 4 is none",
	.default_value = "1",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_int, .value = &msg },
};

/* GIB events */
gib_event_t *cl_player_health_e, *cl_chat_e;

client_static_t cls;
client_state_t cl;

entity_state_t cl_baselines[MAX_EDICTS];

double      connect_time = -1;			// for connection retransmits

quakeparms_t host_parms;
bool        host_initialized;			// true if into command execution
bool        nomaster;

double      host_frametime;
double      realtime;					// without any filtering or bounding
double      oldrealtime;				// last frame run
int         host_framecount;

double      con_frametime;
double      con_realtime;
double      oldcon_realtime;

size_t      host_hunklevel;

int host_speeds;
static cvar_t host_speeds_cvar = {
	.name = "host_speeds",
	.description =
		"set for running times",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &host_speeds },
};

int         fps_count;

static sys_jmpbuf host_abort;

char       *server_version = NULL;		// version of server we connected to

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
pointfile_f (void)
{
	CL_LoadPointFile (cl_world.scene->worldmodel);
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

	cls.qport = qport;

	const char *data = va ("%c%c%c%cconnect %i %i %i \"%s\"\n",
						   255, 255, 255, 255, PROTOCOL_VERSION,
						   cls.qport, cls.challenge,
						   Info_MakeString (cls.userinfo, 0));
	Netchan_SendPacket (strlen (data), data, cls.server_addr);
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

	VID_SetCaption (va ("Connecting to %s", cls.servername->str));
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
	netadr_t    to;

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!rcon_address[0]) {
			Sys_Printf ("You must either be connected, or set the "
						"'rcon_address' cvar to issue rcon commands\n");
			return;
		}
		NET_StringToAdr (rcon_address, &to);
		if (to.port == 0)
			to.port = BigShort (27500);
	}


	const char *message;
	message = va ("\377\377\377\377rcon %s %s", rcon_password, Cmd_Args (1));
	Netchan_SendPacket (strlen (message) + 1, message, to);
}

void
CL_ClearState (void)
{
	int         i;

	S_StopAllSounds ();

	if (Entity_Valid (cl.viewstate.weapon_entity)) {
		Scene_DestroyEntity (cl_world.scene, cl.viewstate.weapon_entity);
	}
	// wipe the entire cl structure
	if (cl.serverinfo)
		Info_Destroy (cl.serverinfo);
	__auto_type players = cl.players;
	memset (&cl, 0, sizeof (cl));
	cl.players = players;
	SCR_SetFullscreen (0);

	cl.maxclients = MAX_CLIENTS;
	cl.viewstate.voffs_enabled = 0;
	cl.viewstate.chasestate = &cl.chasestate;
	cl.chasestate.viewstate = &cl.viewstate;
	cl.viewstate.punchangle = (vec4f_t) {0, 0, 0, 1};

	// Note: we should probably hack around this and give diff values for
	// diff gamedirs
	cl.fpd = FPD_DEFAULT;
	cl.fbskins = FBSKINS_DEFAULT;

	for (i = 0; i < UPDATE_BACKUP; i++)
		cl.frames[i].packet_entities.entities = qw_entstates.frame[i];
	cl.serverinfo = Info_ParseString ("", MAX_INFO_STRING, 0);

	Sys_MaskPrintf (SYS_dev, "Clearing memory\n");
	VID_ClearMemory ();
	Mod_ClearAll ();
	if (host_hunklevel)					// FIXME: check this...
		Hunk_FreeToLowMark (0, host_hunklevel);

	CL_World_Clear ();
	CL_ClearTEnts ();

	cl.viewstate.weapon_entity = Scene_CreateEntity (cl_world.scene);
	CL_Init_Entity (cl.viewstate.weapon_entity);
	auto renderer = Entity_GetRenderer (cl.viewstate.weapon_entity);
	renderer->depthhack = 1;
	renderer->noshadows = cl_player_shadows;
	r_data->view_model = cl.viewstate.weapon_entity;

	CL_TEnts_Precache ();

	SCR_NewScene (0);

	SZ_Clear (&cls.netchan.message);

	Sbar_CenterPrint (0);
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
		cl.viewstate.cshifts[i].percent = 0;
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
	cl_world.scene->worldmodel = NULL;
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

	Sys_MaskPrintf (SYS_dev, "Cmd_Argv (1): '%s'\n", Cmd_Argv (1));
	Info_Destroy (cl.serverinfo);
	cl.serverinfo = Info_ParseString (Cmd_Argv (1), MAX_SERVERINFO_STRING, 0);
	Sys_MaskPrintf (SYS_dev, "cl.serverinfo: '%s'\n",
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

	cl.viewstate.chase = cl.sv_cshifts = cl.no_pogo_stick = cl.teamplay = 0;
	if ((p = Info_ValueForKey (cl.serverinfo, "chase")) && *p) {
		cl.viewstate.chase = atoi (p);
	}
	cl.viewstate.chase |= cls.demoplayback;
	if ((p = Info_ValueForKey (cl.serverinfo, "cshifts")) && *p) {
		cl.sv_cshifts = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "no_pogo_stick")) && *p) {
		cl.no_pogo_stick = atoi (p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "teamplay")) && *p) {
		cl.teamplay = atoi (p);
		Sbar_SetTeamplay (cl.teamplay);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "watervis")) && *p) {
		cl.viewstate.watervis = atoi (p);
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

	Cbuf_InsertText (cl_cbuf, va ("playdemo %s\n", cls.demos[cls.demonum]));
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
	cl.viewstate.intermission = 0;
	SCR_SetFullscreen (0);
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
		&& (cl_paranoid
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

		if (!cl_allow_cmd_pkt
			|| (!NET_CompareBaseAdr (net_from, net_local_adr)
				&& !NET_CompareBaseAdr (net_from, net_loopback_adr)
				&& (!cl_cmd_pkt_adr[0]
					|| !NET_CompareBaseAdr (net_from,
											cl_cmd_packet_address)))) {
			Sys_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		if (cl_cmd_pkt_adr[0]
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

		if (!allowremotecmd && (!*localid ||
								(int) strlen (localid) > len ||
								strncmp (localid, s, len))) {
			if (!*localid) {
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
				 localid, PACKAGE_NAME);
			Sys_Printf ("===========================\n");
			Cvar_Set ("localid", "");
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
				   && (cl_paranoid
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

		// non-packet set up by the demo reader
		if ((int) net_message->message->cursize == -1) {
			continue;
		}

		if (cls.demoplayback && net_packetlog)
			Log_Incoming_Packet (net_message->message->data,
								 net_message->message->cursize, 0);

		// remote command packet
		if (*(int *) net_message->message->data == -1) {
			CL_ConnectionlessPacket ();
			continue;
		}
		if (net_message->message->cursize == 1
			&& *(char *) net_message->message->data == A2A_ACK) {
			Sys_Printf ("ping ack\n");
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
			Sys_MaskPrintf (SYS_dev,
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
		&& realtime - cls.netchan.last_received > cl_timeout) {
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
		SZ_Print (&cls.netchan.message, va ("download %s\n", Cmd_Argv (1)));
	} else {
		Sys_Printf ("error downloading %s: %s\n", Cmd_Argv (1),
					strerror (errno));
	}
}

static void
Force_CenterView_f (void)
{
	cl.viewstate.player_angles[PITCH] = 0;
}

static void
CL_PRotate_f (void)
{
	if ((cl.fpd & FPD_LIMIT_PITCH) || Cmd_Argc() < 2)
		return;

	cl.viewstate.player_angles[PITCH] += atoi (Cmd_Argv (1));

	if (cl.viewstate.player_angles[PITCH] < -70)
		cl.viewstate.player_angles[PITCH] = -70;
	else if (cl.viewstate.player_angles[PITCH] > 80)
		cl.viewstate.player_angles[PITCH] = 80;
}

static void
CL_Rotate_f (void)
{
	if ((cl.fpd & FPD_LIMIT_YAW) || Cmd_Argc() < 2)
		return;

	cl.viewstate.player_angles[YAW] += atoi (Cmd_Argv (1));
	cl.viewstate.player_angles[YAW] = anglemod(cl.viewstate.player_angles[YAW]);
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

	Sys_MaskPrintf (SYS_dev, "CL_SetState (%s)\n", state_names[state]);
	cls.state = state;
	cl.viewstate.active = cls.state == ca_active;
	cl.viewstate.drift_enabled = !cls.demoplayback;
	if (old_state != state) {
		if (old_state == ca_active) {
			// leaving active state
			IN_ClearStates ();
			CL_ClearState ();

			// Auto demo recorder stops here
			if (cl_autorecord && cls.demorecording)
				CL_StopRecording ();

			SCR_NewScene (0);
		} else if (state == ca_active) {
			// entering active state
			VID_SetCaption (cls.servername->str);
			IN_ClearStates ();

			// Auto demo recorder starts here
			if (cl_autorecord && !cls.demoplayback
				&& !cls.demorecording)
				CL_Record (0, -1);
		}
		Sbar_SetActive (state == ca_active);
	}
	Con_SetState (state == ca_active ? con_inactive : con_fullscreen,
				  state == ca_active && !cls.demoplayback);
	if (state != old_state && state == ca_active) {
		CL_Input_Activate (!cls.demoplayback);
	}
}

static void
CL_Shutdown (void *data)
{
	static bool isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	SL_Shutdown ();

	Host_WriteConfiguration ();

	CL_HTTP_Shutdown ();

	if (cl.serverinfo) {
		Info_Destroy (cl.serverinfo);
	}
	Info_Destroy (cls.userinfo);
	Cbuf_DeleteStack (cl_stbuf);
	dstring_delete (cls.servername);
	dstring_delete (cls.downloadtempname);
	dstring_delete (cls.downloadname);
	dstring_delete (cls.downloadurl);
	free (cl.players);

	Mod_ClearAll ();
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
	IN_Init ();
	Mod_Init ();
	R_Init (nullptr);
	r_data->lightstyle = cl.lightstyle;
	Font_Init ();	//FIXME not here

	PI_RegisterPlugins (client_plugin_list);
	CL_Init_Screen ();
	if (con_module) {
		con_module->data->console->dl_name = cls.downloadname;
		con_module->data->console->dl_percent = &cls.downloadpercent;
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = CL_Quit_f;
		//FIXME need to rethink cbuf connections (they can form a stack)
		Cbuf_DeleteStack (con_module->data->console->cbuf);
		con_module->data->console->cbuf = cl_cbuf;
	}
	Con_Init ();
	CL_NetGraph_Init ();

	S_Init (&cl.viewentity, &host_frametime);
	CDAudio_Init ();

	Sbar_Init (cl.stats, cl.item_gettime);

	CL_Init_Input (cl_cbuf);
	CL_Ents_Init ();
	CL_Particles_Init ();
	CL_Effects_Init ();
	CL_TEnts_Init ();
	CL_World_Init ();
	CL_ClearState ();
	Pmove_Init ();

	VID_SendSize ();

	SL_Init ();

	CL_Skin_Init ();
	Locs_Init ();
	V_Init (&cl.viewstate);

	Sys_RegisterShutdown (CL_Shutdown, 0);

	CSQC_Cmds_Init ();

	Info_SetValueForStarKey (cls.userinfo, "*ver", QW_VERSION, 0);

	cls.servername = dstring_newstr ();
	cls.downloadtempname = dstring_newstr ();
	cls.downloadname = dstring_newstr ();
	cls.downloadurl = dstring_newstr ();
	Info_Destroy (cl.serverinfo);
	cl.serverinfo = Info_ParseString ("", MAX_INFO_STRING, 0);
	free (cl.players);
	cl.players = calloc (MAX_CLIENTS, sizeof (player_info_t));
	Sbar_SetPlayers (cl.players, MAX_CLIENTS);

	// register our commands
	Cmd_AddCommand ("pointfile", pointfile_f,
					"Load a pointfile to determine map leaks.");
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
cl_cmd_pkt_adr_f (void *data, const cvar_t *cvar)
{
	if (cl_cmd_pkt_adr[0])
		NET_StringToAdr (cl_cmd_pkt_adr, &cl_cmd_packet_address);
	else
		memset (&cl_cmd_packet_address, 0, sizeof (cl_cmd_packet_address));
}

static void
cl_pitchspeed_f (void *data, const cvar_t *var)
{
	if ((cl.fpd & FPD_LIMIT_PITCH) && cl_pitchspeed > FPD_MAXPITCH) {
		cl_pitchspeed = FPD_MAXPITCH;
	}
}

static void
cl_yawspeed_f (void *data, const cvar_t *var)
{
	if ((cl.fpd & FPD_LIMIT_YAW) && cl_yawspeed > FPD_MAXYAW) {
		cl_yawspeed = FPD_MAXYAW;
	}
}


static void
CL_Init_Cvars (void)
{
	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	CL_Cam_Init_Cvars ();
	CL_Init_Input_Cvars ();
	CL_Prediction_Init_Cvars ();
	CL_NetGraph_Init_Cvars ();
	Game_Init_Cvars ();
	Pmove_Init_Cvars ();
	Team_Init_Cvars ();
	Chase_Init_Cvars ();
	V_Init_Cvars ();

	cvar_t *var;
	var = Cvar_FindVar ("cl_pitchspeed");
	Cvar_AddListener (var, cl_pitchspeed_f, 0);
	var = Cvar_FindVar ("cl_yawspeed");
	Cvar_AddListener (var, cl_yawspeed_f, 0);

	cls.userinfo = Info_ParseString ("", MAX_INFO_STRING, 0);

	Cvar_Register (&cl_model_crcs_cvar, 0, 0);
	Cvar_Register (&cl_allow_cmd_pkt_cvar, 0, 0);
	Cvar_Register (&cl_cmd_pkt_adr_cvar, cl_cmd_pkt_adr_f, 0);
	Cvar_Register (&cl_paranoid_cvar, 0, 0);
	Cvar_Register (&cl_autoexec_cvar, 0, 0);
	Cvar_Register (&cl_quakerc_cvar, 0, 0);
	Cvar_Register (&cl_fb_players_cvar, 0, 0);
	Cvar_Register (&cl_writecfg_cvar, 0, 0);
	Cvar_Register (&cl_draw_locs_cvar, 0, 0);
	Cvar_Register (&cl_shownet_cvar, 0, 0);
	Cvar_Register (&cl_player_shadows_cvar, 0, 0);
	Cvar_Register (&cl_maxfps_cvar, 0, 0);
	Cvar_Register (&cl_timeout_cvar, 0, 0);
	Cvar_Register (&host_speeds_cvar, 0, 0);
	Cvar_Register (&rcon_password_cvar, 0, 0);
	Cvar_Register (&rcon_address_cvar, 0, 0);
	Cvar_Register (&cl_predict_players_cvar, 0, 0);
	Cvar_Register (&cl_solid_players_cvar, 0, 0);
	Cvar_Register (&localid_cvar, 0, 0);
	// info mirrors
	Cvar_Register (&cl_name_cvar, 0, 0);
	Cvar_Register (&password_cvar, 0, 0);
	Cvar_Register (&spectator_cvar, Cvar_Info, &spectator);
	Cvar_Register (&team_cvar, Cvar_Info, &team);
	Cvar_Register (&rate_cvar, Cvar_Info, &rate);
	Cvar_Register (&msg_cvar, Cvar_Info, &msg);
	Cvar_Register (&noaim_cvar, Cvar_Info, &noaim);
	Cvar_Register (&cl_port_cvar, Cvar_Info, &cl_port);
	Cvar_Register (&cl_usleep_cvar, 0, 0);
	Cvar_Register (&cl_autorecord_cvar, 0, 0);
}

/*
	Host_EndGame

	Call this to drop to a console without exiting the qwcl
*/
void
Host_EndGame (const char *message, ...)
{
	va_list     argptr;

	dstring_t *str = dstring_new ();
	va_start (argptr, message);
	dvsprintf (str, message, argptr);
	va_end (argptr);
	Sys_Printf ("\n===========================\n");
	Sys_Printf ("Host_EndGame: %s\n", str->str);
	Sys_Printf ("===========================\n\n");
	dstring_delete (str);

	CL_Disconnect ();

	Sys_longjmp (host_abort);
}

/*
	Host_Error

	This shuts down the client and exits qwcl
*/
void
Host_Error (const char *error, ...)
{
	static bool inerror = false;
	va_list     argptr;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");

	inerror = true;

	dstring_t  *str = dstring_new ();
	va_start (argptr, error);
	dvsprintf (str, error, argptr);
	va_end (argptr);

	Sys_Printf ("Host_Error: %s", str->str);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	if (host_initialized) {
		dstring_delete (str);
		Sys_longjmp (host_abort);
	} else {
		Sys_Error ("Host_Error: %s", str->str);
	}
}

void
Host_WriteConfiguration (void)
{
	if (host_initialized && cl_writecfg) {
		plitem_t   *config = PL_NewDictionary (0);
		Cvar_SaveConfig (config);
		IN_SaveConfig (config);

		const char *path = va ("%s/quakeforge.cfg", qfs_gamedir->dir.def);
		QFile      *f = QFS_WOpen (path, 0);

		if (!f) {
			Sys_Printf ("Couldn't write quakeforge.cfg.\n");
		} else {
			char       *cfg = PL_WritePropertyList (config);
			Qputs (f, cfg);
			free (cfg);
			Qclose (f);
		}
		PL_Release (config);
	}
}

int
Host_ReadConfiguration (const char *cfg_name)
{
	QFile      *cfg_file = QFS_FOpenFile (cfg_name);
	if (!cfg_file) {
		return 0;
	}
	size_t      len = Qfilesize (cfg_file);
	char       *cfg = malloc (len + 1);
	Qread (cfg_file, cfg, len);
	cfg[len] = 0;
	Qclose (cfg_file);

	plitem_t   *config = PL_GetPropertyList (cfg, 0);
	free (cfg);

	if (!config) {
		return 0;
	}

	Cvar_LoadConfig (config);
	IN_LoadConfig (config);

	PL_Release (config);
	return 1;
}

static void
Host_ExecConfig (cbuf_t *cbuf, int skip_quakerc)
{
	// quakeforge.cfg overrides quake.rc as it contains quakeforge-specific
	// commands. If it doesn't exist, then this is the first time quakeforge
	// has been used in this installation, thus any existing legacy config
	// should be used to set up defaults on the assumption that the user has
	// things set up to work with another (hopefully compatible) client
	if (Host_ReadConfiguration ("quakeforge.cfg")) {
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
		timescale = max (0, demo_speed);
		time *= timescale;
	}

	realtime += time;
	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cls.demoplayback)
		return 0;

	if (cl_maxfps <= 0)
		fps = 72;
	else
		fps = min (cl_maxfps, 72);

	timedifference = (timescale / fps) - (realtime - oldrealtime);

	if (timedifference > 0)
		return timedifference;					// framerate is too high
	return 0;
}

static void
write_capture (tex_t *tex, void *data)
{
	QFile      *file = QFS_Open (va ("%s/qfmv%06d.png",
									 qfs_gamedir->dir.shots,
									 cls.demo_capture++), "wb");
	if (file) {
		WritePNG (file, tex);
		Qclose (file);
	}
	free (tex);
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

	if (Sys_setjmp (host_abort))
		// something bad happened, or the server disconnected
		return;

	if (cls.demo_capture)
		time = 1.0 / 30;		//FIXME fixed 30fps atm

	// decide the simulation time
	if ((sleeptime = Host_SimulationTime (time)) != 0) {
#ifdef HAVE_USLEEP
		if (cl_usleep && sleeptime > 0.002) // minimum sleep time
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
		CL_SetUpPlayerPrediction (cl_predict_players);

		// build a refresh entity list
		CL_EmitEntities ();
	}

	// update video
	if (host_speeds)
		time1 = Sys_DoubleTime ();

	r_data->inhibit_viewmodel = (!Cam_DrawViewModel ()
								 || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
								 || cl.stats[STAT_HEALTH] <= 0);
	r_data->frametime = host_frametime;

	cl.viewstate.time = realtime;
	cl.viewstate.realtime = realtime;
	if (!cls.demoplayback && cls.state == ca_active) {
		CL_NetUpdate ();
	}
	Sbar_Update (cl.time);
	cl_realtime = realtime;
	cl_frametime = host_frametime;
	CL_UpdateScreen (&cl.viewstate);

	if (host_speeds)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		mleaf_t    *l;
		byte       *asl = 0;
		vec4f_t     origin;

		origin = Transform_GetWorldPosition (cl.viewstate.camera_transform);
		l = Mod_PointInLeaf (origin, &cl_world.scene->worldmodel->brush);
		if (l)
			asl = l->ambient_sound_level;
		S_Update (cl.viewstate.camera_transform, asl);
		Light_DecayLights (cl_world.scene->lights, host_frametime, realtime);
	} else
		S_Update (nulltransform, 0);

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

	host_framecount++;
	fps_count++;
}

static memhunk_t *
CL_Init_Memory (void)
{
	int         mem_parm = COM_CheckParm ("-mem");
	size_t      mem_size;
	void       *mem_base;

	Cvar_Register (&cl_mem_size_cvar, 0, 0);
	if (mem_parm)
		Cvar_Set ("cl_mem_size", com_argv[mem_parm + 1]);

	if (COM_CheckParm ("-minmemory"))
		cl_mem_size = MINIMUM_MEMORY / (1024 * 1024.0);


	mem_size = ((size_t) cl_mem_size * 1024 * 1024);

	if (mem_size < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   mem_size / (float) 0x100000);

	mem_base = Sys_Alloc (mem_size);

	if (!mem_base)
		Sys_Error ("Can't allocate %zd", mem_size);

	Sys_PageIn (mem_base, mem_size);
	memhunk_t  *hunk = Memory_Init (mem_base, mem_size);

	Sys_Printf ("%4.1f megabyte heap.\n", cl_mem_size);
	return hunk;
}

static void
CL_Autoexec (int phase, void *data)
{
	if (!phase)
		return;
	if (!Host_ReadConfiguration ("quakeforge.cfg")) {
		int         cmd_warncmd_val = cmd_warncmd;

		Cbuf_AddText (cl_cbuf, "cmd_warncmd 0\n");
		Cbuf_AddText (cl_cbuf, "exec config.cfg\n");
		Cbuf_AddText (cl_cbuf, "exec frontend.cfg\n");

		Cbuf_AddText (cl_cbuf, va ("cmd_warncmd %d\n", cmd_warncmd_val));
	}

	if (cl_autoexec) {
		Cbuf_AddText (cl_cbuf, "exec autoexec.cfg\n");
	}
}

void
Host_Init (void)
{
	cl_cbuf = Cbuf_New (&id_interp);
	cl_stbuf = Cbuf_New (&id_interp);

	Sys_Init ();
	GIB_Init (true);
	GIB_Key_Init ();
	COM_ParseConfig (cl_cbuf);

	memhunk_t  *hunk = CL_Init_Memory ();

	pr_gametype = "quakeworld";

	QFS_Init (hunk, "qw");
	QFS_GamedirCallback (CL_Autoexec, 0);
	PI_Init ();

	Sys_RegisterShutdown (Net_LogStop, 0);

	Netchan_Init_Cvars ();

	PR_Init_Cvars ();		// FIXME location

	CL_Init_Cvars ();

	CL_Chat_Init ();

	CL_Cmd_Init ();
	Game_Init ();

	NET_Init (cl_port);
	Netchan_Init ();
	net_realtime = &realtime;
	{
		static const char *sound_precache[MAX_MODELS];
		int i;

		for (i = 0; i < MAX_MODELS; i++)
			sound_precache[i] = cl.sound_name[i];
		Net_Log_Init (sound_precache, 0);
	}
	CL_HTTP_Init ();

	CL_Demo_Init ();

	CL_Init ();

	cl.viewstate.time = realtime;
	cl.viewstate.realtime = realtime;
	cl_realtime = realtime;
	cl_frametime = host_frametime;
	CL_UpdateScreen (&cl.viewstate);
	CL_UpdateScreen (&cl.viewstate);

	Host_ExecConfig (cl_cbuf, !cl_quakerc);

	// make sure all + commands have been executed
	Cbuf_Execute_Stack (cl_cbuf);

	Hunk_AllocName (0, 0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark (0);

	Sys_Printf ("\nClient version %s (build %04d)\n\n", PACKAGE_VERSION,
				build_number ());
	Sys_Printf ("%c\x80\x81\x81\x82 %s initialized \x80\x81\x81\x82\n", 3,
				PACKAGE_NAME);

	host_initialized = true;

	CL_UpdateScreen (&cl.viewstate);
	Con_NewMap ();							// force the menus to be loaded
	CL_UpdateScreen (&cl.viewstate);
	CL_UpdateScreen (&cl.viewstate);

	if (connect_time == -1) {
		Cbuf_AddText (cl_cbuf, "echo Type connect <internet address> or use a "
					  "server browser to connect to a game.\n");
	}
	Cbuf_AddText (cl_cbuf, "set cmd_warncmd 1\n");
}
