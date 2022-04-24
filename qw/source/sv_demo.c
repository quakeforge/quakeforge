/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif
#ifdef HAVE_UNISTD_H
# include "unistd.h"
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "qw/pmove.h"
#include "qw/include/server.h"
#include "qw/include/sv_demo.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_recorder.h"

static QFile   *demo_file;
static byte    *demo_mfile;
static qboolean demo_disk;
static dstring_t *demo_name;		// filename of mvd
static dstring_t *demo_text;		// filename of description file
static void    *demo_dest;
static double   demo_time;

static recorder_t *recorder;
static int      delta_sequence;

#define MIN_DEMO_MEMORY 0x100000
#define USECACHE (sv_demoUseCache && svs.demomemsize)
#define DWRITE(a,b,d) dwrite((QFile *) d, a, b)

static int  demo_max_size;
static int  demo_size;

float sv_demofps;
static cvar_t sv_demofps_cvar = {
	.name = "sv_demofps",
	.description =
		"FIXME",
	.default_value = "20",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_demofps },
};
float sv_demoPings;
static cvar_t sv_demoPings_cvar = {
	.name = "sv_demoPings",
	.description =
		"FIXME",
	.default_value = "3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &sv_demoPings },
};
int sv_demoMaxSize;
static cvar_t sv_demoMaxSize_cvar = {
	.name = "sv_demoMaxSize",
	.description =
		"FIXME",
	.default_value = "20480",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_demoMaxSize },
};
static int sv_demoUseCache;
static cvar_t sv_demoUseCache_cvar = {
	.name = "sv_demoUseCache",
	.description =
		"FIXME",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_demoUseCache },
};
int sv_demoCacheSize;
static cvar_t sv_demoCacheSize_cvar = {
	.name = "sv_demoCacheSize",
	.description =
		"FIXME",
	.default_value = 0,
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &sv_demoCacheSize },
};
int sv_demoMaxDirSize;
static cvar_t sv_demoMaxDirSize_cvar = {
	.name = "sv_demoMaxDirSize",
	.description =
		"FIXME",
	.default_value = "102400",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_demoMaxDirSize },
};
static char *sv_demoDir;
static cvar_t sv_demoDir_cvar = {
	.name = "sv_demoDir",
	.description =
		"FIXME",
	.default_value = "demos",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_demoDir },
};
int sv_demoNoVis;
static cvar_t sv_demoNoVis_cvar = {
	.name = "sv_demoNoVis",
	.description =
		"FIXME",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_demoNoVis },
};
static char *sv_demoPrefix;
static cvar_t sv_demoPrefix_cvar = {
	.name = "sv_demoPrefix",
	.description =
		"FIXME",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_demoPrefix },
};
static char *sv_demoSuffix;
static cvar_t sv_demoSuffix_cvar = {
	.name = "sv_demoSuffix",
	.description =
		"FIXME",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_demoSuffix },
};
static char *sv_onrecordfinish;
static cvar_t sv_onrecordfinish_cvar = {
	.name = "sv_onrecordfinish",
	.description =
		"FIXME",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_onrecordfinish },
};
static char *sv_ondemoremove;
static cvar_t sv_ondemoremove_cvar = {
	.name = "sv_ondemoremove",
	.description =
		"FIXME",
	.default_value = "",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_ondemoremove },
};
static int sv_demotxt;
static cvar_t sv_demotxt_cvar = {
	.name = "sv_demotxt",
	.description =
		"FIXME",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sv_demotxt },
};
static char *serverdemo;
static cvar_t serverdemo_cvar = {
	.name = "serverdemo",
	.description =
		"FIXME",
	.default_value = "",
	.flags = CVAR_SERVERINFO,
	.value = { .type = 0, .value = &serverdemo },
};

static int    (*dwrite) (QFile * file, const void *buf, int count);

#define HEADER  ((int) &((header_t *) 0)->data)

static int
memwrite (QFile *_mem, const void *buffer, int size)
{
	byte      **mem = (byte **) _mem;

	memcpy (*mem, buffer, size);
	*mem += size;

	return size;
}

static void
demo_write (void *unused, sizebuf_t *msg, int unused2)
{
	DWRITE (msg->data, msg->cursize, demo_dest);
}

static int
demo_frame (void *unused)
{
	double      min_fps;

	if (!sv_demofps)
		min_fps = 20.0;
	else
		min_fps = sv_demofps;
	min_fps = max (4, min_fps);
	if (sv.time - demo_time < 1.0 / min_fps)
		return 0;
	demo_time = sv.time;
	return 1;
}

static void
demo_end_frame (recorder_t *r, void *unused)
{
	SVR_SetDelta (r, ++delta_sequence, -1);
}

static void
demo_finish (void *unused, sizebuf_t *msg)
{
	// write a disconnect message to the demo file
	MSG_WriteByte (msg, svc_disconnect);
	MSG_WriteString (msg, "EndOfDemo");
	recorder = 0;
	sv.recording_demo = 0;
}

/*
	SV_Stop

	stop recording a demo
*/
void
SV_Stop (int reason)
{
	if (!recorder) {
		Sys_Printf ("Not recording a demo.\n");
		return;
	}

	SVR_RemoveUser (recorder);

	//if (demo_disk)
	Qclose (demo_file);
	demo_file = NULL;
	recorder = 0;
	sv.recording_demo = 0;
	switch (reason) {
		case 0:
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording completed\n");
			break;
		case 2:
			SV_BroadcastPrintf (PRINT_CHAT,
								"Server recording canceled, demo removed\n");
			Sys_Printf ("%s %s\n", demo_name->str, demo_text->str);
			QFS_Remove (demo_name->str);
			QFS_Remove (demo_text->str);
			break;
		default:
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording stoped\n"
								"Max demo size exceeded\n");
			break;
	}
/*
	if (sv_onrecordfinish[0]) {
		extern redirect_t sv_redirected;
		int         old = sv_redirected;
		char        path[MAX_OSPATH];
		char       *p;

		if ((p = strstr (sv_onrecordfinish, " ")) != NULL)
			*p = 0;						// strip parameters

		strcpy (path, demo_name->str);
		strcpy (path + strlen (demo_name->str) - 3, "txt");

		sv_redirected = RD_NONE;		// onrecord script is called always
										// from the console
		Cmd_TokenizeString (va (0, "script %s \"%s\" \"%s\" \"%s\" %s",
								sv_onrecordfinish, demo.path->str,
								serverdemo,
								path, p != NULL ? p + 1 : ""));
		if (p)
			*p = ' ';
		SV_Script_f ();

		sv_redirected = old;
	}
*/
	Cvar_Set ("serverdemo", "");
}

static void
SV_Stop_f (void)
{
	SV_Stop (0);
}

/*
	SV_Cancel_f

	Stops recording, and removes the demo
*/
static void
SV_Cancel_f (void)
{
	SV_Stop (2);
}

static qboolean
SV_InitRecord (void)
{
	if (!USECACHE) {
		dwrite = &Qwrite;
		demo_dest = demo_file;
		demo_disk = true;
	} else {
		dwrite = &memwrite;
		demo_mfile = svs.demomem;
		demo_dest = &demo_mfile;
	}

	demo_size = 0;

	return true;
}

/*
	SV_WriteRecordDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/

static void
SV_WriteRecordDemoMessage (sizebuf_t *msg)
{
	int         len;
	byte        c;

	c = 0;
	/*demo.size +=*/ DWRITE (&c, sizeof (c), demo_dest);

	c = dem_read;
	/*demo.size +=*/ DWRITE (&c, sizeof (c), demo_dest);

	len = LittleLong (msg->cursize);
	/*demo.size +=*/ DWRITE (&len, 4, demo_dest);

	/*demo.size +=*/ DWRITE (msg->data, msg->cursize, demo_dest);

	if (demo_disk)
		Qflush (demo_file);
}

static void
SV_WriteSetDemoMessage (void)
{
	int         len;
	byte        c;

	c = 0;
	/*demo.size +=*/ DWRITE (&c, sizeof (c), demo_dest);

	c = dem_set;
	/*demo.size +=*/ DWRITE (&c, sizeof (c), demo_dest);


	len = LittleLong (0);
	/*demo.size +=*/ DWRITE (&len, 4, demo_dest);
	len = LittleLong (0);
	/*demo.size +=*/ DWRITE (&len, 4, demo_dest);

	if (demo_disk)
		Qflush (demo_file);
}

static const char *
SV_PrintTeams (void)
{
	const char *teams[MAX_CLIENTS];
	char       *p;
	int         i, j, numcl = 0, numt = 0;
	client_t   *clients[MAX_CLIENTS];
	const char *team;
	static dstring_t *buffer;

	if (!buffer)
		buffer = dstring_new ();

	// count teams and players
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].state != cs_spawned
			&& svs.clients[i].state != cs_server)
			continue;
		if (svs.clients[i].spectator)
			continue;

		team = Info_ValueForKey (svs.clients[i].userinfo, "team");
		clients[numcl++] = &svs.clients[i];

		for (j = 0; j < numt; j++)
			if (!strcmp (team, teams[j]))
				break;
		if (j != numt)
			continue;

		teams[numt++] = team;
	}

	// create output

	if (numcl == 2) {					// duel
		dsprintf (buffer, "team1 %s\nteam2 %s\n", clients[0]->name,
				  clients[1]->name);
	} else if (!teamplay) {	// ffa
		dsprintf (buffer, "players:\n");
		for (i = 0; i < numcl; i++)
			dasprintf (buffer, "  %s\n", clients[i]->name);
	} else {							// teamplay
		for (j = 0; j < numt; j++) {
			dasprintf (buffer, "team %s:\n", teams[j]);
			for (i = 0; i < numcl; i++) {
				team = Info_ValueForKey (svs.clients[i].userinfo, "team");
				if (!strcmp (team, teams[j]))
					dasprintf (buffer, "  %s\n", clients[i]->name);
			}
		}
	}

	if (!numcl)
		return "\n";
	for (p = buffer->str; *p; p++)
		*p = sys_char_map[(byte) * p];
	return buffer->str;
}

static int
make_info_string_filter (const char *key)
{
	return *key == '_' || !Info_FilterForKey (key, client_info_filters);
}

static void
SV_Record (char *name)
{
	sizebuf_t   buf;
	byte        buf_data[MAX_MSGLEN];
	int         n, i;
	const char *info;

	client_t   *player;
	const char *gamedir, *s;

	SV_InitRecord ();

	dstring_copystr (demo_name, name);

	SV_BroadcastPrintf (PRINT_CHAT, "Server started recording (%s):\n%s\n",
						demo_disk ? "disk" : "memory",
						QFS_SkipPath (demo_name->str));
	Cvar_Set ("serverdemo", demo_name->str);

	dstring_copystr (demo_text, name);
	strcpy (demo_text->str + strlen (demo_text->str) - 3, "txt");

	if (sv_demotxt) {
		QFile      *f;

		f = QFS_Open (demo_text->str, "w+t");
		if (f != NULL) {
			char        date[20];
			time_t      tim;

			time (&tim);
			strftime (date, sizeof (date), "%Y-%m-%d-%H-%M", localtime (&tim));
			Qprintf (f, "date %s\nmap %s\nteamplay %d\ndeathmatch %d\n"
					 "timelimit %d\n%s",
					 date, sv.name, teamplay,
					 deathmatch, timelimit,
					 SV_PrintTeams ());
			Qclose (f);
		}
	} else
		QFS_Remove (demo_text->str);

	recorder = SVR_AddUser (demo_write, demo_frame, demo_end_frame,
							demo_finish, 1, 0);
	sv.recording_demo = 1;
	delta_sequence = -1;
	demo_time = sv.time;

/*-------------------------------------------------*/

	// serverdata
	// send the info about the new client to all connected clients
	memset (&buf, 0, sizeof (buf));
	buf.data = buf_data;
	buf.maxsize = sizeof (buf_data);

	// send the serverdata

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	MSG_WriteString (&buf, gamedir);


	MSG_WriteFloat (&buf, sv.time);

	// send full levelname
	MSG_WriteString (&buf, PR_GetString (&sv_pr_state,
										 SVstring (sv.edicts, message)));

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
	MSG_WriteString (&buf, va (0, "fullserverinfo \"%s\"\n",
							   Info_MakeString (svs.info, 0)));

	// flush packet
	SV_WriteRecordDemoMessage (&buf);
	SZ_Clear (&buf);

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.sound_precache[n + 1];
	while (s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.sound_precache[n + 1];
	}

	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.model_precache[n + 1];
	while (s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.model_precache[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// prespawn

	for (n = 0; n < sv.num_signon_buffers; n++) {
		SZ_Write (&buf, sv.signon_buffers[n], sv.signon_buffer_size[n]);

		if (buf.cursize > MAX_MSGLEN / 2) {
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va (0, "cmd spawn %i 0\n", svs.spawncount));

	if (buf.cursize) {
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++) {
		player = svs.clients + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->old_frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, SV_CalcPing (player));

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->lossage);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, realtime - player->connection_started);

		info = player->userinfo ? Info_MakeString (player->userinfo,
												   make_info_string_filter)
								: "";

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, info);

		if (buf.cursize > MAX_MSGLEN / 2) {
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char) i);
		MSG_WriteString (&buf, sv.lightstyles[i]);
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va (0, "skins\n"));

	SV_WriteRecordDemoMessage (&buf);

	SV_WriteSetDemoMessage ();
	// done
}

/*
	SV_CleanName

	Cleans the demo name, removes restricted chars, makes name lowercase
*/

static char *
SV_CleanName (const char *name)
{
	static char *text;
	static size_t text_len;
	char       *out, c;

	if (text_len < strlen (name)) {
		text_len = (strlen (name) + 1023) & ~1023;
		text = realloc (text, text_len);
	}

	out = text;
	do {
		c = sys_char_map[(byte) *name++];
		if (c != '_')
			*out++ = c;
	} while (c);

	return text;
}

/*
	SV_Record_f

	record <demoname>
*/
static void
SV_Record_f (void)
{
	dstring_t  *name = dstring_newstr ();

	if (Cmd_Argc () != 2) {
		Sys_Printf ("record <demoname>\n");
		return;
	}

	if (sv.state != ss_active) {
		Sys_Printf ("Not active yet.\n");
		return;
	}

	if (recorder)
		SV_Stop (0);

	dsprintf (name, "%s/%s/%s%s%s", qfs_gamedir->dir.def, sv_demoDir,
			  sv_demoPrefix, SV_CleanName (Cmd_Argv (1)),
			  sv_demoSuffix);

	// open the demo file
	QFS_DefaultExtension (name, ".mvd");

	demo_file = QFS_Open (name->str, "wb");
	if (!demo_file) {
		Sys_Printf ("ERROR: couldn't open %s\n", name->str);
	} else {
		SV_Record (name->str);
	}

	dstring_delete (name);
}

/*
	SV_EasyRecord_f

	easyrecord [demoname]
*/

static int
Dem_CountPlayers (void)
{
	int         i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			count++;
	}

	return count;
}

static const char *
Dem_Team (int num)
{
	int         i;
	static const char *lastteam[2];
	qboolean    first = true;
	client_t   *client;
	static int  index = 0;
	const char *team;

	index = 1 - index;

	for (i = 0, client = svs.clients; num && i < MAX_CLIENTS; i++, client++) {
		if (!client->name[0] || client->spectator)
			continue;

		team = Info_ValueForKey (svs.clients[i].userinfo, "team");
		if (first || strcmp (lastteam[index], team)) {
			first = false;
			num--;
			lastteam[index] = team;
		}
	}

	if (num)
		return "";

	return lastteam[index];
}

static const char *
Dem_PlayerName (int num)
{
	int         i;
	client_t   *client;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
		if (!client->name[0] || client->spectator)
			continue;

		if (!--num)
			return client->name;
	}

	return "";
}

static void
SV_EasyRecord_f (void)
{
	dstring_t  *name = dstring_newstr ();
	dstring_t  *name2 = dstring_newstr ();
	int         i;

	if (Cmd_Argc () > 2) {
		Sys_Printf ("easyrecord [demoname]\n");
		return;
	}

	if (recorder)
		SV_Stop (0);

	if (Cmd_Argc () == 2)
		dsprintf (name, "%s", Cmd_Argv (1));
	else {
		// guess game type and write demo name
		i = Dem_CountPlayers ();
		if (teamplay && i > 2) {
			// Teamplay
			dsprintf (name, "team_%s_vs_%s_%s",
					  Dem_Team (1), Dem_Team (2), sv.name);
		} else {
			if (i == 2) {
				// Duel
				dsprintf (name, "duel_%s_vs_%s_%s",
						  Dem_PlayerName (1), Dem_PlayerName (2), sv.name);
			} else {
				// FFA
				dsprintf (name, "ffa_%s (%d)", sv.name, i);
			}
		}
	}

	// Make sure the filename doesn't contain illegal characters
	dsprintf (name2, "%s/%s%s%s%s%s",
			  qfs_gamedir->dir.def, sv_demoDir,
			  sv_demoDir[0] ? "/" : "",
			  sv_demoPrefix, SV_CleanName (name->str),
			  sv_demoSuffix);

	if ((demo_file = QFS_NextFile (name, name2->str, ".mvd"))) {
		SV_Record (name->str);
	}

	dstring_delete (name);
	dstring_delete (name2);
}

void
Demo_Init (void)
{
	int         p, size = MIN_DEMO_MEMORY;

	p = COM_CheckParm ("-democache");
	if (p) {
		if (p < com_argc - 1)
			size = atoi (com_argv[p + 1]) * 1024;
		else
			Sys_Error ("Demo_Init: you must specify a size in KB after "
					   "-democache");
	}

	if (size < MIN_DEMO_MEMORY) {
		Sys_Printf ("Minimum memory size for demo cache is %dk\n",
					MIN_DEMO_MEMORY / 1024);
		size = MIN_DEMO_MEMORY;
	}

	demo_name = dstring_newstr ();
	demo_text = dstring_newstr ();

	svs.demomem = Hunk_AllocName (0, size, "demo");
	svs.demomemsize = size;
	demo_max_size = size - 0x80000;
	sv_demoCacheSize_cvar.default_value = nva ("%d", size / 1024);
	Cvar_Register (&serverdemo_cvar, Cvar_Info, &serverdemo);
	Cvar_Register (&sv_demofps_cvar, 0, 0);
	Cvar_Register (&sv_demoPings_cvar, 0, 0);
	Cvar_Register (&sv_demoNoVis_cvar, 0, 0);
	Cvar_Register (&sv_demoUseCache_cvar, 0, 0);
	Cvar_Register (&sv_demoCacheSize_cvar, 0, 0);
	Cvar_Register (&sv_demoMaxSize_cvar, 0, 0);
	Cvar_Register (&sv_demoMaxDirSize_cvar, 0, 0);
	Cvar_Register (&sv_demoDir_cvar, 0, 0);
	Cvar_Register (&sv_demoPrefix_cvar, 0, 0);
	Cvar_Register (&sv_demoSuffix_cvar, 0, 0);
	Cvar_Register (&sv_onrecordfinish_cvar, 0, 0);
	Cvar_Register (&sv_ondemoremove_cvar, 0, 0);
	Cvar_Register (&sv_demotxt_cvar, 0, 0);

	Cmd_AddCommand ("record", SV_Record_f, "FIXME");
	Cmd_AddCommand ("easyrecord", SV_EasyRecord_f, "FIXME");
	Cmd_AddCommand ("stop", SV_Stop_f, "FIXME");
	Cmd_AddCommand ("cancel", SV_Cancel_f, "FIXME");
}
