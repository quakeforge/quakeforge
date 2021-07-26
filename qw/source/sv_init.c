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

#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/quakefs.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "qw/include/crudefile.h"
#include "qw/include/map_cfg.h"
#include "qw/pmove.h"
#include "qw/include/server.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_gib.h"
#include "world.h"

info_t     *localinfo;	// local game info
char        localmodels[MAX_MODELS][5];	// inline model names for precache

server_t    sv;							// local server


int
SV_ModelIndex (const char *name)
{
	int         i;

	if (!name || !name[0])
		return 0;

	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp (sv.model_precache[i], name))
			return i;
	if (i == MAX_MODELS || !sv.model_precache[i])
		Sys_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

static void
SV_NextSignon (void)
{
	int         size;

	if (!sv.max_signon_buffers
		|| sv.num_signon_buffers == sv.max_signon_buffers - 1) {
		sv.max_signon_buffers += MAX_SIGNON_BUFFERS;
		size = sv.max_signon_buffers * sizeof (int);
		sv.signon_buffer_size = realloc (sv.signon_buffer_size, size);
		size = sv.max_signon_buffers * MAX_DATAGRAM;
		sv.signon_buffers = realloc (sv.signon_buffers, size);
	}

	if (sv.num_signon_buffers)
		sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;
	sv.signon.maxsize = sizeof (sv.signon_buffers[0]);
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
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

	SV_NextSignon ();
}

/*
	SV_CreateBaseline

	Entity baselines are used to compress the update messages
	to the clients -- only the fields that differ from the
	baseline will be transmitted
*/
static void
SV_CreateBaseline (void)
{
	int			entnum;
	edict_t    *svent;

	for (entnum = 0; entnum < sv.num_edicts; entnum++) {
		svent = EDICT_NUM (&sv_pr_state, entnum);
		if (svent->free)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !SVfloat (svent, modelindex))
			continue;

		// create entity baseline
		VectorCopy (SVvector (svent, origin), SVdata (svent)->state.origin);
		VectorCopy (SVvector (svent, angles), SVdata (svent)->state.angles);
		SVdata (svent)->state.frame = SVfloat (svent, frame);
		SVdata (svent)->state.skinnum = SVfloat (svent, skin);
		if (entnum > 0 && entnum <= MAX_CLIENTS) {
			SVdata (svent)->state.colormap = entnum;
			SVdata (svent)->state.modelindex = SV_ModelIndex
				("progs/player.mdl");
		} else {
			SVdata (svent)->state.colormap = 0;
			SVdata (svent)->state.modelindex =
				SV_ModelIndex (PR_GetString (&sv_pr_state,
											 SVstring (svent, model)));
		}
		// LordHavoc: setup baseline to include new effects
		SVdata (svent)->state.alpha = 255;
		SVdata (svent)->state.scale = 16;
		SVdata (svent)->state.glow_size = 0;
		SVdata (svent)->state.glow_color = 254;
		SVdata (svent)->state.colormod = 255;

		// flush the signon message out to a separate buffer if nearly full
		SV_FlushSignon ();

		// add to the message
		MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		MSG_WriteByte (&sv.signon, SVdata (svent)->state.modelindex);
		MSG_WriteByte (&sv.signon, SVdata (svent)->state.frame);
		MSG_WriteByte (&sv.signon, SVdata (svent)->state.colormap);
		MSG_WriteByte (&sv.signon, SVdata (svent)->state.skinnum);

		MSG_WriteCoordAngleV (&sv.signon, &SVdata (svent)->state.origin[0],
							  SVdata (svent)->state.angles);//FIXME
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
	svs.serverflags = *sv_globals.serverflags;

	for (i = 0, host_client = svs.clients; i < MAX_CLIENTS; i++, host_client++)
	{
		if (host_client->state == cs_server) {
			// drop server allocated clients (FIXME for now)
			if (host_client->userinfo)
				Info_Destroy (host_client->userinfo);
			memset (host_client, 0, sizeof (*host_client));
			continue;
		}
		if (host_client->state != cs_spawned)
			continue;

		// needs to reconnect
		host_client->state = cs_connected;

		// call the progs to get default spawn parms for the new client
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, host_client->edict);
		PR_ExecuteProgram (&sv_pr_state, sv_funcs.SetChangeParms);
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			host_client->spawn_parms[j] = sv_globals.parms[j];
	}
}

static set_t *
sv_alloc_vis_array (unsigned numleafs)
{
	unsigned    size = SET_SIZE (numleafs);

	if (size > SET_DEFMAP_SIZE * SET_BITS) {
		set_t      *sets = Hunk_Alloc (numleafs * (sizeof (set_t) + size / 8));
		unsigned    words = size / SET_BITS;
		set_bits_t *bits = (set_bits_t *) (&sets[numleafs] + 1);
		for (unsigned i = 0; i < numleafs; i++) {
			sets[i].size = size;
			sets[i].map = bits;
			bits += words;
		}
		return sets;
	} else {
		set_t      *sets = Hunk_Alloc (numleafs * (sizeof (set_t) + size / 8));
		for (unsigned i = 0; i < numleafs; i++) {
			sets[i].size = size;
			sets[i].map = sets[i].defmap;
		}
		return sets;
	}
}

/*
	SV_CalcPHS

	Expands the PVS and calculates the PHS
	(Potentially Hearable Set)
*/
static void
SV_CalcPHS (void)
{
	int64_t     count, vcount;
	int         num, i;

	SV_Printf ("Building PHS...\n");
	num = sv.worldmodel->brush.numleafs;

	sv.pvs = sv_alloc_vis_array (num);
	vcount = 0;
	for (i = 0; i < num; i++) {
		Mod_LeafPVS_set (sv.worldmodel->brush.leafs + i, sv.worldmodel, 0xff,
						 &sv.pvs[i]);
		if (i == 0)
			continue;
		for (set_iter_t *iter = set_first (&sv.pvs[i]); iter;
			 iter = set_next (iter)) {
			vcount++;
		}
	}

	sv.phs = sv_alloc_vis_array (num);
	count = 0;
	for (i = 0; i < num; i++) {
		set_assign (&sv.phs[i], &sv.pvs[i]);

		for (set_iter_t *iter = set_first (&sv.pvs[i]); iter;
			 iter = set_next (iter)) {
			// or this pvs row into the phs
			// +1 because pvs is 1 based
			set_union (&sv.phs[i], &sv.pvs[iter->element + 1]);
		}

		if (i == 0)
			continue;
		for (set_iter_t *iter = set_first (&sv.phs[i]); iter;
			 iter = set_next (iter)) {
			count++;
		}
	}

	SV_Printf ("Average leafs visible / hearable / total: %i / %i / %i\n",
			   (int) (vcount / num), (int) (count / num), num);
}

static unsigned int
SV_CheckModel (const char *mdl)
{
	byte       *buf;
	unsigned short crc = 0;

//	int len;

	buf = (byte *) QFS_LoadFile (QFS_FOpenFile (mdl), 0);
	if (buf) {
		crc = CRC_Block (buf, qfs_filesize);
		free (buf);
	} else {
		SV_Printf ("WARNING: cannot generate checksum for %s\n", mdl);
	}

	return crc;
}

/*
	SV_SpawnServer

	Change the server to a new map, taking all connected
	clients along with it.

	This is called from only the SV_Map_f () function.
*/
void
SV_SpawnServer (const char *server)
{
	byte       *buf;
	edict_t    *ent;
	int         i;
	void       *so_buffers;
	int        *so_sizes;
	int         max_so;
	struct recorder_s *recorders;
	QFile      *ent_file;

	Sys_MaskPrintf (SYS_dev, "SpawnServer: %s\n", server);

	SV_SaveSpawnparms ();

	svs.spawncount++;					// any partially connected client
										// will be restarted

	sv.state = ss_dead;
	sv_pr_state.null_bad = 0;

	Mod_ClearAll ();
	Hunk_FreeToLowMark (host_hunklevel);

	// wipe the entire per-level structure, but don't lose sv.recorders
	// or signon buffers (good thing we're not multi-threaded FIXME?)
	recorders = sv.recorders;
	max_so = sv.max_signon_buffers;
	so_buffers = sv.signon_buffers;
	so_sizes = sv.signon_buffer_size;

	if (sv.name) {
		free (sv.name);
	}
	memset (&sv, 0, sizeof (sv));

	sv.recorders = recorders;
	sv.max_signon_buffers = max_so;
	sv.signon_buffers = so_buffers;
	sv.signon_buffer_size = so_sizes;

	sv.datagram.maxsize = sizeof (sv.datagram_buf);
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.allowoverflow = true;

	sv.reliable_datagram.maxsize = sizeof (sv.reliable_datagram_buf);
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.multicast.maxsize = sizeof (sv.multicast_buf);
	sv.multicast.data = sv.multicast_buf;

	sv.master.maxsize = sizeof (sv.master_buf);
	sv.master.data = sv.master_buf;

	SV_NextSignon ();

	sv.name = strdup(server);

	// load progs to get entity field count which determines how big each
	// edict is
	SV_LoadProgs ();
	SV_FreeAllEdictLeafs ();
	SV_SetupUserCommands ();
	Info_SetValueForStarKey (svs.info, "*progs", va (0, "%i", sv_pr_state.crc),
							 !sv_highchars->int_val);

	// leave slots at start for only clients
	sv.num_edicts = MAX_CLIENTS + 1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = EDICT_NUM (&sv_pr_state, i + 1);
		svs.clients[i].edict = ent;
// ZOID - make sure we update frags right
		svs.clients[i].old_frags = 0;
	}

	sv.time = 1.0;

	snprintf (sv.modelname, sizeof (sv.modelname), "maps/%s.bsp", server);
	map_cfg (sv.modelname, 0);
	sv.worldmodel = Mod_ForName (sv.modelname, true);
	map_cfg (sv.modelname, 1);
	SV_CalcPHS ();

	// clear physics interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = sv_pr_state.pr_strings;

	sv.model_precache[0] = sv_pr_state.pr_strings;
	sv.model_precache[1] = sv.modelname;
	sv.models[1] = sv.worldmodel;
	for (i = 1; i < sv.worldmodel->brush.numsubmodels; i++) {
		sv.model_precache[1 + i] = localmodels[i];
		sv.models[i + 1] = Mod_ForName (localmodels[i], false);
	}

	// check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel ("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel ("progs/eyes.mdl");

	// spawn the rest of the entities on the map

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	sv_pr_state.null_bad = 0;

	ent = EDICT_NUM (&sv_pr_state, 0);
	ent->free = false;
	SVstring (ent, model) = PR_SetString (&sv_pr_state, sv.worldmodel->path);
	SVfloat (ent, modelindex) = 1;				// world model
	SVfloat (ent, solid) = SOLID_BSP;
	SVfloat (ent, movetype) = MOVETYPE_PUSH;

	*sv_globals.mapname = PR_SetString (&sv_pr_state, sv.name);
	// serverflags are for cross level information (sigils)
	*sv_globals.serverflags = svs.serverflags;

	// close all CF files that progs didn't close from the last map
	CF_CloseAllFiles ();

	// run the frame start qc function to let progs check cvars
	SV_ProgStartFrame ();

	// load and spawn all other entities
	*sv_globals.time = sv.time;
	ent_file = QFS_VOpenFile (va (0, "maps/%s.ent", server), 0,
							  sv.worldmodel->vpath);
	if ((buf = QFS_LoadFile (ent_file, 0))) {
		ED_LoadFromFile (&sv_pr_state, (char *) buf);
		free (buf);
	} else {
		ED_LoadFromFile (&sv_pr_state, sv.worldmodel->brush.entities);
	}

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

	Info_SetValueForKey (svs.info, "map", sv.name, !sv_highchars->int_val);
	Sys_MaskPrintf (SYS_dev, "Server spawned.\n");
	if (sv_map_e->func)
		GIB_Event_Callback (sv_map_e, 1, server);
}

void
SV_SetMoveVars (void)
{
	movevars.gravity = sv_gravity->value;
	movevars.stopspeed = sv_stopspeed->value;
	movevars.maxspeed = sv_maxspeed->value;
	movevars.spectatormaxspeed = sv_spectatormaxspeed->value;
	movevars.accelerate = sv_accelerate->value;
	movevars.airaccelerate = sv_airaccelerate->value;
	movevars.wateraccelerate = sv_wateraccelerate->value;
	movevars.friction = sv_friction->value;
	movevars.waterfriction = sv_waterfriction->value;
	movevars.entgravity = 1.0;
}
