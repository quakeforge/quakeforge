/*
	sv_user.c

	server code for moving users

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

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

#include "QF/checksum.h"
#include "QF/cmd.h"
#include "compat.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vfs.h"
#include "bothdefs.h"
#include "msg_ucmd.h"
#include "pmove.h"
#include "server.h"
#include "sv_progs.h"
#include "world.h"

edict_t    *sv_player;

usercmd_t   cmd;

cvar_t     *cl_rollspeed;
cvar_t     *cl_rollangle;
cvar_t     *sv_spectalk;

cvar_t     *sv_mapcheck;

cvar_t     *sv_timekick;
cvar_t     *sv_timekick_fuzz;
cvar_t     *sv_timekick_interval;

extern cvar_t *sv_maxrate;

extern vec3_t player_mins;

extern int  fp_messages, fp_persecond, fp_secondsdead;
extern char fp_msg[];
extern cvar_t *pausable;

void        SV_FullClientUpdateToClient (client_t *client, client_t *cl);

/*
	USER STRINGCMD EXECUTION

	host_client and sv_player will be valid.
*/

/*
	SV_New_f

	Sends the first message from the server to a connected client.
	This will be sent on the initial connection and upon each server load.
*/
void
SV_New_f (void)
{
	char       *gamedir;
	int         playernum;

	if (host_client->state == cs_spawned)
		return;

	host_client->state = cs_connected;
	host_client->connection_started = realtime;

	// send the info about the new client to all connected clients
//  SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//  host_client->sendinfo = true;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf ("WARNING %s: [SV_New] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}
	// send the serverdata
	MSG_WriteByte (&host_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&host_client->netchan.message, svs.spawncount);
	MSG_WriteString (&host_client->netchan.message, gamedir);

	playernum = NUM_FOR_EDICT (&sv_pr_state, host_client->edict) - 1;
	if (host_client->spectator)
		playernum |= 128;
	MSG_WriteByte (&host_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&host_client->netchan.message,
					 PR_GetString (&sv_pr_state, SVFIELD (sv.edicts, message, string)));

	// send the movevars
	MSG_WriteFloat (&host_client->netchan.message, movevars.gravity);
	MSG_WriteFloat (&host_client->netchan.message, movevars.stopspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.maxspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.spectatormaxspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.accelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.airaccelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.wateraccelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.friction);
	MSG_WriteFloat (&host_client->netchan.message, movevars.waterfriction);
	MSG_WriteFloat (&host_client->netchan.message, movevars.entgravity);

	// send music
	MSG_WriteByte (&host_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&host_client->netchan.message, SVFIELD (sv.edicts, sounds, float));

	// send server info string
	MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
	MSG_WriteString (&host_client->netchan.message,
					 va ("fullserverinfo \"%s\"\n", svs.info));
}

/*
	SV_Soundlist_f
*/
void
SV_Soundlist_f (void)
{
	char      **s;
	unsigned    n;

	if (host_client->state != cs_connected) {
		Con_Printf ("soundlist not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawncount) {
		Con_Printf ("SV_Soundlist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = atoi (Cmd_Argv (2));
	if (n >= MAX_SOUNDS) {
		Con_Printf ("SV_Soundlist_f: Invalid soundlist index\n");
		SV_New_f ();
		return;
	}
//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf ("WARNING %s: [SV_Soundlist] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	MSG_WriteByte (&host_client->netchan.message, svc_soundlist);
	MSG_WriteByte (&host_client->netchan.message, n);
	for (s = sv.sound_precache + 1 + n;
		 *s && host_client->netchan.message.cursize < (MAX_MSGLEN / 2);
		 s++, n++) MSG_WriteString (&host_client->netchan.message, *s);

	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	if (*s)
		MSG_WriteByte (&host_client->netchan.message, n);
	else
		MSG_WriteByte (&host_client->netchan.message, 0);
}

/*
	SV_Modellist_f
*/
void
SV_Modellist_f (void)
{
	char      **s;
	unsigned    n;

	if (host_client->state != cs_connected) {
		Con_Printf ("modellist not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawncount) {
		Con_Printf ("SV_Modellist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = atoi (Cmd_Argv (2));
	if (n >= MAX_MODELS) {
		Con_Printf ("SV_Modellist_f: Invalid modellist index\n");
		SV_New_f ();
		return;
	}
//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf ("WARNING %s: [SV_Modellist] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	MSG_WriteByte (&host_client->netchan.message, svc_modellist);
	MSG_WriteByte (&host_client->netchan.message, n);
	for (s = sv.model_precache + 1 + n;
		 *s && host_client->netchan.message.cursize < (MAX_MSGLEN / 2);
		 s++, n++) MSG_WriteString (&host_client->netchan.message, *s);
	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	if (*s)
		MSG_WriteByte (&host_client->netchan.message, n);
	else
		MSG_WriteByte (&host_client->netchan.message, 0);
}

/*
	SV_PreSpawn_f
*/
void
SV_PreSpawn_f (void)
{
	unsigned int buf;
	unsigned int check;

	if (host_client->state != cs_connected) {
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawncount) {
		Con_Printf ("SV_PreSpawn_f from different level\n");
		SV_New_f ();
		return;
	}

	buf = atoi (Cmd_Argv (2));
	if (buf >= sv.num_signon_buffers)
		buf = 0;

	if (!buf) {
		// should be three numbers following containing checksums
		check = atoi (Cmd_Argv (3));

//      Con_DPrintf("Client check = %d\n", check);

		if (sv_mapcheck->int_val && check != sv.worldmodel->checksum &&
			check != sv.worldmodel->checksum2) {
			SV_ClientPrintf (host_client, PRINT_HIGH,
							 "Map model file does not match (%s), %i != %i/%i.\n"
							 "You may need a new version of the map, or the proper install files.\n",
							 sv.modelname, check, sv.worldmodel->checksum,
							 sv.worldmodel->checksum2);
			SV_DropClient (host_client);
			return;
		}
		host_client->checksum = check;
	}
	// NOTE:  This doesn't go through ClientReliableWrite since it's before
	// the user
	// spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf ("WARNING %s: [SV_PreSpawn] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	SZ_Write (&host_client->netchan.message,
			  sv.signon_buffers[buf], sv.signon_buffer_size[buf]);

	buf++;
	if (buf == sv.num_signon_buffers) {	// all done prespawning
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message,
						 va ("cmd spawn %i 0\n", svs.spawncount));
	} else {							// need to prespawn more
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message,
						 va ("cmd prespawn %i %i\n", svs.spawncount, buf));
	}
}

/*
	SV_Spawn_f
*/
void
SV_Spawn_f (void)
{
	int         i;
	client_t   *client;
	edict_t    *ent;
	pr_type_t  *val;
	int         n;

	if (host_client->state != cs_connected) {
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}
// handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawncount) {
		Con_Printf ("SV_Spawn_f from different level\n");
		SV_New_f ();
		return;
	}

	n = atoi (Cmd_Argv (2));

	// make sure n is valid
	if (n < 0 || n > MAX_CLIENTS) {
		Con_Printf ("SV_Spawn_f invalid client start\n");
		SV_New_f ();
		return;
	}
	// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&host_client->netchan.message);

	// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i = n, client = svs.clients + n; i < MAX_CLIENTS; i++, client++)
		SV_FullClientUpdateToClient (client, host_client);

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		ClientReliableWrite_Begin (host_client, svc_lightstyle,
								   3 +
								   (sv.
									lightstyles[i] ? strlen (sv.
															 lightstyles[i]) :
									1));
		ClientReliableWrite_Byte (host_client, (char) i);
		ClientReliableWrite_String (host_client, sv.lightstyles[i]);
	}

	// set up the edict
	ent = host_client->edict;

	memset (&ent->v, 0, sv_pr_state.progs->entityfields * 4);
	SVFIELD (ent, colormap, float) = NUM_FOR_EDICT (&sv_pr_state, ent);
	SVFIELD (ent, team, float) = 0;					// FIXME
	SVFIELD (ent, netname, string) = PR_SetString (&sv_pr_state, host_client->name);

	host_client->entgravity = 1.0;
	val = GetEdictFieldValue (&sv_pr_state, ent, "gravity");
	if (val)
		val->float_var = 1.0;
	host_client->maxspeed = sv_maxspeed->value;
	val = GetEdictFieldValue (&sv_pr_state, ent, "maxspeed");
	if (val)
		val->float_var = sv_maxspeed->value;

//
// force stats to be updated
//
	memset (host_client->stats, 0, sizeof (host_client->stats));

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, *sv_globals.total_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, *sv_globals.total_monsters);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, *sv_globals.found_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, *sv_globals.killed_monsters);

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
	ClientReliableWrite_String (host_client, "skins\n");
}

/*
	SV_SpawnSpectator
*/
void
SV_SpawnSpectator (void)
{
	int         i;
	edict_t    *e;

	VectorCopy (vec3_origin, SVFIELD (sv_player, origin, vector));
	VectorCopy (vec3_origin, SVFIELD (sv_player, view_ofs, vector));
	SVFIELD (sv_player, view_ofs, vector)[2] = 22;

	// search for an info_playerstart to spawn the spectator at
	for (i = MAX_CLIENTS - 1; i < sv.num_edicts; i++) {
		e = EDICT_NUM (&sv_pr_state, i);
		if (!strcmp (PR_GetString (&sv_pr_state, SVFIELD (e, classname, string)), "info_player_start")) {
			VectorCopy (SVFIELD (e, origin, vector), SVFIELD (sv_player, origin, vector));
			return;
		}
	}

}

/*
	SV_Begin_f
*/
void
SV_Begin_f (void)
{
	unsigned int pmodel = 0, emodel = 0;
	int         i;

	if (host_client->state == cs_spawned)
		return;							// don't begin again

	host_client->state = cs_spawned;

	// handle the case of a level changing while a client was connecting
	if (atoi (Cmd_Argv (1)) != svs.spawncount) {
		Con_Printf ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	if (host_client->spectator) {
		SV_SpawnSpectator ();

		if (SpectatorConnect) {
			// copy spawn parms out of the client_t
			for (i = 0; i < NUM_SPAWN_PARMS; i++)
				sv_globals.parms[i] = host_client->spawn_parms[i];

			// call the spawn function
			*sv_globals.time = sv.time;
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
			PR_ExecuteProgram (&sv_pr_state, SpectatorConnect);
		}
	} else {
		// copy spawn parms out of the client_t
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			sv_globals.parms[i] = host_client->spawn_parms[i];

		// call the spawn function
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientConnect);

		// actually spawn the player
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.PutClientInServer);
	}

	// clear the net statistics, because connecting gives a bogus picture
	host_client->last_check = -1;
	host_client->netchan.frame_latency = 0;
	host_client->netchan.frame_rate = 0;
	host_client->netchan.drop_count = 0;
	host_client->netchan.good_count = 0;

	// check he's not cheating
	pmodel = atoi (Info_ValueForKey (host_client->userinfo, "pmodel"));
	emodel = atoi (Info_ValueForKey (host_client->userinfo, "emodel"));

	if (pmodel != sv.model_player_checksum || emodel != sv.eyes_player_checksum)
		SV_BroadcastPrintf (PRINT_HIGH,
							"%s WARNING: non standard player/eyes model detected\n",
							host_client->name);

	// if we are paused, tell the client
	if (sv.paused) {
		ClientReliableWrite_Begin (host_client, svc_setpause, 2);
		ClientReliableWrite_Byte (host_client, sv.paused);
		SV_ClientPrintf (host_client, PRINT_HIGH, "Server is paused.\n");
	}
#if 0
//
// send a fixangle over the reliable channel to make sure it gets there
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM (&sv_pr_state, 1 + (host_client - svs.clients));
	MSG_WriteByte (&host_client->netchan.message, svc_setangle);
	for (i = 0; i < 2; i++)
		MSG_WriteAngle (&host_client->netchan.message, SVFIELD (ent, angles, vector)[i]);
	MSG_WriteAngle (&host_client->netchan.message, 0);
#endif
}

//=============================================================================

/*
	SV_NextDownload_f
*/
void
SV_NextDownload_f (void)
{
	byte        buffer[1024];
	int         r;
	int         percent;
	int         size;

	if (!host_client->download)
		return;

	r = host_client->downloadsize - host_client->downloadcount;
	if (r > 768)
		r = 768;
	r = Qread (host_client->download, buffer, r);
	ClientReliableWrite_Begin (host_client, svc_download, 6 + r);
	ClientReliableWrite_Short (host_client, r);

	host_client->downloadcount += r;
	size = host_client->downloadsize;
	if (!size)
		size = 1;
	percent = host_client->downloadcount * 100 / size;
	ClientReliableWrite_Byte (host_client, percent);
	ClientReliableWrite_SZ (host_client, buffer, r);

	if (host_client->downloadcount != host_client->downloadsize)
		return;

	Qclose (host_client->download);
	host_client->download = NULL;

}

void
OutofBandPrintf (netadr_t where, char *fmt, ...)
{
	va_list     argptr;
	char        send[1024];

	send[0] = 0xff;
	send[1] = 0xff;
	send[2] = 0xff;
	send[3] = 0xff;
	send[4] = A2C_PRINT;
	va_start (argptr, fmt);
	vsnprintf (send + 5, sizeof (send - 5), fmt, argptr);
	va_end (argptr);

	NET_SendPacket (strlen (send) + 1, send, where);
}

/*
	SV_NextUpload
*/
void
SV_NextUpload (void)
{
	int         percent;
	int         size;

	if (!*host_client->uploadfn) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Upload denied\n");
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "stopul");

		// suck out rest of packet
		size = MSG_ReadShort (net_message);
		MSG_ReadByte (net_message);
		net_message->readcount += size;
		return;
	}

	size = MSG_ReadShort (net_message);
	percent = MSG_ReadByte (net_message);

	if (!host_client->upload) {
		host_client->upload = Qopen (host_client->uploadfn, "wb");
		if (!host_client->upload) {
			Con_Printf ("Can't create %s\n", host_client->uploadfn);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
			ClientReliableWrite_String (host_client, "stopul");
			*host_client->uploadfn = 0;
			return;
		}
		Con_Printf ("Receiving %s from %d...\n", host_client->uploadfn,
					host_client->userid);
		if (host_client->remote_snap)
			OutofBandPrintf (host_client->snap_from,
							 "Server receiving %s from %d...\n",
							 host_client->uploadfn, host_client->userid);
	}

	Qwrite (host_client->upload, net_message->message->data + net_message->readcount, size);
	net_message->readcount += size;

	Con_DPrintf ("UPLOAD: %d received\n", size);

	if (percent != 100) {
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "nextul\n");
	} else {
		Qclose (host_client->upload);
		host_client->upload = NULL;

		Con_Printf ("%s upload completed.\n", host_client->uploadfn);

		if (host_client->remote_snap) {
			char       *p;

			if ((p = strchr (host_client->uploadfn, '/')) != NULL)
				p++;
			else
				p = host_client->uploadfn;
			OutofBandPrintf (host_client->snap_from,
							 "%s upload completed.\nTo download, enter:\ndownload %s\n",
							 host_client->uploadfn, p);
		}
	}

}

/*
	SV_BeginDownload_f
*/
void
SV_BeginDownload_f (void)
{
	char       *name;
	VFile      *file;
	int         size;
	char        realname[MAX_OSPATH];
	int         zip;

	extern cvar_t *allow_download;
	extern cvar_t *allow_download_skins;
	extern cvar_t *allow_download_models;
	extern cvar_t *allow_download_sounds;
	extern cvar_t *allow_download_maps;
	extern int  file_from_pak;			// ZOID did file come from pak?

	name = Cmd_Argv (1);
// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check
	if (strstr (name, "..") || !allow_download->int_val
		// leading dot is no good
		|| *name == '.'
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp (name, "skins/", 6) == 0 && !allow_download_skins->int_val)
		// now models
		|| (strncmp (name, "progs/", 6) == 0 && !allow_download_models->int_val)
		// now sounds
		|| (strncmp (name, "sound/", 6) == 0 && !allow_download_sounds->int_val)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp (name, "maps/", 6) == 0 && !allow_download_maps->int_val)
		// MUST be in a subdirectory    
		|| !strstr (name, "/")) {		// don't allow anything with .. path
		ClientReliableWrite_Begin (host_client, svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		return;
	}

	if (host_client->download) {
		Qclose (host_client->download);
		host_client->download = NULL;
	}
	// lowercase name (needed for casesen file systems)
	{
		char       *p;

		for (p = name; *p; p++)
			*p = tolower ((int) *p);
	}

	zip = strchr (Info_ValueForKey (host_client->userinfo, "*cap"), 'z') != 0;

	size = _COM_FOpenFile (name, &file, realname, !zip);

	host_client->download = file;
	host_client->downloadsize = size;
	host_client->downloadcount = 0;

	if (!host_client->download
		// special check for maps, if it came from a pak file, don't allow
		// download  ZOID
		|| (strncmp (name, "maps/", 5) == 0 && file_from_pak)) {
		if (host_client->download) {
			Qclose (host_client->download);
			host_client->download = NULL;
		}

		Con_Printf ("Couldn't download %s to %s\n", name, host_client->name);
		ClientReliableWrite_Begin (host_client, svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		return;
	}

	if (zip && strcmp (realname, name)) {
		Con_Printf ("download renamed to %s\n", realname);
		ClientReliableWrite_Begin (host_client, svc_download,
								   strlen (realname) + 5);
		ClientReliableWrite_Short (host_client, -2);
		ClientReliableWrite_Byte (host_client, 0);
		ClientReliableWrite_String (host_client, realname);
		ClientReliable_FinishWrite (host_client);
	}

	SV_NextDownload_f ();
	Con_Printf ("Downloading %s to %s\n", name, host_client->name);
}

//=============================================================================

/*
	SV_Say
*/
void
SV_Say (qboolean team)
{
	client_t   *client;
	int         j, tmp;
	char       *p;
	char        text[2048];
	char        t1[32], *t2;

	if (Cmd_Argc () < 2)
		return;

	if (team) {
		strncpy (t1, Info_ValueForKey (host_client->userinfo, "team"), 31);
		t1[31] = 0;
	}

	if (host_client->spectator && (!sv_spectalk->int_val || team))
		snprintf (text, sizeof (text), "[SPEC] %s: ", host_client->name);
	else if (team)
		snprintf (text, sizeof (text), "(%s): ", host_client->name);
	else {
		snprintf (text, sizeof (text), "%s: ", host_client->name);
	}

	if (fp_messages) {
		if (!sv.paused && realtime < host_client->lockedtill) {
			SV_ClientPrintf (host_client, PRINT_CHAT,
							 "You can't talk for %d more seconds\n",
							 (int) (host_client->lockedtill - realtime));
			return;
		}
		tmp = host_client->whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10 + tmp;
		if (!sv.paused &&
			host_client->whensaid[tmp]
			&& (realtime - host_client->whensaid[tmp] < fp_persecond)) {
			host_client->lockedtill = realtime + fp_secondsdead;
			if (fp_msg[0])
				SV_ClientPrintf (host_client, PRINT_CHAT,
								 "FloodProt: %s\n", fp_msg);
			else
				SV_ClientPrintf (host_client, PRINT_CHAT,
								 "FloodProt: You can't talk for %d seconds.\n",
								 fp_secondsdead);
			return;
		}
		host_client->whensaidhead++;
		if (host_client->whensaidhead > 9)
			host_client->whensaidhead = 0;
		host_client->whensaid[host_client->whensaidhead] = realtime;
	}

	p = Cmd_Args (1);

	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}

	strncat (text, p, sizeof (text) - strlen (text));
	strncat (text, "\n", sizeof (text) - strlen (text));

	Con_Printf ("%s", text);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state < cs_connected)	// Clients connecting can hear.
			continue;
		if (host_client->spectator && !sv_spectalk->int_val)
			if (!client->spectator)
				continue;

		if (team) {
			// the spectator team
			if (host_client->spectator) {
				if (!client->spectator)
					continue;
			} else {
				t2 = Info_ValueForKey (client->userinfo, "team");
				if (strcmp (t1, t2) || client->spectator)
					continue;			// on different teams
			}
		}
		SV_ClientPrintf (client, PRINT_CHAT, "%s", text);
	}
}

/*
	SV_Say_f
*/
void
SV_Say_f (void)
{
	SV_Say (false);
}

/*
	SV_Say_Team_f
*/
void
SV_Say_Team_f (void)
{
	SV_Say (true);
}



//============================================================================

/*
	SV_Pings_f

	The client is showing the scoreboard, so send new ping times for all
	clients
*/
void
SV_Pings_f (void)
{
	client_t   *client;
	int         j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;

		ClientReliableWrite_Begin (host_client, svc_updateping, 4);
		ClientReliableWrite_Byte (host_client, j);
		ClientReliableWrite_Short (host_client, SV_CalcPing (client));
		ClientReliableWrite_Begin (host_client, svc_updatepl, 4);
		ClientReliableWrite_Byte (host_client, j);
		ClientReliableWrite_Byte (host_client, client->lossage);
	}
}



/*
	SV_Kill_f
*/
void
SV_Kill_f (void)
{
	if (SVFIELD (sv_player, health, float) <= 0) {
		SV_BeginRedirect (RD_CLIENT);
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "Can't suicide -- already dead!\n");
		SV_EndRedirect ();
		return;
	}

	*sv_globals.time = sv.time;
	*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
	PR_ExecuteProgram (&sv_pr_state, sv_funcs.ClientKill);
}

/*
	SV_TogglePause
*/
void
SV_TogglePause (const char *msg)
{
	int         i;
	client_t   *cl;

	sv.paused ^= 1;

	if (msg)
		SV_BroadcastPrintf (PRINT_HIGH, "%s", msg);

	// send notification to all clients
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		ClientReliableWrite_Begin (cl, svc_setpause, 2);
		ClientReliableWrite_Byte (cl, sv.paused);
	}
}


/*
	SV_Pause_f
*/
void
SV_Pause_f (void)
{
	static double lastpausetime;
	double      currenttime;
	char        st[sizeof (host_client->name) + 32];

	currenttime = Sys_DoubleTime ();

	if (lastpausetime + 1 > currenttime) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Pause flood not allowed.\n");
		return;
	}

	lastpausetime = currenttime;

	if (!pausable->int_val) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Pause not allowed.\n");
		return;
	}

	if (host_client->spectator) {
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "Spectators can not pause.\n");
		return;
	}

	if (!sv.paused)
		snprintf (st, sizeof (st), "%s paused the game\n", host_client->name);
	else
		snprintf (st, sizeof (st), "%s unpaused the game\n", host_client->name);

	SV_TogglePause (st);
}


/*
	SV_Drop_f

	The client is going to disconnect, so remove the connection immediately
*/
void
SV_Drop_f (void)
{
	SV_EndRedirect ();
	if (!host_client->spectator)
		SV_BroadcastPrintf (PRINT_HIGH, "%s dropped\n", host_client->name);
	SV_DropClient (host_client);
}

/*
	SV_PTrack_f

	Change the bandwidth estimate for a client
*/
void
SV_PTrack_f (void)
{
	int         i;
	edict_t    *ent, *tent;

	if (!host_client->spectator)
		return;

	if (Cmd_Argc () != 2) {
		// turn off tracking
		host_client->spec_track = 0;
		ent = EDICT_NUM (&sv_pr_state, host_client - svs.clients + 1);
		tent = EDICT_NUM (&sv_pr_state, 0);
		SVFIELD (ent, goalentity, entity) = EDICT_TO_PROG (&sv_pr_state, tent);
		return;
	}

	i = atoi (Cmd_Argv (1));
	if (i < 0 || i >= MAX_CLIENTS || svs.clients[i].state != cs_spawned ||
		svs.clients[i].spectator) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Invalid client to track\n");
		host_client->spec_track = 0;
		ent = EDICT_NUM (&sv_pr_state, host_client - svs.clients + 1);
		tent = EDICT_NUM (&sv_pr_state, 0);
		SVFIELD (ent, goalentity, entity) = EDICT_TO_PROG (&sv_pr_state, tent);
		return;
	}
	host_client->spec_track = i + 1;	// now tracking

	ent = EDICT_NUM (&sv_pr_state, host_client - svs.clients + 1);
	tent = EDICT_NUM (&sv_pr_state, i + 1);
	SVFIELD (ent, goalentity, entity) = EDICT_TO_PROG (&sv_pr_state, tent);
}


/*
	SV_Rate_f

	Change the bandwidth estimate for a client
*/
void
SV_Rate_f (void)
{
	int         rate;

	if (Cmd_Argc () != 2) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Current rate is %i\n",
						 (int) (1.0 / host_client->netchan.rate + 0.5));
		return;
	}

	rate = atoi (Cmd_Argv (1));
	if ((sv_maxrate->int_val) && (rate > sv_maxrate->int_val)) {
		rate = bound (500, rate, sv_maxrate->int_val);
	} else {
		rate = bound (500, rate, 10000);
	}

	SV_ClientPrintf (host_client, PRINT_HIGH, "Net rate set to %i\n", rate);
	host_client->netchan.rate = 1.0 / rate;
}


/*
	SV_Msg_f

	Change the message level for a client
*/
void
SV_Msg_f (void)
{
	if (Cmd_Argc () != 2) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Current msg level is %i\n",
						 host_client->messagelevel);
		return;
	}

	host_client->messagelevel = atoi (Cmd_Argv (1));

	SV_ClientPrintf (host_client, PRINT_HIGH, "Msg level set to %i\n",
					 host_client->messagelevel);
}

/*
	SV_SetInfo_f

	Allow clients to change userinfo
*/
void
SV_SetInfo_f (void)
{
	int         i;
	char        oldval[MAX_INFO_STRING];


	if (Cmd_Argc () == 1) {
		Con_Printf ("User info settings:\n");
		Info_Print (host_client->userinfo);
		return;
	}

	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*')
		return;							// don't set priveledged values

	strcpy (oldval, Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)));

	Info_SetValueForKey (host_client->userinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_INFO_STRING);
// name is extracted below in ExtractFromUserInfo
//  strncpy (host_client->name, Info_ValueForKey (host_client->userinfo, "name")
//      , sizeof(host_client->name)-1); 
//  SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//  host_client->sendinfo = true;

	if (strequal
		(Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)), oldval))
		return;								// key hasn't changed

	// process any changed values
	SV_ExtractFromUserinfo (host_client);

	i = host_client - svs.clients;
	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteString (&sv.reliable_datagram, Cmd_Argv (1));
	MSG_WriteString (&sv.reliable_datagram,
					 Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)));
}

/*
	SV_ShowServerinfo_f

	Dump serverinfo into a string
*/
void
SV_ShowServerinfo_f (void)
{
	Info_Print (svs.info);
}

void
SV_NoSnap_f (void)
{
	if (*host_client->uploadfn) {
		*host_client->uploadfn = 0;
		SV_BroadcastPrintf (PRINT_HIGH, "%s refused remote screenshot\n",
							host_client->name);
	}
}

typedef struct {
	char       *name;
	void        (*func) (void);
	int         no_redirect;
} ucmd_t;

ucmd_t      ucmds[] = {
	{"new", SV_New_f},
	{"modellist", SV_Modellist_f},
	{"soundlist", SV_Soundlist_f},
	{"prespawn", SV_PreSpawn_f},
	{"spawn", SV_Spawn_f},
	{"begin", SV_Begin_f, 1},

	{"drop", SV_Drop_f},
	{"pings", SV_Pings_f},

// issued by hand at client consoles    
	{"rate", SV_Rate_f},
	{"kill", SV_Kill_f, 1},
	{"pause", SV_Pause_f, 1},
	{"msg", SV_Msg_f},

	{"say", SV_Say_f, 1},
	{"say_team", SV_Say_Team_f, 1},

	{"setinfo", SV_SetInfo_f},

	{"serverinfo", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f, 1},
	{"nextdl", SV_NextDownload_f},

	{"ptrack", SV_PTrack_f},			// ZOID - used with autocam

	{"snap", SV_NoSnap_f},

};

static int
ucmds_compare (const void *_a, const void *_b)
{
	ucmd_t *a = (ucmd_t*)_a;
	ucmd_t *b = (ucmd_t*)_b;
	return strcmp (a->name, b->name);
}

/*
	SV_ExecuteUserCommand

	Uhh...execute user command. :)
*/
void
SV_ExecuteUserCommand (char *s)
{
	ucmd_t     *u;
	ucmd_t		cmd;

	Cmd_TokenizeString (s);
	sv_player = host_client->edict;
	cmd.name = Cmd_Argv(0);

	u = (ucmd_t*) bsearch (&cmd, ucmds, sizeof (ucmds) / sizeof (ucmds[0]),
						   sizeof (ucmds[0]), ucmds_compare);

	if (!u) {
		SV_BeginRedirect (RD_CLIENT);
		Con_Printf ("Bad user command: %s\n", Cmd_Argv (0));
		SV_EndRedirect ();
	} else {
		if (!u->no_redirect)
			SV_BeginRedirect (RD_CLIENT);
		u->func ();
		if (!u->no_redirect)
			SV_EndRedirect ();
	}
}

/*
	USER CMD EXECUTION
*/

/*
	SV_CalcRoll

	Used by view and sv_user
*/
float
SV_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t      forward, right, up;
	float       sign;
	float       side;
	float       value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs (side);

	value = cl_rollangle->value;

	if (side < cl_rollspeed->value)
		side = side * value / cl_rollspeed->value;
	else
		side = value;

	return side * sign;

}




//============================================================================

vec3_t      pmove_mins, pmove_maxs;

/*
	AddLinksToPmove
*/
void
AddLinksToPmove (areanode_t *node)
{
	link_t     *l, *next;
	edict_t    *check;
	int         pl;
	int         i;
	physent_t  *pe;

	pl = EDICT_TO_PROG (&sv_pr_state, sv_player);

	// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA (l);

		if (SVFIELD (check, owner, entity) == pl)
			continue;					// player's own missile
		if (SVFIELD (check, solid, float) == SOLID_BSP
			|| SVFIELD (check, solid, float) == SOLID_BBOX || SVFIELD (check, solid, float) == SOLID_SLIDEBOX) {
			if (check == sv_player)
				continue;

			for (i = 0; i < 3; i++)
				if (SVFIELD (check, absmin, vector)[i] > pmove_maxs[i]
					|| SVFIELD (check, absmax, vector)[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			if (pmove.numphysent == MAX_PHYSENTS)
				return;
			pe = &pmove.physents[pmove.numphysent];
			pmove.numphysent++;

			VectorCopy (SVFIELD (check, origin, vector), pe->origin);
			pe->info = NUM_FOR_EDICT (&sv_pr_state, check);

			if (SVFIELD (check, solid, float) == SOLID_BSP) {
				pe->model = sv.models[(int) (SVFIELD (check, modelindex, float))];
			} else {
				pe->model = NULL;
				VectorCopy (SVFIELD (check, mins, vector), pe->mins);
				VectorCopy (SVFIELD (check, maxs, vector), pe->maxs);
			}
		}
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (pmove_maxs[node->axis] > node->dist)
		AddLinksToPmove (node->children[0]);

	if (pmove_mins[node->axis] < node->dist)
		AddLinksToPmove (node->children[1]);
}


/*
	AddAllEntsToPmove

	For debugging
*/
void
AddAllEntsToPmove (void)
{
	int         e;
	edict_t    *check;
	int         i;
	physent_t  *pe;
	int         pl;

	pl = EDICT_TO_PROG (&sv_pr_state, sv_player);
	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (e = 1; e < sv.num_edicts; e++,
								   check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (SVFIELD (check, owner, entity) == pl)
			continue;
		if (SVFIELD (check, solid, float) == SOLID_BSP
			|| SVFIELD (check, solid, float) == SOLID_BBOX
			|| SVFIELD (check, solid, float) == SOLID_SLIDEBOX) {
			if (check == sv_player)
				continue;

			for (i = 0; i < 3; i++)
				if (SVFIELD (check, absmin, vector)[i] > pmove_maxs[i]
					|| SVFIELD (check, absmax, vector)[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			pe = &pmove.physents[pmove.numphysent];

			VectorCopy (SVFIELD (check, origin, vector), pe->origin);
			pmove.physents[pmove.numphysent].info = e;
			if (SVFIELD (check, solid, float) == SOLID_BSP)
				pe->model = sv.models[(int) (SVFIELD (check, modelindex, float))];
			else {
				pe->model = NULL;
				VectorCopy (SVFIELD (check, mins, vector), pe->mins);
				VectorCopy (SVFIELD (check, maxs, vector), pe->maxs);
			}

			if (++pmove.numphysent == MAX_PHYSENTS)
				break;
		}
	}
}

/*
	SV_PreRunCmd

	Done before running a player command.  Clears the touch array
*/
byte        playertouch[(MAX_EDICTS + 7) / 8];

void
SV_PreRunCmd (void)
{
	memset (playertouch, 0, sizeof (playertouch));
}

/*
	SV_RunCmd
*/
extern qboolean nouse;					// 1999-10-29 +USE fix by Maddes

void
SV_RunCmd (usercmd_t *ucmd, qboolean inside)
{
	edict_t    *ent;
	int         i, n, oldmsec;
	double      tmp_time;
	int         tmp_time1;

	if (!inside) {						// prevent infinite loop
		host_client->msecs += ucmd->msec;

		if ((sv_timekick->int_val)
			&& ((tmp_time = realtime - host_client->last_check) >=
				sv_timekick_interval->value)) {

			tmp_time1 = tmp_time * (1000 + sv_timekick_fuzz->value);

			if ((host_client->last_check != -1)	// don't do it if new player
				&& (host_client->msecs > tmp_time1)) {
				host_client->msec_cheating++;
				SV_BroadcastPrintf (PRINT_HIGH,
									va
									("%s thinks there are %d ms in %d seconds (Strike %d/%d)\n",
									 host_client->name, host_client->msecs,
									 (int) tmp_time, host_client->msec_cheating,
									 sv_timekick->int_val));

				if (host_client->msec_cheating >= sv_timekick->int_val) {
					SV_BroadcastPrintf (PRINT_HIGH, va ("Strike %d for %s!!\n",
														host_client->
														msec_cheating,
														host_client->name));
					SV_BroadcastPrintf (PRINT_HIGH,
										"Please see http://www.quakeforge.net/speed_cheat.php for infomation on QuakeForge's time cheat protection, and to explain how some may be cheating without knowing it.\n");
					SV_DropClient (host_client);
				}
			}

			host_client->msecs = 0;
			host_client->last_check = realtime;
		}
	}

	cmd = *ucmd;

	// chop up very long commands
	if (cmd.msec > 50) {
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		SV_RunCmd (&cmd, 1);
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		SV_RunCmd (&cmd, 1);
		return;
	}

	if (!SVFIELD (sv_player, fixangle, float))
		VectorCopy (ucmd->angles, SVFIELD (sv_player, v_angle, vector));

	SVFIELD (sv_player, button0, float) = ucmd->buttons & 1;
// 1999-10-29 +USE fix by Maddes  start
	if (!nouse) {
		SVFIELD (sv_player, button1, float) = (ucmd->buttons & 4) >> 2;
	}
// 1999-10-29 +USE fix by Maddes  end
	SVFIELD (sv_player, button2, float) = (ucmd->buttons & 2) >> 1;
	if (ucmd->impulse)
		SVFIELD (sv_player, impulse, float) = ucmd->impulse;

//
// angles
// show 1/3 the pitch angle and all the roll angle
	if (SVFIELD (sv_player, health, float) > 0) {
		if (!SVFIELD (sv_player, fixangle, float)) {
			SVFIELD (sv_player, angles, vector)[PITCH] = -SVFIELD (sv_player, v_angle, vector)[PITCH] / 3;
			SVFIELD (sv_player, angles, vector)[YAW] = SVFIELD (sv_player, v_angle, vector)[YAW];
		}
		SVFIELD (sv_player, angles, vector)[ROLL] =
			SV_CalcRoll (SVFIELD (sv_player, angles, vector), SVFIELD (sv_player, velocity, vector)) * 4;
	}

	sv_frametime = min (0.1, ucmd->msec * 0.001);

	if (!host_client->spectator) {
		*sv_globals.frametime = sv_frametime;

		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state,
															sv_player);
		PR_ExecuteProgram (&sv_pr_state,
						   sv_funcs.PlayerPreThink);

		SV_RunThink (sv_player);
	}

	for (i = 0; i < 3; i++)
		pmove.origin[i] =
			SVFIELD (sv_player, origin, vector)[i]
			+ (SVFIELD (sv_player, mins, vector)[i] - player_mins[i]);
	VectorCopy (SVFIELD (sv_player, velocity, vector), pmove.velocity);
	VectorCopy (SVFIELD (sv_player, v_angle, vector), pmove.angles);

	pmove.flying = SVFIELD (sv_player, movetype, float) == MOVETYPE_FLY;
	pmove.spectator = host_client->spectator;
	pmove.waterjumptime = SVFIELD (sv_player, teleport_time, float);
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	pmove.cmd = *ucmd;
	pmove.dead = SVFIELD (sv_player, health, float) <= 0;
	pmove.oldbuttons = host_client->oldbuttons;

	movevars.entgravity = host_client->entgravity;
	movevars.maxspeed = host_client->maxspeed;

	for (i = 0; i < 3; i++) {
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}

#if 0
	AddAllEntsToPmove ();
#else
	AddLinksToPmove (sv_areanodes);
#endif

#if 0
	{
		int         before, after;

		before = PM_TestPlayerPosition (pmove.origin);
		PlayerMove ();
		after = PM_TestPlayerPosition (pmove.origin);

		if (SVFIELD (sv_player, health, float) > 0 && before && !after)
			Con_Printf ("player %s got stuck in playermove!!!!\n",
						host_client->name);
	}
#else
	PlayerMove ();
#endif

	host_client->oldbuttons = pmove.oldbuttons;
	SVFIELD (sv_player, teleport_time, float) = pmove.waterjumptime;
	SVFIELD (sv_player, waterlevel, float) = waterlevel;
	SVFIELD (sv_player, watertype, float) = watertype;
	if (onground != -1) {
		SVFIELD (sv_player, flags, float) = (int) SVFIELD (sv_player, flags, float) | FL_ONGROUND;
		SVFIELD (sv_player, groundentity, entity) =
			EDICT_TO_PROG (&sv_pr_state, EDICT_NUM (&sv_pr_state, pmove.physents[onground].info));
	} else {
		SVFIELD (sv_player, flags, float) = (int) SVFIELD (sv_player, flags, float) & ~FL_ONGROUND;
	}
	for (i = 0; i < 3; i++)
		SVFIELD (sv_player, origin, vector)[i] =
			pmove.origin[i] - (SVFIELD (sv_player, mins, vector)[i] - player_mins[i]);

#if 0
	// truncate velocity the same way the net protocol will
	for (i = 0; i < 3; i++)
		SVFIELD (sv_player, velocity, vector)[i] = (int) pmove.velocity[i];
#else
	VectorCopy (pmove.velocity, SVFIELD (sv_player, velocity, vector));
#endif

	VectorCopy (pmove.angles, SVFIELD (sv_player, v_angle, vector));

	if (!host_client->spectator) {
		// link into place and touch triggers
		SV_LinkEdict (sv_player, true);

		// touch other objects
		for (i = 0; i < pmove.numtouch; i++) {
			n = pmove.physents[pmove.touchindex[i]].info;
			ent = EDICT_NUM (&sv_pr_state, n);
			if (!SVFIELD (ent, touch, func) || (playertouch[n / 8] & (1 << (n % 8))))
				continue;
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, ent);
			*sv_globals.other = EDICT_TO_PROG (&sv_pr_state, sv_player);
			PR_ExecuteProgram (&sv_pr_state, SVFIELD (ent, touch, func));
			playertouch[n / 8] |= 1 << (n % 8);
		}
	}
}

/*
	SV_PostRunCmd

	Done after running a player command.
*/
void
SV_PostRunCmd (void)
{
	// run post-think

	if (!host_client->spectator) {
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state,
															sv_player);
		PR_ExecuteProgram (&sv_pr_state,
						   sv_funcs.PlayerPostThink);
		SV_RunNewmis ();
	} else if (SpectatorThink) {
		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state,
															sv_player);
		PR_ExecuteProgram (&sv_pr_state, SpectatorThink);
	}
}


/*
	SV_ExecuteClientMessage

	The current net_message is parsed for the given client
*/
void
SV_ExecuteClientMessage (client_t *cl)
{
	int         c;
	char       *s;
	usercmd_t   oldest, oldcmd, newcmd;
	client_frame_t *frame;
	vec3_t      o;
	qboolean    move_issued = false;	// only allow one move command
	int         checksumIndex;
	byte        checksum, calculatedChecksum;
	int         seq_hash;

	// calc ping time
	frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = realtime - frame->senttime;

	// make sure the reply sequence number matches the incoming
	// sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;		// don't reply, sequences have
										// slipped      

	// save time for ping calculations
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	host_client = cl;
	sv_player = host_client->edict;

//  seq_hash = (cl->netchan.incoming_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
	cl->localtime = sv.time;
	cl->delta_sequence = -1;			// no delta unless requested
	while (1) {
		if (net_message->badread) {
			Con_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte (net_message);
		if (c == -1)
			return;						// Ender: Patched :)

		switch (c) {
			default:
				Con_Printf ("SV_ReadClientMessage: unknown command char\n");
				SV_DropClient (cl);
				return;

			case clc_nop:
				break;

			case clc_delta:
				cl->delta_sequence = MSG_ReadByte (net_message);
				break;

			case clc_move:
				if (move_issued)
					return;				// someone is trying to cheat...

				move_issued = true;

				checksumIndex = MSG_GetReadCount (net_message);
				checksum = (byte) MSG_ReadByte (net_message);

				// read loss percentage
				cl->lossage = MSG_ReadByte (net_message);

				MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
				MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
				MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);

				if (cl->state != cs_spawned)
					break;

				// if the checksum fails, ignore the rest of the packet
				calculatedChecksum =
					COM_BlockSequenceCRCByte (net_message->message->data + checksumIndex +
											  1,
											  MSG_GetReadCount (net_message) -
											  checksumIndex - 1, seq_hash);

				if (calculatedChecksum != checksum) {
					Con_DPrintf
						("Failed command checksum for %s(%d) (%d != %d)\n",
						 cl->name, cl->netchan.incoming_sequence, checksum,
						 calculatedChecksum);
					return;
				}

				if (!sv.paused) {
					SV_PreRunCmd ();

					if (net_drop < 20) {
						while (net_drop > 2) {
							SV_RunCmd (&cl->lastcmd, 0);
							net_drop--;
						}
						if (net_drop > 1)
							SV_RunCmd (&oldest, 0);
						if (net_drop > 0)
							SV_RunCmd (&oldcmd, 0);
					}
					SV_RunCmd (&newcmd, 0);

					SV_PostRunCmd ();
				}

				cl->lastcmd = newcmd;
				cl->lastcmd.buttons = 0;	// avoid multiple fires on lag
				break;


			case clc_stringcmd:
				s = MSG_ReadString (net_message);
				SV_ExecuteUserCommand (s);
				break;

			case clc_tmove:
				o[0] = MSG_ReadCoord (net_message);
				o[1] = MSG_ReadCoord (net_message);
				o[2] = MSG_ReadCoord (net_message);
				// only allowed by spectators
				if (host_client->spectator) {
					VectorCopy (o, SVFIELD (sv_player, origin, vector));
					SV_LinkEdict (sv_player, false);
				}
				break;

			case clc_upload:
				SV_NextUpload ();
				break;

		}
	}
}

/*
	SV_UserInit
*/
void
SV_UserInit (void)
{
	qsort (ucmds, sizeof (ucmds) / sizeof (ucmds[0]), sizeof (ucmds[0]),
		   ucmds_compare);
	cl_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL,
			"How quickly a player straightens out after strafing");
	cl_rollangle = Cvar_Get ("cl_rollangle", "2", CVAR_NONE, NULL,
			"How much a player's screen tilts when strafing");
	sv_spectalk = Cvar_Get ("sv_spectalk", "1", CVAR_NONE, NULL,
			"Toggles the ability of spectators to talk to players");
	sv_mapcheck = Cvar_Get ("sv_mapcheck", "1", CVAR_NONE, NULL, 
		"Toggle the use of map checksumming to check for players who edit maps to cheat");
}

