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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef __sun
/* Sun's model_t in sys/model.h conflicts w/ Quake's model_t */
# define model_t sunmodel_t
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_WINSOCK_H
# include <winsock.h>
#endif

#include <setjmp.h>

#ifdef __sun
# undef model_t
#endif

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/plugin.h"
#include "QF/qendian.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/vid.h"
#include "QF/va.h"
#include "QF/vfs.h"

#include "compat.h"
#include "bothdefs.h"
#include "buildnum.h"
#include "cl_cam.h"
#include "cl_demo.h"
#include "cl_ents.h"
#include "cl_input.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_pred.h"
#include "cl_skin.h"
#include "cl_slist.h"
#include "cl_tent.h"
#include "client.h"
#include "game.h"
#include "host.h"
#include "net.h"
#include "pmove.h"
#include "r_cvar.h"
#include "sbar.h"
#include "view.h"

void        CL_RemoveQFInfoKeys ();

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean    noclip_anglehack;			// remnant from old quake


cvar_t     *fs_globalcfg;
cvar_t     *fs_usercfg;
cvar_t     *rcon_password;

cvar_t     *rcon_address;

cvar_t     *cl_writecfg;
cvar_t     *cl_allow_cmd_pkt;

cvar_t     *cl_timeout;

cvar_t     *cl_shownet;
cvar_t     *cl_autoexec;
cvar_t     *cl_sbar;
cvar_t     *cl_sbar_separator;
cvar_t     *cl_hudswap;
cvar_t     *cl_maxfps;

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
cvar_t     *cl_predict_players2;
cvar_t     *cl_solid_players;

cvar_t     *localid;

cvar_t    *cl_port;

static qboolean allowremotecmd = true;

/*  info mirrors */
cvar_t     *password;
cvar_t     *spectator;
cvar_t     *cl_name;
cvar_t     *team;
cvar_t     *rate;
cvar_t     *noaim;
cvar_t     *msg;

extern cvar_t *cl_hightrack;

extern void R_Particles_Init_Cvars (void);

client_static_t cls;
client_state_t cl;

entity_state_t cl_baselines[MAX_EDICTS];
efrag_t     cl_efrags[MAX_EFRAGS];
entity_t    cl_static_entities[MAX_STATIC_ENTITIES];

double      connect_time = -1;			// for connection retransmits

quakeparms_t host_parms;
qboolean    host_initialized;			// true if into command execution
qboolean    nomaster;

double      host_frametime;
double      realtime;					// without any filtering or bounding
double      oldrealtime;				// last frame run
int         host_framecount;

int         host_hunklevel;

byte       *vid_basepal;
byte       *vid_colormap;

cvar_t     *host_speeds;
cvar_t     *show_fps;
cvar_t     *show_time;
cvar_t     *cl_demospeed;

int         fps_count;

jmp_buf     host_abort;

void        Master_Connect_f (void);

char       *server_version = NULL;		// version of server we connected to

char        emodel_name[] = "emodel";
char        pmodel_name[] = "pmodel";
char        prespawn_name[] = "prespawn %i 0 %i";
char        modellist_name[] = "modellist %i %i";
char        soundlist_name[] = "soundlist %i %i";

cvar_t     *confirm_quit;

void        CL_RSShot_f (void);

void
CL_Sbar_f (cvar_t *var)
{
	vid.recalc_refdef = true;
	r_lineadj = var->int_val ? sb_lines : 0;
}

void
CL_Quit_f (void)
{
	// FIXME: MENUCODE
//	if (confirm_quit->int_val) {
//		M_Menu_Quit_f ();
//		return;
//	}
	CL_Disconnect ();
	Sys_Quit ();
}


void
CL_Version_f (void)
{
	Con_Printf ("%s Version %s\n", PROGRAM, VERSION);
	Con_Printf ("Binary: " __TIME__ " " __DATE__ "\n");
}


/*
	CL_SendConnectPacket

	called by CL_Connect_f and CL_CheckResend
*/
void
CL_SendConnectPacket (void)
{
	netadr_t    adr;
	char        data[2048];
	double      t1, t2;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//       Now, adds lookup time to the connect time.
//       Should I add it to realtime instead?!?!

	if (cls.state != ca_disconnected)
		return;

	t1 = Sys_DoubleTime ();

	if (!NET_StringToAdr (cls.servername, &adr)) {
		Con_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (!NET_IsClientLegal (&adr)) {
		Con_Printf ("Illegal server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	cls.qport = Cvar_VariableValue ("qport");

	// Arrgh, this was not in the old binary only release, and eats up
	// far too much of the 196 chars in the userinfo space, leaving nothing
	// for player use, thus, its commented out for the moment..
	// 
	// Info_SetValueForStarKey (cls.userinfo, "*ip", NET_AdrToString(adr),
	// MAX_INFO_STRING);

//  Con_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cconnect %i %i %i \"%s\"\n",
			  255, 255, 255, 255, PROTOCOL_VERSION, cls.qport, cls.challenge,
			  cls.userinfo);
	NET_SendPacket (strlen (data), data, adr);
}


/*
	CL_CheckForResend

	Resend a connect message if the last one has timed out
*/
void
CL_CheckForResend (void)
{
	netadr_t    adr;
	char        data[2048];
	double      t1, t2;

	if (connect_time == -1)
		return;
	if (cls.state != ca_disconnected)
		return;
	if (connect_time && realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Con_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}
	if (!NET_IsClientLegal (&adr)) {
		Con_Printf ("Illegal server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	VID_SetCaption (va ("Connecting to %s", cls.servername));
	Con_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cgetchallenge\n", 255, 255, 255,
			  255);
	NET_SendPacket (strlen (data), data, adr);
}


void
CL_BeginServerConnect (void)
{
	connect_time = 0;
	CL_CheckForResend ();
}


void
CL_Connect_f (void)
{
	char       *server;

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect ();

	strncpy (cls.servername, server, sizeof (cls.servername) - 1);
	CL_BeginServerConnect ();
}


/*
	CL_Rcon_f

	Send the rest of the command line over as
	an unconnected command.
*/
void
CL_Rcon_f (void)
{
	char        message[1024];
	int         i;
	netadr_t    to;
	int         len;

	snprintf (message, sizeof (message), "\377\377\377\377rcon %s ",
			  rcon_password->string);
	len = strlen (message);

	for (i = 1; i < Cmd_Argc (); i++) {
		strncat (message, Cmd_Argv (i), sizeof (message) - len - 1);
		len += strlen (Cmd_Argv (i));
		strncat (message, " ", sizeof (message) - len - 1);
		len += strlen (" ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!strlen (rcon_address->string)) {
			Con_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
	}

	NET_SendPacket (strlen (message) + 1, message, to);
}


void
CL_ClearState (void)
{
	S_StopAllSounds (true);

	Con_DPrintf ("Clearing memory\n");
	D_FlushCaches ();
	Mod_ClearAll ();
	if (host_hunklevel)					// FIXME: check this...
		Hunk_FreeToLowMark (host_hunklevel);

	CL_ClearEnts ();
	CL_ClearTEnts ();

	R_ClearEfrags ();
	R_ClearDlights ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	SZ_Clear (&cls.netchan.message);

// clear other arrays   
	memset (cl_efrags, 0, sizeof (cl_efrags));
	memset (r_lightstyle, 0, sizeof (r_lightstyle));
}


/*
	CL_StopCshifts

	Cleans the Cshifts, so your screen doesn't stay red after a timedemo :)
*/
void
CL_StopCshifts (void)
{
	int i;
	for (i = 0; i < NUM_CSHIFTS; i++)
		cl.cshifts[i].percent = 0;
	for (i = 0; i < MAX_CL_STATS; i++)
		cl.stats[i] = 0;
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

	connect_time = -1;

	VID_SetCaption ("Disconnected");

	// stop sounds (especially looping!)
	S_StopAllSounds (true);

	// Clean the Cshifts
	CL_StopCshifts ();

	// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state != ca_disconnected) {
		if (cls.demorecording)
			CL_Stop_f ();

		final[0] = clc_stringcmd;
		strcpy (final + 1, "drop");
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);

		cls.state = ca_disconnected;

		cls.demoplayback = cls.demorecording = cls.timedemo = false;

		CL_RemoveQFInfoKeys ();
	}
	Cam_Reset ();

	if (cls.download) {
		Qclose (cls.download);
		cls.download = NULL;
	}

	CL_StopUpload ();

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
void
CL_User_f (void)
{
	int         uid;
	int         i;

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: user <username / userid>\n");
		return;
	}

	uid = atoi (Cmd_Argv (1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid
			|| !strcmp (cl.players[i].name, Cmd_Argv (1))) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Con_Printf ("User not in server.\n");
}


/*
	CL_Users_f

	Dump userids for all current players
*/
void
CL_Users_f (void)
{
	int         i;
	int         c;

	c = 0;
	Con_Printf ("userid frags name\n");
	Con_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Con_Printf ("%6i %4i %s\n", cl.players[i].userid,
						cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Con_Printf ("%i total users\n", c);
}


/*
	CL_FullServerinfo_f

	Sent by server when serverinfo changes
*/
void
CL_FullServerinfo_f (void)
{
	char       *p;

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	Con_DPrintf ("Cmd_Argv(1): '%s'\n", Cmd_Argv (1));
	strcpy (cl.serverinfo, Cmd_Argv (1));
	Con_DPrintf ("cl.serverinfo: '%s'\n", cl.serverinfo);

	if ((p = Info_ValueForKey (cl.serverinfo, "*qf_version")) && *p) {
		if (server_version == NULL)
			Con_Printf ("QuakeForge Version %s Server\n", p);
		server_version = strdup (p);
	} else if ((p = Info_ValueForKey (cl.serverinfo, "*version")) && *p) {
		if (server_version == NULL)
			Con_Printf ("Version %s Server\n", p);
		server_version = strdup (p);
	}

	if ((p = Info_ValueForKey (cl.serverinfo, "*qsg_version")) && *p) {
		if ((cl.stdver = atoi (p)))
			Con_Printf ("QSG Standard version %i\n", cl.stdver);
		else
			Con_Printf ("Invalid standards version: %s", p);
	}
	if ((p = Info_ValueForKey (cl.serverinfo, "skybox")) && *p) {
		//FIXME didn't actually do anything anyway
	}
}


void
CL_AddQFInfoKeys (void)
{
	char        cap[100] = "";			// max of 98 or so flags

	// set the capabilities info. single char flags (possibly with
	// modifiefs)
	// defined capabilities (* = not implemented):
	// z   client can accept gzipped files.
	// h    * http transfers
	// f    * ftp transfers
	// a    * audio channel (voice chat)
	// i    * irc
	// p    pogo stick control
	// t    team messages
	strncpy (cap, "pt", sizeof (cap));
#ifdef HAVE_ZLIB
	strncat (cap, "z", sizeof (cap) - strlen (cap) - 1);
#endif
	Info_SetValueForStarKey (cls.userinfo, "*cap", cap, MAX_INFO_STRING);
	Info_SetValueForStarKey (cls.userinfo, "*qf_version", VERSION,
							 MAX_INFO_STRING);
	Info_SetValueForStarKey (cls.userinfo, "*qsg_version", QW_QSG_VERSION,
							 MAX_INFO_STRING);
	Con_Printf ("QuakeForge server detected\n");
}


void
CL_RemoveQFInfoKeys (void)
{
	Info_RemoveKey (cls.userinfo, "*cap");
	Info_RemoveKey (cls.userinfo, "*qf_version");
	Info_RemoveKey (cls.userinfo, "*qsg_version");
}


/*
	CL_FullInfo_f

	Allow clients to change userinfo
	Casey was here :)
*/
void
CL_FullInfo_f (void)
{
	char        key[512];
	char        value[512];
	char       *o;
	char       *s;

	if (Cmd_Argc () != 2) {
		Con_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv (1);
	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s) {
			Con_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (strcaseequal (key, pmodel_name) || strcaseequal (key, emodel_name))
			continue;

		Info_SetValueForKey (cls.userinfo, key, value, MAX_INFO_STRING);
	}
}


/*
	CL_SetInfo_f

	Allow clients to change userinfo
*/
void
CL_SetInfo_f (void)
{
	if (Cmd_Argc () == 1) {
		Info_Print (cls.userinfo);
		return;
	}
	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (strcaseequal (Cmd_Argv (1), pmodel_name)
		|| strcaseequal (Cmd_Argv (1), emodel_name))
		return;

	Info_SetValueForKey (cls.userinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_INFO_STRING);
	if (cls.state >= ca_connected)
		Cmd_ForwardToServer ();
}


/*
	CL_Packet_f

	packet <destination> <contents>

	Contents allows \n escape character
*/
void
CL_Packet_f (void)
{
	char        send[2048];
	int         i, l;
	char       *in, *out;
	netadr_t    adr;

	if (Cmd_Argc () != 3) {
		Con_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv (1), &adr)) {
		Con_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv (2);
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

	NET_SendPacket (out - send, send, adr);
}


/*
	CL_NextDemo

	Called to play the next demo in the demo loop
*/
void
CL_NextDemo (void)
{
	char        str[1024];

	if (cls.demonum == -1)
		return;							// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
//          Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	snprintf (str, sizeof (str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}


/*
	CL_Changing_f

	Just sent as a hint to the client that they should
	drop to full console
*/
void
CL_Changing_f (void)
{
	if (cls.download)					// don't change when downloading
		return;

	S_StopAllSounds (true);
	cl.intermission = 0;
	cls.state = ca_connected;			// not active anymore, but not
										// disconnected
	Con_Printf ("\nChanging map...\n");
}


/*
	CL_Reconnect_f

	The server is changing levels
*/
void
CL_Reconnect_f (void)
{
	if (cls.download)					// don't change when downloading
		return;

	S_StopAllSounds (true);

	if (cls.state == ca_connected) {
		Con_Printf ("reconnecting...\n");
		VID_SetCaption ("Reconnecting");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) {
		Con_Printf ("No server to reconnect to...\n");
		return;
	}

	CL_Disconnect ();
	CL_BeginServerConnect ();
}


/*
	CL_ConnectionlessPacket

	Responses to broadcasts, etc
*/
void
CL_ConnectionlessPacket (void)
{
	char       *s;
	int         c, clcp_temp;

	MSG_BeginReading (net_message);
	MSG_ReadLong (net_message);					// skip the -1

	c = MSG_ReadByte (net_message);
	clcp_temp = 0;
	if (!cls.demoplayback)
		Con_Printf ("%s: ", NET_AdrToString (net_from));
//  Con_DPrintf ("%s", net_message.data + 5);
	if (c == S2C_CONNECTION) {
		Con_Printf ("connection\n");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Con_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (&cls.netchan, net_from, cls.qport);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		Con_Printf ("Connected.\n");
		allowremotecmd = false;			// localid required now for remote
										// cmds
		return;
	}
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND) {
		char        cmdtext[2048];

		Con_Printf ("client command\n");

		if (!cl_allow_cmd_pkt->int_val
		     || ((*(unsigned int *) net_from.ip != *(unsigned int *) net_local_adr.ip
				 && *(unsigned int *) net_from.ip != htonl (INADDR_LOOPBACK)))) {
			Con_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		s = MSG_ReadString (net_message);

		strncpy (cmdtext, s, sizeof (cmdtext) - 1);
		cmdtext[sizeof (cmdtext) - 1] = 0;

		s = MSG_ReadString (net_message);

		while (*s && isspace ((int) *s))
			s++;
		while (*s && isspace ((int) (s[strlen (s) - 1])))
			s[strlen (s) - 1] = 0;

		if (!allowremotecmd
			&& (!*localid->string || strcmp (localid->string, s))) {
			if (!*localid->string) {
				Con_Printf ("===========================\n");
				Con_Printf ("Command packet received from local host, but no "
							"localid has been set.  You may need to upgrade your server "
							"browser.\n");
				Con_Printf ("===========================\n");
				return;
			}
			Con_Printf ("===========================\n");
			Con_Printf
				("Invalid localid on command packet received from local host. "
				 "\n|%s| != |%s|\n"
				 "You may need to reload your server browser and %s.\n", s,
				 localid->string, PROGRAM);
			Con_Printf ("===========================\n");
			Cvar_Set (localid, "");
			return;
		}

		Con_Printf ("%s\n", cmdtext);
		Cbuf_AddText (cmdtext);
		allowremotecmd = false;
		return;
	}
	// print command from somewhere
	if (c == A2C_PRINT) {
		netadr_t addy;
		server_entry_t *temp;
		s = MSG_ReadString (net_message);
		for (temp = slist; temp; temp = temp->next)
			if (temp->waitstatus)
			{
				NET_StringToAdr (temp->server, &addy);
				if (NET_CompareBaseAdr (net_from, addy))
				{
					int i;
					temp->status = realloc(temp->status, strlen(s) + 1);
					strcpy(temp->status, s);
					temp->waitstatus = 0;
					for (i = 0; i < strlen(temp->status); i++)
						if (temp->status[i] == '\n')
						{
							temp->status[i] = '\\';
							break;
						}
					Con_Printf("status response\n");
					return;
				}
			} 
		Con_Print (s);
		return;
	}
	// ping from somewhere
	if (c == A2A_PING) {
		char        data[6];

		Con_Printf ("ping\n");

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		NET_SendPacket (6, &data, net_from);
		return;
	}

	if (c == S2C_CHALLENGE) {
		Con_Printf ("challenge\n");

		s = MSG_ReadString (net_message);
		cls.challenge = atoi (s);
		if (strstr (s, "QF"))
			CL_AddQFInfoKeys ();
		CL_SendConnectPacket ();
		return;
	}

	if (c == M2C_MASTER_REPLY)
	{
		Con_Printf("Master Server Reply\n");
		clcp_temp = MSG_ReadByte (net_message);
		s = MSG_ReadString (net_message);
		MSL_ParseServerList(s);
		return;
	}
	if (c == svc_disconnect) {
		if (cls.demoplayback)
			Host_EndGame ("End of demo");
		else
			Con_Printf ("svc_disconnect\n");
//      Host_EndGame ("Server disconnected");
		return;
	}

	Con_Printf ("unknown:  %c\n", c);
}


void
CL_PingPacket (void)
{
	server_entry_t *temp;
	netadr_t addy;
	MSG_ReadByte (net_message);;
	for (temp = slist; temp; temp = temp->next)
		if (temp->pingsent && !temp->pongback) {
			NET_StringToAdr (temp->server, &addy);
			if (NET_CompareBaseAdr (net_from, addy)) {
				temp->pongback = Sys_DoubleTime ();
				timepassed(temp->pingsent, &temp->pongback);
			}
		} 
}		


void
CL_ReadPackets (void)
{
//  while (NET_GetPacket ())
	while (CL_GetMessage ()) {

		if (cls.demoplayback && net_packetlog->int_val)
			Log_Incoming_Packet(net_message->message->data,net_message->message->cursize);

		// remote command packet
		if (*(int *) net_message->message->data == -1) {
			CL_ConnectionlessPacket ();
			continue;
		}
		if (*(char *) net_message->message->data == A2A_ACK)
		{
			CL_PingPacket ();
			continue;
		}
		if (net_message->message->cursize < 8) {
			Con_Printf ("%s: Runt packet\n", NET_AdrToString (net_from));
			continue;
		}

		// packet from server
		if (!cls.demoplayback &&
			!NET_CompareAdr (net_from, cls.netchan.remote_address)) {
			Con_DPrintf ("%s:sequenced packet without connection\n",
						 NET_AdrToString (net_from));
			continue;
		}
		if (!Netchan_Process (&cls.netchan))
			continue;					// wasn't accepted for some reason
		CL_ParseServerMessage ();

//      if (cls.demoplayback && cls.state >= ca_active && !CL_DemoBehind())
//          return;
	}

	// 
	// check timeout
	// 
	if (cls.state >= ca_connected
		&& realtime - cls.netchan.last_received > cl_timeout->value) {
		Con_Printf ("\nServer connection timed out.\n");
		CL_Disconnect ();
		return;
	}

}


void
CL_Download_f (void)
{
	if (cls.state == ca_disconnected) {
		Con_Printf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: download <datafile>\n");
		return;
	}

	snprintf (cls.downloadname, sizeof (cls.downloadname), "%s/%s", com_gamedir,
			  Cmd_Argv (1));

	COM_CreatePath (cls.downloadname);

	strncpy (cls.downloadtempname, cls.downloadname,
			 sizeof (cls.downloadtempname));
	cls.download = Qopen (cls.downloadname, "wb");
	if (cls.download) {
		cls.downloadtype = dl_single;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, va ("download %s\n", Cmd_Argv (1)));
	} else {
		Con_Printf ("error downloading %s: %s\n", Cmd_Argv (1),
		            strerror (errno));
	}
}


void
CL_Init (void)
{
	char        st[80];

	cls.state = ca_disconnected;

	Info_SetValueForKey (cls.userinfo, "name", "unnamed", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "topcolor", "0", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "bottomcolor", "0", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "rate", "2500", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "msg", "1", MAX_INFO_STRING);
//  snprintf (st, sizeof(st), "%s-%04d", QW_VERSION, build_number());
	snprintf (st, sizeof (st), "%s", QW_VERSION);
	Info_SetValueForStarKey (cls.userinfo, "*ver", st, MAX_INFO_STRING);
	Info_SetValueForStarKey (cls.userinfo, "stdver", QW_QSG_VERSION,
							 MAX_INFO_STRING);

	CL_Input_Init ();
	CL_Ents_Init ();
	CL_TEnts_Init ();
	Pmove_Init ();

	SList_Init ();
	
// register our commands
	Cmd_AddCommand ("version", CL_Version_f, "Report version information");
	Cmd_AddCommand ("changing", CL_Changing_f, "Used when maps are changing");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "Disconnect from server");
	Cmd_AddCommand ("record", CL_Record_f, "Record a demo 'record filename "
					"server'");
	Cmd_AddCommand ("rerecord", CL_ReRecord_f, "Rerecord a demo on the same "
					"server");
	Cmd_AddCommand ("snap", CL_RSShot_f, "Take a screenshot and upload it to the server");
	Cmd_AddCommand ("stop", CL_Stop_f, "Stop recording a demo");
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "Play a recorded demo");
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f, "Play a demo as fast as your "
					"hardware can. Useful for benchmarking.");
	Cmd_AddCommand ("maplist", COM_Maplist_f, "List maps available");
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
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f, "Used by QuakeSpy and Qlist to "
					"set setinfo variables");
	Cmd_AddCommand ("fullserverinfo", CL_FullServerinfo_f, "Used by QuakeSpy "
					"and Qlist to obtain server variables");
	Cmd_AddCommand ("download", CL_Download_f, "Manually download a quake "
					"file from the server");
	Cmd_AddCommand ("nextul", CL_NextUpload, "Tells the client to send the "
					"next upload");
	Cmd_AddCommand ("stopul", CL_StopUpload, "Tells the client to stop "
					"uploading");

// forward to server commands
	Cmd_AddCommand ("kill", Cmd_ForwardToServer, "Suicide :)");
	Cmd_AddCommand ("pause", Cmd_ForwardToServer, "Pause the game");
	Cmd_AddCommand ("say", Cmd_ForwardToServer, "Say something to all other "
					"players");
	Cmd_AddCommand ("say_team", Cmd_ForwardToServer, "Say something only to "
					"people on your team");
	Cmd_AddCommand ("serverinfo", Cmd_ForwardToServer, "Report the current "
					"server info");
}


void
CL_Init_Cvars (void)
{
	cl_model_crcs = Cvar_Get ("cl_model_crcs", "1", CVAR_ARCHIVE, NULL,
		"Controls setting of emodel and pmodel info vars. Required by some "
		"servers, but clearing this can make the difference between "
		"connecting and not connecting on some others.");
	confirm_quit = Cvar_Get ("confirm_quit", "1", CVAR_ARCHIVE, NULL,
							 "confirm quit command");
	cl_allow_cmd_pkt = Cvar_Get ("cl_allow_cmd_pkt", "1", CVAR_NONE, NULL,
								 "enables packets from the likes of gamespy");
	cl_autoexec = Cvar_Get ("cl_autoexec", "0", CVAR_ROM, NULL,
							"exec autoexec.cfg on gamedir change");
	cl_cshift_bonus = Cvar_Get ("cl_cshift_bonus", "1", CVAR_ARCHIVE, NULL,
							"Show bonus flash on item pickup");
	cl_cshift_contents = Cvar_Get ("cl_cshift_content", "1", CVAR_ARCHIVE,
								   NULL, "Shift view colors for contents "
								   "(water, slime, etc)");
	cl_cshift_damage = Cvar_Get ("cl_cshift_damage", "1", CVAR_ARCHIVE, NULL,
							"Shift view colors on damage");
	cl_cshift_powerup = Cvar_Get ("cl_cshift_powerup", "1", CVAR_ARCHIVE, NULL,
							"Shift view colors for powerups");
	cl_demospeed = Cvar_Get ("cl_demospeed", "1.0", CVAR_NONE, NULL,
							 "adjust demo playback speed. 1.0 = normal, "
							 "< 1 slow-mo, > 1 timelapse");
	cl_warncmd = Cvar_Get ("cl_warncmd", "0", CVAR_NONE, NULL,
						   "inform when execing a command");
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL,
								 "turn `run' speed multiplier");
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL,
							 "backward speed");
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
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL,
						   "show network packets. 0=off, 1=basic, 2=verbose");
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, CL_Sbar_f, "status bar mode");
	cl_sbar_separator = Cvar_Get ("cl_sbar_separator", "0", CVAR_ARCHIVE, NULL,
								  "turns on status bar separator");
	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL,
						   "new HUD on left side?");
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL,
						  "maximum frames rendered in one second. 0 == 32");
	cl_timeout = Cvar_Get ("cl_timeout", "60", CVAR_ARCHIVE, NULL, "server "
						   "connection timeout (since last packet received)");
	host_speeds = Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL,
							"display host processing times");
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL, "Snap view "
						   "to center when moving and no mlook/klook");
	m_forward = Cvar_Get ("m_forward", "1", CVAR_NONE, NULL,
						  "mouse forward/back speed");
	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL,
						"mouse pitch (up/down) multipier");
	m_side = Cvar_Get ("m_side", "0.8", CVAR_NONE, NULL, "mouse strafe speed");
	m_yaw = Cvar_Get ("m_yaw", "0.022", CVAR_NONE, NULL,
					  "mouse yaw (left/right) multiplier");
	rcon_password = Cvar_Get ("rcon_password", "", CVAR_NONE, NULL,
							  "remote control password");
	rcon_address = Cvar_Get ("rcon_address", "", CVAR_NONE, NULL, "server IP "
							 "address when client not connected - for "
							 "sending rcon commands");
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL,
						 "display realtime frames per second");
	show_time = Cvar_Get ("show_time", "0", CVAR_NONE, NULL,
						  "display the current time");
	cl_predict_players2 = Cvar_Get ("cl_predict_players2", "1", CVAR_NONE,
									NULL, "If this and cl_predict_players are "
									"0, no player prediction is done");
	cl_predict_players = Cvar_Get ("cl_predict_players", "1", CVAR_NONE, NULL,
								   "If this and cl_predict_players2 is 0, no "
								   "player prediction is done");
	cl_solid_players = Cvar_Get ("cl_solid_players", "1", CVAR_NONE, NULL,
								 "Are players solid? If off, you can walk "
								 "through them with difficulty");
	localid = Cvar_Get ("localid", "", CVAR_NONE, NULL, "FIXME: This has "
						"something to do with client authentication."
						"No description");

	// info mirrors
	//
	cl_name = Cvar_Get ("name", "unnamed", CVAR_ARCHIVE | CVAR_USERINFO,
						Cvar_Info, "Player name");
	password = Cvar_Get ("password", "", CVAR_USERINFO, Cvar_Info,
						 "Server password");
	spectator = Cvar_Get ("spectator", "", CVAR_USERINFO, Cvar_Info,
						  "Set to 1 before connecting to become a spectator");
	team = Cvar_Get ("team", "", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					 "Team player is on.");
	rate = Cvar_Get ("rate", "2500", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					 "Amount of bytes per second server will send/download "
					 "to you");
	msg = Cvar_Get ("msg", "1", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					"Determines the type of messages reported 0 is maximum, "
					"4 is none");
	noaim = Cvar_Get ("noaim", "0", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
					  "Auto aim off switch. Set to 1 to turn off.");
	cl_port = Cvar_Get ("cl_port", PORT_CLIENT, CVAR_NONE, Cvar_Info,
						"UDP Port for client to use.");

	R_Particles_Init_Cvars ();
}


/*
	Host_EndGame

	Call this to drop to a console without exiting the qwcl
*/
void
Host_EndGame (char *message, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, message);
	vsnprintf (string, sizeof (string), message, argptr);
	va_end (argptr);
	Con_Printf ("\n===========================\n");
	Con_Printf ("Host_EndGame: %s\n", string);
	Con_Printf ("===========================\n\n");

	CL_Disconnect ();

	longjmp (host_abort, 1);
}


/*
	Host_Error

	This shuts down the client and exits qwcl
*/
void
Host_Error (char *error, ...)
{
	va_list     argptr;
	char        string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n", string);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

// FIXME
	Sys_Error ("Host_Error: %s\n", string);
}


/*
	Host_WriteConfiguration

	Writes key bindings and archived cvars to config.cfg
*/
void
Host_WriteConfiguration (void)
{
	VFile      *f;

	if (cl_writecfg->int_val && host_initialized) {
		char       *path = va ("%s/config.cfg", com_gamedir);

		COM_CreatePath (path);
		f = Qopen (path, "w");
		if (!f) {
			Con_Printf ("Couldn't write config.cfg.\n");
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
	float       fps;
	float		timescale = 1.0;
	float		timedifference;

	if (cls.demoplayback) {
		timescale = max (0, cl_demospeed->value);
		time *= timescale;
	}

	realtime += time;
	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cl_maxfps->int_val)
		fps = bound (1, cl_maxfps->value, 72);
	else
		fps = bound (30, rate->value / 80.0, 72);

	timedifference = (timescale / fps) - (realtime - oldrealtime);

	if (!cls.timedemo && (timedifference > 0))
		return timedifference;					// framerate is too high
	return 0;
}


/*
	Host_Frame

	Runs all active servers
*/
int         nopacketcount;
void
Host_Frame (float time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int         pass1, pass2, pass3;
	float sleeptime;

	if (setjmp (host_abort))	// something bad happened, or the server disconnected
		return;

	// decide the simulation time
	if ((sleeptime = Host_SimulationTime (time)) != 0) {
#ifdef HAVE_USLEEP
		if (sleeptime > 0.01) // minimum sleep time
			usleep((unsigned long)((sleeptime - 0.001) * 1000000));
#endif
		return;					// framerate is too high
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
	host_frametime = min (host_frametime, 0.2);

	// get new key events
	IN_SendKeyEvents ();

	// allow mouses or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// fetch results from server
	CL_ReadPackets ();

	if (atoi (Info_ValueForKey (cl.serverinfo, "no_pogo_stick")))
		Cvar_Set (no_pogo_stick, "1");

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected) {
		CL_CheckForResend ();
	} else
		CL_SendCmd ();

	// Set up prediction for other players
	CL_SetUpPlayerPrediction (false);

	// do client side motion prediction
	CL_PredictMove ();

	// Set up prediction for other players
	CL_SetUpPlayerPrediction (true);

	// build a refresh entity list
	CL_EmitEntities ();

	// update video
	if (host_speeds->int_val)
		time1 = Sys_DoubleTime ();

	r_inhibit_viewmodel = (!Cam_DrawViewModel ()
						   || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
						   || cl.stats[STAT_HEALTH] <= 0);
	r_force_fullscreen = cl.intermission;
	r_paused = cl.paused;
	r_active = cls.state == ca_active;
	r_view_model = &cl.viewent;
	r_frametime = host_frametime;

	// don't allow cheats in multiplayer
	if (!atoi (Info_ValueForKey (cl.serverinfo, "watervis")))
		Cvar_SetValue (r_wateralpha, 1);

	CL_UpdateScreen (realtime);

	if (host_speeds->int_val)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		S_Update (r_origin, vpn, vright, vup);
		R_DecayLights (host_frametime);
	} else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update ();

	if (host_speeds->int_val) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	host_framecount++;
	fps_count++;
}


static int
check_quakerc (void)
{
	VFile *f;
	char *l, *p;
	int ret = 1;

	COM_FOpenFile ("quake.rc", &f);
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


void
Host_Init (void)
{
	if (COM_CheckParm ("-minmemory"))
		host_parms.memsize = MINIMUM_MEMORY;

	if (host_parms.memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				   host_parms.memsize / (float) 0x100000);

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Memory_Init (host_parms.membase, host_parms.memsize);
	Cvar_Init ();
	Sys_Init_Cvars ();
	Sys_Init ();

	Cbuf_Init ();
	Cmd_Init ();
	Locs_Init ();

	// execute +set as early as possible
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();

	// execute the global configuration file if it exists
	// would have been nice if Cmd_Exec_f could have been used, but it
	// only reads from within the quake file system, and changing that is
	// probably Not A Good Thing (tm).
	fs_globalcfg = Cvar_Get ("fs_globalcfg", FS_GLOBALCFG, CVAR_ROM, NULL,
			"global configuration file");
	Cmd_Exec_File (fs_globalcfg->string);
	Cbuf_Execute_Sets ();

	// execute +set again to override the config file
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();

	fs_usercfg = Cvar_Get ("fs_usercfg", FS_USERCFG, CVAR_ROM, NULL,
						   "user configuration file");
	Cmd_Exec_File (fs_usercfg->string);
	Cbuf_Execute_Sets ();

	// execute +set again to override the config file
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();

	PI_Init ();

	CL_Cam_Init_Cvars ();
	CL_Input_Init_Cvars ();
	CL_Skin_Init_Cvars ();
	CL_Init_Cvars ();
	CL_Prediction_Init_Cvars ();
	COM_Init_Cvars ();
	Con_Init_Cvars ();
	COM_Filesystem_Init_Cvars ();
	Game_Init_Cvars ();
	IN_Init_Cvars ();
	Key_Init_Cvars ();
	Mod_Init_Cvars ();
	Netchan_Init_Cvars ();
	Pmove_Init_Cvars ();
	R_Init_Cvars ();
	S_Init_Cvars ();
	Team_Init_Cvars ();
	V_Init_Cvars ();
	VID_Init_Cvars ();

	cl_Cmd_Init ();
	V_Init ();
	COM_Filesystem_Init ();
	Game_Init ();
	COM_Init ();

	NET_Init (cl_port->int_val);
	Netchan_Init ();
	{
		static char *sound_precache[MAX_MODELS];
		int i;

		for (i = 0; i < MAX_MODELS; i++)
			sound_precache[i] = cl.sound_name[i];
		Net_Log_Init (sound_precache);
	}

	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	Con_Init ();
	Mod_Init ();

//  Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf ("%4.1f megs RAM used.\n", host_parms.memsize / (1024 * 1024.0));

	vid_basepal = (byte *) COM_LoadHunkFile ("gfx/palette.lmp");
	if (!vid_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	vid_colormap = (byte *) COM_LoadHunkFile ("gfx/colormap.lmp");
	if (!vid_colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");
#ifdef __linux__
	CDAudio_Init ();
	VID_Init (vid_basepal);
	IN_Init ();
	Draw_Init ();
	SCR_Init ();
	R_Init ();

	S_Init ();

	cls.state = ca_disconnected;
	Sbar_Init ();
	CL_Skin_Init ();
	CL_Init ();
#else
	VID_Init (vid_basepal);
	Draw_Init ();
	SCR_Init ();
	R_Init ();

	S_Init ();

	cls.state = ca_disconnected;
	CDAudio_Init ();
	Sbar_Init ();
	CL_Skin_Init ();
	CL_Init ();
	IN_Init ();
#endif

	Cbuf_InsertText ("exec quake.rc\n");
	Cmd_Exec_File (fs_usercfg->string);
	// Reparse the command line for + commands.
	// (Note, no non-base commands exist yet)
	if (check_quakerc ()) {
		Cmd_StuffCmds_f ();
	}

	Cbuf_AddText
		("echo Type connect <internet address> or use a server browser to connect to a game.\n");
	Cbuf_AddText ("cl_warncmd 1\n");

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	Con_Printf ("\nClient version %s (build %04d)\n\n", VERSION,
				build_number ());

	Con_Printf ("ÄÅÅÇ %s initialized ÄÅÅÇ\n", PROGRAM);

	CL_UpdateScreen (realtime);
}


/*
	Host_Shutdown

	FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
	to run quit through here before the final handoff to the sys code.
*/
void
Host_Shutdown (void)
{
	static qboolean isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	SL_Shutdown (slist);

	Host_WriteConfiguration ();

	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	if (vid_basepal)
		VID_Shutdown ();
}
