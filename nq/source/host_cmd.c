/*
	host_cmd.c

	@description@

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/keys.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/screen.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "client.h"
#include "compat.h"
#include "host.h"
#include "server.h"
#include "sv_progs.h"
#include "world.h"

int         current_skill;

void
Host_Quit_f (void)
{
//	if (key_dest != key_console && cls.state != ca_dedicated) {
//		M_Menu_Quit_f ();
//		return;
//	}
	if (!con_module)
		Con_Printf ("I hope you wanted to quit\n");
	CL_Disconnect ();
	Host_ShutdownServer (false);

	Sys_Quit ();
}

static void
Host_Status_f (void)
{
	client_t   *client;
	int         seconds;
	int         minutes;
	int         hours = 0;
	int         j;
	void        (*print) (const char *fmt, ...);

	if (cmd_source == src_command) {
		if (!sv.active) {
			CL_Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	} else
		print = SV_ClientPrintf;

	print ("host:    %s\n", Cvar_VariableString ("hostname"));
	print ("version: %4.2f\n", VERSION);
	if (tcpipAvailable)
		print ("tcp/ip:  %s\n", my_tcpip_address);
	if (ipxAvailable)
		print ("ipx:     %s\n", my_ipx_address);
	print ("map:     %s\n", sv.name);
	print ("players: %i active (%i max)\n\n", net_activeconnections,
		   svs.maxclients);
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client->active)
			continue;
		seconds = (int) (net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes) {
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		} else
			hours = 0;
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j + 1, client->name,
			   (int) SVfloat (client->edict, frags), hours, minutes, seconds);
		print ("   %s\n", client->netconnection->address);
	}
}


/*
  Host_God_f

  Sets client to godmode
*/
static void
Host_God_f (void)
{
	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (*sv_globals.deathmatch && !host_client->privileged)
		return;

	SVfloat (sv_player, flags) = (int) SVfloat (sv_player, flags) ^ FL_GODMODE;
	if (!((int) SVfloat (sv_player, flags) & FL_GODMODE))
		SV_ClientPrintf ("godmode OFF\n");
	else
		SV_ClientPrintf ("godmode ON\n");
}

static void
Host_Notarget_f (void)
{
	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (*sv_globals.deathmatch && !host_client->privileged)
		return;

	SVfloat (sv_player, flags) = (int) SVfloat (sv_player, flags) ^
		FL_NOTARGET;
	if (!((int) SVfloat (sv_player, flags) & FL_NOTARGET))
		SV_ClientPrintf ("notarget OFF\n");
	else
		SV_ClientPrintf ("notarget ON\n");
}

qboolean    noclip_anglehack;

static void
Host_Noclip_f (void)
{
	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (*sv_globals.deathmatch && !host_client->privileged)
		return;

	if (SVfloat (sv_player, movetype) != MOVETYPE_NOCLIP) {
		noclip_anglehack = true;
		SVfloat (sv_player, movetype) = MOVETYPE_NOCLIP;
		SV_ClientPrintf ("noclip ON\n");
	} else {
		noclip_anglehack = false;
		SVfloat (sv_player, movetype) = MOVETYPE_WALK;
		SV_ClientPrintf ("noclip OFF\n");
	}
}

/*
  Host_Fly_f

  Sets client to flymode
*/
static void
Host_Fly_f (void)
{
	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (*sv_globals.deathmatch && !host_client->privileged)
		return;

	if (SVfloat (sv_player, movetype) != MOVETYPE_FLY) {
		SVfloat (sv_player, movetype) = MOVETYPE_FLY;
		SV_ClientPrintf ("flymode ON\n");
	} else {
		SVfloat (sv_player, movetype) = MOVETYPE_WALK;
		SV_ClientPrintf ("flymode OFF\n");
	}
}

static void
Host_Ping_f (void)
{
	int         i, j;
	float       total;
	client_t   *client;

	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		if (!client->active)
			continue;
		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total += client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int) (total * 1000), client->name);
	}
}

// SERVER TRANSITIONS =========================================================

static const char *
nice_time (float time)
{
	int         t = time + 0.5;

	if (t < 60) {
		return va ("%ds", t);
	} else if (t < 3600) {
		return va ("%dm%02ds", t / 60, t % 60);
	} else if (t < 86400) {
		return va ("%dh%02dm%02ds", t / 3600, (t / 60) % 60, t % 60);
	} else {
		return va ("%dd%02dh%02dm%02ds",
				   t / 86400, (t / 3600) % 24, (t / 60) % 60, t % 60);
	}
}

/*
  Host_Map_f

  handle a 
  map <servername>
  command from the console.  Active clients are kicked off.
*/
static void
Host_Map_f (void)
{
	int         i;
	char        name[MAX_QPATH];
	const char *expanded;
	QFile      *f;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () > 2) {
		Con_Printf ("map <levelname> : continue game on a new level\n");
		return;
	}
	if (Cmd_Argc () == 1) {
		Con_Printf ("map is %s (%s)\n", sv.name, nice_time (sv.time));
		return;
	}

	// check to make sure the level exists
	expanded = va ("maps/%s.bsp", Cmd_Argv (1));
	QFS_FOpenFile (expanded, &f);
	if (!f) {
		Con_Printf ("Can't find %s\n", expanded);
		return;
	}
	Qclose (f);

	cls.demonum = -1;					// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer (false);

//	SCR_BeginLoadingPlaque ();

	cls.mapstring[0] = 0;
	for (i = 0; i < Cmd_Argc (); i++) {
		strcat (cls.mapstring, Cmd_Argv (i));
		strcat (cls.mapstring, " ");
	}
	strcat (cls.mapstring, "\n");

	svs.serverflags = 0;				// haven't completed an episode yet
	strcpy (name, Cmd_Argv (1));
	SV_SpawnServer (name);
	if (!sv.active)
		return;

	if (cls.state != ca_dedicated) {
		strcpy (cls.spawnparms, "");

		for (i = 2; i < Cmd_Argc (); i++) {
			strcat (cls.spawnparms, Cmd_Argv (i));
			strcat (cls.spawnparms, " ");
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
  Host_Changelevel_f

  Goes to a new map, taking all clients along
*/
static void
Host_Changelevel_f (void)
{
	char        level[MAX_QPATH];

	if (Cmd_Argc () != 2) {
		Con_Printf ("changelevel <levelname> : continue game on a new "
					"level\n");
		return;
	}
	if (!sv.active || cls.demoplayback) {
		Con_Printf ("Only the server may changelevel\n");
		return;
	}
	SV_SaveSpawnparms ();
	strcpy (level, Cmd_Argv (1));
	SV_SpawnServer (level);
}

/*
  Host_Restart_f

  Restarts the current server for a dead player
*/
static void
Host_Restart_f (void)
{
	char        mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;
	strcpy (mapname, sv.name);			// must copy out, because it gets
										// cleared in sv_spawnserver
	SV_SpawnServer (mapname);
}

/*
  Host_Reconnect_f

  This command causes the client to wait for the signon messages again.
  This is sent just before a server changes levels
*/
static void
Host_Reconnect_f (void)
{
//	SCR_BeginLoadingPlaque ();
	cls.signon = 0;						// need new connection messages
}

/*
  Host_Connect_f

  User command to connect to server
*/
static void
Host_Connect_f (void)
{
	char        name[MAX_QPATH];

	cls.demonum = -1;					// stop demo loop in case this fails
	if (cls.demoplayback) {
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	strcpy (name, Cmd_Argv (1));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}

// LOAD / SAVE GAME ===========================================================

#define	SAVEGAME_VERSION	5

/*
  Host_SavegameComment

  Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current 
*/
static void
Host_SavegameComment (QFile *file)
{
	unsigned    i;
	dstring_t  *comment = dstring_newstr ();

	dsprintf (comment, "%-21s kills:%3i/%3i", cl.levelname,
			  cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	if ((i = comment->size - 1) < SAVEGAME_COMMENT_LENGTH) {
		comment->size = SAVEGAME_COMMENT_LENGTH + 1;
		dstring_adjust (comment);
		while (i < comment->size - 1)
			comment->str[i++] = ' ';
	}
	comment->str[SAVEGAME_COMMENT_LENGTH] = '\n';
	comment->str[SAVEGAME_COMMENT_LENGTH + 1] = '\0';
	// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		if (comment->str[i] == ' ')
			comment->str[i] = '_';
	Qwrite (file, comment->str, SAVEGAME_COMMENT_LENGTH + 1);
}

static void
Host_Savegame_f (void)
{
	char        name[256];
	QFile      *f;
	int         i;

	if (cmd_source != src_command)
		return;

	if (!sv.active) {
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission) {
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1) {
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr (Cmd_Argv (1), "..")) {
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i = 0; i < svs.maxclients; i++) {
		if (svs.clients[i].active && (SVfloat (svs.clients[i].edict, health)
									  <= 0)) {
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	snprintf (name, sizeof (name), "%s/%s",
			  qfs_gamedir->dir.def, Cmd_Argv (1));
	QFS_DefaultExtension (name, ".sav");

	Con_Printf ("Saving game to %s...\n", name);
	f = QFS_WOpen (name, 0);
	if (!f) {
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Qprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (f);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		Qprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	Qprintf (f, "%d\n", current_skill);
	Qprintf (f, "%s\n", sv.name);
	Qprintf (f, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		if (sv.lightstyles[i])
			Qprintf (f, "%s\n", sv.lightstyles[i]);
		else
			Qprintf (f, "m\n");
	}


	ED_WriteGlobals (&sv_pr_state, f);
	for (i = 0; i < sv.num_edicts; i++) {
		ED_Write (&sv_pr_state, f, EDICT_NUM (&sv_pr_state, i));
		Qflush (f);
	}
	Qclose (f);
	Con_Printf ("done.\n");
}

static void
Host_Loadgame_f (void)
{
	dstring_t  *name = 0;
	QFile      *f;
	char       *mapname = 0;
	script_t   *script = 0;
	char       *str = 0;
	float       time, tfloat;
	unsigned int i;
	edict_t    *ent;
	int         entnum;
	int         version;
	float       spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		goto end;

	if (Cmd_Argc () != 2) {
		Con_Printf ("load <savename> : load a game\n");
		goto end;
	}

	cls.demonum = -1;					// stop demo loop in case this fails

	name = dstring_newstr ();
	dsprintf (name, "%s/%s", qfs_gamedir->dir.def, Cmd_Argv (1));
	name->size += 4;
	dstring_adjust (name);
			  
	QFS_DefaultExtension (name->str, ".sav");

	// we can't call SCR_BeginLoadingPlaque, because too much stack space has
	// been used.  The menu calls it before stuffing loadgame command
//  SCR_BeginLoadingPlaque ();

	Con_Printf ("Loading game from %s...\n", name->str);
	f = QFS_Open (name->str, "rz");
	if (!f) {
		Con_Printf ("ERROR: couldn't open.\n");
		goto end;
	}
	str = malloc (Qfilesize (f) + 1);
	i = Qread (f, str, Qfilesize (f));
	str[i] = 0;
	Qclose (f);

	script = Script_New ();
	Script_Start (script, name->str, str);

	Script_GetToken (script, 1);
	sscanf (script->token->str, "%i", &version);
	if (version != SAVEGAME_VERSION) {
		Con_Printf ("Savegame is version %i, not %i\n", version,
					SAVEGAME_VERSION);
		goto end;
	}

	// savegame comment (ignored)
	Script_GetToken (script, 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++) {
		Script_GetToken (script, 1);
		sscanf (script->token->str, "%f", &spawn_parms[i]);
	}

	// this silliness is so we can load 1.06 save files, which have float skill
	// values
	Script_GetToken (script, 1);
	sscanf (script->token->str, "%f", &tfloat);
	current_skill = (int) (tfloat + 0.1);
	Cvar_SetValue (skill, (float) current_skill);

	Script_GetToken (script, 1);
	mapname = strdup (script->token->str);

	Script_GetToken (script, 1);
	sscanf (script->token->str, "%f", &time);

	CL_Disconnect_f ();

	SV_SpawnServer (mapname);
	if (!sv.active) {
		Con_Printf ("Couldn't load map %s\n", mapname);
		goto end;
	}
	sv.paused = true;					// pause until all clients connect
	sv.loadgame = true;

	// load the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		char       *s;

		Script_GetToken (script, 1);
		s = Hunk_Alloc (strlen (script->token->str) + 1);
		strcpy (s, script->token->str);
		sv.lightstyles[i] = s;
	}

	// load the edicts out of the savegame file
	entnum = -1;						// -1 is the globals
	while (Script_GetToken (script, 1)) {
		if (strcmp (script->token->str, "{"))
			Sys_Error ("First token isn't a brace");

		if (entnum == -1) {				// parse the global vars
			ED_ParseGlobals (&sv_pr_state, script);
		} else {						// parse an edict

			ent = EDICT_NUM (&sv_pr_state, entnum);
			memset (&ent->v, 0, sv_pr_state.progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict (&sv_pr_state, script, ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated) {
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
end:
	if (mapname)
		free (mapname);
	if (script)
		Script_Delete (script);
	if (str)
		free (str);
	if (name)
		dstring_delete (name);
}

//============================================================================

static void
Host_Name_f (void)
{
	const char *newName;

	if (Cmd_Argc () == 1) {
		Con_Printf ("\"name\" is \"%s\"\n", cl_name->string);
		return;
	}
	if (Cmd_Argc () == 2)
		newName = Cmd_Argv (1);
	else
		newName = Cmd_Args (1);

	if (cmd_source == src_command) {
		if (strcmp (cl_name->string, newName) == 0)
			return;
		Cvar_Set (cl_name, va ("%.15s", newName));
		if (cls.state == ca_connected)
			CL_Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp (host_client->name, "unconnected"))
		if (strcmp (host_client->name, newName) != 0)
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);
	strcpy (host_client->name, newName);
	SVstring (host_client->edict, netname) =
		PR_SetString (&sv_pr_state, host_client->name);

	// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

static void
Host_Version_f (void)
{
	Con_Printf ("Version %s\n", VERSION);
	Con_Printf ("Exe: " __TIME__ " " __DATE__ "\n");
}

static void
Host_Say (qboolean teamonly)
{
	client_t   *client;
	client_t   *save;
	int         j;
	char       *p;
	char        text[64];
	qboolean    fromServer = false;

	if (cmd_source == src_command) {
		if (cls.state == ca_dedicated) {
			fromServer = true;
			teamonly = false;
		} else {
			CL_Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = Hunk_TempAlloc (strlen (Cmd_Args (1)) + 1);
	strcpy (p, Cmd_Args (1));
	// remove quotes if present
	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}
	// turn on color set 1
	if (!fromServer)
		snprintf (text, sizeof (text), "%c%s: ", 1, save->name);
	else
		snprintf (text, sizeof (text), "%c<%s> ", 1, hostname->string);

	j = sizeof (text) - 2 - strlen (text);	// -2 for /n and null terminator
	if ((int) strlen (p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay->int_val && teamonly && SVfloat (client->edict, team) !=
			SVfloat (save->edict, team))
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
	}
	host_client = save;

	Con_Printf ("%s", &text[1]);
}

static void
Host_Say_f (void)
{
	Host_Say (false);
}

static void
Host_Say_Team_f (void)
{
	Host_Say (true);
}

static void
Host_Tell_f (void)
{
	client_t   *client;
	client_t   *save;
	int         j;
	char       *p;
	char        text[64];

	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	strcpy (text, host_client->name);
	strcat (text, ": ");

	p = Hunk_TempAlloc (strlen (Cmd_Args (1)) + 1);
	strcpy (p, Cmd_Args (1));

	// remove quotes if present
	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}
	// check length & truncate if necessary
	j = sizeof (text) - 2 - strlen (text);	// -2 for /n and null terminator
	if ((int) strlen (p) > j)
		p[j] = 0;

	strcat (text, p);
	strcat (text, "\n");

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client->active || !client->spawned)
			continue;
		if (strcasecmp (client->name, Cmd_Argv (1)))
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
		break;
	}
	host_client = save;
}

static void
Host_Kill_f (void)
{
	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (SVfloat (sv_player, health) <= 0) {
		SV_ClientPrintf ("Can't suicide -- already dead!\n");
		return;
	}

	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientKill);
}

static void
Host_Pause_f (void)
{

	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}
	if (!pausable->int_val)
		SV_ClientPrintf ("Pause not allowed.\n");
	else {
		sv.paused ^= 1;

		if (sv.paused) {
			SV_BroadcastPrintf ("%s paused the game\n",
								PR_GetString (&sv_pr_state,
											  SVstring (sv_player, netname)));
		} else {
			SV_BroadcastPrintf ("%s unpaused the game\n",
								PR_GetString (&sv_pr_state,
											  SVstring (sv_player, netname)));
		}

		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================

static void
Host_PreSpawn_f (void)
{
	if (cmd_source == src_command) {
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned) {
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;
}

static void
Host_Spawn_f (void)
{
	int         i;
	client_t   *client;
	edict_t    *ent;

	if (cmd_source == src_command) {
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned) {
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}
	// run the entrance script
	if (sv.loadgame) {				// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.paused = false;
	} else {
		// set up the edict
		ent = host_client->edict;
		memset (&ent->v, 0, sv_pr_state.progs->entityfields * 4);
		SVfloat (ent, colormap) = NUM_FOR_EDICT (&sv_pr_state, ent);
		SVfloat (ent, team) = (host_client->colors & 15) + 1;
		SVstring (ent, netname) = PR_SetString (&sv_pr_state,
												host_client->name);

		// copy spawn parms out of the client_t
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			sv_globals.parms[i] = host_client->spawn_parms[i];

		// call the spawn function
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientConnect);
		if ((Sys_DoubleTime () - host_client->netconnection->connecttime) <=
			sv.time) Con_Printf ("%s entered the game\n", host_client->name);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.PutClientInServer);
	}

	// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

	// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char) i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

	// send some stats
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message,
				   *sv_globals.total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message,
				   *sv_globals.total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message,
				   *sv_globals.found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message,
				   *sv_globals.killed_monsters);

	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	ent = EDICT_NUM (&sv_pr_state, 1 + (host_client - svs.clients));
	MSG_WriteByte (&host_client->message, svc_setangle);
	MSG_WriteAngle (&host_client->message, SVvector (ent, angles)[0]);
	MSG_WriteAngle (&host_client->message, SVvector (ent, angles)[1]);
	MSG_WriteAngle (&host_client->message, 0);

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

static void
Host_Begin_f (void)
{
	if (cmd_source == src_command) {
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================

/*
  Host_Kick_f

  Kicks a user off of the server
*/
static void
Host_Kick_f (void)
{
	const char *who;
	const char *message = NULL;
	client_t   *save;
	int         i;
	qboolean    byNumber = false;

	if (cmd_source == src_command) {
		if (!sv.active) {
			CL_Cmd_ForwardToServer ();
			return;
		}
	}
		else if (*sv_globals.deathmatch
				 && !host_client->privileged) return;

	save = host_client;

	if (Cmd_Argc () > 2 && strcmp (Cmd_Argv (1), "#") == 0) {
		i = atof (Cmd_Argv (2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	} else {
		for (i = 0, host_client = svs.clients; i < svs.maxclients;
			 i++, host_client++) {
			if (!host_client->active)
				continue;
			if (strcasecmp (host_client->name, Cmd_Argv (1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients) {
		if (cmd_source == src_command)
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name->string;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc () > 2) {
			message = COM_Parse (Cmd_Args (1));
			if (byNumber) {
				message++;				// skip the #
				while (*message == ' ')	// skip white space
					message++;
				message += strlen (Cmd_Argv (2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

// DEBUGGING TOOLS ============================================================

// FIXME: This cheat should be re-implemented OUTSIDE the engine!
#if 0
void
Host_Give_f (void)
{
	char       *t;
	int         v;

	if (cmd_source == src_command) {
		CL_Cmd_ForwardToServer ();
		return;
	}

	if (*sv_globals.deathmatch && !host_client->privileged)
		return;

	t = Cmd_Argv (1);
	v = atoi (Cmd_Argv (2));

	switch (t[0]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			// MED 01/04/97 added hipnotic give stuff
			if (hipnotic) {
				if (t[0] == '6') {
					if (t[1] == 'a')
						SVfloat (sv_player, items) =
							(int) SVfloat (sv_player, items)
							| HIT_PROXIMITY_GUN;
					else
						SVfloat (sv_player, items) =
							(int) SVfloat (sv_player, items)
							| IT_GRENADE_LAUNCHER;
				} else if (t[0] == '9')
					SVfloat (sv_player, items) =
						(int) SVfloat (sv_player, items) | HIT_LASER_CANNON;
				else if (t[0] == '0')
					SVfloat (sv_player, items) =
						(int) SVfloat (sv_player, items) | HIT_MJOLNIR;
				else if (t[0] >= '2')
					SVfloat (sv_player, items) =
						(int) SVfloat (sv_player, items)
						| (IT_SHOTGUN << (t[0] - '2'));
			} else {
				if (t[0] >= '2')
					SVfloat (sv_player, items) =
						(int) SVfloat (sv_player, items)
						| (IT_SHOTGUN << (t[0] - '2'));
			}
			break;

		case 's':
			if (rogue) {
				SVfloat (sv_player, ammo_shells1) = v;
			}

			SVfloat (sv_player, ammo_shells) = v;
			break;
		case 'n':
			if (rogue) {
				SVfloat (sv_player, ammo_nails1) = v;
				if (SVfloat (sv_player, weapon) <= IT_LIGHTNING)
					SVfloat (sv_player, ammo_nails) = v;
			} else {
				SVfloat (sv_player, ammo_nails) = v;
			}
			break;
		case 'l':
			if (rogue) {
				SVfloat (sv_player, ammo_lava_nails) = v;
				if (SVfloat (sv_player, weapon) > IT_LIGHTNING)
					SVfloat (sv_player, ammo_nails) = v;
			}
			break;
		case 'r':
			if (rogue) {
				SVfloat (sv_player, ammo_rockets1) = v;
				if (SVfloat (sv_player, weapon) <= IT_LIGHTNING)
					SVfloat (sv_player, ammo_rockets) = v;
			} else {
				SVfloat (sv_player, ammo_rockets) = v;
			}
			break;
		case 'm':
			if (rogue) {
				SVfloat (sv_player, ammo_multi_rockets) = 0;
				if (SVfloat (sv_player, weapon) > IT_LIGHTNING)
					SVfloat (sv_player, ammo_rockets) = v;
			}
			break;
		case 'h':
			SVfloat (sv_player, health) = v;
			break;
		case 'c':
			if (rogue) {
				SVfloat (sv_player, ammo_cells1) = v;
				if (SVfloat (sv_player, weapon) <= IT_LIGHTNING)
					SVfloat (sv_player, ammo_cells) = v;
			} else {
				SVfloat (sv_player, ammo_cells) = v;
			}
			break;
		case 'p':
			if (rogue) {
				SVfloat (sv_player, ammo_plasma) = v;
				if (SVfloat (sv_player, weapon) > IT_LIGHTNING)
					SVfloat (sv_player, ammo_cells) = v;
			}
			break;
	}
}
#endif

static edict_t    *
FindViewthing (void)
{
	int         i;
	edict_t    *e;

	for (i = 0; i < sv.num_edicts; i++) {
		e = EDICT_NUM (&sv_pr_state, i);
		if (!strcmp (PR_GetString (&sv_pr_state, SVstring (e, classname)),
					 "viewthing"))
			return e;
	}
	Con_Printf ("No viewthing on map\n");
	return NULL;
}

static void
Host_Viewmodel_f (void)
{
	edict_t    *e;
	model_t    *m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv (1), false);
	if (!m) {
		Con_Printf ("Can't load %s\n", Cmd_Argv (1));
		return;
	}

	SVfloat (e, frame) = 0;
	cl.model_precache[(int) SVfloat (e, modelindex)] = m;
}

static void
Host_Viewframe_f (void)
{
	edict_t    *e;
	int         f;
	model_t    *m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int) SVfloat (e, modelindex)];

	f = atoi (Cmd_Argv (1));
	if (f >= m->numframes)
		f = m->numframes - 1;

	SVfloat (e, frame) = f;
}

static void
PrintFrameName (model_t *m, int frame)
{
	aliashdr_t *hdr;
	maliasframedesc_t *pframedesc;

	hdr = Cache_TryGet (&m->cache);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
	Cache_Release (&m->cache);
}

static void
Host_Viewnext_f (void)
{
	edict_t    *e;
	model_t    *m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int) SVfloat (e, modelindex)];

	SVfloat (e, frame) = SVfloat (e, frame) + 1;
	if (SVfloat (e, frame) >= m->numframes)
		SVfloat (e, frame) = m->numframes - 1;

	PrintFrameName (m, SVfloat (e, frame));
}

static void
Host_Viewprev_f (void)
{
	edict_t    *e;
	model_t    *m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int) SVfloat (e, modelindex)];

	SVfloat (e, frame) = SVfloat (e, frame) - 1;
	if (SVfloat (e, frame) < 0)
		SVfloat (e, frame) = 0;

	PrintFrameName (m, SVfloat (e, frame));
}

// DEMO LOOP CONTROL ==========================================================

static void
Host_Startdemos_f (void)
{
	int         i, c;

	if (cls.state == ca_dedicated) {
		if (!sv.active)
			Cbuf_AddText (host_cbuf, "map start\n");
		return;
	}

	c = Cmd_Argc () - 1;
	if (c > MAX_DEMOS) {
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		strncpy (cls.demos[i - 1], Cmd_Argv (i), sizeof (cls.demos[0]) - 1);

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback) {
		cls.demonum = 0;
		CL_NextDemo ();
	} else
		cls.demonum = -1;
}

/*
  Host_Demos_f

  Return to looping demos
*/
static void
Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
  Host_Stopdemo_f

  Return to looping demos
*/
static void
Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (!cls.demoplayback)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}

//=============================================================================

void
Host_InitCommands (void)
{
	Cmd_AddCommand ("status", Host_Status_f, "No Description");
	if (!con_module || isDedicated)
		Cmd_AddCommand ("quit", Host_Quit_f, "No Description");
	Cmd_AddCommand ("god", Host_God_f, "No Description");
	Cmd_AddCommand ("notarget", Host_Notarget_f, "No Description");
	Cmd_AddCommand ("fly", Host_Fly_f, "No Description");
	Cmd_AddCommand ("map", Host_Map_f, "No Description");
	Cmd_AddCommand ("restart", Host_Restart_f, "No Description");
	Cmd_AddCommand ("changelevel", Host_Changelevel_f, "No Description");
	Cmd_AddCommand ("connect", Host_Connect_f, "No Description");
	Cmd_AddCommand ("reconnect", Host_Reconnect_f, "No Description");
	Cmd_AddCommand ("name", Host_Name_f, "No Description");
	Cmd_AddCommand ("noclip", Host_Noclip_f, "No Description");
	Cmd_AddCommand ("version", Host_Version_f, "No Description");
	Cmd_AddCommand ("say", Host_Say_f, "No Description");
	Cmd_AddCommand ("say_team", Host_Say_Team_f, "No Description");
	Cmd_AddCommand ("tell", Host_Tell_f, "No Description");
	Cmd_AddCommand ("kill", Host_Kill_f, "No Description");
	Cmd_AddCommand ("pause", Host_Pause_f, "No Description");
	Cmd_AddCommand ("spawn", Host_Spawn_f, "No Description");
	Cmd_AddCommand ("begin", Host_Begin_f, "No Description");
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f, "No Description");
	Cmd_AddCommand ("kick", Host_Kick_f, "No Description");
	Cmd_AddCommand ("ping", Host_Ping_f, "No Description");
	Cmd_AddCommand ("load", Host_Loadgame_f, "No Description");
	Cmd_AddCommand ("save", Host_Savegame_f, "No Description");

	Cmd_AddCommand ("startdemos", Host_Startdemos_f, "No Description");
	Cmd_AddCommand ("demos", Host_Demos_f, "No Description");
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f, "No Description");

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f, "No Description");
	Cmd_AddCommand ("viewframe", Host_Viewframe_f, "No Description");
	Cmd_AddCommand ("viewnext", Host_Viewnext_f, "No Description");
	Cmd_AddCommand ("viewprev", Host_Viewprev_f, "No Description");

	Cmd_AddCommand ("mcache", Mod_Print, "No Description");
}
