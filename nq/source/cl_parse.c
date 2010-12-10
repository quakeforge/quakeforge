/*
	cl_parse.c

	parse a message received from the server

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
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/input.h"
#include "QF/msg.h"
#include "QF/qfplist.h"
#include "QF/sys.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sound.h" // FIXME: DEFAULT_SOUND_PACKET_*
#include "QF/va.h"

#include "client.h"
#include "compat.h"
#include "host.h"
#include "sbar.h"
#include "server.h"
#include "game.h"

const char *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",				// [long] server version
	"svc_setview",				// [short] entity number
	"svc_sound",				// <see code>
	"svc_time",					// [float] server time
	"svc_print",				// [string] null terminated string
	"svc_stufftext",			// [string] stuffed into client's console
								// buffer the string should be \n terminated
	"svc_setangle",				// [vec3] set view angle to this absolute value

	"svc_serverinfo",			// [long] version
								// [string] signon string
								// [string]..[0]model cache
								// [string]...[0]sounds cache
								// [string]..[0]item cache
	"svc_lightstyle",			// [byte] [string]
	"svc_updatename",			// [byte] [string]
	"svc_updatefrags",			// [byte] [short]
	"svc_clientdata",			// <shortbits + data>
	"svc_stopsound",			// <see code>
	"svc_updatecolors",			// [byte] [byte]
	"svc_particle",				// [vec3] <variable>
	"svc_damage",				// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",			// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",				// [string] music [string] text
	"svc_cdtrack",				// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
	// end of iD svc
	// FIXME switchable extensions?
	// protocol 666
	"",
	"",
	"svc_skybox",
	"",
	"",
	"svc_bf",
	"svc_fog",
	"svc_spawnbaseline2",
	"svc_spawnstatic2",
	"svc_spawnstaticsound2",
};

float r_gravity;
dstring_t  *centerprint;

static void
CL_LoadSky (void)
{
	plitem_t   *item;
	const char *name = 0;

	if (!cl.worldspawn) {
		R_LoadSkys (0);
		return;
	}
	if ((item = PL_ObjectForKey (cl.worldspawn, "sky")) 
		|| (item = PL_ObjectForKey (cl.worldspawn, "skyname"))
		|| (item = PL_ObjectForKey (cl.worldspawn, "skyname"))) {
		name = PL_String (item);
	}
	R_LoadSkys (name);
}

/*
	CL_EntityNum

	This error checks and tracks the total number of entities
*/
static cl_entity_state_t *
CL_EntityNum (int num)
{
	if (num < 0 || num >= MAX_EDICTS)
		Host_Error ("CL_EntityNum: %i is an invalid number", num);
	if (num >= cl.num_entities) {
		while (cl.num_entities <= num) {
			cl_baselines[cl.num_entities].ent =
				&cl_entities[cl.num_entities];
			cl_entities[cl.num_entities].colormap = vid.colormap8;
			cl_entities[cl.num_entities].lerpflags |= (LERP_RESETMOVE
													   | LERP_RESETANIM);
			cl.num_entities++;
		}
	}

	return &cl_baselines[num];
}

static void
CL_ParseStartSoundPacket (void)
{
	float       attenuation;
	int         channel, ent, field_mask, sound_num, volume;
	vec3_t      pos;

	field_mask = MSG_ReadByte (net_message);

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte (net_message);
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte (net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (field_mask & SND_LARGEENTITY) {
		ent = (uint16_t) MSG_ReadShort (net_message);
		channel = MSG_ReadByte (net_message);
	} else {
		channel = (uint16_t) MSG_ReadShort (net_message);
		ent = channel >> 3;
		channel &= 7;
	}

	if (field_mask & SND_LARGESOUND)
		sound_num = (uint16_t) MSG_ReadShort (net_message);
	else
		sound_num = MSG_ReadByte (net_message);

	if (sound_num >= MAX_SOUNDS)
		Host_Error ("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	MSG_ReadCoordV (net_message, pos);

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos,
				  volume / 255.0, attenuation);
}

/*
	CL_KeepaliveMessage

	When the client is taking a long time to load stuff, send keepalive
	messages so the server doesn't disconnect.
*/
static void
CL_KeepaliveMessage (void)
{
	byte        olddata[8192];
	float       time;
	static float lastmsg;
	int         ret;
	sizebuf_t   old;

	if (sv.active)
		return;							// no need if server is local
	if (cls.demoplayback)
		return;

	// read messages from server, should just be nops
	old = *net_message->message;
	memcpy (olddata, net_message->message->data,
			net_message->message->cursize);

	do {
		ret = CL_GetMessage ();
		switch (ret) {
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");
		case 0:
			break;						// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			if (MSG_ReadByte (net_message) != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	*net_message->message = old;
	memcpy (net_message->message->data, olddata,
			net_message->message->cursize);

	// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

	// write out a nop
	Sys_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

static void
map_cfg (const char *mapname, int all)
{
	char       *name = malloc (strlen (mapname) + 4 + 1);
	cbuf_t     *cbuf = Cbuf_New (&id_interp);
	QFile      *f;

	QFS_StripExtension (mapname, name);
	strcat (name, ".cfg");
	QFS_FOpenFile (name, &f);
	if (f) {
		Qclose (f);
		Cmd_Exec_File (cbuf, name, 1);
	} else {
		Cmd_Exec_File (cbuf, "maps_default.cfg", 1);
	}
	if (all) {
		Cbuf_Execute_Stack (cbuf);
	} else {
		Cbuf_Execute_Sets (cbuf);
	}
	free (name);
	Cbuf_Delete (cbuf);
}

static void
CL_NewMap (const char *mapname)
{
	R_NewMap (cl.worldmodel, cl.model_precache, MAX_MODELS);
	Con_NewMap ();
	Sbar_CenterPrint (0);

	if (cl.model_precache[1] && cl.model_precache[1]->entities) {
		static progs_t edpr;

		cl.edicts = ED_Parse (&edpr, cl.model_precache[1]->entities);
		if (cl.edicts) {
			cl.worldspawn = PL_ObjectAtIndex (cl.edicts, 0);
			CL_LoadSky ();
			Fog_ParseWorldspawn (cl.worldspawn);
		}
	}

	map_cfg (mapname, 1);
}

static void
CL_ParseServerInfo (void)
{
	char        model_precache[MAX_MODELS][MAX_QPATH];
	char        sound_precache[MAX_SOUNDS][MAX_QPATH];
	const char *str;
	int         nummodels, numsounds, i;

	Sys_MaskPrintf (SYS_DEV, "Serverinfo packet received.\n");

	S_BlockSound ();
	S_StopAllSounds ();

	// wipe the client_state_t struct
	CL_ClearState ();

	// parse protocol version number
	i = MSG_ReadLong (net_message);
	if (i != PROTOCOL_NETQUAKE && i!= PROTOCOL_FITZQUAKE) {
		Host_Error ("Server returned version %i, not %i or %i\n", i,
					PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE);
		goto done;
	}
	cl.protocol = i;
	if (cl.protocol == PROTOCOL_FITZQUAKE)
		write_angles = MSG_WriteAngle16V;
	else
		write_angles = MSG_WriteAngleV;
	// parse maxclients
	cl.maxclients = MSG_ReadByte (net_message);
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD) {
		Sys_Printf ("Bad maxclients (%u) from server\n", cl.maxclients);
		goto done;
	}
	cl.scores = Hunk_AllocName (cl.maxclients * sizeof (*cl.scores), "scores");

	// parse gametype
	cl.gametype = MSG_ReadByte (net_message);

	// parse signon message
	str = MSG_ReadString (net_message);
	strncpy (cl.levelname, str, sizeof (cl.levelname) - 1);

	// separate the printfs so the server message can have a color
	Sys_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
				"\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n"
				"\n");
	Sys_Printf ("%c%s\n", 2, str);

	// first we go through and touch all of the precache data that still
	// happens to be in the cache, so precaching something else doesn't
	// needlessly purge it

	// precache models
	memset (cl.model_precache, 0, sizeof (cl.model_precache));
	for (nummodels = 1;; nummodels++) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS) {
			Sys_Printf ("Server sent too many model precaches\n");
			goto done;
		}
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

	// precache sounds
	memset (cl.sound_precache, 0, sizeof (cl.sound_precache));
	for (numsounds = 1;; numsounds++) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS) {
			Sys_Printf ("Server sent too many sound precaches\n");
			goto done;
		}
		strcpy (sound_precache[numsounds], str);
	}

	// now we try to load everything else until a cache allocation fails
	if (model_precache[1])
		map_cfg (model_precache[1], 0);

	for (i = 1; i < nummodels; i++) {
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == NULL) {
			Sys_Printf ("Model %s not found\n", model_precache[i]);
			goto done;
		}
		CL_KeepaliveMessage ();
	}

	for (i = 1; i < numsounds; i++) {
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}

	// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];
	if (!centerprint)
		centerprint = dstring_newstr ();
	else
		dstring_clearstr (centerprint);
	CL_NewMap (model_precache[1]);

	Hunk_Check ();						// make sure nothing is hurt

	noclip_anglehack = false;			// noclip is turned off at start    
	r_gravity = 800.0;					// Set up gravity for renderer effects
done:
	S_UnblockSound ();
}

int         bitcounts[16];

/*
	CL_ParseUpdate

	Parse an entity update message from the server
	If an entities model or origin changes from frame to frame, it must be
	relinked.  Other attributes can change without relinking.
*/
static void
CL_ParseUpdate (int bits)
{
	entity_t   *ent;
	cl_entity_state_t *state;
	int         modnum, num, skin, i;
	model_t    *model;
	qboolean    forcelink;

	if (cls.signon == SIGNONS - 1) {
		// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS) {
		i = MSG_ReadByte (net_message);
		bits |= (i << 8);
	}

	if (cl.protocol == PROTOCOL_FITZQUAKE) {
		if (bits & U_EXTEND1)
			bits |= MSG_ReadByte(net_message) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte(net_message) << 24;
	}
	
	if (bits & U_LONGENTITY)
		num = MSG_ReadShort (net_message);
	else
		num = MSG_ReadByte (net_message);

	state = CL_EntityNum (num);
	ent = state->ent;

	for (i = 0; i < 16; i++)
		if (bits & (1 << i))
			bitcounts[i]++;

	if (state->msgtime != cl.mtime[1])
		forcelink = true;				// no previous frame to lerp from
	else
		forcelink = false;

	if (state->msgtime + 0.2 < cl.mtime[0]) {
		//more than 0.2 seconds since the last message (most entities think
		//every 0.1 sec)
		//if we missed a think, we'd be lerping from the wrong frame
		ent->lerpflags |= LERP_RESETANIM;
	}

	if (forcelink) {		// FIXME: do this right (ie, protocol support)
		ent->colormod[0] = ent->colormod[1] = ent->colormod[2] =
			ent->colormod[3] = 1.0;
		ent->scale = 1.0;
	}

	state->msgtime = cl.mtime[0];

	if (bits & U_MODEL) {
		modnum = MSG_ReadByte (net_message);
		if (modnum >= MAX_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	} else
		modnum = state->baseline.modelindex;

	if (bits & U_FRAME)
		ent->frame = MSG_ReadByte (net_message);
	else
		ent->frame = state->baseline.frame;

	if (bits & U_COLORMAP)
		i = MSG_ReadByte (net_message);
	else
		i = state->baseline.colormap;
	if (!i)
		ent->colormap = vid.colormap8;
	else {
		if (i > cl.maxclients)
			Sys_Error ("i >= cl.maxclients");
		ent->colormap = cl.scores[i - 1].translations;
	}

	if (bits & U_SKIN)
		skin = MSG_ReadByte (net_message);
	else
		skin = state->baseline.skin;
	if (skin != ent->skinnum) {
		ent->skinnum = skin;
		if (num > 0 && num <= cl.maxclients) {
			if (!ent->skin)
				ent->skin = Skin_NewTempSkin ();
			if (ent->skin)
				CL_NewTranslation (num - 1, ent->skin);
		}
	}

	if (bits & U_EFFECTS)
		state->effects = MSG_ReadByte (net_message);
	else
		state->effects = state->baseline.effects;

	// shift the known values for interpolation
	VectorCopy (state->msg_origins[0], state->msg_origins[1]);
	VectorCopy (state->msg_angles[0], state->msg_angles[1]);

	if (bits & U_ORIGIN1)
		state->msg_origins[0][0] = MSG_ReadCoord (net_message);
	else
		state->msg_origins[0][0] = state->baseline.origin[0];
	if (bits & U_ANGLE1)
		state->msg_angles[0][0] = MSG_ReadAngle (net_message);
	else
		state->msg_angles[0][0] = state->baseline.angles[0];

	if (bits & U_ORIGIN2)
		state->msg_origins[0][1] = MSG_ReadCoord (net_message);
	else
		state->msg_origins[0][1] = state->baseline.origin[1];
	if (bits & U_ANGLE2)
		state->msg_angles[0][1] = MSG_ReadAngle (net_message);
	else
		state->msg_angles[0][1] = state->baseline.angles[1];

	if (bits & U_ORIGIN3)
		state->msg_origins[0][2] = MSG_ReadCoord (net_message);
	else
		state->msg_origins[0][2] = state->baseline.origin[2];
	if (bits & U_ANGLE3)
		state->msg_angles[0][2] = MSG_ReadAngle (net_message);
	else
		state->msg_angles[0][2] = state->baseline.angles[2];

	if (bits & U_STEP) {
		ent->lerpflags |= LERP_MOVESTEP;
		forcelink = true;
	} else {
		ent->lerpflags &= ~LERP_MOVESTEP;
	}

	if (cl.protocol == PROTOCOL_FITZQUAKE) {
		if (bits & U_ALPHA)
			state->alpha = MSG_ReadByte(net_message);
		else
			state->alpha = state->baseline.alpha;
		if (bits & U_FRAME2)
			ent->frame |= MSG_ReadByte(net_message) << 8;
		if (bits & U_MODEL2)
			modnum |= MSG_ReadByte(net_message) << 8;
		if (bits & U_LERPFINISH) {
			float       finish_time = MSG_ReadByte (net_message);
			ent->lerpfinish = state->msgtime + finish_time / 255;
			ent->lerpflags |= LERP_FINISH;
		} else {
			ent->lerpflags &= ~LERP_FINISH;
		}
	} else {
		state->alpha = state->baseline.alpha;
	}

	model = cl.model_precache[modnum];
	if (model != ent->model) {
		ent->model = model;
		// automatic animation (torches, etc) can be either all together
		// or randomized
		if (model) {
			if (model->synctype == ST_RAND)
				ent->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		} else
			forcelink = true;		// hack to make null model players work
		if (num > 0 && num <= cl.maxclients) {
			if (!ent->skin)
				ent->skin = Skin_NewTempSkin ();
			if (ent->skin)
				CL_NewTranslation (num - 1, ent->skin);
		}
		//johnfitz -- don't lerp animation across model changes
		ent->lerpflags |= LERP_RESETANIM;
	}

	if (forcelink) {					// didn't have an update last message
		VectorCopy (state->msg_origins[0], state->msg_origins[1]);
		VectorCopy (state->msg_origins[0], ent->origin);
		VectorCopy (state->msg_angles[0], state->msg_angles[1]);
		VectorCopy (state->msg_angles[0], ent->angles);
		state->forcelink = true;
	}
}

static void
CL_ParseBaseline (cl_entity_state_t *state, int version)
{
	int         bits = 0;

	if (version == 2)
		bits = MSG_ReadByte (net_message);

	if (bits & B_LARGEMODEL)
		state->baseline.modelindex = MSG_ReadShort (net_message);
	else
		state->baseline.modelindex = MSG_ReadByte (net_message);

	if (bits & B_LARGEFRAME)
		state->baseline.frame = MSG_ReadShort (net_message);
	else
		state->baseline.frame = MSG_ReadByte (net_message);

	state->baseline.colormap = MSG_ReadByte (net_message);
	state->baseline.skin = MSG_ReadByte (net_message);

	MSG_ReadCoordAngleV (net_message, state->baseline.origin,
						 state->baseline.angles);

	if (bits & B_ALPHA)
		state->baseline.alpha = MSG_ReadByte (net_message);
	else
		state->baseline.alpha = ENTALPHA_DEFAULT;
}

/*
	CL_ParseClientdata

	Server information pertaining to only this client
*/
static void
CL_ParseClientdata (void)
{
	int         i, j;
	int         bits;

	bits = (uint16_t) MSG_ReadShort (net_message);
	if (bits & SU_EXTEND1)
		bits |= MSG_ReadByte (net_message) << 16;
	if (bits & SU_EXTEND2)
		bits |= MSG_ReadByte (net_message) << 24;

	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = ((signed char) MSG_ReadByte (net_message));
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = ((signed char) MSG_ReadByte (net_message));
	else
		cl.idealpitch = 0;

	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i = 0; i < 3; i++) {
		if (bits & (SU_PUNCH1 << i))
			cl.punchangle[i] = ((signed char) MSG_ReadByte (net_message));
		else
			cl.punchangle[i] = 0;
		if (bits & (SU_VELOCITY1 << i))
			cl.mvelocity[0][i] = ((signed char) MSG_ReadByte (net_message))
				* 16;
		else
			cl.mvelocity[0][i] = 0;
	}

	//FIXME
	//if (!VectorCompare (v_punchangles[0], cl.punchangle[0])) {
	//	VectorCopy (v_punchangles[0], v_punchangles[1]);
	//	VectorCopy (cl.punchangle, v_punchangles[0]);
	//}

	// [always sent]    if (bits & SU_ITEMS)
	i = MSG_ReadLong (net_message);

	if (cl.stats[STAT_ITEMS] != i) {				// set flash times
		Sbar_Changed ();
		for (j = 0; j < 32; j++)
			if ((i & (1 << j)) && !(cl.stats[STAT_ITEMS] & (1 << j)))
				cl.item_gettime[j] = cl.time;
		cl.stats[STAT_ITEMS] = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte (net_message);
	else
		cl.stats[STAT_WEAPONFRAME] = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte (net_message);
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i) {
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	if (bits & SU_WEAPON)
		i = MSG_ReadByte (net_message);
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i) {
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();
		//johnfitz -- lerping
		if (cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
			cl.viewent.lerpflags |= LERP_RESETANIM;
	}

	i = MSG_ReadShort (net_message);
	if (cl.stats[STAT_HEALTH] != i) {
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte (net_message);
	if (cl.stats[STAT_AMMO] != i) {
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i = 0; i < 4; i++) {
		j = MSG_ReadByte (net_message);
		if (cl.stats[STAT_SHELLS + i] != j) {
			cl.stats[STAT_SHELLS + i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte (net_message);

	if (standard_quake) {
		if (cl.stats[STAT_ACTIVEWEAPON] != i) {
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	} else {
		if (cl.stats[STAT_ACTIVEWEAPON] != (1 << i)) {
			cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
			Sbar_Changed ();
		}
	}

	if (bits & SU_WEAPON2)
		cl.stats[STAT_WEAPON] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_ARMOR2)
		cl.stats[STAT_ARMOR] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_AMMO2)
		cl.stats[STAT_AMMO] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_SHELLS2)
		cl.stats[STAT_SHELLS] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_NAILS2)
		cl.stats[STAT_NAILS] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_ROCKETS2)
		cl.stats[STAT_ROCKETS] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_CELLS2)
		cl.stats[STAT_CELLS] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_WEAPONFRAME2)
		cl.stats[STAT_WEAPONFRAME] |= MSG_ReadByte (net_message) << 8;
	if (bits & SU_WEAPONALPHA) {
		byte alpha = MSG_ReadByte (net_message);
		cl.viewent.colormod[3] = ENTALPHA_DECODE (alpha);
	} else {
		cl.viewent.colormod[3] = 1.0;
	}
}

static void
CL_ParseStatic (int version)
{
	cl_entity_state_t state;
	entity_t   *ent;

	ent = R_AllocEntity ();
	CL_Init_Entity (ent);

	CL_ParseBaseline (&state, version);

	// copy it to the current state
	ent->model = cl.model_precache[state.baseline.modelindex];
	ent->lerpflags |= LERP_RESETANIM;
	ent->frame = state.baseline.frame;

	ent->colormap = vid.colormap8;
	ent->skinnum = state.baseline.skin;
	VectorSet (1.0, 1.0, 1.0, ent->colormod);
	ent->colormod[3] = ENTALPHA_DECODE (state.baseline.alpha);
	ent->scale = 1.0;

	VectorCopy (state.baseline.origin, ent->origin);
	VectorCopy (state.baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

static void
CL_ParseStaticSound (int version)
{
	int         sound_num, vol, atten;
	vec3_t      org;

	MSG_ReadCoordV (net_message, org);
	if (version == 2)
		sound_num = MSG_ReadShort (net_message);
	else
		sound_num = MSG_ReadByte (net_message);
	vol = MSG_ReadByte (net_message);
	atten = MSG_ReadByte (net_message);

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

#define SHOWNET(x) \
	if (cl_shownet->int_val == 2) \
		Sys_Printf ("%3i:%s\n", net_message->readcount - 1, x);

int viewentity;

void
CL_ParseServerMessage (void)
{
	int         cmd, i;
	const char *str;

	// if recording demos, copy the message out
	if (cl_shownet->int_val == 1)
		Sys_Printf ("%i ", net_message->message->cursize);
	else if (cl_shownet->int_val == 2)
		Sys_Printf ("------------------\n");

	cl.onground = false;				// unless the server says otherwise 

	// parse the message
	MSG_BeginReading (net_message);

	while (1) {
		if (net_message->badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte (net_message);

		if (cmd == -1) {
			SHOWNET ("END OF MESSAGE");
			return;						// end of message
		}
		// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL) {
			SHOWNET ("fast update");
			CL_ParseUpdate (cmd & ~U_SIGNAL);
			continue;
		}

		SHOWNET (va ("%s(%d)", svc_strings[cmd], cmd));

		// other commands
		switch (cmd) {
			default:
				Host_Error ("CL_ParseServerMessage: Illegible server "
							"message\n");
				break;

			case svc_nop:
				break;

			case svc_time:
				cl.mtime[1] = cl.mtime[0];
				cl.mtime[0] = MSG_ReadFloat (net_message);
				break;

			case svc_clientdata:
				CL_ParseClientdata ();
				break;

			case svc_version:
				i = MSG_ReadLong (net_message);
				if (i != PROTOCOL_NETQUAKE && i!= PROTOCOL_FITZQUAKE)
					Host_Error ("CL_ParseServerMessage: Server is protocol %i "
								"instead of %i or %i\n", i, PROTOCOL_NETQUAKE,
								PROTOCOL_FITZQUAKE);
				cl.protocol = i;
				break;

			case svc_disconnect:
				Host_EndGame ("Server disconnected\n");

			case svc_print:
				Sys_Printf ("%s", MSG_ReadString (net_message));
				break;

			case svc_centerprint:
				str = MSG_ReadString (net_message);
				if (strcmp (str, centerprint->str)) {
					dstring_copystr (centerprint, str);
					//FIXME logging
				}
				Sbar_CenterPrint (str);
				break;

			case svc_stufftext:
				Cbuf_AddText (host_cbuf, MSG_ReadString (net_message));
				break;

			case svc_damage:
				V_ParseDamage ();
				break;

			case svc_serverinfo:
				CL_ParseServerInfo ();
				vid.recalc_refdef = true;	// leave intermission full screen
				break;

			case svc_setangle:
				MSG_ReadAngleV (net_message, cl.viewangles);
				break;

			case svc_setview:
				cl.viewentity = MSG_ReadShort (net_message);
				viewentity = cl.viewentity; // FIXME: evil hack
				break;

			case svc_lightstyle:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_LIGHTSTYLES)
					Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
				strcpy (r_lightstyle[i].map, MSG_ReadString (net_message));
				r_lightstyle[i].length = strlen (r_lightstyle[i].map);
				// FIXME extra info
				break;

			case svc_sound:
				CL_ParseStartSoundPacket ();
				break;

			case svc_stopsound:
				i = MSG_ReadShort (net_message);
				S_StopSound (i >> 3, i & 7);
				break;

			case svc_updatename:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients)
					Host_Error ("CL_ParseServerMessage: svc_updatename > "
								"MAX_SCOREBOARD");
				strcpy (cl.scores[i].name, MSG_ReadString (net_message));
				break;

			case svc_updatefrags:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients)
					Host_Error ("CL_ParseServerMessage: svc_updatefrags > "
								"MAX_SCOREBOARD");
				cl.scores[i].frags = MSG_ReadShort (net_message);
				break;

			case svc_updatecolors:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients) {
					Host_Error ("CL_ParseServerMessage: svc_updatecolors > "
								"MAX_SCOREBOARD");
				} else {
					entity_t   *ent = &cl_entities[i+1];
					cl.scores[i].colors = MSG_ReadByte (net_message);
					if (!ent->skin)
						ent->skin = Skin_NewTempSkin ();
					if (ent->skin)
						CL_NewTranslation (i, ent->skin);
				}
				break;

			case svc_particle:
				CL_ParseParticleEffect ();
				break;

			case svc_spawnbaseline:
				i = MSG_ReadShort (net_message);
				// must use CL_EntityNum () to force cl.num_entities up
				CL_ParseBaseline (CL_EntityNum (i), 1);
				break;
			case svc_spawnstatic:
				CL_ParseStatic (1);
				break;
			case svc_temp_entity:
				CL_ParseTEnt ();
				break;

			case svc_setpause:
			{
				r_paused = cl.paused = MSG_ReadByte (net_message);

				if (cl.paused) {
					CDAudio_Pause ();
				} else {
					CDAudio_Resume ();
				}
			}
			break;

			case svc_signonnum:
				i = MSG_ReadByte (net_message);
				if (i <= cls.signon)
					Host_Error ("Received signon %i when at %i", i,
								cls.signon);
				cls.signon = i;
				CL_SignonReply ();
				break;

			case svc_killedmonster:
				cl.stats[STAT_MONSTERS]++;
				break;

			case svc_foundsecret:
				cl.stats[STAT_SECRETS]++;
				break;

			case svc_updatestat:
				i = MSG_ReadByte (net_message);
				if (i < 0 || i >= MAX_CL_STATS)
					Sys_Error ("svc_updatestat: %i is invalid", i);
				cl.stats[i] = MSG_ReadLong (net_message);
				break;

			case svc_spawnstaticsound:
				CL_ParseStaticSound (1);
				break;

			case svc_cdtrack:
				cl.cdtrack = MSG_ReadByte (net_message);
				cl.looptrack = MSG_ReadByte (net_message);
				if ((cls.demoplayback || cls.demorecording)
					&& (cls.forcetrack != -1))
					CDAudio_Play ((byte) cls.forcetrack, true);
				else
					CDAudio_Play ((byte) cl.cdtrack, true);
				break;

			case svc_intermission:
				cl.intermission = 1;
				r_force_fullscreen = 1;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				break;

			case svc_finale:
				cl.intermission = 2;
				r_force_fullscreen = 1;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				str = MSG_ReadString (net_message);
				if (strcmp (str, centerprint->str)) {
					dstring_copystr (centerprint, str);
					//FIXME logging
				}
				Sbar_CenterPrint (str);
				break;

			case svc_cutscene:
				cl.intermission = 3;
				r_force_fullscreen = 1;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				str = MSG_ReadString (net_message);
				if (strcmp (str, centerprint->str)) {
					dstring_copystr (centerprint, str);
					//FIXME logging
				}
				Sbar_CenterPrint (str);
				break;

			case svc_sellscreen:
				Cmd_ExecuteString ("help", src_command);
				break;

			// PROTOCOL_FITZQUAKE
			case svc_skybox:
				R_LoadSkys (MSG_ReadString(net_message));
				break;
			case svc_bf:
				Cmd_ExecuteString ("bf", src_command);
				break;
			case svc_fog:
				{
					float density, red, green, blue, time;
					density = MSG_ReadByte (net_message) / 255.0;
					red = MSG_ReadByte (net_message) / 255.0;
					green = MSG_ReadByte (net_message) / 255.0;
					blue = MSG_ReadByte (net_message) / 255.0;
					time = MSG_ReadShort (net_message) / 100.0;
					time = max (0.0, time);
					Fog_Update (density, red, green, blue, time);
				}
				break;
			case svc_spawnbaseline2:
				i = MSG_ReadShort (net_message);
				// must use CL_EntityNum() to force cl.num_entities up
				CL_ParseBaseline (CL_EntityNum(i), 2);
				break;
			case svc_spawnstatic2:
				CL_ParseStatic (2);
				break;
			case svc_spawnstaticsound2:
				CL_ParseStaticSound (2);
				break;
		}
	}
}
