/*
	sv_init.c

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

#include "crc.h"
#include "msg.h"
#include "progdefs.h"
#include "quakefs.h"
#include "server.h"
#include "world.h"
#include "va.h"

server_t    sv;							// local server

char        localmodels[MAX_MODELS][5];	// inline model names for precache

char        localinfo[MAX_LOCALINFO_STRING + 1];	// local game info

entity_state_t baselines[MAX_EDICTS];

/*
	SV_ModelIndex
*/
int
SV_ModelIndex (char *name)
{
	int         i;

	if (!name || !name[0])
		return 0;

	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp (sv.model_precache[i], name))
			return i;
	if (i == MAX_MODELS || !sv.model_precache[i])
		SV_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
	SV_FlushSignon

	Moves to the next signon buffer if needed
*/
void
SV_FlushSignon (void)
{
	if (sv.signon.cursize < sv.signon.maxsize - 512)
		return;

	if (sv.num_signon_buffers == MAX_SIGNON_BUFFERS - 1)
		SV_Error ("sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
}

/*
	SV_CreateBaseline

	Entity baselines are used to compress the update messages
	to the clients -- only the fields that differ from the
	baseline will be transmitted
*/
void
SV_CreateBaseline (void)
{
	int         i;
	edict_t    *svent;
	int         entnum;

	for (entnum = 0; entnum < sv.num_edicts; entnum++) {
		svent = EDICT_NUM (&sv_pr_state, entnum);
		if (svent->free)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !((entvars_t*)&svent->v)->modelindex)
			continue;

		// 
		// create entity baseline
		// 
		VectorCopy (((entvars_t*)&svent->v)->origin, ((entity_state_t*)svent->data)->origin);
		VectorCopy (((entvars_t*)&svent->v)->angles, ((entity_state_t*)svent->data)->angles);
		((entity_state_t*)svent->data)->frame = ((entvars_t*)&svent->v)->frame;
		((entity_state_t*)svent->data)->skinnum = ((entvars_t*)&svent->v)->skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS) {
			((entity_state_t*)svent->data)->colormap = entnum;
			((entity_state_t*)svent->data)->modelindex = SV_ModelIndex ("progs/player.mdl");
		} else {
			((entity_state_t*)svent->data)->colormap = 0;
			((entity_state_t*)svent->data)->modelindex =
				SV_ModelIndex (PR_GetString (&sv_pr_state, ((entvars_t*)&svent->v)->model));
		}
		// LordHavoc: setup baseline to include new effects
		((entity_state_t*)svent->data)->alpha = 255;
		((entity_state_t*)svent->data)->scale = 16;
		((entity_state_t*)svent->data)->glowsize = 0;
		((entity_state_t*)svent->data)->glowcolor = 254;
		((entity_state_t*)svent->data)->colormap = 255;

		// 
		// flush the signon message out to a seperate buffer if
		// nearly full
		// 
		SV_FlushSignon ();

		// 
		// add to the message
		// 
		MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		MSG_WriteByte (&sv.signon, ((entity_state_t*)svent->data)->modelindex);
		MSG_WriteByte (&sv.signon, ((entity_state_t*)svent->data)->frame);
		MSG_WriteByte (&sv.signon, ((entity_state_t*)svent->data)->colormap);
		MSG_WriteByte (&sv.signon, ((entity_state_t*)svent->data)->skinnum);
		for (i = 0; i < 3; i++) {
			MSG_WriteCoord (&sv.signon, ((entity_state_t*)svent->data)->origin[i]);
			MSG_WriteAngle (&sv.signon, ((entity_state_t*)svent->data)->angles[i]);
		}
	}
}


/*
	SV_SaveSpawnparms

	Grabs the current state of the progs serverinfo flags 
	and each client for saving across the
	transition to another level
*/
void
SV_SaveSpawnparms (void)
{
	int         i, j;

	if (!sv.state)
		return;							// no progs loaded yet

	// serverflags is the only game related thing maintained
	svs.serverflags = ((globalvars_t*)sv_pr_state.pr_globals)->serverflags;

	for (i = 0, host_client = svs.clients; i < MAX_CLIENTS; i++, host_client++) {
		if (host_client->state != cs_spawned)
			continue;

		// needs to reconnect
		host_client->state = cs_connected;

		// call the progs to get default spawn parms for the new client
		((globalvars_t*)sv_pr_state.pr_globals)->self = EDICT_TO_PROG (&sv_pr_state, host_client->edict);
		PR_ExecuteProgram (&sv_pr_state, ((globalvars_t*)sv_pr_state.pr_globals)->SetChangeParms);
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			host_client->spawn_parms[j] = (&((globalvars_t*)sv_pr_state.pr_globals)->parm1)[j];
	}
}

/*
	SV_CalcPHS

	Expands the PVS and calculates the PHS
	(Potentially Hearable Set)
*/
void
SV_CalcPHS (void)
{
	int         rowbytes, rowwords;
	int         i, j, k, l, index, num;
	int         bitbyte;
	unsigned int *dest, *src;
	byte       *scan;
	int         count, vcount;

	Con_Printf ("Building PHS...\n");

	num = sv.worldmodel->numleafs;
	rowwords = (num + 31) >> 5;
	rowbytes = rowwords * 4;

	sv.pvs = Hunk_Alloc (rowbytes * num);
	scan = sv.pvs;
	vcount = 0;
	for (i = 0; i < num; i++, scan += rowbytes) {
		memcpy (scan, Mod_LeafPVS (sv.worldmodel->leafs + i, sv.worldmodel),
				rowbytes);
		if (i == 0)
			continue;
		for (j = 0; j < num; j++) {
			if (scan[j >> 3] & (1 << (j & 7))) {
				vcount++;
			}
		}
	}


	sv.phs = Hunk_Alloc (rowbytes * num);
	count = 0;
	scan = sv.pvs;
	dest = (unsigned int *) sv.phs;
	for (i = 0; i < num; i++, dest += rowwords, scan += rowbytes) {
		memcpy (dest, scan, rowbytes);
		for (j = 0; j < rowbytes; j++) {
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k = 0; k < 8; k++) {
				if (!(bitbyte & (1 << k)))
					continue;
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = ((j << 3) + k + 1);
				if (index >= num)
					continue;
				src = (unsigned int *) sv.pvs + index * rowwords;
				for (l = 0; l < rowwords; l++)
					dest[l] |= src[l];
			}
		}

		if (i == 0)
			continue;
		for (j = 0; j < num; j++)
			if (((byte *) dest)[j >> 3] & (1 << (j & 7)))
				count++;
	}

	Con_Printf ("Average leafs visible / hearable / total: %i / %i / %i\n",
				vcount / num, count / num, num);
}

unsigned int
SV_CheckModel (char *mdl)
{
	byte        stackbuf[1024];			// avoid dirtying the cache heap
	byte       *buf;
	unsigned short crc = 0;

//  int len;

	buf = (byte *) COM_LoadStackFile (mdl, stackbuf, sizeof (stackbuf));
	if (buf) {
		crc = CRC_Block (buf, com_filesize);
	} else {
		Con_Printf ("WARNING: cannot generate checksum for %s\n", mdl);
	}

	return crc;
}

/*
	SV_SpawnServer

	Change the server to a new map, taking all connected
	clients along with it.

	This is only called from the SV_Map_f() function.
*/
void
SV_SpawnServer (char *server)
{
	edict_t    *ent;
	int         i;

	Con_DPrintf ("SpawnServer: %s\n", server);

	SV_SaveSpawnparms ();

	svs.spawncount++;					// any partially connected client
										// will be
	// restarted

	sv.state = ss_dead;
	sv_pr_state.null_bad = 0;

	Mod_ClearAll ();
	Hunk_FreeToLowMark (host_hunklevel);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof (sv));

	sv.datagram.maxsize = sizeof (sv.datagram_buf);
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.allowoverflow = true;

	sv.reliable_datagram.maxsize = sizeof (sv.reliable_datagram_buf);
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.multicast.maxsize = sizeof (sv.multicast_buf);
	sv.multicast.data = sv.multicast_buf;

	sv.master.maxsize = sizeof (sv.master_buf);
	sv.master.data = sv.master_buf;

	sv.signon.maxsize = sizeof (sv.signon_buffers[0]);
	sv.signon.data = sv.signon_buffers[0];
	sv.num_signon_buffers = 1;

	strcpy (sv.name, server);

	// load progs to get entity field count
	// which determines how big each edict is
	SV_LoadProgs ();
	Info_SetValueForStarKey (svs.info, "*progs", va ("%i", sv_pr_state.crc),
							 MAX_SERVERINFO_STRING);

	// allocate edicts
	sv_pr_state.pr_edictareasize = sv_pr_state.pr_edict_size * MAX_EDICTS;
	sv.edicts = Hunk_AllocName (sv_pr_state.pr_edictareasize, "edicts");

	// leave slots at start for clients only
	sv.num_edicts = MAX_CLIENTS + 1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = EDICT_NUM (&sv_pr_state, i + 1);
		ent->data = &baselines[i];
		svs.clients[i].edict = ent;
//ZOID - make sure we update frags right
		svs.clients[i].old_frags = 0;
	}

	sv.time = 1.0;

	strncpy (sv.name, server, sizeof (sv.name));
	snprintf (sv.modelname, sizeof (sv.modelname), "maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, true);
	SV_CalcPHS ();

	// 
	// clear physics interaction links
	// 
	SV_ClearWorld ();

	sv.sound_precache[0] = sv_pr_state.pr_strings;

	sv.model_precache[0] = sv_pr_state.pr_strings;
	sv.model_precache[1] = sv.modelname;
	sv.models[1] = sv.worldmodel;
	for (i = 1; i < sv.worldmodel->numsubmodels; i++) {
		sv.model_precache[1 + i] = localmodels[i];
		sv.models[i + 1] = Mod_ForName (localmodels[i], false);
	}

	// check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel ("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel ("progs/eyes.mdl");

	// 
	// spawn the rest of the entities on the map
	// 

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	sv_pr_state.null_bad = 0;

	ent = EDICT_NUM (&sv_pr_state, 0);
	ent->free = false;
	((entvars_t*)&ent->v)->model = PR_SetString (&sv_pr_state, sv.worldmodel->name);
	((entvars_t*)&ent->v)->modelindex = 1;				// world model
	((entvars_t*)&ent->v)->solid = SOLID_BSP;
	((entvars_t*)&ent->v)->movetype = MOVETYPE_PUSH;

	((globalvars_t*)sv_pr_state.pr_globals)->mapname = PR_SetString (&sv_pr_state, sv.name);
	// serverflags are for cross level information (sigils)
	((globalvars_t*)sv_pr_state.pr_globals)->serverflags = svs.serverflags;

	// run the frame start qc function to let progs check cvars
	SV_ProgStartFrame ();

	// load and spawn all other entities
	ED_LoadFromFile (&sv_pr_state, sv.worldmodel->entities);

	// look up some model indexes for specialized message compression
	SV_FindModelNumbers ();

	// all spawning is completed, any further precache statements
	// or prog writes to the signon message are errors
	sv.state = ss_active;
	sv_pr_state.null_bad = 1;

	// run two frames to allow everything to settle
	sv_frametime = 0.1;
	SV_Physics ();
	SV_Physics ();

	// save movement vars
	SV_SetMoveVars ();

	// create a baseline for more efficient communications
	SV_CreateBaseline ();
	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;

	Info_SetValueForKey (svs.info, "map", sv.name, MAX_SERVERINFO_STRING);
	Con_DPrintf ("Server spawned.\n");
}
