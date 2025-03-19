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
#include <ctype.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "qw/bothdefs.h"
#include "qw/include/server.h"
#include "qw/include/sv_demo.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_qtv.h"
#include "qw/include/sv_recorder.h"

bool        sv_allow_cheats;

int         fp_messages = 4, fp_persecond = 4, fp_secondsdead = 10;
char        fp_msg[255] = { 0 };
int sv_leetnickmatch;
static cvar_t sv_leetnickmatch_cvar = {
	.name = "sv_3133735_7h4n_7h0u",
	.description =
		"Match '1' as 'i' and such in nicks",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_leetnickmatch },
};

static bool
match_char (char a, char b)
{
	a = tolower ((byte) sys_char_map[(byte) a]);
	b = tolower ((byte) sys_char_map[(byte) b]);

	if (a == b || (sv_leetnickmatch
		&& (   (a == '1' && b == 'i') || (a == 'i' && b == '1')
			|| (a == '1' && b == 'l') || (a == 'l' && b == '1')
			|| (a == '3' && b == 'e') || (a == 'e' && b == '3')
			|| (a == '4' && b == 'a') || (a == 'a' && b == '4')
			|| (a == '5' && b == 'g') || (a == 'g' && b == '5')
			|| (a == '5' && b == 'r') || (a == 'r' && b == '5')
			|| (a == '7' && b == 't') || (a == 't' && b == '7')
			|| (a == '9' && b == 'p') || (a == 'p' && b == '9')
			|| (a == '0' && b == 'o') || (a == 'o' && b == '0')
			|| (a == '$' && b == 's') || (a == 's' && b == '$'))))
		return true;
	return false;
}

/*
    FIXME: this function and it's callers are getting progressively
    uglier as more features are added :)
*/
static client_t *
SV_Match_User (const char *substr)
{
	int         i, j, uid;
	int         count = 0;
	char       *str;
	client_t   *cl;
	client_t   *match = 0;

	if (!substr || !*substr)
		return 0;
	uid = strtol (substr, &str, 10);

	if (!*str) {
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (cl->state < cs_zombie)
				continue;
			if (cl->userid == uid) {
				SV_Printf ("User %04d matches with name: %s\n",
							cl->userid, cl->name);
				return cl;
			}
		}
		SV_Printf ("No such uid: %d\n", uid);
		return 0;
	}

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_zombie)
			continue;
		for (str = cl->name; *str && !match_char (*str, substr[0]); str++)
			;
		while (*str) {
			for (j = 0; substr[j] && str[j]; j++)
				if (!match_char (substr[j], str[j]))
					break;
			if (!substr[j]) {		// found a match;
				match = cl;
				count++;
				SV_Printf ("User %04d matches with name: %s\n",
							cl->userid, cl->name);
				break;
			}
			str++;
		}
	}
	if (count != 1) {
		SV_Printf ("No unique matches, ignoring command!\n");
		return 0;
	}
	return match;
}

/*
	OPERATOR CONSOLE ONLY COMMANDS

	These commands can be entered only from stdin or by a remote operator
	datagram
*/

/*
	SV_SetMaster_f

	Make a master server current
*/
static void
SV_SetMaster_f (void)
{
	char        data[2];
	int         i;

	memset (&master_adr, 0, sizeof (master_adr));

	for (i = 1; i < Cmd_Argc (); i++) {
		if (i > MAX_MASTERS) {
			SV_Printf ("Too many masters specified. Using only the first %d\n",
					   MAX_MASTERS);
			break;
		}
		if (!strcmp (Cmd_Argv (i), "none")
			|| !NET_StringToAdr (Cmd_Argv (i), &master_adr[i - 1])) {
			SV_Printf ("Setting nomaster mode.\n");
			return;
		}
		if (master_adr[i - 1].port == 0)
			master_adr[i - 1].port = BigShort (27000);

		SV_Printf ("Master server at %s\n",
					NET_AdrToString (master_adr[i - 1]));

		SV_Printf ("Sending a ping.\n");

		data[0] = A2A_PING;
		data[1] = 0;
		Netchan_SendPacket (2, data, master_adr[i - 1]);
	}

	svs.last_heartbeat = -99999;
}

#define RESTART_CLSTUFF \
	"wait;wait;wait;wait;wait;"\
	"wait;wait;wait;wait;wait;"\
	"wait;wait;wait;reconnect\n"

static void
SV_Restart_f (void)
{
	client_t   *client;
	int         j;

	SZ_Clear (net_message->message);

	MSG_WriteByte (net_message->message, svc_print);
	MSG_WriteByte (net_message->message, PRINT_HIGH);
	MSG_WriteString (net_message->message,
		"\x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9f\n"
		" Server \xf2\xe5\xf3\xf4\xe1\xf2\xf4 engaged\xae\xae\xae\n"
		"\x9d\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9e\x9e\x9e\x9e\x9e\x9e"
		"\x9e\x9e\x9f\n\n");
	MSG_WriteByte (net_message->message, svc_stufftext);
	MSG_WriteString (net_message->message, RESTART_CLSTUFF);
	MSG_WriteByte (net_message->message, svc_disconnect);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state >= cs_spawned)
			Netchan_Transmit (&client->netchan, net_message->message->cursize,
							  net_message->message->data);
	}
	Sys_Printf ("Shutting down: server restart, shell must relaunch server\n");
	SV_Shutdown (0);
	// Error code 2 on exit, indication shell must restart the server
	exit (2);
}

static void
SV_Quit_f (void)
{
	SV_FinalMessage ("server shutdown\n");
	SV_Printf ("Shutting down.\n");
	Sys_Quit ();
}

static void
SV_Fraglogfile_f (void)
{
	dstring_t  *name;

	if (sv_fraglogfile) {
		SV_Printf ("Frag file logging off.\n");
		Qclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}
	name = dstring_new ();
	if (!(sv_fraglogfile = QFS_NextFile (name,
										 va (0, "%s/frag_",
											 qfs_gamedir->dir.def), ".log"))) {
		SV_Printf ("Can't open any logfiles.\n%s\n", name->str);
	} else {
		SV_Printf ("Logging frags to %s.\n", name->str);
	}
	dstring_delete (name);
}

/*
	SV_SetPlayer

	Sets host_client and sv_player to the player with idnum Cmd_Argv (1)
*/
static bool
SV_SetPlayer (void)
{
	client_t   *cl;
	int         i, idnum;

	idnum = atoi (Cmd_Argv (1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_zombie)
			continue;
		if (cl->userid == idnum) {
			host_client = cl;
			sv_player = host_client->edict;
			return true;
		}
	}
	SV_Printf ("Userid %i is not on the server\n", idnum);
	return false;
}

/*
	SV_God_f

	Sets client to godmode
*/
static void
SV_God_f (void)
{
	if (!sv_allow_cheats) {
		SV_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SVfloat (sv_player, flags) = (int) SVfloat (sv_player, flags) ^ FL_GODMODE;
	if (!((int) SVfloat (sv_player, flags) & FL_GODMODE))
		SV_ClientPrintf (1, host_client, PRINT_HIGH, "godmode OFF\n");
	else
		SV_ClientPrintf (1, host_client, PRINT_HIGH, "godmode ON\n");
}

static void
SV_Noclip_f (void)
{
	if (!sv_allow_cheats) {
		SV_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (SVfloat (sv_player, movetype) != MOVETYPE_NOCLIP) {
		SVfloat (sv_player, movetype) = MOVETYPE_NOCLIP;
		SV_ClientPrintf (1, host_client, PRINT_HIGH, "noclip ON\n");
	} else {
		SVfloat (sv_player, movetype) = MOVETYPE_WALK;
		SV_ClientPrintf (1, host_client, PRINT_HIGH, "noclip OFF\n");
	}
}

static void
SV_Give_f (void)
{
	const char *t;
	int         v;

	if (!sv_allow_cheats) {
		SV_Printf
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
			SVfloat (sv_player, items) =
				(int) SVfloat (sv_player, items) | IT_SHOTGUN << (t[0] - '2');
			break;

		case 's':
			SVfloat (sv_player, ammo_shells) = v;
			break;
		case 'n':
			SVfloat (sv_player, ammo_nails) = v;
			break;
		case 'r':
			SVfloat (sv_player, ammo_rockets) = v;
			break;
		case 'h':
			SVfloat (sv_player, health) = v;
			break;
		case 'c':
			SVfloat (sv_player, ammo_cells) = v;
			break;
	}
}

// Use this to keep track of current level  --KB
static dstring_t *curlevel;

const char *
SV_Current_Map (void)
{
	return curlevel ? curlevel->str : "";
}

static const char *
nice_time (double time)
{
	int         t = time + 0.5;

#if 0 //FIXME ditch or cvar?
	if (t < 60) {
		return va (0, "%ds", t);
	} else if (t < 600) {
		return va (0, "%dm%02ds", t / 60, t % 60);
	} else if (t < 3600) {
		return va (0, "%dm", t / 60);
	} else if (t < 36000) {
		t /= 60;
		return va (0, "%dh%02dm", t / 60, t % 60);
	} else if (t < 86400) {
		return va (0, "%dh", t / 3600);
	} else {
		t /= 3600;
		return va (0, "%dd%02dh", t / 24, t % 24);
	}
#endif
	if (t < 60) {
		return va (0, "%ds", t);
	} else if (t < 3600) {
		return va (0, "%dm%02ds", t / 60, t % 60);
	} else if (t < 86400) {
		return va (0, "%dh%02dm%02ds", t / 3600, (t / 60) % 60, t % 60);
	} else {
		return va (0, "%dd%02dh%02dm%02ds",
				   t / 86400, (t / 3600) % 24, (t / 60) % 60, t % 60);
	}
}

/*
	SV_Map_f

	handle a
	map <mapname>
	command from the console or progs.
*/
static void
SV_Map_f (void)
{
	const char *level;
	char       *expanded;
	QFile      *f;

	if (!curlevel)
		curlevel = dstring_newstr ();

	if (Cmd_Argc () > 2) {
		SV_Printf ("map <levelname> : continue game on a new level\n");
		return;
	}
	if (Cmd_Argc () == 1) {
		SV_Printf ("map is %s \"%s\" (%s)\n", curlevel->str,
				   PR_GetString (&sv_pr_state, SVstring (sv.edicts, message)),
				   nice_time (sv.time));
		return;
	}
	level = Cmd_Argv (1);

	// check to make sure the level exists
	expanded = nva ("maps/%s.bsp", level);
	f = QFS_FOpenFile (expanded);
	if (!f) {
		SV_Printf ("Can't find %s\n", expanded);
		free (expanded);
		return;
	}
	Qclose (f);
	free (expanded);

	if (sv.recording_demo)
		SV_Stop (0);

	SV_qtvChanging ();
	SV_BroadcastCommand ("changing\n");
	SV_SendMessagesToAll ();

	dstring_copystr (curlevel, level);
	SV_SpawnServer (level);

	SV_qtvReconnect ();
	SV_BroadcastCommand ("reconnect\n");
}

/*
	SV_Kick_f

	Kick a user off of the server
*/
static void
SV_Kick_f (void)
{
	client_t   *cl;
	int         argc = Cmd_Argc ();
	const char *reason;

	if (argc < 2) {
		SV_Printf ("usage: kick <name/userid>\n");
		return;
	}
	if (!(cl = SV_Match_User (Cmd_Argv (1))))
		return;

	// print directly, because the dropped client won't get the
	// SV_BroadcastPrintf message
	if (argc > 2) {
		reason = Cmd_Args (2);
		SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked: %s\n", cl->name,
							reason);
		SV_ClientPrintf (1, cl, PRINT_HIGH,
						 "You were kicked from the game: %s\n", reason);
	} else {
		SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
		SV_ClientPrintf (1, cl, PRINT_HIGH, "You were kicked from the game\n");
	}
	SV_DropClient (cl);
}

void
SV_Status_f (void)
{
	int         i;
	client_t   *cl;
	float       cpu, avg, pak, demo = 0;
	const char *s;


	cpu = (svs.stats.latched_active + svs.stats.latched_idle);
	if (cpu) {
		demo = 100 * svs.stats.latched_demo / cpu;
		cpu = 100 * svs.stats.latched_active / cpu;
	}
	avg = 1000 * svs.stats.latched_active / STATFRAMES;
	pak = (float) svs.stats.latched_packets / STATFRAMES;

	SV_Printf ("net address      : %s\n", NET_AdrToString (net_local_adr));
	SV_Printf ("uptime           : %s\n",
			   nice_time (Sys_DoubleTime () - Sys_DoubleTimeBase ()));
	SV_Printf ("cpu utilization  : %3i%% (%3i%%)\n", (int) cpu, (int)demo);
	SV_Printf ("avg response time: %i ms\n", (int) avg);
	SV_Printf ("packets/frame    : %5.2f\n", pak);

	// min fps lat drp
	if (sv_redirected != RD_NONE && sv_redirected != RD_MOD) {
		// most remote clients are 40 columns
		//          0123456789012345678901234567890123456789
		SV_Printf ("name               userid frags\n");
		SV_Printf ("  address          rate ping drop\n");
		SV_Printf ("  ---------------- ---- ---- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;

			SV_Printf ("%-16.16s  ", cl->name);

			SV_Printf ("%6i %5i", cl->userid, (int) SVfloat (cl->edict, frags));
			if (cl->spectator)
				SV_Printf (" (s)\n");
			else
				SV_Printf ("\n");

			s = NET_BaseAdrToString (cl->netchan.remote_address);
			SV_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected) {
				SV_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				SV_Printf ("ZOMBIE\n");
				continue;
			}
			if (cl->state == cs_server) {
				SV_Printf ("SERVER %d\n", cl->ping);
				continue;
			}
			SV_Printf ("%4i %4i %5.2f\n",
					   (int) (1000 * cl->netchan.frame_rate),
					   (int) SV_CalcPing (cl),
					   100.0 * cl->netchan.drop_count /
							cl->netchan.incoming_sequence);
		}
	} else {
		SV_Printf ("frags userid address         name            rate ping "
				   "drop  qport\n");
		SV_Printf ("----- ------ --------------- --------------- ---- ---- "
				   "----- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;
			SV_Printf ("%5i %6i ", (int) SVfloat (cl->edict, frags),
					   cl->userid);

			s = NET_BaseAdrToString (cl->netchan.remote_address);

			SV_Printf ("%-15.15s ", s);

			SV_Printf ("%-15.15s ", cl->name);

			if (cl->state == cs_connected) {
				SV_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				SV_Printf ("ZOMBIE\n");
				continue;
			}
			if (cl->state == cs_server) {
				SV_Printf ("SERVER %d\n", cl->ping);
				continue;
			}
			SV_Printf ("%4i %4i %4.1f%% %5i",
						(int) (1000 * cl->netchan.frame_rate),
						(int) SV_CalcPing (cl),
						100.0 * cl->netchan.drop_count /
						cl->netchan.incoming_sequence, cl->netchan.qport);
			if (cl->spectator)
				SV_Printf (" (s)\n");
			else
				SV_Printf ("\n");
		}
	}
	SV_Printf ("\n");
}

#define MAXPENALTY 10.0

static void
SV_Punish (int mode)
{
	int         i;
	double      mins = 0.5;
	bool        all = false, done = false;
	client_t    *cl = 0;
	dstring_t  *text = dstring_new();
	const char *cmd = 0;
	const char *cmd_do = 0;
	//FIXME const char *cmd_undo = 0;
	int         field_offs = 0;

	switch (mode) {
		case 0:
			cmd = "cuff";
			cmd_do = "cuffed";
			//FIXME cmd_undo = "un-cuffed";
			field_offs = offsetof (client_t, cuff_time);
			break;
		case 1:
			cmd = "mute";
			cmd_do = "muted";
			//FIXME cmd_undo = "can speak";
			field_offs = offsetof (client_t, lockedtill);
			break;
	}

	if (Cmd_Argc () != 2 && Cmd_Argc () != 3) {
		SV_Printf ("usage: %s <name/userid/ALL> [minutes]\n"
				   "       (default = 0.5, 0 = cancel cuff).\n", cmd);
		return;
	}

	if (strequal (Cmd_Argv (1), "ALL")) {
		all = true;
	} else {
		cl = SV_Match_User (Cmd_Argv (1));
	}
	if (!all && !cl)
		return;
	if (Cmd_Argc () == 3) {
		mins = atof (Cmd_Argv (2));
		if (mins < 0.0 || mins > MAXPENALTY)
			mins = MAXPENALTY;
	}
	if (cl) {
		*(double *)((char *)cl + field_offs) = realtime + mins * 60.0;
		if (mins) {
			dsprintf (text, "You are %s for %.1f minutes\n\n"
					 "reconnecting won't help...\n", cmd_do, mins);
			MSG_ReliableWrite_Begin (&cl->backbuf, svc_centerprint,
									 2 + strlen (text->str));
			MSG_ReliableWrite_String (&cl->backbuf, text->str);
		}
	}
	if (all) {
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (cl->state < cs_zombie)
				continue;
			*(double *)((char *)cl + field_offs) = realtime + mins * 60.0;
			done = true;
			if (mins) {
				dsprintf (text, "You are %s for %.1f minutes\n\n"
						 "reconnecting won't help...\n", cmd_do, mins);
				MSG_ReliableWrite_Begin (&cl->backbuf, svc_centerprint,
										 2 + strlen (text->str));
				MSG_ReliableWrite_String (&cl->backbuf, text->str);
			}
		}
	}
	if (done) {
		if (mins)
			SV_BroadcastPrintf (PRINT_HIGH, "%s %s for %.1f minutes.\n",
								all? "All Users" : cl->name, cmd_do, mins);
		else
			SV_BroadcastPrintf (PRINT_HIGH, "%s %s.\n",
								all? "All Users" : cl->name, cmd_do);
	}
	dstring_delete (text);
}

static void
SV_Cuff_f (void)
{
	SV_Punish (0);
}


static void
SV_Mute_f (void)
{
	SV_Punish (1);
}

static void
SV_Ban_f (void)
{
	double      mins = 30.0, m;
	client_t   *cl;
	char       *e;
	const char *a, *reason;
	int         argc = Cmd_Argc (), argr = 2;

	if (argc < 2) {
		SV_Printf ("usage: ban <name/userid> [minutes] [reason]\n"
				   "       (default = 30, 0 = permanent).\n");
		return;
	}
	if (!(cl = SV_Match_User (Cmd_Argv (1))))
		return;
	if (argc >= 3) {
		a = Cmd_Argv (2);
		m = strtod (a, &e);
		if (e != a) {
			argr++;
			mins = m;
			if (mins < 0.0 || mins > 1000000.0)			// bout 2 yrs
				mins = 0.0;
		}
	}
	if (argc > argr) {
		reason = Cmd_Args (argr);
		SV_BroadcastPrintf (PRINT_HIGH, "Admin Banned user %s %s: %s\n",
							cl->name, mins ? va (0, "for %.1f minutes", mins)
										   : "permanently", reason);
	} else {
		SV_BroadcastPrintf (PRINT_HIGH, "Admin Banned user %s %s\n",
							cl->name, mins ? va (0, "for %.1f minutes", mins)
										   : "permanently");
	}
	SV_DropClient (cl);
	Cmd_ExecuteString (va (0, "addip %s %f",
						   NET_BaseAdrToString (cl->netchan.remote_address),
						   mins), src_command);
}

static void
SV_Match_f (void)
{
	if (Cmd_Argc () != 2) {
		SV_Printf ("usage: match <name/userid>\n");
		return;
	}

	if (!SV_Match_User (Cmd_Argv (1)))
		SV_Printf ("No unique uid/name matched\n");
}


static void
SV_ConSay (const char *prefix, client_t *client)
{
	char       *p;
	dstring_t  *text;
	int         j;

	if (Cmd_Argc () < 2)
		return;

	p = Hunk_TempAlloc (0, strlen (Cmd_Args (1)) + 1);
	strcpy (p, Cmd_Args (1));
	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}
	text = dstring_new ();
	dstring_copystr (text, prefix);
	dstring_appendstr (text, "\x8d ");				// arrow
	j = strlen (text->str);
	dstring_appendstr (text, p);
	SV_Printf ("%s\n", text->str);
	while (text->str[j])
		text->str[j++] |= 0x80;				// non-bold text

	if (client) {
		if (client->state >= cs_zombie) {
			SV_ClientPrintf (1, client, PRINT_CHAT, "\n"); // bell
			SV_ClientPrintf (1, client, PRINT_HIGH, "%s\n", text->str);
			SV_ClientPrintf (1, client, PRINT_CHAT, "%s", ""); // bell
		}
	} else {
		for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
			if (client->state < cs_zombie)
				continue;
			SV_ClientPrintf (0, client, PRINT_HIGH, "%s\n", text->str);
			if (*prefix != 'I')		// beep, except for Info says
				SV_ClientPrintf (0, client, PRINT_CHAT, "%s", "");
		}
		if (sv.recorders) {
			sizebuf_t *dbuf;
			dbuf = SVR_WriteBegin (dem_all, 0, strlen (text->str) + 7);
			MSG_WriteByte (dbuf, svc_print);
			MSG_WriteByte (dbuf, PRINT_HIGH);
			MSG_WriteString (dbuf, va (0, "%s\n", text->str));
			MSG_WriteByte (dbuf, svc_print);
			MSG_WriteByte (dbuf, PRINT_CHAT);
			MSG_WriteString (dbuf, "");
		}
	}
}

static void
SV_Tell_f (void)
{
	client_t   *cl;

	if (Cmd_Argc () < 3) {
		SV_Printf ("usage: tell <name/userid> <text...>\n");
		return;
	}
	if (!(cl = SV_Match_User (Cmd_Argv (1))))
		return;

	if (rcon_from_user)
		SV_ConSay ("[\xd0\xd2\xc9\xd6\xc1\xd4\xc5] Admin", cl);
	else
		SV_ConSay ("[\xd0\xd2\xc9\xd6\xc1\xd4\xc5] Console", cl);
}

static void
SV_ConSay_f (void)
{
	if (rcon_from_user)
		SV_ConSay ("Admin", 0);
	else
		SV_ConSay ("Console", 0);
}

static void
SV_ConSay_Info_f (void)
{
	SV_ConSay ("Info", 0);
}

static void
SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999;
}

static void
SV_SendServerInfoChange (const char *key, const char *value)
{
	if (!sv.state)
		return;

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

void
Cvar_Info (void *data, const cvar_t *cvar)
{
	if (cvar->flags & CVAR_SERVERINFO) {
		const char *cvar_str = Cvar_VarString (cvar);
		Info_SetValueForKey (svs.info, cvar->name, cvar_str, !sv_highchars);
		SV_SendServerInfoChange (cvar->name, cvar_str);
	}
}

/*
	SV_Serverinfo_f

	Examine or change the serverinfo string
*/
static void
SV_Serverinfo_f (void)
{
	cvar_t     *var;
	const char *key;
	const char *value;

	switch (Cmd_Argc ()) {
		case 1:
			SV_Printf ("Server info settings:\n");
			Info_Print (svs.info);
			return;
		case 2:
			key = Cmd_Argv (1);
			value = Info_ValueForKey (svs.info, key);
			if (Info_Key (svs.info, key))
				SV_Printf ("Server info %s: \"%s\"\n", key, value);
			else
				SV_Printf ("No such key %s\n", Cmd_Argv(1));
			return;
		case 3:
			key = Cmd_Argv (1);
			value = Cmd_Argv (2);
			break;
		default:
			SV_Printf ("usage: serverinfo [ <key> <value> ]\n");
			return;
	}

	if (Cmd_Argv (1)[0] == '*') {
		SV_Printf ("Star variables cannot be changed.\n");
		return;
	}

	// if this is a cvar, change it too
	var = Cvar_FindVar (key);
	if (var && (var->flags & CVAR_SERVERINFO)) {
		Cvar_SetVar (var, value);
	} else {
		Info_SetValueForKey (svs.info, key, value, !sv_highchars);
		SV_SendServerInfoChange (key, value);
	}
}

void
SV_SetLocalinfo (const char *key, const char *value)
{
	char       *oldvalue = 0;

	if (sv_funcs.LocalinfoChanged)
		oldvalue = strdup (Info_ValueForKey (localinfo, key));

	if (*value)
		Info_SetValueForKey (localinfo, key, value, !sv_highchars);
	else
		Info_RemoveKey (localinfo, key);

	if (sv_funcs.LocalinfoChanged) {
		*sv_globals.time = sv.time;
		*sv_globals.self = 0;
		PR_PushFrame (&sv_pr_state);
		auto params = PR_SaveParams (&sv_pr_state);
		PR_SetupParams (&sv_pr_state, 3, 1);
		P_STRING (&sv_pr_state, 0) = PR_SetTempString (&sv_pr_state, key);
		P_STRING (&sv_pr_state, 1) = PR_SetTempString (&sv_pr_state, oldvalue);
		P_STRING (&sv_pr_state, 2) = PR_SetTempString (&sv_pr_state, value);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.LocalinfoChanged);
		PR_RestoreParams (&sv_pr_state, params);
		PR_PopFrame (&sv_pr_state);
	}

	if (oldvalue)
		free (oldvalue);
}

/*
	SV_Serverinfo_f

	Examine or change the serverinfo string
*/
static void
SV_Localinfo_f (void)
{
	const char *key;
	const char *value;

	switch (Cmd_Argc ()) {
		case 1:
			SV_Printf ("Local info settings:\n");
			Info_Print (localinfo);
			return;
		case 2:
			key = Cmd_Argv (1);
			value = Info_ValueForKey (localinfo, key);
			if (Info_Key (localinfo, key))
				SV_Printf ("Localinfo %s: \"%s\"\n", key, value);
			else
				SV_Printf ("No such key %s\n", Cmd_Argv(1));
			return;
		case 3:
			key = Cmd_Argv (1);
			value = Cmd_Argv (2);
			break;
		default:
			SV_Printf ("usage: localinfo [ <key> <value> ]\n");
			return;
	}

	if (key[0] == '*') {
		SV_Printf ("Star variables cannot be changed.\n");
		return;
	}
	SV_SetLocalinfo (key, value);
}

/*
	SV_User_f

	Examine a users info strings
*/
static void
SV_User_f (void)
{
	if (Cmd_Argc () != 2) {
		SV_Printf ("Usage: user <userid>\n");
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
static void
SV_Gamedir (void)
{
	const char *dir;

	if (Cmd_Argc () == 1) {
		SV_Printf ("Current *gamedir: %s\n",
					Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc () != 2) {
		SV_Printf ("Usage: sv_gamedir <newgamedir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":")) {
		SV_Printf ("*Gamedir should be a single filename, not a path\n");
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir,
							 !sv_highchars);
}

/*
	SV_Floodport_f

	Sets the gamedir and path to a different directory.
*/
static void
SV_Floodprot_f (void)
{
	int         arg1, arg2, arg3;

	if (Cmd_Argc () == 1) {
		if (fp_messages) {
			SV_Printf
				("Current floodprot settings: \nAfter %d msgs per %d seconds, "
				 "silence for %d seconds\n",
				 fp_messages, fp_persecond, fp_secondsdead);
			return;
		} else
			SV_Printf ("No floodprots enabled.\n");
	}

	if (Cmd_Argc () != 4) {
		SV_Printf
			("Usage: floodprot <# of messages> <per # of seconds> <seconds to "
			 "silence>\n");
		SV_Printf
			("Use floodprotmsg to set a custom message to say to the "
			 "flooder.\n");
		return;
	}

	arg1 = atoi (Cmd_Argv (1));
	arg2 = atoi (Cmd_Argv (2));
	arg3 = atoi (Cmd_Argv (3));

	if (arg1 <= 0 || arg2 <= 0 || arg3 <= 0) {
		SV_Printf ("All values must be positive numbers\n");
		return;
	}

	if (arg1 > 10) {
		SV_Printf ("Can track up to only 10 messages.\n");
		return;
	}

	fp_messages = arg1;
	fp_persecond = arg2;
	fp_secondsdead = arg3;
}

static void
SV_Floodprotmsg_f (void)
{
	if (Cmd_Argc () == 1) {
		SV_Printf ("Current msg: %s\n", fp_msg);
		return;
	} else if (Cmd_Argc () != 2) {
		SV_Printf ("Usage: floodprotmsg \"<message>\"\n");
		return;
	}
	snprintf (fp_msg, sizeof (fp_msg), "%s", Cmd_Argv (1));
}

static void
SV_Snap (int uid)
{
	client_t   *cl;
	int         i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_zombie)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS) {
		SV_Printf ("userid not found\n");
		return;
	}

	if (!cl->uploadfn)
		cl->uploadfn = dstring_new ();

	if (!(cl->upload = QFS_NextFile (cl->uploadfn,
									 va (0, "%s/snap/%d-",
										 qfs_gamedir->dir.def, uid), ".pcx"))) {
		SV_Printf ("Snap: Couldn't create a file, clean some out.\n%s\n",
				   cl->uploadfn->str);
		dstring_delete (cl->uploadfn);
		cl->uploadfn = 0;
		return;
	}
	cl->upload_started = 0;

	memcpy (&cl->snap_from, &net_from, sizeof (net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	MSG_ReliableWrite_Begin (&cl->backbuf, svc_stufftext, 24);
	MSG_ReliableWrite_String (&cl->backbuf, "cmd snap\n");
	SV_Printf ("Requesting snap from user %d...\n", uid);
}

static void
SV_Snap_f (void)
{
	int         uid;

	if (Cmd_Argc () != 2) {
		SV_Printf ("Usage:  snap <userid>\n");
		return;
	}

	uid = atoi (Cmd_Argv (1));

	SV_Snap (uid);
}

static void
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

void
SV_InitOperatorCommands (void)
{
	if (COM_CheckParm ("-cheats")) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON", 0);
	}

	Cmd_AddCommand ("fraglogfile", SV_Fraglogfile_f, "Enables logging of kills "
					"to frag_##.log");

	Cmd_AddCommand ("snap", SV_Snap_f, "Take a screenshot of userid");
	Cmd_AddCommand ("snapall", SV_SnapAll_f, "Take a screenshot of all users");
	Cmd_AddCommand ("kick", SV_Kick_f, "Remove a user from the server (kick "
					"userid)");
	Cmd_AddCommand ("status", SV_Status_f, "Report information on the current "
					"connected clients and the server - displays userids");

	Cmd_AddCommand ("map", SV_Map_f, "Change to a new map (map mapname)");
	Cmd_AddCommand ("setmaster", SV_SetMaster_f, "Lists the server with up to "
					"eight masters.\n"
					"When a server is listed with a master, the master is "
					"aware of the server's IP address and port and it is added "
					"to the\n"
					"list of current servers connected to a master. A "
					"heartbeat is sent to the master from the server to "
					"indicated that the\n"
					"server is still running and alive.\n\n"
					"Examples:\n"
					"setmaster 192.246.40.12:27002\n"
					"setmaster 192.246.40.12:27002 192.246.40.12:27004");
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f, "Force a heartbeat to be sent "
					"to the master server.\n"
					"A heartbeat tells the Master the server's IP address and "
					"that it is still alive.");
	Cmd_AddCommand ("restart", SV_Restart_f, "Restart the server (with shell "
					"support)");
	Cmd_AddCommand ("quit", SV_Quit_f, "Shut down the server");
	Cmd_AddCommand ("god", SV_God_f, "Toggle god cheat to userid (god userid) "
					"Requires cheats are enabled");
	Cmd_AddCommand ("give", SV_Give_f, "Give userid items, or health.\n"
					"Items: 1 Axe, 2 Shotgun, 3 Double-Barrelled Shotgun, 4 "
					"Nailgun, 5 Super Nailgun, 6 Grenade Launcher, 7 Rocket "
					"Launcher,\n"
					"8 ThunderBolt, C Cells, H Health, N Nails, R Rockets, S "
					"Shells. Requires cheats to be enabled. (give userid item "
					"amount)");
	Cmd_AddCommand ("noclip", SV_Noclip_f, "Toggle no clipping cheat for "
					"userid. Requires cheats to be enabled. (noclip userid)");
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f, "Reports or sets "
					"information about server.\n"
					"The information stored in this space is broadcast on the "
					"network to all players.\n"
					"Values:\n"
					"dq - Drop Quad Damage when a player dies.\n"
					"dr - Drop Ring of Shadows when a player dies.\n"
					"rj - Sets the multiplier rate for splash damage kick.\n"
					"needpass - Displays the passwords enabled on the server.\n"
					"watervis - Toggle the use of r_watervis by OpenGL "
					"clients.\n"
					"Note: Keys with (*) in front cannot be changed. Maximum "
					"key size cannot exceed 64-bytes.\n"
					"Maximum size for all keys cannot exceed 512-bytes.\n"
					"(serverinfo key value)");
	Cmd_AddCommand ("localinfo", SV_Localinfo_f, "Shows or sets localinfo "
					"variables.\n"
					"Useful for mod programmers who need to allow the admin to "
					"change settings.\n"
					"This is an alternative storage space to the serverinfo "
					"space for mod variables.\n"
					"The variables stored in this space are not broadcast on "
					"the network.\n"
					"This space also has a 32-kilobyte limit which is much "
					"greater then the 512-byte limit on the serverinfo space.\n"
					"Special Keys: (current map) (next map) - Using this "
					"combination will allow the creation of a custom map cycle "
					"without editing code.\n\n"
					"Example:\n"
					"localinfo dm2 dm4\n"
					"localinfo dm4 dm6\n"
					"localinfo dm6 dm2\n"
					"(localinfo key value)");
	Cmd_AddCommand ("user", SV_User_f, "Report information about the user "
					"(user userid)");
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir, "Displays or determines the "
					"value of the serverinfo *gamedir variable.\n"
					"Note: Useful when the physical gamedir directory has a "
					"different name than the widely accepted gamedir "
					"directory.\n"
					"Example:\n"
					"gamedir tf2_5; sv_gamedir fortress\n"
					"gamedir ctf4_2; sv_gamedir ctf\n"
					"(sv_gamedir dirname)");
	Cmd_AddCommand ("floodprot", SV_Floodprot_f, "Sets the options for flood "
					"protection.\n"
					"Default: 4 4 10\n"
					"(floodprot (number of messages) (number of seconds) "
					"(silence time in seconds))");
	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f, "Sets the message "
					"displayed after flood protection is invoked (floodprotmsg "
					"message)");
	Cmd_AddCommand ("maplist", Con_Maplist_f, "List all maps on the server");
	Cmd_AddCommand ("say", SV_ConSay_f, "Say something to everyone on the "
					"server. Will show up as the name 'Console' (or 'Admin') "
					"in game");
	Cmd_AddCommand ("sayinfo", SV_ConSay_Info_f, "Say something to everyone on "
					"the server. Will show up as the name 'Info' in game");
	Cmd_AddCommand ("tell", SV_Tell_f, "Say something to a specific user on "
					"the server. Will show up as the name 'Console' (or "
					"'Admin') in game");
	Cmd_AddCommand ("ban", SV_Ban_f, "ban a player for a specified time");
	Cmd_AddCommand ("cuff", SV_Cuff_f, "\"hand-cuff\" a player for a "
					"specified time");
	Cmd_AddCommand ("mute", SV_Mute_f, "silience a player for a specified "
					"time");
	Cmd_AddCommand ("match", SV_Match_f, "matches nicks as ban/cuff/mute "
					"commands do, so you can check safely");

	// poor description
	Cvar_Register (&sv_leetnickmatch_cvar, 0, 0);
}
