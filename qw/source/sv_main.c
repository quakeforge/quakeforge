/*
	sv_main.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_WINDOWS_H
# include "winquake.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
# define INET6
# define model_t sun_model_t
# include <netinet/in.h>
# undef model_t
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_WINSOCK_H
# include <winsock.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>

#include "QF/cbuf.h"
#include "QF/idparse.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/ver_check.h"
#include "QF/zone.h"

#include "QF/plugin/console.h"

#include "buildnum.h"
#include "compat.h"
#include "netchan.h"

#include "qw/bothdefs.h"
#include "qw/include/crudefile.h"
#include "qw/include/game.h"
#include "qw/pmove.h"
#include "qw/include/server.h"
#include "qw/include/sv_demo.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_gib.h"
#include "qw/include/sv_qtv.h"
#include "qw/include/sv_recorder.h"

SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};

cbuf_t     *sv_cbuf;
cbuf_args_t *sv_args;

client_t   *host_client;				// current client
entity_state_t cl_entities[MAX_CLIENTS][UPDATE_BACKUP+1][MAX_PACKET_ENTITIES]; // client entities

double      sv_frametime;
double      realtime;					// without any filtering or bounding

size_t      host_hunklevel;

netadr_t    master_adr[MAX_MASTERS];	// address of group servers

quakeparms_t host_parms;

qboolean    host_initialized;			// true if into command execution
qboolean    rcon_from_user;

// DoS protection
// FLOOD_PING, FLOOD_LOG, FLOOD_CONNECT, FLOOD_STATUS, FLOOD_RCON, FLOOD_BAN
// FIXME: these default values need to be tweaked after more testing

double      netdosexpire[DOSFLOODCMDS] = { 1, 1, 2, 0.9, 1, 5 };
double      netdosvalues[DOSFLOODCMDS] = { 12, 1, 3, 1, 1, 1 };

float sv_mem_size;
static cvar_t sv_mem_size_cvar = {
	.name = "sv_mem_size",
	.description =
		"Amount of memory (in MB) to allocate for the "
		PACKAGE_NAME
		" heap",
	.default_value = "8",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_float, .value = &sv_mem_size },
};

char *sv_console_plugin;
static cvar_t sv_console_plugin_cvar = {
	.name = "sv_console_plugin",
	.description =
		"Plugin used for the console",
	.default_value = "server",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &sv_console_plugin },
};

int sv_allow_status;
static cvar_t sv_allow_status_cvar = {
	.name = "sv_allow_status",
	.description =
		"Allow remote status queries (qstat etc)",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_allow_status },
};
int sv_allow_log;
static cvar_t sv_allow_log_cvar = {
	.name = "sv_allow_log",
	.description =
		"Allow remote logging",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_allow_log },
};
int sv_allow_ping;
static cvar_t sv_allow_ping_cvar = {
	.name = "sv_allow_pings",
	.description =
		"Allow remote pings (qstat etc)",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_allow_ping },
};

int sv_extensions;
static cvar_t sv_extensions_cvar = {
	.name = "sv_extensions",
	.description =
		"Use protocol extensions for QuakeForge clients",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_extensions },
};

float sv_mintic;
static cvar_t sv_mintic_cvar = {
	.name = "sv_mintic",
	.description =
		"The minimum amount of time the server will wait before sending "
		"packets to a client. Set to .5 to make modem users happy",
	.default_value = "0.03",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_mintic },
};
float sv_maxtic;
static cvar_t sv_maxtic_cvar = {
	.name = "sv_maxtic",
	.description =
		"The maximum amount of time in seconds before a client a receives an "
		"update from the server",
	.default_value = "0.1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_maxtic },
};

int sv_netdosprotect;
static cvar_t sv_netdosprotect_cvar = {
	.name = "sv_netdosprotect",
	.description =
		"DoS flood attack protection",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_netdosprotect },
};

float sv_timeout;
static cvar_t sv_timeout_cvar = {
	.name = "timeout",
	.description =
		"Sets the amount of time in seconds before a client is considered "
		"disconnected if the server does not receive a packet",
	.default_value = "65",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_timeout },
};
float zombietime;
static cvar_t zombietime_cvar = {
	.name = "zombietime",
	.description =
		"The number of seconds that the server will keep the character of a "
		"player on the map who seems to have disconnected",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &zombietime },
};
										// disconnect

char *rcon_password;
static cvar_t rcon_password_cvar = {
	.name = "rcon_password",
	.description =
		"Set the password for rcon 'root' commands",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &rcon_password },
};
char *admin_password;
static cvar_t admin_password_cvar = {
	.name = "admin_password",
	.description =
		"Set the password for rcon admin commands",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0/* not used */, .value = &admin_password },
};

char *password;
static cvar_t password_cvar = {
	.name = "password",
	.description =
		"Set the server password for players",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &password },
};
char *spectator_password;
static cvar_t spectator_password_cvar = {
	.name = "spectator_password",
	.description =
		"Set the spectator password",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &spectator_password },
};
										// spectator

int allow_download;
static cvar_t allow_download_cvar = {
	.name = "allow_download",
	.description =
		"Toggle if clients can download game data from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download },
};
int allow_download_skins;
static cvar_t allow_download_skins_cvar = {
	.name = "allow_download_skins",
	.description =
		"Toggle if clients can download skins from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download_skins },
};
int allow_download_models;
static cvar_t allow_download_models_cvar = {
	.name = "allow_download_models",
	.description =
		"Toggle if clients can download models from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download_models },
};
int allow_download_sounds;
static cvar_t allow_download_sounds_cvar = {
	.name = "allow_download_sounds",
	.description =
		"Toggle if clients can download sounds from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download_sounds },
};
int allow_download_maps;
static cvar_t allow_download_maps_cvar = {
	.name = "allow_download_maps",
	.description =
		"Toggle if clients can download maps from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download_maps },
};
int allow_download_demos;
static cvar_t allow_download_demos_cvar = {
	.name = "allow_download_demos",
	.description =
		"Toggle if clients can download maps from the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &allow_download_demos },
};

int sv_highchars;
static cvar_t sv_highchars_cvar = {
	.name = "sv_highchars",
	.description =
		"Toggle the use of high character color names for players",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_highchars },
};
int sv_phs;
static cvar_t sv_phs_cvar = {
	.name = "sv_phs",
	.description =
		"Possibly Hearable Set. If set to zero, the server calculates sound "
		"hearability in realtime",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_phs },
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


char *sv_minqfversion;
static cvar_t sv_minqfversion_cvar = {
	.name = "sv_minqfversion",
	.description =
		"Minimum QF version on client",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = 0, .value = &sv_minqfversion },
};
										// connect

int sv_timestamps;
static cvar_t sv_timestamps_cvar = {
	.name = "sv_timestamps",
	.description =
		"Time/date stamps in log entries",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_timestamps },
};
char *sv_timefmt;
static cvar_t sv_timefmt_cvar = {
	.name = "sv_timefmt",
	.description =
		"Time/date format to use",
	.default_value = "[%b %e %X] ",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_timefmt },
};

// game rules mirrored in svs.info
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
float samelevel;
static cvar_t samelevel_cvar = {
	.name = "samelevel",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &samelevel },
};
int maxclients;
static cvar_t maxclients_cvar = {
	.name = "maxclients",
	.description =
		"Sets how many clients can connect to your server, this includes "
		"spectators and players",
	.default_value = "8",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &maxclients },
};
int maxspectators;
static cvar_t maxspectators_cvar = {
	.name = "maxspectators",
	.description =
		"Sets how many spectators can connect to your server. The maxclients "
		"value takes precedence over this value so this value should always be"
		" equal-to or less-then the maxclients value",
	.default_value = "8",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &maxspectators },
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
int coop;
static cvar_t coop_cvar = {
	.name = "coop",
	.description =
		"0 or 1",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &coop },
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
float spawn;
static cvar_t spawn_cvar = {
	.name = "spawn",
	.description =
		"Spawn the player entity",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_float, .value = &spawn },
};
int watervis;
static cvar_t watervis_cvar = {
	.name = "watervis",
	.description =
		"Set nonzero to enable r_wateralpha on clients",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = &cexpr_int, .value = &watervis },
};

char *hostname;
static cvar_t hostname_cvar = {
	.name = "hostname",
	.description =
		"None",
	.default_value = "UNNAMED",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &hostname },
};

QFile      *sv_fraglogfile;

int         sv_net_initialized;

const char *client_info_filters[] = {  // Info keys needed by client
	"name",
	"chat",
	"topcolor",
	"bottomcolor",
	"rate",
	"msg",
	"skin",
	"team",
	"noaim",
	"pmodel",
	"emodel",
	"spectator",
	"*spectator",
	"*ver",
	NULL
};

/*
	Master_Shutdown

	Informs all masters that this server is going down
*/
static void
Master_Shutdown (void)
{
	char        string[2048];
	int         i;

	snprintf (string, sizeof (string), "%c\n", S2M_SHUTDOWN);

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
		if (master_adr[i].port) {
			SV_Printf ("Sending heartbeat to %s\n",
						NET_AdrToString (master_adr[i]));
			Netchan_SendPacket (strlen (string), string, master_adr[i]);
		}
}

/*
	SV_Shutdown

	Quake calls this before calling Sys_Quit or Sys_Error
*/
void
SV_Shutdown (void *data)
{
	Master_Shutdown ();
	if (sv_fraglogfile) {
		Qclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
	}
	if (sv.recording_demo)
		SV_Stop (0);
}

/*
	SV_Error

	Sends a datagram to all the clients informing them of the server crash,
	then exits
*/
static __attribute__((format(PRINTF, 1, 0))) void
SV_Error (const char *error, va_list argptr)
{
	static qboolean inerror = false;
	dstring_t  *string;

	if (inerror)
		return;

	inerror = true;
	string = dstring_new ();
	dvsprintf (string, error, argptr);

	Sys_Printf ("%s\n", string->str);

	if (sv_net_initialized) {
		dstring_insertstr (string, 0, "server crashed: ");
		dstring_appendstr (string, "\n");
		SV_FinalMessage (string->str);
	}
	dstring_delete (string);
}

/*
	SV_FinalMessage

	Used by SV_Error and SV_Quit_f to send a final message to all connected
	clients before the server goes down.  The messages are sent immediately,
	not just stuck on the outgoing message list, because the server is going
	to totally exit after returning from this function.
*/
void
SV_FinalMessage (const char *message)
{
	client_t   *cl;
	int         i;

	SZ_Clear (net_message->message);
	MSG_WriteByte (net_message->message, svc_print);
	MSG_WriteByte (net_message->message, PRINT_HIGH);
	MSG_WriteString (net_message->message, message);
	MSG_WriteByte (net_message->message, svc_disconnect);

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		if (cl->state >= cs_connected)
			Netchan_Transmit (&cl->netchan, net_message->message->cursize,
							  net_message->message->data);
	SV_qtvFinalMessage (message);
}

/*
	SV_DropClient

	Called when the player is totally leaving the server, either willingly
	or unwillingly.  This is NOT called if the entire server is quiting
	or crashing.
*/
void
SV_DropClient (client_t *drop)
{
	SV_SavePenaltyFilter (drop, ft_mute, drop->lockedtill);
	SV_SavePenaltyFilter (drop, ft_cuff, drop->cuff_time);

	// add the disconnect
	MSG_WriteByte (&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned) {//FIXME cs_server?
		if (!drop->spectator) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, drop->edict);
			PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientDisconnect);
		} else if (sv_funcs.SpectatorDisconnect) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, drop->edict);
			PR_ExecuteProgram (&sv_pr_state, sv_funcs.SpectatorDisconnect);
		}
	}
	if (drop->spectator)
		SV_Printf ("Spectator %s removed\n", drop->name);
	else
		SV_Printf ("Client %s removed\n", drop->name);

	if (drop->download) {
		Qclose (drop->download);
		drop->download = NULL;
	}
	if (drop->upload) {
		Qclose (drop->upload);
		drop->upload = NULL;
	}
	if (drop->uploadfn)
		dstring_delete (drop->uploadfn);
	drop->uploadfn = 0;

	drop->state = cs_zombie;			// become free in a few seconds
	drop->connection_started = realtime;	// for zombie timeout

	drop->old_frags = 0;
	SVfloat (drop->edict, frags) = 0;
	drop->name[0] = 0;

	if (drop->userinfo)
		Info_Destroy (drop->userinfo);
	drop->userinfo = 0;

	// send notification to all remaining clients
	SV_FullClientUpdate (drop, &sv.reliable_datagram);

	// Trigger GIB event
	if (sv_client_disconnect_e->func)
		GIB_Event_Callback (sv_client_disconnect_e, 1,
							va (0, "%u", drop->userid));
}

int
SV_CalcPing (client_t *cl)
{
	float       ping;
	int         count, i;
	register client_frame_t *frame;

	if (cl->state == cs_server)
		return cl->ping;
	ping = 0;
	count = 0;
	for (frame = cl->delta.frames, i = 0; i < UPDATE_BACKUP; i++, frame++) {
		if (frame->ping_time > 0) {
			ping += frame->ping_time;
			count++;
		}
	}
	if (!count)
		return 9999;
	ping /= count;

	return ping * 1000;
}

static int
make_info_string_filter (const char *key)
{
	return *key == '_' || !Info_FilterForKey (key, client_info_filters);
}

/*
	SV_FullClientUpdate

	Writes all update values to a sizebuf
*/
void
SV_FullClientUpdate (client_t *client, sizebuf_t *buf)
{
	const char *info;
	int         i;

	i = client - svs.clients;

//	SV_Printf ("SV_FullClientUpdate:  Updated frags for client %d\n", i);

	MSG_WriteByte (buf, svc_updatefrags);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, client->old_frags);

	MSG_WriteByte (buf, svc_updateping);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, SV_CalcPing (client));

	MSG_WriteByte (buf, svc_updatepl);
	MSG_WriteByte (buf, i);
	MSG_WriteByte (buf, client->lossage);

	MSG_WriteByte (buf, svc_updateentertime);
	MSG_WriteByte (buf, i);
	MSG_WriteFloat (buf, realtime - client->connection_started);

	info = client->userinfo ? Info_MakeString (client->userinfo,
											   make_info_string_filter) : "";

	MSG_WriteByte (buf, svc_updateuserinfo);
	MSG_WriteByte (buf, i);
	MSG_WriteLong (buf, client->userid);
	MSG_WriteString (buf, info);
}

/*
	SV_FullClientUpdateToClient

	Writes all update values to a client's reliable stream
*/
void
SV_FullClientUpdateToClient (client_t *client, backbuf_t *backbuf)
{
	if (client->state < cs_connected && client->state != cs_server)
		return;
	MSG_ReliableCheckBlock (backbuf, 24 + Info_CurrentSize (client->userinfo));
	if (backbuf->num_backbuf) {
		SV_FullClientUpdate (client, &backbuf->backbuf);
		MSG_Reliable_FinishWrite (backbuf);
	} else
		SV_FullClientUpdate (client, &backbuf->netchan->message);
}

/* CONNECTIONLESS COMMANDS */

/*
	CheckForFlood :: EXPERIMENTAL

	Makes it more difficult to use Quake servers for DoS attacks against
	other sites.

	Bad sides: affects gamespy and spytools somewhat...
*/
static int
CheckForFlood (flood_enum_t cmdtype)
{
	double      currenttime;
	double      oldestTime;
	static double lastmessagetime = 0;
	static flood_t floodstatus[DOSFLOODCMDS][DOSFLOODIP];
	int         oldest, i;
	static qboolean firsttime = true;

	if (!sv_netdosprotect)
		return 0;

	oldestTime = 0x7fffffff;
	oldest = 0;

	if (firsttime) {
		memset (floodstatus, 0, sizeof (flood_t) * DOSFLOODCMDS * DOSFLOODIP);

		firsttime = false;
	}

	currenttime = Sys_DoubleTime ();

	for (i = 0; i < DOSFLOODIP; i++) {
		if (NET_CompareBaseAdr (net_from, floodstatus[cmdtype][i].adr))
			break;
		if (floodstatus[cmdtype][i].issued < oldestTime) {
			oldestTime = floodstatus[cmdtype][i].issued;
			oldest = i;
		}
	}

	if (i < DOSFLOODIP && floodstatus[cmdtype][i].issued) {
		if ((floodstatus[cmdtype][i].issued + netdosexpire[cmdtype])
			> currenttime) {
			floodstatus[cmdtype][i].floodcount += 1;
			if (floodstatus[cmdtype][i].floodcount > netdosvalues[cmdtype]) {
				if ((lastmessagetime + 5) < currenttime)
					SV_Printf ("Blocking type %d flood from (or to) %s\n",
								cmdtype, NET_AdrToString (net_from));
				floodstatus[cmdtype][i].floodcount = 0;
				floodstatus[cmdtype][i].issued = currenttime;
				floodstatus[cmdtype][i].cmdcount += 1;
				lastmessagetime = currenttime;
				return 1;
			}
		} else {
			floodstatus[cmdtype][i].floodcount = 0;
		}
	}

	if (i == DOSFLOODIP) {
		i = oldest;
		floodstatus[cmdtype][i].adr = net_from;
		floodstatus[cmdtype][i].firstseen = currenttime;
		floodstatus[cmdtype][i].cmdcount = 0;
		floodstatus[cmdtype][i].floodcount = 0;
	}
	floodstatus[cmdtype][i].issued = currenttime;
	floodstatus[cmdtype][i].cmdcount += 1;
	return 0;
}

/*
	SVC_Status

	Responds with all the info that qplug or qspy can see
	This message can be up to around 5k with worst case string lengths.
*/
static void
SVC_Status (void)
{
	client_t   *cl;
	int         ping, bottom, top, i;

	if (!sv_allow_status)
		return;
	if (CheckForFlood (FLOOD_STATUS))
		return;

	con_printf_no_log = 1;
	SV_BeginRedirect (RD_PACKET);
	SV_Printf ("%s\n", Info_MakeString (svs.info, 0));
	for (i = 0; i < MAX_CLIENTS; i++) {
		cl = &svs.clients[i];
		if ((cl->state >= cs_connected) && !cl->spectator) {
			top = atoi (Info_ValueForKey (cl->userinfo, "topcolor"));
			bottom = atoi (Info_ValueForKey (cl->userinfo, "bottomcolor"));
			top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
			bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
			ping = SV_CalcPing (cl);
			SV_Printf ("%i %i %i %i \"%s\" \"%s\" %i %i\n", cl->userid,
						cl->old_frags,
						(int) (realtime - cl->connection_started) / 60, ping,
						cl->name, Info_ValueForKey (cl->userinfo, "skin"), top,
						bottom);
		}
	}
	SV_EndRedirect ();
	con_printf_no_log = 0;
}

/* SV_CheckLog */
#define	LOG_HIGHWATER	(MAX_DATAGRAM - 100)
#define	LOG_FLUSH		10*60

static void
SV_CheckLog (void)
{
	sizebuf_t  *sz;

	sz = &svs.log[svs.logsequence & 1];

	// bump sequence if allmost full, or ten minutes have passed and
	// there is something still sitting there
	if (sz->cursize > LOG_HIGHWATER
		|| (realtime - svs.logtime > LOG_FLUSH && sz->cursize)) {
		// swap buffers and bump sequence
		svs.logtime = realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence & 1];
		sz->cursize = 0;
		SV_Printf ("beginning fraglog sequence %i\n", svs.logsequence);
	}
}

/*
	SVC_Log

	Responds with all the logged frags for ranking programs.
	If a sequence number is passed as a parameter and it is
	the same as the current sequence, an A2A_NACK will be returned
	instead of the data.
*/
static void
SVC_Log (void)
{
	char        data[MAX_DATAGRAM + 64];
	int         seq;

	if (!sv_allow_log)
		return;
	if (CheckForFlood (FLOOD_LOG))
		return;

	if (Cmd_Argc () == 2)
		seq = atoi (Cmd_Argv (1));
	else
		seq = -1;

	if (seq == svs.logsequence - 1 || !sv_fraglogfile) {
		// they already have this data, or we aren't logging frags
		data[0] = A2A_NACK;
		Netchan_SendPacket (1, data, net_from);
		return;
	}

	Sys_MaskPrintf (SYS_dev, "sending log %i to %s\n", svs.logsequence - 1,
					NET_AdrToString (net_from));

//	snprintf (data, sizeof (data), "stdlog %i\n", svs.logsequence - 1);
//	strncat (data,  (char *) svs.log_buf[((svs.logsequence - 1) & 1)],
//	sizeof (data) - strlen (data));
	snprintf (data, sizeof (data), "stdlog %i\n%s",
			  svs.logsequence - 1,
			  (char *) svs.log_buf[((svs.logsequence - 1) & 1)]);

	Netchan_SendPacket (strlen (data) + 1, data, net_from);
}

/*
	SVC_Ping

	Just responds with an acknowledgement
*/
static void
SVC_Ping (void)
{
	char        data;

	if (!sv_allow_ping)
		return;
	if (CheckForFlood (FLOOD_PING))
		return;

	data = A2A_ACK;

	Netchan_SendPacket (1, &data, net_from);
}

/*
	SVC_GetChallenge

	Returns a challenge number that can be used
	in a subsequent client_connect command.
	We do this to prevent denial of service attacks that
	flood the server with invalid connection IPs.  With a
	challenge, they must give a valid IP address.
*/
static void
SVC_GetChallenge (void)
{
	int         oldest, oldestTime, i;
	const char *extended = "";

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = (rand () << 16) ^ rand ();
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = realtime;
		i = oldest;
	}

	if (sv_extensions) {
		extended = " QF qtv EXT";
	}

	// send it to the client
	SV_OutOfBandPrint (net_from, "%c%i%s", S2C_CHALLENGE,
					   svs.challenges[i].challenge, extended);
}

client_t *
SV_AllocClient (int spectator, int server)
{
	client_t   *cl;
	int         i, clients, spectators, free, bots;
	static int  userid;

	// count up the clients and spectators
	clients = 0;
	spectators = 0;
	free = 0;
	bots = 0;
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_free)
			free++;
		else if (cl->state == cs_server)
			bots++;
		else if (cl->spectator)
			spectators++;
		else
			clients++;
	}

	// if at server limits, refuse connection
	if (!free ||
		(!server && ((spectator && spectators >= maxspectators)
					 || (!spectator && clients >= maxclients)))) {
		return 0;
	}
	// find a client slot
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_free) {
			svs.num_clients++;
			memset (cl, 0, sizeof (client_t));
			cl->userid = userid++;	// so every client gets a unique id
			if (userid < 0)			// reserve -ve userids for special purposes
				userid = 0;
			cl->edict = EDICT_NUM (&sv_pr_state, (cl - svs.clients) + 1);
			return cl;
		}
	}
	SV_Printf ("WARNING: miscounted available clients\n");
	return 0;
}

/*
	SVC_DirectConnect

	A connection request that did not come from the master
*/
static void
SVC_DirectConnect (void)
{
	info_t     *userinfo = 0;
	const char *s;
	client_t   *cl, *newcl;
	int         challenge, version, i, qtv = 0;
	int         qport;
	netadr_t    adr;
	qboolean    spectator;
	client_frame_t *frames;

	if (CheckForFlood (FLOOD_CONNECT))
		return;

	s = Cmd_Argv (1);
	if (!strcmp (s, "qtv")) {
		qtv = 1;
	} else {
		version = atoi (s);
		if (version != PROTOCOL_VERSION) {
			SV_OutOfBandPrint (net_from, "%c\nServer is version %s.\n",
							   A2C_PRINT, QW_VERSION);
			SV_Printf ("* rejected connect from version %i\n", version);
			return;
		}
	}

	qport = atoi (Cmd_Argv (2));

	challenge = atoi (Cmd_Argv (3));

	if (strlen (Cmd_Argv (4)) < MAX_INFO_STRING)
		userinfo = Info_ParseString (Cmd_Argv (4), 1023,
									 !sv_highchars);

	// Validate the userinfo string.
	if (!userinfo) {
		SV_OutOfBandPrint (net_from, "%c\nInvalid userinfo string.\n",
						   A2C_PRINT);
		return;
	}

	// see if the challenge is valid
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr)) {
			if (challenge == svs.challenges[i].challenge)
				break;					// good
			SV_OutOfBandPrint (net_from, "%c\nBad challenge.\n", A2C_PRINT);
			return;
		}
	}
	if (i == MAX_CHALLENGES) {
		SV_OutOfBandPrint (net_from, "%c\nNo challenge for address.\n",
						   A2C_PRINT);
		return;
	}

	if (qtv) {
		SV_qtvConnect (qport, userinfo);
		return;
	}

	s = Info_ValueForKey (userinfo, "*qf_version");
	if ((!s[0]) || sv_minqfversion[0]) {	// kick old clients?
		if (ver_compare (s, sv_minqfversion) < 0) {
			SV_Printf ("%s: Version %s is less than minimum version %s.\n",
						NET_AdrToString (net_from), s,
					   sv_minqfversion);

			SV_OutOfBandPrint (net_from, "%c\nserver requires QuakeForge "
							   "v%s or greater. Get it from "
							   "http://www.quakeforge.net/\n", A2C_PRINT,
							   sv_minqfversion);
			return;
		}
	}
	// check for password or spectator_password
	s = Info_ValueForKey (userinfo, "spectator");
	if (s[0] && strcmp (s, "0")) {
		if (spectator_password[0] &&
			!strcaseequal (spectator_password, "none") &&
			!strequal (spectator_password, s)) {	// failed
			SV_Printf ("%s: spectator password failed\n",
						NET_AdrToString (net_from));
			SV_OutOfBandPrint (net_from,
							   "%c\nrequires a spectator password\n\n",
							   A2C_PRINT);
			return;
		}
		Info_RemoveKey (userinfo, "spectator");	// remove passwd
		Info_SetValueForStarKey (userinfo, "*spectator", "1",
								 !sv_highchars);
		spectator = true;
	} else {
		s = Info_ValueForKey (userinfo, "password");
		if (password[0]
			&& !strcaseequal (password, "none")
			&& !strequal (password, s)) {
			SV_Printf ("%s:password failed\n", NET_AdrToString (net_from));
			SV_OutOfBandPrint (net_from,
							   "%c\nserver requires a password\n\n",
							   A2C_PRINT);
			return;
		}
		spectator = false;
		Info_RemoveKey (userinfo, "password");	// remove passwd
	}

	adr = net_from;

	// if there is already a slot for this ip, drop it
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_zombie)
			continue;
		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address)
			&& (cl->netchan.qport == qport
				|| adr.port == cl->netchan.remote_address.port)) {
			if (cl->state == cs_connected) {
				SV_Printf ("%s:dup connect\n", NET_AdrToString (adr));
				return;
			}

			SV_Printf ("%s:reconnect\n", NET_AdrToString (adr));
			SV_DropClient (cl);
			break;
		}
	}
	if (!(newcl = SV_AllocClient (spectator, 0))) {
		SV_Printf ("%s:full connect\n", NET_AdrToString (adr));
		SV_OutOfBandPrint (adr, "%c\nserver is full\n\n", A2C_PRINT);
		return;
	}
	newcl->userinfo = userinfo;
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	frames = newcl->delta.frames;
	for (i = 0; i < UPDATE_BACKUP; i++) {
		frames[i].entities.entities = cl_entities[newcl-svs.clients][i];
		memset (cl_entities[newcl-svs.clients][i], 0,
				sizeof (cl_entities[newcl-svs.clients][i]));
	}
	newcl->delta.client = newcl;

	SV_OutOfBandPrint (adr, "%c", S2C_CONNECTION);

	Netchan_Setup (&newcl->netchan, adr, qport, NC_QPORT_READ);
	newcl->backbuf.netchan = &newcl->netchan;
	newcl->backbuf.name = newcl->name;

	newcl->state = cs_connected;
	newcl->prespawned = false;
	newcl->spawned = false;

	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof (newcl->datagram_buf);

	// spectator mode can be set ONLY at join time
	newcl->spectator = spectator;

	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl);

	// JACK: Init the floodprot stuff.
	for (i = 0; i < 10; i++)
		newcl->whensaid[i] = 0.0;
	newcl->whensaidhead = 0;
	newcl->lockedtill = SV_RestorePenaltyFilter (newcl, ft_mute);

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.SetNewParms);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		newcl->spawn_parms[i] = sv_globals.parms[i];

	if (newcl->spectator)
		SV_Printf ("Spectator %s (%s) connected\n", newcl->name,
				   NET_AdrToString (adr));
	else
		SV_Printf ("Client %s (%s) connected\n", newcl->name,
					NET_AdrToString (adr));
	newcl->sendinfo = true;

	// QuakeForge stuff.
	newcl->msecs = 0;
	newcl->msec_cheating = 0;
	newcl->last_check = -1;

	newcl->cuff_time = SV_RestorePenaltyFilter (newcl, ft_cuff);
}

static int
Rcon_Validate (const char *pass)
{
	if (!strlen (pass))
		return 0;

	if (strcmp (Cmd_Argv (1), pass))
		return 0;

	return 1;
}

static char *
Name_of_sender (void)
{
	client_t   *cl;
	int         i;

	for (i=0, cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++) {
		if (cl->state < cs_zombie)
			continue;
		if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
			continue;
		if (cl->netchan.remote_address.port != net_from.port)
			continue;
		return cl->name;
	}
	return NULL;
}

/*
  SVC_RemoteCommand

  A client issued an rcon command.
  Shift down the remaining args
  Redirect all printfs
*/
static void
SVC_RemoteCommand (void)
{
	const char *command;
	char       *name;
	qboolean    admin_cmd = false;
	qboolean    do_cmd = false;

	if (CheckForFlood (FLOOD_RCON))
		return;

	if (Rcon_Validate (rcon_password)) {
		do_cmd = true;
	} else if (Rcon_Validate (admin_password)) {
		admin_cmd = true;
		if (strcmp (Cmd_Argv (2), "say") == 0
			||  strcmp (Cmd_Argv (2), "kick") == 0
			||  strcmp (Cmd_Argv (2), "ban") == 0
			||  strcmp (Cmd_Argv (2), "map") == 0
			||  strcmp (Cmd_Argv (2), "mute") == 0
			||  strcmp (Cmd_Argv (2), "cuff") == 0
			||  strcmp (Cmd_Argv (2), "exec") == 0
			||  strcmp (Cmd_Argv (2), "addip") == 0
			||  strcmp (Cmd_Argv (2), "listip") == 0
			||  strcmp (Cmd_Argv (2), "writeip") == 0
			||  strcmp (Cmd_Argv (2), "removeip") == 0)
			do_cmd = true;
	}
	if ((name = Name_of_sender ())) {			// log issuer
		if (do_cmd && admin_cmd) {
			SV_BroadcastPrintf (PRINT_HIGH, "Admin %s issued %s command:\n",
								name, Cmd_Argv (2));
		} else if (admin_cmd) {
			SV_Printf ("Admin %s issued %s command:\n", name, Cmd_Argv (2));
		} else {
			SV_Printf ("User %s %s rcon command:\n", name,
					   do_cmd ? "issued" : "attempted");
		}
	}

	if (do_cmd) {
		command = Cmd_Args (2);

		SV_Printf ("Rcon from %s:\n\trcon (hidden) %s\n",
					NET_AdrToString (net_from), command);

		SV_BeginRedirect (RD_PACKET);
		if (name)
			rcon_from_user = true;
		Cmd_ExecuteString (command, src_command);
		rcon_from_user = false;
	} else {
		SV_Printf ("Bad rcon from %s:\n\t rcon (hidden) %s\n",
				   NET_AdrToString (net_from), Cmd_Args (2));

		SV_BeginRedirect (RD_PACKET);
		if (admin_cmd) {
			SV_Printf ("Command not valid with admin password.\n");
		} else {
			SV_Printf ("Bad rcon_password.\n");
		}
	}

	SV_EndRedirect ();
}

/*
  SV_ConnectionlessPacket

  A connectionless packet has four leading 0xff
  characters to distinguish it from a game channel.
  Clients that are in the game can still send
  connectionless packets.
*/
static void
SV_ConnectionlessPacket (void)
{
	const char *c, *s;

	MSG_BeginReading (net_message);
	MSG_ReadLong (net_message);					// skip the -1 marker

	s = MSG_ReadString (net_message);

	COM_TokenizeString (s, sv_args);
	cmd_args = sv_args;

	c = sv_args->argv[0]->str;

	if (!strcmp (c, "ping")
		|| (c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n'))) {
		SVC_Ping ();
		return;
	}
	if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n')) {
		SV_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
		return;
	} else if (!strcmp (c, "status")) {
		SVC_Status ();
		return;
	} else if (!strcmp (c, "log")) {
		SVC_Log ();
		return;
	} else if (!strcmp (c, "connect")) {
		SVC_DirectConnect ();
		return;
	} else if (!strcmp (c, "getchallenge")) {
		SVC_GetChallenge ();
		return;
	} else if (!strcmp (c, "rcon"))
		SVC_RemoteCommand ();
	else
		SV_Printf ("bad connectionless packet from %s:\n%s\n",
					NET_AdrToString (net_from), s);
}

/*
	PACKET FILTERING

	You can add or remove addresses from the filter list with:

	addip <ip>
	removeip <ip>

	The ip address is specified in dot format, and any unspecified digits
	will match any value, so you can specify an entire class C network with
	"addip 192.246.40".

	Removeip will remove only an address specified exactly the same way.
	You cannot addip a subnet, then removeip a single host.

	listip
	Prints the current list of filters.

	writeip
	Dumps "addip <ip>" commands to listip.cfg so it can be execed at a
	later date.  The filter lists are not saved and restored by default,
	because I beleive it would cause too much confusion.

	filterban <0 or 1>

	If 1 (the default), then ip addresses matching the current list will be
	prohibited from entering the game.  This is the default setting.

	If 0, then only addresses matching the list will be allowed.  This lets
	you easily set up a private game, or a game that allows only players
	from your local network.
*/

typedef struct {
	int				mask;
#ifdef HAVE_IPV6
	byte			ip[16];
#else
	byte			ip[4];
#endif
	double			time;
	filtertype_t	type;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

int filterban;
static cvar_t filterban_cvar = {
	.name = "filterban",
	.description =
		"Determines the rules for the IP list 0 Only IP addresses on the Ban "
		"list will be allowed onto the server, 1 Only IP addresses NOT on the "
		"Ban list will be allowed onto the server",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &filterban },
};
int sv_filter_automask;
static cvar_t sv_filter_automask_cvar = {
	.name = "sv_filter_automask",
	.description =
		"Automatically determine the mask length when it is not explicitely "
		"given.  e.g. \"addip 1.2.0.0\" would be the same as \"addip "
		"1.2.0.0/16\"",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_filter_automask },
};
int         numipfilters;
ipfilter_t  ipfilters[MAX_IPFILTERS];
unsigned int ipmasks[33]; // network byte order

static void
SV_GenerateIPMasks (void)
{
	int i;
	unsigned int j = 0xFFFFFFFF;

	for (i = 32; i >= 0; i--) {
		ipmasks[i] = htonl (j);
		j = j << 1;
	}
}

// Note: this function is non-reentrant and not threadsafe
static const char *
SV_PrintIP (byte *ip)
{
#ifdef HAVE_IPV6
	static char buf[INET6_ADDRSTRLEN];
	if (!inet_ntop (AF_INET6, ip, buf, INET6_ADDRSTRLEN))
#else
# ifdef _WIN32
	static char buf[16];
	strcpy (buf, inet_ntoa (*(struct in_addr*)ip));
	if (0)
# else
	static char buf[INET_ADDRSTRLEN];
	if (!inet_ntop (AF_INET, ip, buf, INET_ADDRSTRLEN))
# endif
#endif
        Sys_Error ("SV_CleanIPList: inet_ntop_failed.  wtf?");
	return buf;
}

static inline void
SV_MaskIPTrim (byte *ip, int mask)
{
    int i;
#ifdef HAVE_IPV6
    int intcount = 4;
#else
    int intcount = 1;
#endif

    for (i = 0; i < intcount; i++) {
        ((unsigned int *)ip)[i] &= ipmasks[mask > 32 ? 32 : mask];
        if ((mask -= 32) < 0)
            mask = 0;
    }
}

// assumes b has already been masked
static inline __attribute__((pure)) qboolean
SV_MaskIPCompare (byte *a, byte *b, int mask)
{
	int i;
#ifdef HAVE_IPV6
	int intcount = 4;
#else
	int intcount = 1;
#endif

	for (i = 0; i < intcount; i++) {
		unsigned int x, y;
		memcpy (&x, a + i * 4, 4);
		memcpy (&y, b + i * 4, 4);
		if ((x & ipmasks[mask > 32 ? 32 : mask]) != y)
			return false;
		if ((mask -= 32) < 0)
			mask = 0;
	}

	return true;
}

static inline qboolean
SV_IPCompare (byte *a, byte *b)
{
	int i;
#ifdef HAVE_IPV6
	int intcount = 4;
#else
	int intcount = 1;
#endif

	for (i = 0; i < intcount; i++)
		if (((unsigned int *)a)[i] != ((unsigned int *)b)[i])
			return false;

	return true;
}

static inline void
SV_IPCopy (byte *dest, byte *src)
{
	int i;
#ifdef HAVE_IPV6
	int intcount = 4;
#else
	int intcount = 1;
#endif

	for (i = 0; i < intcount; i++)
		((unsigned int *)dest)[i] = ((unsigned int *)src)[i];
}

static qboolean
SV_StringToFilter (const char *address, ipfilter_t *f)
{
#ifdef HAVE_IPV6
	byte		b[16] = {0};
#else
	byte        b[4] = {0};
#endif
	int			mask = 0;
	unsigned int i;
	char		*s;
	char		*slash;
	char		*c;

	s = strdup (address);
	if (!s)
		Sys_Error ("SV_StringToFilter: memory allocation failure");

	// Parse out the mask (the /8 part)
	if ((slash = strchr (s, '/'))) {
		char *endptr;
		*slash = '\0';
		slash++;
		if (*slash < '0' || *slash > '9' || strchr (slash, '/'))
			goto bad_address;
		mask = strtol (slash, &endptr, 10);
		if (!*slash || *endptr)
			goto bad_address;
	} else
		mask = -1;

	// parse the ip for ipv6
#ifdef HAVE_IPV6
	if (inet_pton (AF_INET6, s, b) != 1) {
		b[10] = 0xFF; // Prefix bytes for hosts that don't support ipv6
		b[11] = 0xFF; // (see RFC 2373, section 2.5.4)
		i = 12;
#else
		i = 0;
#endif
		c = s;

		// parse for ipv4, as dotted quad, only we have implicit trailing segments
		do {
			int j;
			if (*c == '.')
				c++;
			j = strtol (c, &c, 10);
			if (j < 0 || j > 255 || i >= sizeof (b))
				goto bad_address;
			b[i++] = j;
		} while (*c == '.');
		if (*c)
			goto bad_address;

		// change trailing 0 segments to be a mask, eg 1.2.0.0 gives a /16 mask
		if (mask == -1) {
			if (sv_filter_automask) {
				mask = sizeof (b) * 8;
				i = sizeof (b);
				while (i > 0 && !b[i - 1]) {
					mask -= 8;
					i--;
				}
			} else
				mask = 0;
		}
#ifdef HAVE_IPV6
	} else {
		mask = 128;
	}
#endif

#ifdef HAVE_IPV6
	if (mask > 128)
#else
	if (mask > 32)
#endif
		goto bad_address;

	// incase they did 1.2.3.4/16, change it to 1.2.0.0 for easier comparison
	SV_MaskIPTrim (b, mask);

	// yada :)
	f->mask = mask;
	SV_IPCopy (f->ip, b);

	free (s);
	return true;

bad_address:
	SV_Printf ("Bad filter address: %s\n", address);
	free (s);
	return false;
}

static void
SV_RemoveIPFilter (int i)
{
	for (; i + 1 < numipfilters; i++)
		ipfilters[i] = ipfilters[i + 1];

	numipfilters--;
}


static void
SV_CleanIPList (void)
{
	int         i;
	const char *type;

	for (i = 0; i < numipfilters;) {
		if (ipfilters[i].time && (ipfilters[i].time <= realtime)) {
			switch (ipfilters[i].type) {
				case ft_ban: type = "Ban"; break;
				case ft_mute: type = "Mute"; break;
				case ft_cuff: type = "Cuff"; break;
				default: Sys_Error ("SV_CleanIPList: invalid filter type");
			}
			SV_Printf ("SV_CleanIPList: %s for %s/%d removed\n", type,
					   SV_PrintIP (ipfilters[i].ip), ipfilters[i].mask);
			SV_RemoveIPFilter (i);
		} else
			i++;
	}
}

static void
SV_AddIP_f (void)
{
	int         i;
	double		bantime;
	filtertype_t	type;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 4) {
		SV_Printf ("Usage: addip <ip>/<mask> [<time> [<ban/mute/cuff>] ]\n");
		return;
	}

	if (Cmd_Argc () >= 3)
		bantime = atof (Cmd_Argv (2)) * 60;
	else
		bantime = 0.0;

	if (Cmd_Argc () >= 4) {
		if (strequal ("ban", Cmd_Argv (3)))
			type = ft_ban;
		else if (strequal ("mute", Cmd_Argv (3)))
			type = ft_mute;
		else if (strequal ("cuff", Cmd_Argv (3)))
			type = ft_cuff;
		else {
			SV_Printf ("Unknown filter type '%s'\n", Cmd_Argv (3));
			return;
		}
	} else
		type = ft_ban;

	if (numipfilters == MAX_IPFILTERS) {
		SV_Printf ("IP filter list is full\n");
		return;
	}

	if (SV_StringToFilter (Cmd_Argv (1), &ipfilters[numipfilters])) {
		ipfilters[numipfilters].time = bantime ? realtime + bantime : 0.0;
		ipfilters[numipfilters].type = type;
		// FIXME: this should boot any matching clients
	    for (i = 0; i < MAX_CLIENTS; i++) {
			client_t *cl = &svs.clients[i];
			const char *text;
			const char *typestr;
			char timestr[1024];

			if (cl->state < cs_zombie)
				continue;

			if (SV_MaskIPCompare (cl->netchan.remote_address.ip,
								  ipfilters[numipfilters].ip,
								  ipfilters[numipfilters].mask)) {
				switch (type) {
					case ft_ban:
						typestr = "banned";
						SV_DropClient (cl);
						break;
					case ft_mute:
						typestr = "muted";
						cl->lockedtill = bantime;
						break;
					case ft_cuff:
						typestr = "cuffed";
						cl->cuff_time = bantime;
						break;
					default:
						Sys_Error ("SV_AddIP_f: unknown filter type %d", type);
				}
				if (bantime)
					snprintf (timestr, sizeof (timestr), "for %.1f minutes",
							  bantime / 60);
				else
					strncpy (timestr, "permanently", sizeof (timestr));
				text = va (0, "You are %s %s\n%s",
						   typestr, timestr, type == ft_ban ? "" :
						   "\nReconnecting won't help...");
				MSG_ReliableWrite_Begin (&cl->backbuf, svc_centerprint,
										 strlen (text) + 2);
				MSG_ReliableWrite_String (&cl->backbuf, text);
				// FIXME: print on the console too
			}
		}
		SV_Printf ("Added IP Filter for %s/%u\n",
				   SV_PrintIP (ipfilters[numipfilters].ip),
				   ipfilters[numipfilters].mask);
		numipfilters++;
	}
}

static void
SV_RemoveIP_f (void)
{
	int         i;
	ipfilter_t  f;

	if (!SV_StringToFilter (Cmd_Argv (1), &f))
		return;
	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && SV_IPCompare (ipfilters[i].ip, f.ip)) {
			SV_RemoveIPFilter (i);
			SV_Printf ("Removed IP Filter for %s/%u.\n",
					   SV_PrintIP (f.ip), f.mask);
			return;
		}
	SV_Printf ("Didn't find %s/%u.\n", SV_PrintIP (f.ip), f.mask);
}

static void
SV_ListIP_f (void)
{
	int         i;
	const char *type;
	char		timestr[30];

	SV_Printf ("IP Filter list:\n");
	for (i = 0; i < numipfilters; i++) {
		switch (ipfilters[i].type) {
			case ft_ban: type = "Ban:"; break;
			case ft_mute: type = "Mute:"; break;
			case ft_cuff: type = "Cuff:"; break;
			default: Sys_Error ("SV_ListIP_f: invalid filter type");
		}

		if (ipfilters[i].time)
			snprintf (timestr, sizeof (timestr), "%ds",
					  (int) (ipfilters[i].time ? ipfilters[i].time - realtime : 0));
		else
			strcpy (timestr, "Permanent");

		SV_Printf ("%-5s %-10s %s/%u\n", type, timestr,
				   SV_PrintIP (ipfilters[i].ip), ipfilters[i].mask);
	}
	SV_Printf ("%d IP Filter%s\n", numipfilters,
			   numipfilters == 1 ? "" : "s");
}

static void
SV_WriteIP_f (void)
{
	char        name[MAX_OSPATH];
	int         i;
	QFile      *f;
	const char *type;

	snprintf (name, sizeof (name), "%s/listip.cfg", qfs_gamedir->dir.def);

	SV_Printf ("Writing IP Filters to %s.\n", name);

	f = QFS_Open (name, "wb");
	if (!f) {
		SV_Printf ("Couldn't open %s\n", name);
		return;
	}

	for (i = 0; i < numipfilters; i++) {
		switch (ipfilters[i].type) {
			case ft_ban: type = "ban"; break;
			case ft_mute: type = "mute"; break;
			case ft_cuff: type = "cuff"; break;
			default: Sys_Error ("SV_WriteIP_f: invalid filter type");
		}
		Qprintf (f, "addip %s/%u 0 %s\n", SV_PrintIP (ipfilters[i].ip),
				 ipfilters[i].mask, type);
	}

	Qclose (f);
}

static void
SV_netDoSexpire_f (void)
{
	int         arg1, i;

	if (Cmd_Argc () == 1) {
		SV_Printf ("Current DoS prot. expire settings: ");
		for (i = 0; i < DOSFLOODCMDS; i++)
			SV_Printf ("%f ", netdosexpire[i]);
		SV_Printf ("\n");
		if (!sv_netdosprotect)
			SV_Printf ("(disabled)\n");
		return;
	}

	if (Cmd_Argc () != DOSFLOODCMDS + 1) {
		SV_Printf ("Usage: netdosexpire <ping> <log> <connect> <status> "
				   "<rcon> <ban>\n");
		return;
	}

	for (i = 0; i < DOSFLOODCMDS; i++) {
		arg1 = atoi (Cmd_Argv (i + 1));
		if (arg1 > 0)
			netdosexpire[i] = arg1;
	}
	return;
}

static void
SV_netDoSvalues_f (void)
{
	int         arg1, i;

	if (Cmd_Argc () == 1) {
		SV_Printf ("Current DoS prot. value settings: ");
		for (i = 0; i < DOSFLOODCMDS; i++)
			SV_Printf ("%f ", netdosvalues[i]);
		SV_Printf ("\n");
		if (!sv_netdosprotect)
			SV_Printf ("(disabled)\n");
		return;
	}

	if (Cmd_Argc () != DOSFLOODCMDS + 1) {
		SV_Printf ("Usage: netdosvalues <ping> <log> <connect> <status> "
				   "<rcon> <ban>\n");
		return;
	}

	for (i = 0; i < DOSFLOODCMDS; i++) {
		arg1 = atoi (Cmd_Argv (i + 1));
		if (arg1 > 0)
			netdosvalues[i] = arg1;
	}
	return;
}

static void
SV_SendBan (double till)
{
	char        data[128];
//	char       *data2;

	if (CheckForFlood (FLOOD_BAN))
		return;

	data[0] = data[1] = data[2] = data[3] = 0xff;
	data[4] = A2C_PRINT;

	if (till) {
		snprintf (data + 5, sizeof (data) - 5,
				  "\nbanned for %.1f more minutes.\n", (till - realtime)/60.0);
	} else {
		snprintf (data + 5, sizeof (data) - 5, "\nbanned permanently.\n");
	}

	Netchan_SendPacket (strlen (data), data, net_from);

/*	data[4] = A2C_CLIENT_COMMAND;
	snprintf (data + 5, sizeof (data) - 5, "disconnect\n");
	data2 = data + strlen (data) + 1;
	snprintf (data2, sizeof (data) - (data2 - data), "12345");
	Netchan_SendPacket (strlen (data) + strlen (data2) + 2, data, net_from);*/
	// FIXME: this should send a disconnect to the client!
}

static qboolean
SV_FilterIP (byte *ip, double *until)
{
	int         i;

	*until = 0;

	for (i = 0; i < numipfilters; i++) {
		if (ipfilters[i].type != ft_ban)
			continue;
		if (SV_MaskIPCompare (ip, ipfilters[i].ip, ipfilters[i].mask)) {
			if (!ipfilters[i].time) {
				// normal ban
				return filterban;
			} else if (ipfilters[i].time > realtime) {
				*until = ipfilters[i].time;
				return true;	// banned no matter what
			} else {
				// time expired, set free
				SV_RemoveIPFilter (i);
				i--; // counter the increment, so we don't skip any
			}
		}
	}
	return !filterban;
}

void
SV_SavePenaltyFilter (client_t *cl, filtertype_t type, double pentime)
{
	int i;
	byte *b;
	if (pentime < realtime)   // no point
		return;

	b = cl->netchan.remote_address.ip;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == 32 && SV_IPCompare (ipfilters[i].ip, b)
			&& ipfilters[i].type == type) {
			SV_Printf ("Penalty for user %d already exists\n", cl->userid);
			return;
		}

	if (numipfilters == MAX_IPFILTERS) {
		SV_Printf ("IP filter list is full\n");
		return;
	}

	SV_Printf ("Penalty saved for user %d\n", cl->userid);
	ipfilters[numipfilters].mask = 32;
	SV_IPCopy (ipfilters[numipfilters].ip, b);
	ipfilters[numipfilters].time = pentime;
	ipfilters[numipfilters].type = type;
	numipfilters++;
	return;
}

double
SV_RestorePenaltyFilter (client_t *cl, filtertype_t type)
{
	int         i;
	byte		*ip;

	ip = cl->netchan.remote_address.ip;

	// search for existing penalty filter of same type
	for (i = 0; i < numipfilters; i++) {
		if (type == ipfilters[i].type && SV_IPCompare (ip, ipfilters[i].ip)) {
			SV_Printf ("Penalty restored for user %d\n", cl->userid);
			return ipfilters[i].time;
		}
	}
	return 0.0;
}

void
SV_OutOfBand (netadr_t adr, unsigned length, byte *data)
{
	if (net_packetlog) {
		Log_Outgoing_Packet (data, length, 0);
	}
	Netchan_OutOfBand (adr, length, data);
}

void
SV_OutOfBandPrint (netadr_t adr, const char *format, ...)
{
	static dstring_t *string;
	va_list     argptr;

	if (!string) {
		string = dstring_new ();
	}

	va_start (argptr, format);
	dvsprintf (string, format, argptr);
	va_end (argptr);

	SV_OutOfBand (adr, string->size - 1, (byte *) string->str);
}

static void
SV_ReadPackets (void)
{
	//NOTE star volatile, not volatile star
	client_t   *volatile cl;			// * volatile for longjmp
	int         i;
	int         qport;
	double      until;

	while (NET_GetPacket ()) {
		if (net_packetlog)
			Log_Incoming_Packet (net_message->message->data,
								 net_message->message->cursize, 1);
		if (SV_FilterIP (net_from.ip, &until)) {
			SV_SendBan (until);			// tell them we aren't listening...
			continue;
		}
		// check for connectionless packet (0xffffffff) first
		if (*(int *) net_message->message->data == -1) {
			SV_ConnectionlessPacket ();
			continue;
		}

		if (net_message->message->cursize < 11) {
			SV_Printf ("%s: Runt packet: %d\n", NET_AdrToString (net_from),
					   net_message->message->cursize);
			continue;
		}
		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading (net_message);
		MSG_ReadLong (net_message);				// sequence number
		MSG_ReadLong (net_message);				// sequence number
		qport = MSG_ReadShort (net_message) & 0xffff;

		// check for packets from connected clients
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (cl->state < cs_zombie)
				continue;
			if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
				continue;
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port) {
				Sys_MaskPrintf (SYS_dev, "SV_ReadPackets: fixing up a "
								"translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}
			if (Netchan_Process (&cl->netchan)) {
				// this is a valid, sequenced packet, so process it
				svs.stats.packets++;
				cl->send_message = true;	// reply at end of frame
				if (cl->state != cs_zombie) {
					SV_ExecuteClientMessage (cl);
				}
			}
			break;
		}

		if (i != MAX_CLIENTS)
			continue;

		if (SV_qtvPacket (qport))
			continue;

		// packet is not from a known client
//		SV_Printf ("%s:sequenced packet without connection\n",
//				   NET_AdrToString (net_from));
	}
}

/*
  SV_CheckTimeouts

  If a packet has not been received from a client in timeout.value
  seconds, drop the conneciton.

  When a client is normally dropped, the client_t goes into a zombie
  state for a few seconds to make sure any final reliable message gets
  resent if necessary
*/
static void
SV_CheckTimeouts (void)
{
	client_t   *cl;
	float       droptime;
	int         nclients, i;

	droptime = realtime - sv_timeout;
	nclients = 0;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state >= cs_connected) {
			if (!cl->spectator)
				nclients++;
			if (cl->netchan.last_received < droptime) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
				SV_DropClient (cl);
				cl->state = cs_free;	// don't bother with zombie state
				svs.num_clients--;
			}
		}
		if (cl->state == cs_zombie &&
			realtime - cl->connection_started > zombietime) {
			cl->state = cs_free;		// can now be reused
			svs.num_clients--;
		}
	}
	SV_qtvCheckTimeouts ();
	if (sv.paused && !nclients) {
		// nobody left, unpause the server
		SV_TogglePause ("Pause released since no players are left.\n");
	}
}

/*
	SV_CheckVars
*/
static void
SV_CheckVars (void)
{
	static char const *pw, *spw;
	int         v;

	if (password == pw && spectator_password == spw)
		return;
	pw = password;
	spw = spectator_password;

	v = 0;
	if (pw && pw[0] && strcmp (pw, "none"))
		v |= 1;
	if (spw && spw[0] && strcmp (spw, "none"))
		v |= 2;

	SV_Printf ("Updated needpass.\n");
	if (!v)
		Info_SetValueForKey (svs.info, "needpass", "",
							 !sv_highchars);
	else
		Info_SetValueForKey (svs.info, "needpass", va (0, "%i", v),
							 !sv_highchars);
}

void
SV_Frame (float time)
{
	static double start, end;
	double      demo_start, demo_end;

	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;

	// keep the random time dependent
	rand ();

	// decide the simulation time
	realtime += time;
	if (!sv.paused) {
		sv.time += time;
	}
	// check timeouts
	SV_CheckTimeouts ();

	// toggle the log buffer if full
	SV_CheckLog ();

	// clean out expired bans/cuffs/mutes
	SV_CleanIPList ();

	// move autonomous things around if enough time has passed
	if (!sv.paused) {
		static double old_time;

		// don't bother running a frame if sys_ticrate seconds haven't passed
		sv_frametime = sv.time - old_time;
		if (sv_frametime < 0) {
			old_time = sv.time - sv_mintic;
			sv_frametime = sv_mintic;
		}
		if (sv_frametime >= sv_mintic) {
			if (sv_frametime > sv_maxtic)
				sv_frametime = sv_maxtic;
			old_time = sv.time;

			*sv_globals.frametime = sv_frametime;

			SV_Physics ();
		}
	}

	// get packets
	SV_ReadPackets ();

	// check for typing at the console
	Con_ProcessInput ();

	// process gib threads
	GIB_Thread_Execute ();

	// process console commands
	Cbuf_Execute_Stack (sv_cbuf);

	SV_CheckVars ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	demo_start = Sys_DoubleTime ();
	if (sv.recorders)
		SVR_SendMessages ();
	demo_end = Sys_DoubleTime ();
	svs.stats.demo += demo_end - demo_start;

	SV_qtvSendMessages ();

	// send a heartbeat to the master if needed
	Master_Heartbeat ();

	// collect timing statistics
	end = Sys_DoubleTime ();
	svs.stats.active += end - start;
	if (++svs.stats.count == STATFRAMES) {
		svs.stats.latched_active = svs.stats.active;
		svs.stats.latched_idle = svs.stats.idle;
		svs.stats.latched_packets = svs.stats.packets;
		svs.stats.latched_demo = svs.stats.demo;
		svs.stats.active = 0;
		svs.stats.idle = 0;
		svs.stats.packets = 0;
		svs.stats.count = 0;
		svs.stats.demo = 0;
		Con_DrawConsole ();
	}
}

static void
maxspectators_f (void *data, const cvar_t *cvar)
{
	if (maxspectators > MAX_CLIENTS)
		maxspectators = maxclients;
	else if (maxspectators + maxclients > MAX_CLIENTS)
		maxspectators = MAX_CLIENTS - maxclients;
	else if (maxspectators < 0)
		maxspectators = 0;
	Cvar_Info (data, cvar);
}

static void
maxclients_f (void *data, const cvar_t *cvar)
{
	if (maxclients > MAX_CLIENTS)
		maxclients = MAX_CLIENTS;
	else if (maxclients < 1)
		maxclients = 1;
	Cvar_Info (data, cvar);
	maxspectators_f (0, &maxspectators_cvar);
}

static void
gamedir_f (int phase)
{
	if (!phase)
		return;
	if (qfs_gamedir->gamedir)
		Info_SetValueForStarKey (svs.info, "*gamedir",
								 qfs_gamedir->gamedir, 0);
	else
		Info_RemoveKey (svs.info, "*gamedir");
}

static void
SV_InitLocal (void)
{
	int         i;

	SV_UserInit ();

	Cvar_Register (&rcon_password_cvar, 0, 0);
	Cvar_Register (&admin_password_cvar, 0, 0);
	Cvar_Register (&password_cvar, 0, 0);
	Cvar_Register (&spectator_password_cvar, 0, 0);
	Cvar_Register (&sv_mintic_cvar, 0, 0);
	Cvar_Register (&sv_maxtic_cvar, 0, 0);
	Cvar_Register (&fraglimit_cvar, Cvar_Info, &fraglimit);
	Cvar_Register (&timelimit_cvar, Cvar_Info, &timelimit);
	Cvar_Register (&teamplay_cvar, Cvar_Info, &teamplay);
	Cvar_Register (&samelevel_cvar, 0, 0);
	Cvar_Register (&maxclients_cvar, maxclients_f, 0);
	Cvar_Register (&maxspectators_cvar, maxspectators_f, 0);
	Cvar_Register (&hostname_cvar, 0, 0);
	Cvar_Register (&deathmatch_cvar, 0, 0);
	Cvar_Register (&coop_cvar, 0, 0);
	Cvar_Register (&skill_cvar, 0, 0);
	Cvar_Register (&spawn_cvar, Cvar_Info, &spawn);
	Cvar_Register (&watervis_cvar, Cvar_Info, &watervis);
	Cvar_Register (&sv_timeout_cvar, 0, 0);
	Cvar_Register (&zombietime_cvar, 0, 0);
	Cvar_Register (&sv_extensions_cvar, 0, 0);
	Cvar_Register (&sv_minqfversion_cvar, Cvar_Info, &sv_minqfversion);
	Cvar_Register (&sv_allow_log_cvar, 0, 0);
	Cvar_Register (&sv_allow_status_cvar, 0, 0);
	Cvar_Register (&sv_allow_ping_cvar, 0, 0);
	Cvar_Register (&sv_netdosprotect_cvar, 0, 0);
	Cvar_Register (&sv_timestamps_cvar, 0, 0);
	Cvar_Register (&sv_timefmt_cvar, 0, 0);
	Cvar_Register (&filterban_cvar, 0, 0);
	Cvar_Register (&sv_filter_automask_cvar, 0, 0);
	Cvar_Register (&allow_download_cvar, 0, 0);
	Cvar_Register (&allow_download_skins_cvar, 0, 0);
	Cvar_Register (&allow_download_models_cvar, 0, 0);
	Cvar_Register (&allow_download_sounds_cvar, 0, 0);
	Cvar_Register (&allow_download_maps_cvar, 0, 0);
	Cvar_Register (&allow_download_demos_cvar, 0, 0);
	Cvar_Register (&sv_highchars_cvar, 0, 0);
	Cvar_Register (&sv_phs_cvar, 0, 0);
	Cvar_Register (&pausable_cvar, 0, 0);
	// DoS protection
	Cmd_AddCommand ("netdosexpire", SV_netDoSexpire_f, "FIXME: part of DoS "
					"protection obviously, but I don't know what it does. No "
					"Description");
	Cmd_AddCommand ("netdosvalues", SV_netDoSvalues_f, "FIXME: part of DoS "
					"protection obviously, but I don't know what it does. No "
					"Description");
	Cmd_AddCommand ("addip", SV_AddIP_f, "Add a single IP or a domain of IPs "
					"to the IP list of the server.\n"
					"Useful for banning people. (addip (ipnumber))");
	Cmd_AddCommand ("removeip", SV_RemoveIP_f, "Remove an IP address from the "
					"server IP list. (removeip (ipnumber))");
	Cmd_AddCommand ("listip", SV_ListIP_f, "Print out the current list of IPs "
					"on the server list.");
	Cmd_AddCommand ("writeip", SV_WriteIP_f, "Record all IP addresses on the "
					"server IP list. The file name is listip.cfg");

	for (i = 0; i < MAX_MODELS; i++)
		snprintf (localmodels[i], sizeof (localmodels[i]), "*%i", i);

	Info_SetValueForStarKey (svs.info, "*version", QW_VERSION,
							 !sv_highchars);

	// Brand server as QF, with appropriate QSG standards version  --KB
	Info_SetValueForStarKey (svs.info, "*qf_version", PACKAGE_VERSION,
							 !sv_highchars);
	Info_SetValueForStarKey (svs.info, "*qsg_version", QW_QSG_VERSION,
							 !sv_highchars);

	CF_Init ();

	// init fraglog stuff
	svs.logsequence = 1;
	svs.logtime = realtime;
	svs.log[0].data = svs.log_buf[0];
	svs.log[0].maxsize = sizeof (svs.log_buf[0]);
	svs.log[0].cursize = 0;
	svs.log[0].allowoverflow = true;
	svs.log[1].data = svs.log_buf[1];
	svs.log[1].maxsize = sizeof (svs.log_buf[1]);
	svs.log[1].cursize = 0;
	svs.log[1].allowoverflow = true;
}

/*
	Master_Heartbeat

	Send a message to the master every few minutes to
	let it know we are alive, and log information
*/
#define	HEARTBEAT_SECONDS	300
void
Master_Heartbeat (void)
{
	char        string[2048];
	int         active, i;

	if (realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return;							// not time to send yet

	svs.last_heartbeat = realtime;

	// count active users
	active = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.clients[i].state >= cs_connected)
			active++;

	svs.heartbeat_sequence++;
	snprintf (string, sizeof (string), "%c\n%i\n%i\n", S2M_HEARTBEAT,
			  svs.heartbeat_sequence, active);


	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
		if (master_adr[i].port) {
			SV_Printf ("Sending heartbeat to %s\n",
						NET_AdrToString (master_adr[i]));
			Netchan_SendPacket (strlen (string), string, master_adr[i]);
		}
}

static inline qboolean
iswhitespace (char c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

/*
	SV_ExtractFromUserinfo

	Pull specific info from a newly changed userinfo string
	into a more C friendly form.
*/
void
SV_ExtractFromUserinfo (client_t *cl)
{
	const char *val;
	const char *p;
	char       *r;
	char        newname[MAX_NAME];
	client_t   *client;
	int         i;
	qboolean	badname = false;

	// name from the info string
	val = Info_ValueForKey (cl->userinfo, "name");

	// copy it, while converting \r, \n, and \t to space, trimming any
	// leading/trailing whitespace, and merging consecutive spaces
	for (p = val; iswhitespace (*p); p++);
	for (r = newname; *p && r != newname + sizeof (newname) - 1; p++) {
		if (iswhitespace (*p)) {
			if (!iswhitespace (p[1]) && p[1] != '\0')
				*r++ = *p;
		} else
			*r++ = *p;
	}
	*r = '\0';

	// empty (or only whitespace) name
	if (!*newname)
		badname = true;

	// impersonating an user-xxx name.  if they're using it
	// legitimately it'll be a no-op later on
	if (!strncasecmp (newname, "user-", 5))
		badname = true;

	// impersonating the admin
	if (strcasestr (newname, "console") || strcasestr (newname, "admin"))
		badname = true;

	// check for duplicate names
	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
		if (client->state != cs_spawned || client == cl)
			continue;
		if (!strcasecmp (client->name, val)) {
			badname = true;
			break;
		}
	}

	// give them a name we like
	if (badname)
		snprintf (newname, sizeof (newname), "user-%d", cl->userid);

	// set the name
	if (strcmp (newname, val) || strcmp (cl->name, newname)) {
		Info_SetValueForKey (cl->userinfo, "name", newname,
							 !sv_highchars);
		val = Info_ValueForKey (cl->userinfo, "name");
		SVstring (cl->edict, netname) = PR_SetString (&sv_pr_state, newname);

		// If the new name was not set (due to the info string being too
		// long), drop the client.
		if (strcmp (val, newname)) {
			SV_OutOfBandPrint (net_from, "%c\nPlease choose a "
							   "different name.\n", A2C_PRINT);
			SV_ClientPrintf (1, cl, PRINT_HIGH, "Please choose a "
							 "different name.\n");
			SV_Printf ("Client %d kicked for having a invalid name\n",
					   cl->userid);
			cl->drop = true;
			return;
		}

		// check for spamming
		if (!sv.paused) {
			if (!cl->lastnametime || realtime - cl->lastnametime > 5) {
				cl->lastnamecount = 0;
				cl->lastnametime = realtime;
			} else if (cl->lastnamecount++ > 4) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked for "
									"name spam\n", cl->name);
				SV_ClientPrintf (1, cl, PRINT_HIGH, "You were kicked "
								 "from the game for name spamming\n");
				cl->drop = true;
				return;
			}
		}

		// finally, report it to all our friends
		if (!cl->spectator && *cl->name)
			SV_BroadcastPrintf (PRINT_HIGH, "%s changed name to %s\n",
								cl->name, newname);
		strcpy (cl->name, newname);
	}

	// rate command
	val = Info_ValueForKey (cl->userinfo, "rate");
	if (strlen (val)) {
		i = atoi (val);

		if (sv_maxrate) {
			i = bound (500, i, sv_maxrate);
		} else {
			i = max (500, i);
		}
		cl->netchan.rate = 1.0 / i;
	}

	// msg command
	val = Info_ValueForKey (cl->userinfo, "msg");
	if (strlen (val)) {
		cl->messagelevel = atoi (val);
	}

	cl->stdver = atof (Info_ValueForKey (cl->userinfo, "*qsg_version"));
}

static void
SV_InitNet (void)
{
	int         port, p;

	SV_GenerateIPMasks ();

	port = PORT_SERVER;
	p = COM_CheckParm ("-port");
	if (p && p < com_argc) {
		port = atoi (com_argv[p + 1]);
		SV_Printf ("Port: %i\n", port);
	}
	NET_Init (port);

	Netchan_Init ();
	net_realtime = &realtime;

	Net_Log_Init (sv.sound_precache, 1);

	svs.last_heartbeat = -99999;		// send immediately
	sv_net_initialized = 1;
}

static memhunk_t *
SV_Init_Memory (void)
{
	int         mem_parm = COM_CheckParm ("-mem");
	size_t      mem_size;
	void       *mem_base;

	Cvar_Register (&sv_mem_size_cvar, 0, 0);
	if (mem_parm)
		Cvar_Set ("sv_mem_size", com_argv[mem_parm + 1]);

	if (COM_CheckParm ("-minmemory"))
		sv_mem_size = MINIMUM_MEMORY / (1024 * 1024.0);


	mem_size = (size_t) (sv_mem_size * 1024 * 1024);

	if (mem_size < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   mem_size / (float) 0x100000);

	mem_base = Sys_Alloc (mem_size);

	if (!mem_base)
		Sys_Error ("Can't allocate %zd", mem_size);

	return Memory_Init (mem_base, mem_size);
}

void
SV_Init (void)
{
	sv_cbuf = Cbuf_New (&id_interp);
	sv_args = Cbuf_ArgsNew ();

	Sys_RegisterShutdown (SV_Shutdown, 0);

	Sys_Init ();
	GIB_Init (true);
	COM_ParseConfig (sv_cbuf);

	cmd_warncmd = 1;

	// snax: Init experimental object system and run a test
	//Object_Init();

	memhunk_t  *hunk = SV_Init_Memory ();

	QFS_GamedirCallback (gamedir_f);
	svs.maxclients = MAX_CLIENTS;
	svs.info = Info_ParseString ("", MAX_SERVERINFO_STRING, 0);
	localinfo = Info_ParseString ("", 0, 0);	// unlimited
	SV_InitOperatorCommands ();
	SV_GIB_Init ();

	QFS_Init (hunk, "qw");
	PI_Init ();

	Cvar_Register (&sv_console_plugin_cvar, 0, 0);
	PI_RegisterPlugins (server_plugin_list);
	Con_Init (sv_console_plugin);
	if (con_module)
		con_module->data->console->cbuf = sv_cbuf;
	con_list_print = Sys_Printf;
	Sys_SetStdPrintf (SV_Print);
	Sys_SetErrPrintf (SV_Error);

	Game_Init_Cvars ();
	Mod_Init_Cvars ();
	Netchan_Init_Cvars ();
	Pmove_Init_Cvars ();

	// and now reprocess the cmdline's sets for overrides
	Cmd_StuffCmds (sv_cbuf);
	Cbuf_Execute_Sets (sv_cbuf);

	SV_Sbar_Init ();

	Game_Init ();

	SV_Progs_Init ();
	Mod_Init ();

	SV_InitNet ();

	SV_InitLocal ();
	Pmove_Init ();

	SVR_Init ();
	Demo_Init ();

	Hunk_AllocName (0, 0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark (0);

	Cbuf_InsertText (sv_cbuf, "exec server.cfg\n");

	host_initialized = true;

//	SV_Printf ("Exe: "__TIME__" "__DATE__"\n");
	SV_Printf ("%4.1f megabyte heap\n", sv_mem_size);

	SV_Printf ("\n");
	SV_Printf ("%s server, Version %s (build %04d)\n",
			   PACKAGE_NAME, PACKAGE_VERSION,
			   build_number ());
	SV_Printf ("\n");

	SV_Printf ("<==> %s initialized <==>\n", PACKAGE_NAME);

	// process command line arguments
	Cmd_Exec_File (sv_cbuf, fs_usercfg, 0);
	Cmd_StuffCmds (sv_cbuf);
	Cbuf_Execute_Stack (sv_cbuf);

	// if a map wasn't specified on the command line, spawn start.map
	if (sv.state == ss_dead)
		Cmd_ExecuteString ("map start", src_command);
	if (sv.state == ss_dead)
		Sys_Error ("Could not initialize server");
}
