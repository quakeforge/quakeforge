/*
	sv_ccmds.c

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

	$Id$
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

#include <stdlib.h>

#include "bothdefs.h"
#include "QF/cmd.h"
#include "QF/compat.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "server.h"
#include "sv_progs.h"
#include "QF/sys.h"
#include "QF/va.h"

qboolean    sv_allow_cheats;

int         fp_messages = 4, fp_persecond = 4, fp_secondsdead = 10;
char        fp_msg[255] = { 0 };
extern cvar_t *cl_warncmd;
extern redirect_t sv_redirected;


/*
	OPERATOR CONSOLE ONLY COMMANDS

	These commands can only be entered from stdin or by a remote operator
	datagram
*/

/*
	SV_SetMaster_f

	Make a master server current
*/
void
SV_SetMaster_f (void)
{
	char        data[2];
	int         i;

	memset (&master_adr, 0, sizeof (master_adr));

	for (i = 1; i < Cmd_Argc (); i++) {
		if (!strcmp (Cmd_Argv (i), "none")
			|| !NET_StringToAdr (Cmd_Argv (i), &master_adr[i - 1])) {
			Con_Printf ("Setting nomaster mode.\n");
			return;
		}
		if (master_adr[i - 1].port == 0)
			master_adr[i - 1].port = BigShort (27000);

		Con_Printf ("Master server at %s\n",
					NET_AdrToString (master_adr[i - 1]));

		Con_Printf ("Sending a ping.\n");

		data[0] = A2A_PING;
		data[1] = 0;
		NET_SendPacket (2, data, master_adr[i - 1]);
	}

	svs.last_heartbeat = -99999;
}


/*
	SV_Quit_f
*/
void
SV_Quit_f (void)
{
	SV_FinalMessage ("server shutdown\n");
	Con_Printf ("Shutting down.\n");
	SV_Shutdown ();
	Sys_Quit ();
}

/*
	SV_Logfile_f
*/
void
SV_Logfile_f (void)
{
	char        name[MAX_OSPATH];

	if (sv_logfile) {
		Con_Printf ("File logging off.\n");
		Qclose (sv_logfile);
		sv_logfile = NULL;
		return;
	}

	snprintf (name, sizeof (name), "%s/qconsole.log", com_gamedir);
	Con_Printf ("Logging text to %s.\n", name);
	sv_logfile = Qopen (name, "w");
	if (!sv_logfile)
		Con_Printf ("failed.\n");
}


/*
	SV_Fraglogfile_f
*/
void
SV_Fraglogfile_f (void)
{
	char        name[MAX_OSPATH];
	int         i;

	if (sv_fraglogfile) {
		Con_Printf ("Frag file logging off.\n");
		Qclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}
	// find an unused name
	for (i = 0; i < 1000; i++) {
		snprintf (name, sizeof (name), "%s/frag_%i.log", com_gamedir, i);
		sv_fraglogfile = Qopen (name, "r");
		if (!sv_fraglogfile) {			// can't read it, so create this one
			sv_fraglogfile = Qopen (name, "w");
			if (!sv_fraglogfile)
				i = 1000;				// give error
			break;
		}
		Qclose (sv_fraglogfile);
	}
	if (i == 1000) {
		Con_Printf ("Can't open any logfiles.\n");
		sv_fraglogfile = NULL;
		return;
	}

	Con_Printf ("Logging frags to %s.\n", name);
}


/*
	SV_SetPlayer

	Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
*/
qboolean
SV_SetPlayer (void)
{
	client_t   *cl;
	int         i;
	int         idnum;

	idnum = atoi (Cmd_Argv (1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == idnum) {
			host_client = cl;
			sv_player = host_client->edict;
			return true;
		}
	}
	Con_Printf ("Userid %i is not on the server\n", idnum);
	return false;
}


/*
	SV_God_f

	Sets client to godmode
*/
void
SV_God_f (void)
{
	if (!sv_allow_cheats) {
		Con_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SVFIELD (sv_player, flags, float) = (int) SVFIELD (sv_player, flags, float) ^ FL_GODMODE;
	if (!((int) SVFIELD (sv_player, flags, float) & FL_GODMODE))
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode OFF\n");
	else
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode ON\n");
}


void
SV_Noclip_f (void)
{
	if (!sv_allow_cheats) {
		Con_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (SVFIELD (sv_player, movetype, float) != MOVETYPE_NOCLIP) {
		SVFIELD (sv_player, movetype, float) = MOVETYPE_NOCLIP;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	} else {
		SVFIELD (sv_player, movetype, float) = MOVETYPE_WALK;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}


/*
	SV_Give_f
*/
void
SV_Give_f (void)
{
	char       *t;
	int         v;

	if (!sv_allow_cheats) {
		Con_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	t = Cmd_Argv (2);
	v = atoi (Cmd_Argv (3));

	switch (t[0]) {
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			SVFIELD (sv_player, items, float) =
				(int) SVFIELD (sv_player, items, float) | IT_SHOTGUN << (t[0] - '2');
			break;

		case 's':
			SVFIELD (sv_player, ammo_shells, float) = v;
			break;
		case 'n':
			SVFIELD (sv_player, ammo_nails, float) = v;
			break;
		case 'r':
			SVFIELD (sv_player, ammo_rockets, float) = v;
			break;
		case 'h':
			SVFIELD (sv_player, health, float) = v;
			break;
		case 'c':
			SVFIELD (sv_player, ammo_cells, float) = v;
			break;
	}
}

// Use this to keep track of current level  --KB
static char curlevel[MAX_QPATH] = "";

/*
	SV_Map_f

	handle a 
	map <mapname>
	command from the console or progs.
*/
void
SV_Map_f (void)
{
	char        level[MAX_QPATH];
	char        expanded[MAX_QPATH];
	QFile      *f;

	if (Cmd_Argc () != 2) {
		Con_Printf ("map <levelname> : continue game on a new level\n");
		return;
	}
	strncpy (level, Cmd_Argv (1), sizeof (level) - 1);
	level[sizeof (level) - 1] = 0;

	// check to make sure the level exists
	snprintf (expanded, sizeof (expanded), "maps/%s.bsp", level);
	COM_FOpenFile (expanded, &f);
	if (!f) {
		Con_Printf ("Can't find %s\n", expanded);
		// If curlevel == level, something is SCREWED!  --KB
		if (strcaseequal (level, curlevel))
			SV_Error ("map: cannot restart level\n");
		else
			Cbuf_AddText (va ("map %s", curlevel));
		return;
	}
	Qclose (f);

	SV_BroadcastCommand ("changing\n");
	SV_SendMessagesToAll ();

	strncpy (curlevel, level, sizeof (curlevel) - 1);
	curlevel[sizeof (curlevel) - 1] = 0;
	SV_SpawnServer (level);

	SV_BroadcastCommand ("reconnect\n");
}


/*
	SV_Kick_f

	Kick a user off of the server
*/
void
SV_Kick_f (void)
{
	int         i;
	client_t   *cl;
	int         uid;

	uid = atoi (Cmd_Argv (1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == uid) {
			SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
			// print directly, because the dropped client won't get the
			// SV_BroadcastPrintf message
			SV_ClientPrintf (cl, PRINT_HIGH, "You were kicked from the game\n");
			SV_DropClient (cl);
			return;
		}
	}

	Con_Printf ("Couldn't find user number %i\n", uid);
}


/*
	SV_Status_f
*/
void
SV_Status_f (void)
{
	int         i;
	client_t   *cl;
	float       cpu, avg, pak;
	char       *s;


	cpu = (svs.stats.latched_active + svs.stats.latched_idle);
	if (cpu)
		cpu = 100 * svs.stats.latched_active / cpu;
	avg = 1000 * svs.stats.latched_active / STATFRAMES;
	pak = (float) svs.stats.latched_packets / STATFRAMES;

	Con_Printf ("net address      : %s\n", NET_AdrToString (net_local_adr));
	Con_Printf ("cpu utilization  : %3i%%\n", (int) cpu);
	Con_Printf ("avg response time: %i ms\n", (int) avg);
	Con_Printf ("packets/frame    : %5.2f\n", pak);

// min fps lat drp
	if (sv_redirected != RD_NONE) {
		// most remote clients are 40 columns
		// 0123456789012345678901234567890123456789
		Con_Printf ("name               userid frags\n");
		Con_Printf ("  address          rate ping drop\n");
		Con_Printf ("  ---------------- ---- ---- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;

			Con_Printf ("%-16.16s  ", cl->name);

			Con_Printf ("%6i %5i", cl->userid, (int) SVFIELD (cl->edict, frags, float));
			if (cl->spectator)
				Con_Printf (" (s)\n");
			else
				Con_Printf ("\n");

			s = NET_BaseAdrToString (cl->netchan.remote_address);
			Con_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected) {
				Con_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				Con_Printf ("ZOMBIE\n");
				continue;
			}
			Con_Printf ("%4i %4i %5.2f\n", (int) (1000 * cl->netchan.frame_rate)
						, (int) SV_CalcPing (cl)
						,
						100.0 * cl->netchan.drop_count /
						cl->netchan.incoming_sequence);
		}
	} else {
		Con_Printf
			("frags userid address         name            rate ping drop  qport\n");
		Con_Printf
			("----- ------ --------------- --------------- ---- ---- ----- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;
			Con_Printf ("%5i %6i ", (int) SVFIELD (cl->edict, frags, float), cl->userid);

			s = NET_BaseAdrToString (cl->netchan.remote_address);

			Con_Printf ("%-15.15s ", s);

			Con_Printf ("%-15.15s ", cl->name);

			if (cl->state == cs_connected) {
				Con_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				Con_Printf ("ZOMBIE\n");
				continue;
			}
			Con_Printf ("%4i %4i %3.1f %4i",
						(int) (1000 * cl->netchan.frame_rate),
						(int) SV_CalcPing (cl),
						100.0 * cl->netchan.drop_count /
						cl->netchan.incoming_sequence, cl->netchan.qport);
			if (cl->spectator)
				Con_Printf (" (s)\n");
			else
				Con_Printf ("\n");


		}
	}
	Con_Printf ("\n");
}

/*
	SV_ConSay_f
*/
void
SV_ConSay_f (void)
{
	client_t   *client;
	int         j;
	char       *p;
	char        text[1024];

	if (Cmd_Argc () < 2)
		return;

	strcpy (text, "console: ");
	p = Cmd_Args ();

	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}

	strncat (text, p, sizeof (text) - strlen (text));

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf (client, PRINT_CHAT, "%s\n", text);
	}
}


/*
	SV_Heartbeat_f
*/
void
SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999;
}

void
SV_SendServerInfoChange (char *key, char *value)
{
	if (!sv.state)
		return;

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

/*
	SV_Serverinfo_f

	Examine or change the serverinfo string
*/
char       *CopyString (char *s);
void
SV_Serverinfo_f (void)
{
	cvar_t     *var;

	if (Cmd_Argc () == 1) {
		Con_Printf ("Server info settings:\n");
		Info_Print (svs.info);
		return;
	}

	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: serverinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*') {
		Con_Printf ("Star variables cannot be changed.\n");
		return;
	}
	Info_SetValueForKey (svs.info, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_SERVERINFO_STRING);

	// if this is a cvar, change it too 
	var = Cvar_FindVar (Cmd_Argv (1));
	if (var)
		Cvar_Set (var, Cmd_Argv (2));

	if (!var || !(var->flags & CVAR_SERVERINFO))
		// Cvar_Set will send the change if CVAR_SERVERINFO is set
		SV_SendServerInfoChange (Cmd_Argv (1), Cmd_Argv (2));
}


/*
	SV_Serverinfo_f

	Examine or change the serverinfo string
*/
char       *CopyString (char *s);
void
SV_Localinfo_f (void)
{
	if (Cmd_Argc () == 1) {
		Con_Printf ("Local info settings:\n");
		Info_Print (localinfo);
		return;
	}

	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: localinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*') {
		Con_Printf ("Star variables cannot be changed.\n");
		return;
	}
	Info_SetValueForKey (localinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_LOCALINFO_STRING);
}


/*
	SV_User_f

	Examine a users info strings
*/
void
SV_User_f (void)
{
	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: user <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Info_Print (host_client->userinfo);
}

/*
	SV_Gamedir

	Sets the fake *gamedir to a different directory.
*/
void
SV_Gamedir (void)
{
	char       *dir;

	if (Cmd_Argc () == 1) {
		Con_Printf ("Current *gamedir: %s\n",
					Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: sv_gamedir <newgamedir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":")) {
		Con_Printf ("*Gamedir should be a single filename, not a path\n");
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
	SV_Floodport_f

	Sets the gamedir and path to a different directory.
*/

void
SV_Floodprot_f (void)
{
	int         arg1, arg2, arg3;

	if (Cmd_Argc () == 1) {
		if (fp_messages) {
			Con_Printf
				("Current floodprot settings: \nAfter %d msgs per %d seconds, silence for %d seconds\n",
				 fp_messages, fp_persecond, fp_secondsdead);
			return;
		} else
			Con_Printf ("No floodprots enabled.\n");
	}

	if (Cmd_Argc () != 4) {
		Con_Printf
			("Usage: floodprot <# of messages> <per # of seconds> <seconds to silence>\n");
		Con_Printf
			("Use floodprotmsg to set a custom message to say to the flooder.\n");
		return;
	}

	arg1 = atoi (Cmd_Argv (1));
	arg2 = atoi (Cmd_Argv (2));
	arg3 = atoi (Cmd_Argv (3));

	if (arg1 <= 0 || arg2 <= 0 || arg3 <= 0) {
		Con_Printf ("All values must be positive numbers\n");
		return;
	}

	if (arg1 > 10) {
		Con_Printf ("Can only track up to 10 messages.\n");
		return;
	}

	fp_messages = arg1;
	fp_persecond = arg2;
	fp_secondsdead = arg3;
}

void
SV_Floodprotmsg_f (void)
{
	if (Cmd_Argc () == 1) {
		Con_Printf ("Current msg: %s\n", fp_msg);
		return;
	} else if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: floodprotmsg \"<message>\"\n");
		return;
	}
	snprintf (fp_msg, sizeof (fp_msg), "%s", Cmd_Argv (1));
}

/*
	SV_Snap
*/
void
SV_Snap (int uid)
{
	client_t   *cl;
	char        pcxname[80];
	char        checkname[MAX_OSPATH];
	int         i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS) {
		Con_Printf ("userid not found\n");
		return;
	}

	snprintf (pcxname, sizeof (pcxname), "%d-00.pcx", uid);

	snprintf (checkname, sizeof (checkname), "%s/snap", com_gamedir);
	COM_CreatePath (va ("%s/dummy", checkname));

	for (i = 0; i <= 99; i++) {
		pcxname[strlen (pcxname) - 6] = i / 10 + '0';
		pcxname[strlen (pcxname) - 5] = i % 10 + '0';
		snprintf (checkname, sizeof (checkname), "%s/snap/%s", com_gamedir,
				  pcxname);
		if (Sys_FileTime (checkname) == -1)
			break;						// file doesn't exist
	}
	if (i == 100) {
		Con_Printf ("Snap: Couldn't create a file, clean some out.\n");
		return;
	}
	strcpy (cl->uploadfn, checkname);

	memcpy (&cl->snap_from, &net_from, sizeof (net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	ClientReliableWrite_Begin (cl, svc_stufftext, 24);
	ClientReliableWrite_String (cl, "cmd snap\n");
	Con_Printf ("Requesting snap from user %d...\n", uid);
}

/*
	SV_Snap_f
*/
void
SV_Snap_f (void)
{
	int         uid;

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage:  snap <userid>\n");
		return;
	}

	uid = atoi (Cmd_Argv (1));

	SV_Snap (uid);
}

/*
	SV_Snap
*/
void
SV_SnapAll_f (void)
{
	client_t   *cl;
	int         i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_connected || cl->spectator)
			continue;
		SV_Snap (cl->userid);
	}
}

/*
	SV_InitOperatorCommands
*/
void
SV_InitOperatorCommands (void)
{
	if (COM_CheckParm ("-cheats")) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON",
								 MAX_SERVERINFO_STRING);
	}

	Cmd_AddCommand ("logfile", SV_Logfile_f, "Toggles logging of console text to qconsole.log");
	Cmd_AddCommand ("fraglogfile", SV_Fraglogfile_f, "Enables logging of kills to frag_##.log");

	Cmd_AddCommand ("snap", SV_Snap_f, "FIXME: Take a screenshot of userid? No Description");
	Cmd_AddCommand ("snapall", SV_SnapAll_f, "FIXME: No Description");
	Cmd_AddCommand ("kick", SV_Kick_f, "Remove a user from the server (kick userid)");
	Cmd_AddCommand ("status", SV_Status_f, "Report information on the current connected clients and the server - displays userids");

	Cmd_AddCommand ("map", SV_Map_f, "Change to a new map (map mapname)");
	Cmd_AddCommand ("setmaster", SV_SetMaster_f, "Lists the server with up to eight masters.\n"
		"When a server is listed with a master, the master is aware of the server's IP address and port and it is added to the\n"
		"list of current servers connected to a master. A heartbeat is sent to the master from the server to indicated that the\n"
		"server is still running and alive.\n"
   "\n"
		"Examples:\n"
		"setmaster 192.246.40.12:27002\n"
		"setmaster 192.246.40.12:27002 192.246.40.12:27004");

	Cmd_AddCommand ("say", SV_ConSay_f, "Say something to everyone on the server, will show up as the name 'console' in game");
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f, "Force a heartbeat to be sent to the master server.\n"
		"A heartbeat tells the Master the server's IP address and that it is still alive.");
	Cmd_AddCommand ("quit", SV_Quit_f, "Shut down the server");
	Cmd_AddCommand ("god", SV_God_f, "Toggle god cheat to userid (god userid) Requires cheats are enabled");
	Cmd_AddCommand ("give", SV_Give_f, "Give userid items, or health.\n"
		"Items: 1 Axe, 2 Shotgun, 3 Double-Barrelled Shotgun, 4 Nailgun, 5 Super Nailgun, 6 Grenade Launcher, 7 Rocket Launcher,\n"
		"8 ThunderBolt, C Cells, H Health, N Nails, R Rockets, S Shells.	Requires cheats are enabled. (give userid item amount)");
	Cmd_AddCommand ("noclip", SV_Noclip_f, "Toggle no clipping cheat for userid. Requires cheats are enabled. (noclip userid)");
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f, "Reports or sets information about server.\n"
		"The information stored in this space is broadcast on the network to all players.\n"
		"Values:\n"
		"dq - Drop Quad Damage when a player dies.\n"
		"dr - Drop Ring of Shadows when a player dies.\n"
		"rj - Sets the multiplier rate for splash damage kick.\n"
		"needpass - Displays the passwords enabled on the server.\n"
		"watervis - Toggle the use of r_watervis by OpenGL clients.\n"
		"Note: Keys with (*) in front cannot be changed. Maximum key size cannot exceed 64-bytes.\n"
		"Maximum size for all keys cannot exceed 512-bytes.\n"
		"(serverinfo key value)");
		
	Cmd_AddCommand ("localinfo", SV_Localinfo_f, "Shows or sets localinfo variables.\n"
		"Useful for mod programmers who need to allow the admin to change settings.\n"
		"This is an alternative storage space to the serverinfo space for mod variables.\n"
		"The variables stored in this space are not broadcast on the network.\n"
		"This space also has a 32-kilobyte limit which is much greater then the 512-byte limit on the serverinfo space.\n"
		"Special Keys: (current map) (next map) - Using this combination will allow the creation of a custom map cycle without editing code.\n"
		"\n"
		"Example:\n"
		"localinfo dm2 dm4\n"
		"localinfo dm4 dm6\n"
		"localinfo dm6 dm2\n"
		"(localinfo key value)");

	Cmd_AddCommand ("user", SV_User_f, "Report information about the user (user userid)");
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir, "Displays or determines the value of the serverinfo *gamedir variable.\n"
		"Note: Useful when the physical gamedir directory has a different name than the widely accepted gamedir directory.\n"
		"Example:\n"
		"gamedir tf2_5; sv_gamedir fortress\n"
		"gamedir ctf4_2; sv_gamedir ctf\n"
		"(sv_gamedir dirname)");

	Cmd_AddCommand ("floodprot", SV_Floodprot_f, "Sets the options for flood protection.\n"
		"Default: 4 4 10\n"
		"(floodprot (number of messages) (number of seconds) (silence time in seconds))");

	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f, "Sets the message displayed after flood protection is invoked (floodprotmsg message)");
	Cmd_AddCommand ("maplist", COM_Maplist_f, "List all maps on the server");

	cl_warncmd =
		Cvar_Get ("cl_warncmd", "1", CVAR_NONE, NULL,
				"Toggles the display of error messages for unknown commands"); 
																			// poor 
																			// description
}
